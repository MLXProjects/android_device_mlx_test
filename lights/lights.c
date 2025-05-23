#include <hardware/lights.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define BACKLIGHT_PATH "/sys/class/backlight/panel/brightness"

static int write_int(const char* path, int value) {
    int fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
        write(fd, buffer, bytes);
        close(fd);
        return 0;
    }
    return -errno;
}

static int set_light_backlight(struct light_device_t* dev, struct light_state_t const* state) {
    int brightness = (state->color >> 16) & 0xFF; // Use red component
    return write_int(BACKLIGHT_PATH, brightness);
}

static int open_lights(const struct hw_module_t* module, const char* id, struct hw_device_t** device) {
    static struct light_device_t backlight_dev;
    memset(&backlight_dev, 0, sizeof(backlight_dev));

    backlight_dev.common.tag = HARDWARE_DEVICE_TAG;
    backlight_dev.common.version = 0;
    backlight_dev.common.module = (struct hw_module_t*) module;
    backlight_dev.common.close = NULL;
    backlight_dev.set_light = set_light_backlight;

    *device = (struct hw_device_t*) &backlight_dev;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open = open_lights
};

const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Generic backlight module",
    .author = "MLXProjects",
    .methods = &lights_module_methods,
};