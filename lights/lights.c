/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <hardware/lights.h>
#include <hardware/hardware.h>

#define BACKLIGHT_FILE "/sys/class/backlight/panel/brightness"
#define TOUCHKEY_BACKLIGHT_FILE "/sys/class/backlight/touchkey-led/brightness"
#define TOUCHKEY_MAX_BRIGHTNESS_FILE "/sys/class/backlight/touchkey-led/max_brightness"
#define FLASHLIGHT_FILE "/sys/class/camera/rear/rear_flash"
#define MAX_BRIGHTNESS 255

struct lights_device_t {
    struct hw_device_t common;
    int (*set_light)(struct lights_device_t* dev,
            struct light_state_t const* state);
};

static int write_int(const char* path, int value)
{
    int fd;
    int bytes;
    char buffer[20];

    fd = open(path, O_RDWR);
    if (fd < 0) {
        return -errno;
    }

    bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
    write(fd, buffer, bytes);

    close(fd);
    return 0;
}

static int read_int(const char* path, int def)
{
    int fd;
    char buffer[20];
    int value;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return def;
    }

    if (read(fd, buffer, sizeof(buffer) - 1) <= 0) {
        close(fd);
        return def;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    value = atoi(buffer);

    close(fd);
    return value;
}

static int rgb_to_intensity(struct light_state_t const* state)
{
    unsigned int color = state->color & 0x00ffffff;

    return ((77 * ((color >> 16) & 0x00ff)) +
            (150 * ((color >> 8) & 0x00ff)) +
            (29 * (color & 0x00ff))) >> 8;
}

static int set_light_backlight(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    int brightness = rgb_to_intensity(state);
    if (brightness > MAX_BRIGHTNESS)
        brightness = MAX_BRIGHTNESS;
    if (brightness < 0)
        brightness = 0;
    return write_int(BACKLIGHT_FILE, brightness);
}

static int set_light_keyboard(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}

static int set_light_buttons(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    static int max_brightness = 0;

    if (max_brightness <= 0)
        max_brightness = read_int(TOUCHKEY_MAX_BRIGHTNESS_FILE, MAX_BRIGHTNESS);

    return write_int(TOUCHKEY_BACKLIGHT_FILE,
            rgb_to_intensity(state) > 0 ? max_brightness : 0);
}

static int set_light_battery(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}

static int set_light_notifications(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}

static int set_light_attention(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}

static int set_light_flashlight(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    return write_int(FLASHLIGHT_FILE,
            rgb_to_intensity(state) > 0 ? 1 : 0);
}

static int set_light_other(struct lights_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}

static int close_lights(struct hw_device_t* device)
{
    struct lights_device_t* dev = (struct lights_device_t*)device;
    if (dev) {
        free(dev);
    }
    return 0;
}

static int open_lights(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    struct lights_device_t* dev = malloc(sizeof(*dev));
    if (dev == NULL) {
        return -ENOMEM;
    }

    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = close_lights;
    dev->set_light = set_light_other;

    if (0 == strcmp(name, LIGHT_ID_BACKLIGHT))
        dev->set_light = set_light_backlight;
    else if (0 == strcmp(name, LIGHT_ID_KEYBOARD))
        dev->set_light = set_light_keyboard;
    else if (0 == strcmp(name, LIGHT_ID_BUTTONS))
        dev->set_light = set_light_buttons;
    else if (0 == strcmp(name, LIGHT_ID_BATTERY))
        dev->set_light = set_light_battery;
    else if (0 == strcmp(name, LIGHT_ID_NOTIFICATIONS))
        dev->set_light = set_light_notifications;
    else if (0 == strcmp(name, LIGHT_ID_ATTENTION))
        dev->set_light = set_light_attention;
    else if (0 == strcmp(name, LIGHT_ID_FLASHLIGHT))
        dev->set_light = set_light_flashlight;

    *device = &dev->common;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open = open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Test lights HAL",
    .author = "MLX",
    .methods = &lights_module_methods,
};