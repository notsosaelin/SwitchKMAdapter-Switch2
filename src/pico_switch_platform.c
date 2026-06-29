#include <stdio.h>
#include <string.h>

#include <pico/cyw43_arch.h>
#include <pico/multicore.h>
#include <pico/async_context.h>
#include <uni.h>
#include "pico/time.h"

#include "sdkconfig.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "usb.h"
#include "report.h"
#include "SwitchDescriptors.h"
#include "KeyboardKeys.h"
#include "config.h"

// Sanity check
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

#define JOYSTICK_CENTER 0x80

static uint32_t last_mouse_move_time_ms = 0;

// Declarations
SwitchOutReport report[CONFIG_BLUEPAD32_MAX_DEVICES];
SwitchIdxOutReport idx_r;
uint8_t connected_controllers;

typedef struct {
    bool has_keyboard;
    bool has_mouse;
    uni_keyboard_t keyboard;
    uni_mouse_t mouse;
} CombinedControllerState;

static CombinedControllerState combined_states[CONFIG_BLUEPAD32_MAX_DEVICES];

// Helper functions
static void
empty_gamepad_report(SwitchOutReport *gamepad)
{
	gamepad->buttons = 0;
	gamepad->hat = SWITCH_HAT_NOTHING;
	gamepad->lx = SWITCH_JOYSTICK_MID;
	gamepad->ly = SWITCH_JOYSTICK_MID;
	gamepad->rx = SWITCH_JOYSTICK_MID;
	gamepad->ry = SWITCH_JOYSTICK_MID;
}

// Clamp between 0 and 255
static uint8_t clamp_stick_value(int val)
{
	if (val < 0) return 0;
	if (val > 255) return 255;
	return (uint8_t)val;
}

// Map a keyboard report onto the Switch report using the active bindings.
static void fill_gamepad_report_from_keyboard(int idx, const uni_keyboard_t* kb)
{
	// Modifier keys (Left/Right Ctrl, Shift, Alt, GUI) live in bits 0..7.
	for (int b = 0; b < 8; b++) {
		if (kb->modifiers & (1u << b))
			config_apply_action(&report[idx], g_config.modifier_action[b]);
	}

	// Regular keys: the HID usage code indexes the keymap directly.
	for (int i = 0; i < UNI_KEYBOARD_PRESSED_KEYS_MAX; i++) {
		uint8_t key = kb->pressed_keys[i];
		if (key != 0)
			config_apply_action(&report[idx], g_config.key_action[key]);
	}
}

// Map a mouse report onto the Switch report using the active bindings.
static void fill_gamepad_report_from_mouse(int idx, const uni_mouse_t* mouse)
{
	uint32_t now_ms = to_ms_since_boot(get_absolute_time());

	// Mouse buttons (left/right/middle/aux) live in bits 0..7.
	for (int b = 0; b < 8; b++) {
		if (mouse->buttons & (1u << b))
			config_apply_action(&report[idx], g_config.mouse_button_action[b]);
	}

	// Scroll wheel. Consume the delta so one notch fires exactly once.
	if (mouse->scroll_wheel > 0) {
		config_apply_action(&report[idx], g_config.scroll_up_action);
		((uni_mouse_t*)mouse)->scroll_wheel = 0;
	} else if (mouse->scroll_wheel < 0) {
		config_apply_action(&report[idx], g_config.scroll_down_action);
		((uni_mouse_t*)mouse)->scroll_wheel = 0;
	}

	// Mouse motion drives the configured analog stick (default: right / aim).
	uint8_t *stick_x = (g_config.mouse_stick == MOUSE_STICK_LEFT) ? &report[idx].lx : &report[idx].rx;
	uint8_t *stick_y = (g_config.mouse_stick == MOUSE_STICK_LEFT) ? &report[idx].ly : &report[idx].ry;

	if (mouse->delta_x != 0 || mouse->delta_y != 0) {
		last_mouse_move_time_ms = now_ms;
		*stick_x = clamp_stick_value(JOYSTICK_CENTER + (mouse->delta_x * g_config.mouse_sensitivity));
		*stick_y = clamp_stick_value(JOYSTICK_CENTER + (mouse->delta_y * g_config.mouse_sensitivity));
	} else if ((now_ms - last_mouse_move_time_ms) >= g_config.mouse_idle_timeout_ms) {
		*stick_x = JOYSTICK_CENTER;
		*stick_y = JOYSTICK_CENTER;
	}
}

static void
set_led_status() {
	if (connected_controllers == 0)
		cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
	else
		cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
}

//
// Platform Overrides
//
static void pico_switch_platform_init(int argc, const char** argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("my_platform: init()\n");

	connected_controllers = 0;

	// Load bindings from flash, falling back to built-in defaults if the
	// stored config is missing or invalid.
	config_load_from_flash();

	uni_gamepad_mappings_t mappings = GAMEPAD_DEFAULT_MAPPINGS;

	// remaps
	mappings.button_b = UNI_GAMEPAD_MAPPINGS_BUTTON_A;
	mappings.button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_B;
	mappings.button_y = UNI_GAMEPAD_MAPPINGS_BUTTON_X;
	mappings.button_x = UNI_GAMEPAD_MAPPINGS_BUTTON_Y;

	uni_gamepad_set_mappings(&mappings);

	idx_r.idx = 0;
	idx_r.report.buttons = 0;
	idx_r.report.hat = SWITCH_HAT_NOTHING;
	idx_r.report.lx = 0;
	idx_r.report.ly = 0;
	idx_r.report.rx = 0;
	idx_r.report.ry = 0;
	set_global_gamepad_report(&idx_r);
}

static void pico_switch_platform_on_init_complete(void) {
    logi("my_platform: on_init_complete()\n");

    // Safe to call "unsafe" functions since they are called from BT thread

    // Start scanning and auto-connect to discovered keyboards/mice
    uni_bt_start_scanning_and_autoconnect_unsafe();

    // Based on runtime condition you can delete or list the stored BT keys.
    if (1)
        uni_bt_del_keys_unsafe();
    else
        uni_bt_list_keys_unsafe();

    // Turn off LED once init is done.
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

	logi("BLUEPAD: ready to fill reports");
}

static void pico_switch_platform_on_device_connected(uni_hid_device_t* d) {
    logi("my_platform: device connected: %p\n", d);
}

static void pico_switch_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("my_platform: device disconnected: %p\n", d);
	// NOT WORKING?
	// This is complicated. uni_hid_device_get_idx_for_instance
	// no longer gives us the index once the device is disconnected.
	// We assume in this case that a device disconnecting is in a state of no gameplay
	// so we set momentarelly all gamepad reports to 0.
	// If this disconnection happens during gameplay, the gamepad would be stuck in the last state.
	for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
		empty_gamepad_report(&report[i]);
		idx_r.idx = i;
		idx_r.report = report[i];
		set_global_gamepad_report(&idx_r);
	}
	connected_controllers--;
	set_led_status();
}

static uni_error_t pico_switch_platform_on_device_ready(uni_hid_device_t* d) {
    logi("my_platform: device ready: %p\n", d);

	connected_controllers++;
	set_led_status();
    return UNI_ERROR_SUCCESS;
}

static void pico_switch_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl)
{
	uint8_t idx = 0;
    CombinedControllerState* state = &combined_states[idx];

    // Update input state
    if (ctl->klass == UNI_CONTROLLER_CLASS_KEYBOARD)
	{
        state->has_keyboard = true;
        state->keyboard = ctl->keyboard;
    }
	else if (ctl->klass == UNI_CONTROLLER_CLASS_MOUSE)
	{
        state->has_mouse = true;
        state->mouse = ctl->mouse;
    }

	//empty report
	empty_gamepad_report(&report[idx]);

	//fill report with new mouse and keyboard data
    if (state->has_keyboard)
	{
		fill_gamepad_report_from_keyboard(idx, &state->keyboard);
	}

    if (state->has_mouse)
	{
        fill_gamepad_report_from_mouse(idx, &state->mouse);
	}

    idx_r.idx = idx;
    idx_r.report = report[idx];
    set_global_gamepad_report(&idx_r);
}

static const uni_property_t* pico_switch_platform_get_property(uni_property_idx_t idx) {
    // Deprecated
    ARG_UNUSED(idx);
    return NULL;
}

static void pico_switch_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
	ARG_UNUSED(event);
	ARG_UNUSED(data);
	return;
}

//
// Entry Point
//
struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "My Platform",
        .init = pico_switch_platform_init,
        .on_init_complete = pico_switch_platform_on_init_complete,
        .on_device_connected = pico_switch_platform_on_device_connected,
        .on_device_disconnected = pico_switch_platform_on_device_disconnected,
        .on_device_ready = pico_switch_platform_on_device_ready,
        .on_oob_event = pico_switch_platform_on_oob_event,
        .on_controller_data = pico_switch_platform_on_controller_data,
        .get_property = pico_switch_platform_get_property,
    };
    return &plat;
}
