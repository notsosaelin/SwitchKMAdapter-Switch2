#ifndef _USB_MODE_H_
#define _USB_MODE_H_

// The adapter has two mutually-exclusive USB personalities, chosen at boot.
// Switching between them is done by rebooting (see usb_mode_reboot_to), so the
// mode is fixed for the lifetime of a boot and the USB descriptor callbacks can
// branch on g_usb_mode.
typedef enum {
	USB_MODE_CONTROLLER = 0,  // HORIPAD HID for the console (default, Switch-safe)
	USB_MODE_CONFIG     = 1,  // USB CDC serial for the Web Serial config tool
} usb_mode_t;

// The mode this boot is running in. Set once by usb_mode_init_from_boot().
extern usb_mode_t g_usb_mode;

// Read the requested mode from the watchdog scratch register into g_usb_mode.
// Call once at the very start of main(). A cold (power-on) boot always resolves
// to USB_MODE_CONTROLLER, so plugging into a console is always safe.
void usb_mode_init_from_boot(void);

// Request `mode` on the next boot, then reboot via the watchdog. Does not return.
void usb_mode_reboot_to(usb_mode_t mode);

#endif  // _USB_MODE_H_
