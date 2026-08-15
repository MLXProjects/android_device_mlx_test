/*
 * vtouch - multi-touch to single-touch input proxy daemon.
 *
 * Finds the first /dev/input/event* node that reports itself as a
 * multi-touch screen (falling back to a plain absolute single-touch
 * device), creates a single-touch uinput device named "mlx_vtouch",
 * preserves the original node, then substitutes the virtual node in
 * its place (e.g. /dev/input/event4 is replaced by the virtual node).
 *
 * Signals:
 *   SIGUSR1  stop the relay, destroy the virtual device, recreate it
 *            and resume relaying (source fd stays open)
 *   SIGINT/SIGTERM  stop the relay, destroy the virtual device,
 *            restore the original node and exit cleanly
 *
 * Usage:
 *   vtouch      run the proxy daemon
 *   vtouch -r   signal the running daemon to restart the relay
 *   vtouch -q   signal the running daemon to quit (restore + exit)
 *   vtouch -h   show help
 *   vtouch -v   show version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define VIRT_NAME       "mlx_vtouch"
#define VERSION         "1.2.0"
#define MAX_SLOTS       16

#define LOG_PATH        "/tmp/vtouch.log"
#define SAVED_NODE      "/dev/input/vtouch_saved"

#ifndef ABS_MT_TOUCH_MAJOR
#define ABS_MT_TOUCH_MAJOR      0x30
#endif
#ifndef ABS_MT_SLOT
#define ABS_MT_SLOT             0x2f
#endif
#ifndef ABS_MT_POSITION_X
#define ABS_MT_POSITION_X       0x35
#endif
#ifndef ABS_MT_POSITION_Y
#define ABS_MT_POSITION_Y       0x36
#endif
#ifndef ABS_MT_TRACKING_ID
#define ABS_MT_TRACKING_ID      0x39
#endif

#define WORD_SIZE       (8 * sizeof(unsigned long))

enum {
    CMD_NONE = 0,
    CMD_RESTART = 1,
    CMD_EXIT = 2,
};

static volatile sig_atomic_t g_cmd = CMD_NONE;

static int g_rfd = -1;
static int g_vfd = -1;
static char g_real_path[128];
static char g_saved_path[128];
static char g_virt_node[16];

static int bit_test(const unsigned long *bits, int bit)
{
    return (bits[bit / WORD_SIZE] >> (bit % WORD_SIZE)) & 1;
}

static void vtlog(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);

    {
        FILE *f = fopen(LOG_PATH, "a");
        if (f) {
            va_start(ap, fmt);
            vfprintf(f, fmt, ap);
            va_end(ap);
            fclose(f);
        }
    }
}

static void ensure_tmp(void)
{
    mkdir("/tmp", 0755);
}

static void print_help(void)
{
    printf("MLX VirtualTouch v%s\n", VERSION);
    printf("usage: vtouch [COMMAND] [DEVICE]\n");
    printf("[DEVICE] the source device node, e.g. /dev/input/event4\n");
    printf("[COMMAND] one of the following:\n");
    printf("-h Show this help\n");
    printf("-q Quit event relay\n");
    printf("-r Restart event relay\n");
    printf("-v Show version\n");
}

static void on_usr1(int sig)
{
    (void)sig;
    g_cmd = CMD_RESTART;
}

static void on_term(int sig)
{
    (void)sig;
    g_cmd = CMD_EXIT;
}

static void install_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* deliberately no SA_RESTART: let read() return EINTR */
    sa.sa_handler = on_usr1;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = on_term;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* Find the first running vtouch instance other than ourselves. */
static int find_vtouch_pid(void)
{
    DIR *d = opendir("/proc");
    struct dirent *de;
    int pid;

    if (!d)
        return -1;
    while ((de = readdir(d))) {
        char path[64];
        char comm[32];
        int fd, i;

        for (i = 0; de->d_name[i] >= '0' && de->d_name[i] <= '9'; i++)
            ;
        if (de->d_name[i] != '\0')
            continue;
        pid = atoi(de->d_name);
        if (pid == getpid())
            continue;
        snprintf(path, sizeof(path), "/proc/%s/comm", de->d_name);
        fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;
        i = read(fd, comm, sizeof(comm) - 1);
        close(fd);
        if (i <= 0)
            continue;
        comm[i] = '\0';
        if (comm[i - 1] == '\n')
            comm[i - 1] = '\0';
        if (strcmp(comm, "vtouch") == 0) {
            closedir(d);
            return pid;
        }
    }
    closedir(d);
    return -1;
}

static int signal_daemon(int sig)
{
    int pid = find_vtouch_pid();

    if (pid <= 0) {
        vtlog("vtouch: no running instance found\n");
        return 1;
    }
    if (kill(pid, sig) != 0) {
        vtlog("vtouch: signaling pid %d: %s\n", pid, strerror(errno));
        return 1;
    }
    vtlog("vtouch: signaled pid %d with sig %d\n", pid, sig);
    return 0;
}

static int scan_events(char names[][16], int max)
{
    DIR *d = opendir("/dev/input");
    struct dirent *de;
    int n = 0;

    if (!d)
        return 0;
    while ((de = readdir(d)) && n < max) {
        if (strncmp(de->d_name, "event", 5))
            continue;
        if (!de->d_name[5])
            continue;
        snprintf(names[n], 16, "%s", de->d_name);
        n++;
    }
    closedir(d);
    return n;
}

/* Prefer a multi-touch screen, fall back to a plain single-touch device. */
static int find_touch_device(char names[][16], int count,
                             char real_path[], size_t rsize, int *is_mt)
{
    unsigned long abs[((ABS_MAX + 1) / WORD_SIZE) + 1];
    unsigned long keys[((KEY_MAX + 1) / WORD_SIZE) + 1];
    char path[128];
    int fd, i;

    for (i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "/dev/input/%s", names[i]);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            vtlog("vtouch: failed to open %s\n", path);
            continue;
        }
        memset(abs, 0, sizeof(abs));
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) >= 0 &&
            bit_test(abs, ABS_MT_POSITION_X) &&
            bit_test(abs, ABS_MT_POSITION_Y)) {
            snprintf(real_path, rsize, "%s", path);
            *is_mt = 1;
            return fd;
        }
        vtlog("vtouch: %s was not multitouch\n", path);
        close(fd);
    }

    for (i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "/dev/input/%s", names[i]);
        fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;
        memset(abs, 0, sizeof(abs));
        memset(keys, 0, sizeof(keys));
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs);
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
        if (bit_test(abs, ABS_X) && bit_test(abs, ABS_Y) &&
            bit_test(keys, BTN_TOUCH)) {
            snprintf(real_path, rsize, "%s", path);
            *is_mt = 0;
            return fd;
        }
        close(fd);
    }
    return -1;
}

static int uinput_create(int rfd)
{
    unsigned long keys[((KEY_MAX + 1) / WORD_SIZE) + 1];
    struct input_absinfo xi, yi;
    struct uinput_user_dev udev;
    int ufd, i;

    vtlog("uinput: opening /dev/uinput\n");
    ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) {
        vtlog("uinput: open /dev/uinput: %s\n", strerror(errno));
        return -1;
    }

    vtlog("uinput: setting EV/KEY/ABS bits\n");
    ioctl(ufd, UI_SET_EVBIT, EV_KEY);
    ioctl(ufd, UI_SET_EVBIT, EV_ABS);
    ioctl(ufd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(ufd, UI_SET_ABSBIT, ABS_X);
    ioctl(ufd, UI_SET_ABSBIT, ABS_Y);

    memset(keys, 0, sizeof(keys));
    if (ioctl(rfd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0) {
        for (i = 0; i <= KEY_MAX; i++) {
            if (bit_test(keys, i)) {
                vtlog("uinput: advertising key 0x%x\n", i);
                ioctl(ufd, UI_SET_KEYBIT, i);
            }
        }
    }

    if (ioctl(rfd, EVIOCGABS(ABS_MT_POSITION_X), &xi) != 0) {
        vtlog("uinput: no ABS_MT_POSITION_X range, using 0..0x7fff\n");
        xi.minimum = 0;
        xi.maximum = 0x7fff;
    }
    if (ioctl(rfd, EVIOCGABS(ABS_MT_POSITION_Y), &yi) != 0) {
        vtlog("uinput: no ABS_MT_POSITION_Y range, using 0..0x7fff\n");
        yi.minimum = 0;
        yi.maximum = 0x7fff;
    }
    vtlog("uinput: abs x=[%d,%d] y=[%d,%d]\n",
          xi.minimum, xi.maximum, yi.minimum, yi.maximum);

    memset(&udev, 0, sizeof(udev));
    strncpy(udev.name, VIRT_NAME, UINPUT_MAX_NAME_SIZE - 1);
    udev.id.bustype = 0x03;
    udev.absmin[ABS_X] = xi.minimum;
    udev.absmax[ABS_X] = xi.maximum;
    udev.absmin[ABS_Y] = yi.minimum;
    udev.absmax[ABS_Y] = yi.maximum;
    vtlog("uinput: writing device struct\n");
    if (write(ufd, &udev, sizeof(udev)) != (ssize_t)sizeof(udev)) {
        vtlog("uinput: write: %s\n", strerror(errno));
        close(ufd);
        return -1;
    }
    vtlog("uinput: UI_DEV_CREATE\n");
    if (ioctl(ufd, UI_DEV_CREATE) != 0) {
        vtlog("uinput: UI_DEV_CREATE: %s\n", strerror(errno));
        close(ufd);
        return -1;
    }
    vtlog("uinput: created '%s' fd=%d\n", VIRT_NAME, ufd);
    return ufd;
}

/* Find the /dev/input/eventN node of our virtual device by name. */
static int find_self_node(char node[], size_t nsize)
{
    DIR *d = opendir("/sys/class/input");
    struct dirent *de;
    char path[256];
    char name[UINPUT_MAX_NAME_SIZE + 1];

    if (!d)
        return -1;
    while ((de = readdir(d))) {
        int fd;
        ssize_t n;

        if (strncmp(de->d_name, "event", 5) || !de->d_name[5])
            continue;
        snprintf(path, sizeof(path), "/sys/class/input/%s/device/name",
                 de->d_name);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            snprintf(path, sizeof(path), "/sys/class/input/%s/name",
                     de->d_name);
            fd = open(path, O_RDONLY);
            if (fd < 0)
                continue;
        }
        n = read(fd, name, sizeof(name) - 1);
        close(fd);
        if (n <= 0)
            continue;
        name[n] = '\0';
        if (name[n - 1] == '\n')
            name[n - 1] = '\0';
        if (strcmp(name, VIRT_NAME) == 0) {
            snprintf(node, nsize, "%s", de->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

static void emit(int ufd, __u16 type, __u16 code, __s32 value)
{
    struct input_event ev;

    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, NULL);
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(ufd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev))
        vtlog("vtouch: write uinput event: %s\n", strerror(errno));
}

/* Returns CMD_RESTART/CMD_EXIT on signal, -1 on read error. */
static int relay_loop(int rfd, int ufd, int is_mt)
{
    struct input_event ev;
    int slot[MAX_SLOTS];
    int sx[MAX_SLOTS], sy[MAX_SLOTS];
    int first, cur = 0, down = 0, i;

    memset(slot, 0, sizeof(slot));
    memset(sx, 0, sizeof(sx));
    memset(sy, 0, sizeof(sy));

    for (;;) {
        if (read(rfd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev)) {
            if (errno == EINTR) {
                if (g_cmd != CMD_NONE)
                    return g_cmd;
                continue;
            }
            return -1;
        }

        switch (ev.type) {
        case EV_ABS:
            switch (ev.code) {
            case ABS_MT_SLOT:
                cur = ev.value;
                break;
            case ABS_MT_POSITION_X:
                if (cur >= 0 && cur < MAX_SLOTS)
                    sx[cur] = ev.value;
                break;
            case ABS_MT_POSITION_Y:
                if (cur >= 0 && cur < MAX_SLOTS)
                    sy[cur] = ev.value;
                break;
            case ABS_MT_TRACKING_ID:
                if (cur >= 0 && cur < MAX_SLOTS)
                    slot[cur] = ev.value >= 0;
                break;
            case ABS_X:
                emit(ufd, EV_ABS, ABS_X, ev.value);
                break;
            case ABS_Y:
                emit(ufd, EV_ABS, ABS_Y, ev.value);
                break;
            }
            break;

        case EV_KEY:
            if (ev.code == BTN_TOUCH && is_mt) {
                /* MT sources: BTN_TOUCH is synthesized below. */
                break;
            }
            emit(ufd, EV_KEY, ev.code, ev.value);
            break;

        case EV_SYN:
            if (ev.code != SYN_REPORT)
                break;
            first = -1;
            for (i = 0; i < MAX_SLOTS; i++) {
                if (slot[i]) {
                    first = i;
                    break;
                }
            }
            if (first >= 0) {
                if (!down)
                    emit(ufd, EV_KEY, BTN_TOUCH, 1);
                emit(ufd, EV_ABS, ABS_X, sx[first]);
                emit(ufd, EV_ABS, ABS_Y, sy[first]);
                down = 1;
            } else if (down) {
                emit(ufd, EV_KEY, BTN_TOUCH, 0);
                down = 0;
            }
            emit(ufd, EV_SYN, SYN_REPORT, 0);
            break;
        }
    }
}

static int node_exists(const char *name)
{
    char path[128];

    if (name[0] == '\0')
        return 0;
    snprintf(path, sizeof(path), "/dev/input/%s", name);
    return access(path, F_OK) == 0;
}

/* Remove the virtual device. If the node name ueventd assigned is known,
 * hand the node back to it first so the removal uevent cleans it up, then
 * wait for the node to actually disappear before a later recreate (closes
 * the race where a queued removal uevent could delete the recreated node). */
static void depose_virtual(void)
{
    char path[128];
    int waits;

    if (g_vfd >= 0) {
        vtlog("vtouch: destroying virtual device\n");
        if (g_virt_node[0] != '\0') {
            snprintf(path, sizeof(path), "/dev/input/%s", g_virt_node);
            if (rename(g_real_path, path) != 0)
                unlink(g_real_path);
        } else {
            unlink(g_real_path);
        }
        close(g_vfd);
        g_vfd = -1;
    } else {
        unlink(g_real_path);
    }

    if (g_virt_node[0] != '\0') {
        for (waits = 0; waits < 50; waits++) {
            if (!node_exists(g_virt_node))
                break;
            usleep(30000);
        }
        if (waits >= 50)
            vtlog("vtouch: warning: stale node %s not removed\n", g_virt_node);
    }
}

static void restore_original(void)
{
    depose_virtual();
    if (rename(g_saved_path, g_real_path) != 0)
        vtlog("vtouch: restore %s -> %s: %s\n", g_saved_path, g_real_path,
              strerror(errno));
    else
        vtlog("vtouch: restored original node %s\n", g_real_path);
}

/* Create the virtual uinput device and move its node onto g_real_path.
 * first=1: detect the node as the newest /dev/input event node (no prior
 * name known yet). first=0: wait for the remembered node name; if the
 * numbering ever changes, fall back to a sysfs name scan and resync. */
static int activate_virtual(int first)
{
    char pre[64][16];
    char node[16];
    char new_path[128];
    int pren = 0;
    int tries, i, k;

    memset(pre, 0, sizeof(pre));
    if (first)
        pren = scan_events(pre, 64);

    g_vfd = uinput_create(g_rfd);
    if (g_vfd < 0)
        return -1;

    memset(node, 0, sizeof(node));
    if (first) {
        for (tries = 0; tries < 200; tries++) {
            char now[64][16];
            int n;

            n = scan_events(now, 64);
            for (i = 0; i < n; i++) {
                for (k = 0; k < pren; k++)
                    if (!strcmp(now[i], pre[k]))
                        break;
                if (k == pren) {
                    snprintf(node, sizeof(node), "%s", now[i]);
                    break;
                }
            }
            if (node[0] != '\0')
                break;
            vtlog("vtouch: waiting for virtual node (try %d/200)\n", tries);
            usleep(50000);
        }
    } else {
        for (tries = 0; tries < 200; tries++) {
            if (g_virt_node[0] != '\0' && node_exists(g_virt_node)) {
                snprintf(node, sizeof(node), "%s", g_virt_node);
                break;
            }
            vtlog("vtouch: waiting for node %s (try %d/200)\n",
                  g_virt_node[0] ? g_virt_node : "(unknown)", tries);
            usleep(50000);
        }
    }

    if (node[0] == '\0' && find_self_node(node, sizeof(node)) == 0)
        vtlog("vtouch: recovered virtual node by name: %s\n", node);

    if (node[0] == '\0') {
        vtlog("vtouch: virtual node did not appear\n");
        depose_virtual();
        return -1;
    }

    snprintf(g_virt_node, sizeof(g_virt_node), "%s", node);
    snprintf(new_path, sizeof(new_path), "/dev/input/%s", node);
    unlink(g_real_path);
    vtlog("vtouch: renaming %s -> %s\n", new_path, g_real_path);
    if (rename(new_path, g_real_path) != 0) {
        vtlog("vtouch: rename virtual node: %s\n", strerror(errno));
        depose_virtual();
        return -1;
    }
    return 0;
}

/* Report whether an opened device carries multi-touch axes (used to pick
 * relay behaviour for a user-specified device, without the search test). */
static int device_is_mt(int fd)
{
    unsigned long abs[((ABS_MAX + 1) / WORD_SIZE) + 1];

    memset(abs, 0, sizeof(abs));
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) < 0)
        return 0;
    return bit_test(abs, ABS_MT_POSITION_X) && bit_test(abs, ABS_MT_POSITION_Y);
}

int main(int argc, char **argv)
{
    char pre[64][16];
    int pren, i;
    int is_mt = 0;

    ensure_tmp();

    if (argc > 1) {
        if (!strcmp(argv[1], "-v")) {
            printf("MLX VirtualTouch v%s\n", VERSION);
            return 0;
        }
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            print_help();
            return 0;
        }
        if (!strcmp(argv[1], "-r"))
            return signal_daemon(SIGUSR1);
        if (!strcmp(argv[1], "-q"))
            return signal_daemon(SIGINT);
        vtlog("vtouch: unknown option '%s'\n", argv[1]);
        print_help();
        return 1;
    }

    install_signals();
    vtlog("vtouch: starting (v%s)\n", VERSION);

    if (argc > 1 && argv[1][0] == '/') {
        g_rfd = open(argv[1], O_RDONLY);
        if (g_rfd >= 0) {
            snprintf(g_real_path, sizeof(g_real_path), "%s", argv[1]);
            is_mt = device_is_mt(g_rfd);
            vtlog("vtouch: using device %s (multi-touch=%d)\n", g_real_path,
                  is_mt);
        } else {
            vtlog("vtouch: invalid device path '%s': %s\n", argv[1],
                  strerror(errno));
            vtlog("vtouch: falling back to device search\n");
        }
    }

    if (g_rfd < 0) {
        for (i = 0;; i++) {
            pren = scan_events(pre, 64);
            vtlog("vtouch: scan %d: %d event* node(s), looking for touch screen\n",
                  i, pren);
            g_rfd = find_touch_device(pre, pren, g_real_path, sizeof(g_real_path),
                                      &is_mt);
            if (g_rfd >= 0)
                break;
            usleep(200000);
        }
        vtlog("vtouch: real device found: %s (multi-touch=%d)\n", g_real_path,
              is_mt);
    }

    /* Preserve the real node so it can be restored later. */
    snprintf(g_saved_path, sizeof(g_saved_path), "%s", SAVED_NODE);
    unlink(g_saved_path);
    if (rename(g_real_path, g_saved_path) != 0) {
        vtlog("vtouch: preserve real node: %s\n", strerror(errno));
        return 1;
    }
    vtlog("vtouch: preserved real node as %s\n", g_saved_path);

    if (activate_virtual(1) != 0) {
        vtlog("vtouch: activate virtual device failed\n");
        restore_original();
        return 1;
    }

    vtlog("vtouch: relaying on %s\n", g_real_path);

    for (;;) {
        int rc = relay_loop(g_rfd, g_vfd, is_mt);

        if (rc == CMD_RESTART) {
            vtlog("vtouch: restarting virtual device\n");
            g_cmd = CMD_NONE;
            depose_virtual();
            if (activate_virtual(0) != 0) {
                vtlog("vtouch: re-activation failed, restoring original\n");
                break;
            }
            vtlog("vtouch: relaying on %s\n", g_real_path);
            continue;
        }

        if (rc < 0)
            vtlog("vtouch: relay error: %s\n", strerror(errno));
        else
            vtlog("vtouch: relay loop exited\n");
        break;
    }

    vtlog("vtouch: restoring original device node\n");
    restore_original();
    vtlog("vtouch: goodbye\n");
    return 0;
}