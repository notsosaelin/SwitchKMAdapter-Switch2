#include "bootsel_button.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

// Read the BOOTSEL button by momentarily driving the QSPI CS pin as an input.
// Must run from RAM with interrupts disabled (no flash access during the read).
// Canonical technique from pico-examples (picoboard/button), RP2040 + RP2350.
static bool __no_inline_not_in_flash_func(get_bootsel_button)(void)
{
	const uint CS_PIN_INDEX = 1;

	uint32_t flags = save_and_disable_interrupts();

	hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
	                GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
	                IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

	for (volatile int i = 0; i < 1000; ++i)
		;

#if PICO_RP2040
	bool button_state = !(sio_hw->gpio_hi_in & (1u << 1));
#else
	bool button_state = !(sio_hw->gpio_hi_in & SIO_GPIO_HI_IN_QSPI_CSN_BITS);
#endif

	hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
	                GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
	                IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

	restore_interrupts(flags);
	return button_state;
}

bool bootsel_button_read(bool lockout_other_core)
{
	bool pressed;
	if (lockout_other_core) {
		multicore_lockout_start_blocking();
		pressed = get_bootsel_button();
		multicore_lockout_end_blocking();
	} else {
		pressed = get_bootsel_button();
	}
	return pressed;
}

#define CLICK_GAP_MAX_MS 500u  // max time allowed between consecutive presses

bool bootsel_triple_click_poll(bool lockout_other_core)
{
	static bool prev_pressed = false;
	static int click_count = 0;
	static uint32_t last_click_ms = 0;

	bool pressed = bootsel_button_read(lockout_other_core);
	uint32_t now = to_ms_since_boot(get_absolute_time());
	bool triple = false;

	// Drop a stale sequence if the user paused too long.
	if (click_count > 0 && (now - last_click_ms) > CLICK_GAP_MAX_MS)
		click_count = 0;

	// Count a press on the rising edge.
	if (pressed && !prev_pressed) {
		click_count++;
		last_click_ms = now;
		if (click_count >= 3) {
			triple = true;
			click_count = 0;
		}
	}
	prev_pressed = pressed;

	return triple;
}

void bootsel_wait_for_release(bool lockout_other_core)
{
	// Wait for the button to come up, then debounce, so a reboot triggered by
	// the press doesn't leave BOOTSEL held at reset (which the bootrom reads as
	// "enter the UF2 bootloader").
	while (bootsel_button_read(lockout_other_core))
		sleep_ms(5);
	sleep_ms(50);
}
