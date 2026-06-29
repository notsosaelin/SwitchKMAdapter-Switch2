#include "config_usb.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "pico/stdlib.h"
#include "tusb.h"

#include "bootsel_button.h"
#include "usb_mode.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Web Serial config protocol (config mode only)
//
// Host -> device: short line commands (easy to parse in C):
//   get                  dump current config as one JSON line
//   info                 {"id":"switchkmadapter","proto":1,"ver":N}
//   actions              {"actions":["None","A",...]}  (index = action value)
//   k <code> <action>    set keyboard HID code -> action
//   m <bit> <action>     set modifier bit -> action
//   b <bit> <action>     set mouse-button bit -> action
//   su <action>          scroll-up -> action
//   sd <action>          scroll-down -> action
//   stick <0|1>          mouse drives right(0)/left(1) stick
//   sens <1..255>        mouse sensitivity
//   idle <0..5000>       mouse idle timeout (ms)
//   clear                clear all key/mod/button/scroll bindings (to None)
//   reset                restore built-in defaults
//   save                 persist to flash -> {"ok":true|false}
//   exit                 reboot into controller mode
//
// Device -> host: JSON, one object per line (so the web page can JSON.parse it).
// ---------------------------------------------------------------------------

static void cdc_write_all(const char *data, uint32_t len)
{
	uint32_t sent = 0;
	int stalls = 0;
	while (sent < len) {
		uint32_t w = tud_cdc_write(data + sent, len - sent);
		sent += w;
		tud_cdc_write_flush();
		tud_task();  // pump so the TX FIFO drains
		if (w == 0 && ++stalls > 1000)
			break;   // host isn't reading; give up rather than spin forever
	}
}

static void cdc_printf(const char *fmt, ...)
{
	static char b[256];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(b, sizeof(b), fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n >= (int)sizeof(b))
		n = sizeof(b) - 1;
	cdc_write_all(b, (uint32_t)n);
}

static void send_ok(void)          { cdc_write_all("{\"ok\":true}\r\n", 13); }
static void send_err(const char *e) { cdc_printf("{\"err\":\"%s\"}\r\n", e); }

static void send_config_json(void)
{
	static char buf[3072];
	int n = 0;
	bool first;

	n += snprintf(buf + n, sizeof(buf) - n, "{\"ver\":%u,\"keys\":{", g_config.version);

	first = true;
	for (int i = 0; i < 256; i++) {
		if (g_config.key_action[i]) {
			n += snprintf(buf + n, sizeof(buf) - n, "%s\"%d\":%d",
			              first ? "" : ",", i, g_config.key_action[i]);
			first = false;
		}
	}

	n += snprintf(buf + n, sizeof(buf) - n, "},\"mods\":{");
	first = true;
	for (int i = 0; i < 8; i++) {
		if (g_config.modifier_action[i]) {
			n += snprintf(buf + n, sizeof(buf) - n, "%s\"%d\":%d",
			              first ? "" : ",", i, g_config.modifier_action[i]);
			first = false;
		}
	}

	n += snprintf(buf + n, sizeof(buf) - n, "},\"mbtns\":{");
	first = true;
	for (int i = 0; i < 8; i++) {
		if (g_config.mouse_button_action[i]) {
			n += snprintf(buf + n, sizeof(buf) - n, "%s\"%d\":%d",
			              first ? "" : ",", i, g_config.mouse_button_action[i]);
			first = false;
		}
	}

	n += snprintf(buf + n, sizeof(buf) - n,
	              "},\"scroll_up\":%d,\"scroll_down\":%d,"
	              "\"stick\":%d,\"sens\":%d,\"idle\":%d}\r\n",
	              g_config.scroll_up_action, g_config.scroll_down_action,
	              g_config.mouse_stick, g_config.mouse_sensitivity,
	              g_config.mouse_idle_timeout_ms);

	if (n > (int)sizeof(buf))
		n = sizeof(buf);
	cdc_write_all(buf, (uint32_t)n);
}

static void send_actions_json(void)
{
	static char buf[512];
	int n = 0;
	n += snprintf(buf + n, sizeof(buf) - n, "{\"actions\":[");
	for (int i = 0; i < ACTION_COUNT; i++) {
		n += snprintf(buf + n, sizeof(buf) - n, "%s\"%s\"",
		              i ? "," : "", config_action_names[i]);
	}
	n += snprintf(buf + n, sizeof(buf) - n, "]}\r\n");
	if (n > (int)sizeof(buf))
		n = sizeof(buf);
	cdc_write_all(buf, (uint32_t)n);
}

// Parse the next whitespace-delimited integer token. Returns false if missing
// or not a clean number.
static bool next_int(int *out)
{
	char *t = strtok(NULL, " \t");
	if (!t)
		return false;
	char *end;
	long v = strtol(t, &end, 10);
	if (end == t || *end != '\0')
		return false;
	*out = (int) v;
	return true;
}

static void process_line(char *line)
{
	char *cmd = strtok(line, " \t");
	if (!cmd)
		return;

	if (!strcmp(cmd, "get")) {
		send_config_json();
	} else if (!strcmp(cmd, "info")) {
		cdc_printf("{\"id\":\"switchkmadapter\",\"proto\":1,\"ver\":%u}\r\n",
		           g_config.version);
	} else if (!strcmp(cmd, "actions")) {
		send_actions_json();
	} else if (!strcmp(cmd, "k")) {
		int code, act;
		if (next_int(&code) && next_int(&act) && code >= 0 && code < 256 &&
		    act >= 0 && act < ACTION_COUNT) {
			g_config.key_action[code] = (uint8_t) act;
			send_ok();
		} else {
			send_err("bad k");
		}
	} else if (!strcmp(cmd, "m")) {
		int bit, act;
		if (next_int(&bit) && next_int(&act) && bit >= 0 && bit < 8 &&
		    act >= 0 && act < ACTION_COUNT) {
			g_config.modifier_action[bit] = (uint8_t) act;
			send_ok();
		} else {
			send_err("bad m");
		}
	} else if (!strcmp(cmd, "b")) {
		int bit, act;
		if (next_int(&bit) && next_int(&act) && bit >= 0 && bit < 8 &&
		    act >= 0 && act < ACTION_COUNT) {
			g_config.mouse_button_action[bit] = (uint8_t) act;
			send_ok();
		} else {
			send_err("bad b");
		}
	} else if (!strcmp(cmd, "su")) {
		int act;
		if (next_int(&act) && act >= 0 && act < ACTION_COUNT) {
			g_config.scroll_up_action = (uint8_t) act;
			send_ok();
		} else {
			send_err("bad su");
		}
	} else if (!strcmp(cmd, "sd")) {
		int act;
		if (next_int(&act) && act >= 0 && act < ACTION_COUNT) {
			g_config.scroll_down_action = (uint8_t) act;
			send_ok();
		} else {
			send_err("bad sd");
		}
	} else if (!strcmp(cmd, "stick")) {
		int v;
		if (next_int(&v) && (v == 0 || v == 1)) {
			g_config.mouse_stick = (uint8_t) v;
			send_ok();
		} else {
			send_err("bad stick");
		}
	} else if (!strcmp(cmd, "sens")) {
		int v;
		if (next_int(&v) && v >= 1 && v <= 255) {
			g_config.mouse_sensitivity = (uint8_t) v;
			send_ok();
		} else {
			send_err("bad sens");
		}
	} else if (!strcmp(cmd, "idle")) {
		int v;
		if (next_int(&v) && v >= 0 && v <= 5000) {
			g_config.mouse_idle_timeout_ms = (uint16_t) v;
			send_ok();
		} else {
			send_err("bad idle");
		}
	} else if (!strcmp(cmd, "clear")) {
		memset(g_config.key_action, 0, sizeof(g_config.key_action));
		memset(g_config.modifier_action, 0, sizeof(g_config.modifier_action));
		memset(g_config.mouse_button_action, 0, sizeof(g_config.mouse_button_action));
		g_config.scroll_up_action = 0;
		g_config.scroll_down_action = 0;
		send_ok();
	} else if (!strcmp(cmd, "reset")) {
		config_set_defaults(&g_config);
		send_ok();
	} else if (!strcmp(cmd, "save")) {
		bool ok = config_save_to_flash();
		cdc_printf("{\"ok\":%s}\r\n", ok ? "true" : "false");
	} else if (!strcmp(cmd, "exit")) {
		send_ok();
		sleep_ms(50);
		usb_mode_reboot_to(USB_MODE_CONTROLLER);
	} else {
		send_err("unknown");
	}
}

// Config mode: single core, USB CDC only, no Bluetooth.
void config_core_task(void)
{
	tusb_init();
	config_load_from_flash();

	static char line[160];
	static int line_len = 0;
	uint32_t last_bootsel_ms = 0;

	while (1) {
		tud_task();

		// Accumulate input into lines and dispatch on newline.
		while (tud_cdc_available()) {
			uint8_t c;
			if (tud_cdc_read(&c, 1) != 1)
				break;
			if (c == '\n' || c == '\r') {
				if (line_len > 0) {
					line[line_len] = '\0';
					process_line(line);
					line_len = 0;
				}
			} else if (line_len < (int)sizeof(line) - 1) {
				line[line_len++] = (char) c;
			} else {
				line_len = 0;  // overflow: drop the line
			}
		}

		// Triple-click BOOTSEL -> back to controller mode (single core: no lockout).
		uint32_t now = to_ms_since_boot(get_absolute_time());
		if (now - last_bootsel_ms >= 20) {
			last_bootsel_ms = now;
			if (bootsel_triple_click_poll(false)) {
				bootsel_wait_for_release(false);
				usb_mode_reboot_to(USB_MODE_CONTROLLER);
			}
		}
	}
}
