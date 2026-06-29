#include "config.h"

#include <string.h>
#include <stddef.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "KeyboardKeys.h"

adapter_config_t g_config;

const char *const config_action_names[ACTION_COUNT] = {
	"None",
	"A", "B", "X", "Y",
	"L", "R", "ZL", "ZR",
	"Minus", "Plus", "L3", "R3",
	"Home", "Capture",
	"DpadUp", "DpadDown", "DpadLeft", "DpadRight",
	"LStickUp", "LStickDown", "LStickLeft", "LStickRight",
	"RStickUp", "RStickDown", "RStickLeft", "RStickRight",
};
_Static_assert(sizeof(config_action_names) / sizeof(config_action_names[0]) == ACTION_COUNT,
               "config_action_names must stay in sync with switch_action_t");

void config_apply_action(SwitchOutReport *r, switch_action_t a)
{
	switch (a) {
	case ACTION_A:       r->buttons |= SWITCH_MASK_A; break;
	case ACTION_B:       r->buttons |= SWITCH_MASK_B; break;
	case ACTION_X:       r->buttons |= SWITCH_MASK_X; break;
	case ACTION_Y:       r->buttons |= SWITCH_MASK_Y; break;
	case ACTION_L:       r->buttons |= SWITCH_MASK_L; break;
	case ACTION_R:       r->buttons |= SWITCH_MASK_R; break;
	case ACTION_ZL:      r->buttons |= SWITCH_MASK_ZL; break;
	case ACTION_ZR:      r->buttons |= SWITCH_MASK_ZR; break;
	case ACTION_MINUS:   r->buttons |= SWITCH_MASK_MINUS; break;
	case ACTION_PLUS:    r->buttons |= SWITCH_MASK_PLUS; break;
	case ACTION_L3:      r->buttons |= SWITCH_MASK_L3; break;
	case ACTION_R3:      r->buttons |= SWITCH_MASK_R3; break;
	case ACTION_HOME:    r->buttons |= SWITCH_MASK_HOME; break;
	case ACTION_CAPTURE: r->buttons |= SWITCH_MASK_CAPTURE; break;

	case ACTION_HAT_UP:    r->hat = SWITCH_HAT_UP; break;
	case ACTION_HAT_DOWN:  r->hat = SWITCH_HAT_DOWN; break;
	case ACTION_HAT_LEFT:  r->hat = SWITCH_HAT_LEFT; break;
	case ACTION_HAT_RIGHT: r->hat = SWITCH_HAT_RIGHT; break;

	case ACTION_LSTICK_UP:    r->ly = SWITCH_JOYSTICK_MIN; break;
	case ACTION_LSTICK_DOWN:  r->ly = SWITCH_JOYSTICK_MAX; break;
	case ACTION_LSTICK_LEFT:  r->lx = SWITCH_JOYSTICK_MIN; break;
	case ACTION_LSTICK_RIGHT: r->lx = SWITCH_JOYSTICK_MAX; break;

	case ACTION_RSTICK_UP:    r->ry = SWITCH_JOYSTICK_MIN; break;
	case ACTION_RSTICK_DOWN:  r->ry = SWITCH_JOYSTICK_MAX; break;
	case ACTION_RSTICK_LEFT:  r->rx = SWITCH_JOYSTICK_MIN; break;
	case ACTION_RSTICK_RIGHT: r->rx = SWITCH_JOYSTICK_MAX; break;

	case ACTION_NONE:
	default:
		break;
	}
}

void config_set_defaults(adapter_config_t *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->magic = CONFIG_MAGIC;
	cfg->version = CONFIG_VERSION;
	cfg->size = (uint16_t)sizeof(*cfg);

	// Left stick movement on WASD
	cfg->key_action[KEY_W] = ACTION_LSTICK_UP;
	cfg->key_action[KEY_S] = ACTION_LSTICK_DOWN;
	cfg->key_action[KEY_A] = ACTION_LSTICK_LEFT;
	cfg->key_action[KEY_D] = ACTION_LSTICK_RIGHT;

	// Face buttons
	cfg->key_action[KEY_Q] = ACTION_A;
	cfg->key_action[KEY_SPACE] = ACTION_B;
	cfg->key_action[KEY_R] = ACTION_X;
	cfg->key_action[KEY_E] = ACTION_Y;

	// D-pad (note: original had no "left" binding)
	cfg->key_action[KEY_F] = ACTION_HAT_UP;
	cfg->key_action[KEY_B] = ACTION_HAT_DOWN;
	cfg->key_action[KEY_I] = ACTION_HAT_RIGHT;

	// System buttons
	cfg->key_action[KEY_TAB] = ACTION_MINUS;
	cfg->key_action[KEY_ESC] = ACTION_PLUS;
	cfg->key_action[KEY_H] = ACTION_HOME;
	cfg->key_action[KEY_C] = ACTION_CAPTURE;

	// Modifiers (bit index per UNI_KEYBOARD_MODIFIER_*)
	cfg->modifier_action[0] = ACTION_R3;  // Left Control
	cfg->modifier_action[1] = ACTION_L3;  // Left Shift

	// Mouse buttons (bit index per UNI_MOUSE_BUTTON_*)
	cfg->mouse_button_action[0] = ACTION_ZR;        // Left click
	cfg->mouse_button_action[1] = ACTION_ZL;        // Right click
	cfg->mouse_button_action[2] = ACTION_HAT_LEFT;  // Middle click

	// Scroll wheel
	cfg->scroll_up_action = ACTION_L;
	cfg->scroll_down_action = ACTION_R;

	// Mouse motion -> right stick (aim)
	cfg->mouse_stick = MOUSE_STICK_RIGHT;
	cfg->mouse_sensitivity = 5;
	cfg->mouse_deadzone = 0;
	cfg->mouse_idle_timeout_ms = 40;

	cfg->crc = config_crc32(cfg);
}

uint32_t config_crc32(const adapter_config_t *cfg)
{
	const uint8_t *data = (const uint8_t *)cfg;
	size_t len = offsetof(adapter_config_t, crc);
	uint32_t crc = 0xFFFFFFFFu;

	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int b = 0; b < 8; b++)
			crc = (crc >> 1) ^ (0xEDB88520u & (uint32_t)(-(int32_t)(crc & 1u)));
	}
	return ~crc;
}

// ---------------------------------------------------------------------------
// Flash persistence
//
// One 4 KB sector is reserved for the config, placed just below the flash
// region BTstack/cyw43 use for BT link keys (the last 2 sectors). Reads go
// through the XIP mapping; writes go through flash_safe_execute so the BT core
// (core1) is locked out during the erase/program.
// ---------------------------------------------------------------------------
#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (3u * FLASH_SECTOR_SIZE))
#define CONFIG_FLASH_ADDR   (XIP_BASE + CONFIG_FLASH_OFFSET)
#define CONFIG_STORE_SIZE   (2u * FLASH_PAGE_SIZE)  // 512 B, holds the struct

_Static_assert(sizeof(adapter_config_t) <= CONFIG_STORE_SIZE,
               "adapter_config_t no longer fits in the flash store");

static bool config_is_valid(const adapter_config_t *cfg)
{
	return cfg->magic == CONFIG_MAGIC
	    && cfg->version == CONFIG_VERSION
	    && cfg->size == (uint16_t)sizeof(adapter_config_t)
	    && cfg->crc == config_crc32(cfg);
}

void config_load_from_flash(void)
{
	const adapter_config_t *stored = (const adapter_config_t *)CONFIG_FLASH_ADDR;
	if (config_is_valid(stored))
		memcpy(&g_config, stored, sizeof(g_config));
	else
		config_set_defaults(&g_config);
}

bool config_save_to_flash(void)
{
	g_config.crc = config_crc32(&g_config);

	static uint8_t buf[CONFIG_STORE_SIZE];
	memset(buf, 0xFF, sizeof(buf));
	memcpy(buf, &g_config, sizeof(g_config));

	// Config is only ever saved from config mode, where the BT core is not
	// running, so a direct write with interrupts disabled is safe (no other
	// core is executing from flash during the erase/program).
	uint32_t flags = save_and_disable_interrupts();
	flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(CONFIG_FLASH_OFFSET, buf, CONFIG_STORE_SIZE);
	restore_interrupts(flags);

	return true;
}
