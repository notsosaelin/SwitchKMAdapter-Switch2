#ifndef _CONFIG_USB_H_
#define _CONFIG_USB_H_

// Entry point for "config mode": brings up the USB CDC serial interface and
// services the Web Serial config protocol. Runs on a single core (no Bluetooth)
// and does not return.
void config_core_task(void);

#endif  // _CONFIG_USB_H_
