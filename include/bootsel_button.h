#ifndef _BOOTSEL_BUTTON_H_
#define _BOOTSEL_BUTTON_H_

#include <stdbool.h>

// Read the BOOTSEL button. Reading it briefly takes over the QSPI CS line, so
// when the other core is running code from flash (controller mode, BT on core1)
// pass lockout_other_core = true to pause it during the read. That core must
// have called multicore_lockout_victim_init().
bool bootsel_button_read(bool lockout_other_core);

// Call periodically (e.g. every ~20 ms). Returns true exactly once when three
// presses occur within the click window. Keeps its own internal state.
bool bootsel_triple_click_poll(bool lockout_other_core);

// Block until BOOTSEL is released (plus a short debounce). Call before rebooting
// in response to a press, otherwise the bootrom sees BOOTSEL held at reset and
// enters its UF2 mass-storage bootloader instead of booting the firmware.
void bootsel_wait_for_release(bool lockout_other_core);

#endif  // _BOOTSEL_BUTTON_H_
