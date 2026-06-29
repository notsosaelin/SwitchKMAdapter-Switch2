#include "usb.h"

#include <tusb.h>
#include <stdint.h>
#include <stdbool.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <pico/multicore.h>
#include <pico/async_context.h>

#include "report.h"
#include "SwitchDescriptors.h"
#include "bootsel_button.h"
#include "usb_mode.h"

void
usb_core_task()
{
	tusb_init();

	SwitchIdxOutReport r;
	r.idx = 0;
	r.report.buttons = 0;
	r.report.hat = SWITCH_HAT_NOTHING;
	r.report.lx = 0;
	r.report.ly = 0;
	r.report.rx = 0;
	r.report.ry = 0;

	// send empty reports while bluepad32 is still not set
	uint8_t runs =
	        50;  // run for at least 5 seconds sending empty reports, garanteeing host will see the device
	while (runs > 0) {
		if (tud_hid_n_ready(r.idx)) {
			tud_hid_n_report(r.idx, 0, &r.report, sizeof(r.report));
		}
		runs--;
		sleep_ms(100);
	}

	while (1) {
		get_global_gamepad_report(&r);

		tud_task();

		// Triple-click BOOTSEL -> reboot into config mode. The read briefly
		// takes over the QSPI bus, so lock out the BT core (core1) during it.
		static uint32_t last_bootsel_ms = 0;
		uint32_t now = to_ms_since_boot(get_absolute_time());
		if (now - last_bootsel_ms >= 50) {
			last_bootsel_ms = now;
			if (bootsel_triple_click_poll(true)) {
				bootsel_wait_for_release(true);
				usb_mode_reboot_to(USB_MODE_CONFIG);
			}
		}

		if (tud_suspended()) {
			tud_remote_wakeup();
			continue;
		}

		if (tud_hid_n_ready(r.idx)) {
			tud_hid_n_report(r.idx, 0, &r.report, sizeof(r.report));
		}
	}
}
