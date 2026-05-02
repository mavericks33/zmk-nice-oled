/*
 * Copyright (c) 2024 ZMK Contributors
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_display_toggle
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zmk/behavior.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
static bool display_enabled = true;
static bool manually_disabled = false;
static struct k_work_delayable idle_work;
static int64_t last_activity_time;
#define IDLE_TIMEOUT_MS (CONFIG_ZMK_BEHAVIOR_DISPLAY_TOGGLE_IDLE_MS)
static void idle_work_handler(struct k_work *work) {
    if (manually_disabled) return;
    int64_t elapsed = k_uptime_get() - last_activity_time;
    if (elapsed >= IDLE_TIMEOUT_MS) {
        const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
        if (device_is_ready(display)) {
            display_blanking_on(display);
            display_enabled = false;
            LOG_DBG("Display auto OFF (idle %lld ms)", elapsed);
        }
    }
    k_work_reschedule(&idle_work, K_MSEC(IDLE_TIMEOUT_MS));
}
static void display_toggle_input_cb(struct input_event *ev) {
    if (ev->type != INPUT_EV_KEY) return;
    if (manually_disabled) return;
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) return;
    if (!display_enabled) {
        display_blanking_off(display);
        display_enabled = true;
        LOG_DBG("Display ON (activity)");
    }
    last_activity_time = k_uptime_get();
    k_work_reschedule(&idle_work, K_MSEC(IDLE_TIMEOUT_MS));
}
INPUT_CALLBACK_DEFINE(NULL, display_toggle_input_cb);
static int behavior_display_toggle_init(const struct device *dev) {
    last_activity_time = k_uptime_get();
    k_work_init_delayable(&idle_work, idle_work_handler);
    k_work_schedule(&idle_work, K_MSEC(IDLE_TIMEOUT_MS));
    return 0;
}
static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) return -ENODEV;
    if (display_enabled) {
        display_blanking_on(display);
        display_enabled = false;
        manually_disabled = true;
        LOG_DBG("Display manual OFF");
    } else {
        display_blanking_off(display);
        display_enabled = true;
        manually_disabled = false;
        last_activity_time = k_uptime_get();
        k_work_reschedule(&idle_work, K_MSEC(IDLE_TIMEOUT_MS));
        LOG_DBG("Display manual ON");
    }
    return ZMK_BEHAVIOR_OPAQUE;
}
static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}
static const struct behavior_driver_api behavior_display_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
};
DEVICE_DT_INST_DEFINE(0, behavior_display_toggle_init, NULL, NULL, NULL,
                      APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                      &behavior_display_toggle_driver_api);
