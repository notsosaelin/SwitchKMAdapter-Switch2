#include "usb_mode.h"

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"

// scratch[0..3] are free for application use; scratch[4..7] are used by the SDK
// reboot path. The magic survives a watchdog reboot but not a power cycle, so a
// cold boot reads 0 here and falls through to controller mode.
#define USB_MODE_SCRATCH_INDEX 0
#define USB_MODE_CONFIG_MAGIC  0xC0FF1900u

usb_mode_t g_usb_mode = USB_MODE_CONTROLLER;

void usb_mode_init_from_boot(void)
{
	uint32_t requested = watchdog_hw->scratch[USB_MODE_SCRATCH_INDEX];
	g_usb_mode = (requested == USB_MODE_CONFIG_MAGIC) ? USB_MODE_CONFIG
	                                                   : USB_MODE_CONTROLLER;

	// One-shot: clear the flag so any non-deliberate reset returns to
	// controller mode.
	watchdog_hw->scratch[USB_MODE_SCRATCH_INDEX] = 0;
}

void usb_mode_reboot_to(usb_mode_t mode)
{
	watchdog_hw->scratch[USB_MODE_SCRATCH_INDEX] =
	    (mode == USB_MODE_CONFIG) ? USB_MODE_CONFIG_MAGIC : 0u;

	watchdog_reboot(0, 0, 1);  // reboot ~1ms from now
	while (1)
		tight_loop_contents();
}
