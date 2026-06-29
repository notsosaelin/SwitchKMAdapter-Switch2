#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include "SwitchDescriptors.h"

// A "switch action" is the result of an input (a key, mouse button, etc.).
// Stored as a single byte so the binding tables serialize trivially to
// flash and over the config serial link.
typedef enum {
	ACTION_NONE = 0,

	// Face / shoulder / trigger / misc buttons
	ACTION_A, ACTION_B, ACTION_X, ACTION_Y,
	ACTION_L, ACTION_R, ACTION_ZL, ACTION_ZR,
	ACTION_MINUS, ACTION_PLUS, ACTION_L3, ACTION_R3,
	ACTION_HOME, ACTION_CAPTURE,

	// D-pad (hat)
	ACTION_HAT_UP, ACTION_HAT_DOWN, ACTION_HAT_LEFT, ACTION_HAT_RIGHT,

	// Left stick, digital (full deflection)
	ACTION_LSTICK_UP, ACTION_LSTICK_DOWN, ACTION_LSTICK_LEFT, ACTION_LSTICK_RIGHT,

	// Right stick, digital (full deflection)
	ACTION_RSTICK_UP, ACTION_RSTICK_DOWN, ACTION_RSTICK_LEFT, ACTION_RSTICK_RIGHT,

	ACTION_COUNT
} switch_action_t;

// Which analog stick the mouse motion drives.
typedef enum {
	MOUSE_STICK_RIGHT = 0,
	MOUSE_STICK_LEFT = 1,
} mouse_stick_t;

#define CONFIG_MAGIC   0x4B4D4144u  // 'KMAD'
#define CONFIG_VERSION 1

// The full adapter configuration. Fixed-size and self-describing so it can be
// blitted to/from a flash sector and validated with magic/version/crc.
typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t size;                   // sizeof(adapter_config_t), sanity check

	uint8_t  key_action[256];        // HID keyboard usage code -> switch_action_t
	uint8_t  modifier_action[8];     // HID modifier bit index   -> switch_action_t
	uint8_t  mouse_button_action[8]; // uni mouse button bit idx -> switch_action_t
	uint8_t  scroll_up_action;
	uint8_t  scroll_down_action;

	uint8_t  mouse_stick;            // mouse_stick_t
	uint8_t  mouse_sensitivity;      // mouse delta -> stick-unit multiplier
	uint8_t  mouse_deadzone;         // reserved (mouse path has no deadzone yet)
	uint16_t mouse_idle_timeout_ms;  // how long to hold the stick after motion stops

	uint32_t crc;                    // CRC32 of everything above
} adapter_config_t;

// The live, in-RAM configuration used by the input-mapping code.
extern adapter_config_t g_config;

// Fill `cfg` with the built-in default bindings (mirrors the original hardcoded map).
void config_set_defaults(adapter_config_t *cfg);

// Apply a single action onto a report (sets a button bit / hat / stick axis).
void config_apply_action(SwitchOutReport *report, switch_action_t action);

// Human-readable name for each action, indexed by switch_action_t. Lets the
// config tool build its UI from the firmware's actual action list.
extern const char *const config_action_names[ACTION_COUNT];

// CRC32 over the struct, excluding the trailing crc field.
uint32_t config_crc32(const adapter_config_t *cfg);

// Load config from the reserved flash sector into g_config; falls back to
// built-in defaults if the stored data is missing or invalid.
void config_load_from_flash(void);

// Persist g_config to the reserved flash sector. Multicore-safe (locks out the
// BT core during the erase/program). Returns true on success.
bool config_save_to_flash(void);

#endif  // _CONFIG_H_
