# SwitchKMAdapter (Pico 2 W)

Turn a **Raspberry Pi Pico 2 W** (or the original **Pico W**) into a Bluetooth **keyboard + mouse adapter** for the Nintendo
**Switch, Switch 2, and Switch Lite**. It emulates a wired controller (a licensed HORIPAD-style HID
gamepad), so the console accepts it with no extra setup, while you play with a Bluetooth keyboard and
mouse. Every key, button, and the mouse can be remapped — live, from a browser — with no reflashing.

> Fork of [notsosaelin/SwitchKMAdapter-Switch2](https://github.com/notsosaelin/SwitchKMAdapter-Switch2),
> **ported from the original Pico W (RP2040) to the Pico 2 W (RP2350)** and extended with a
> flash-persisted, fully remappable config plus a Web Serial configuration tool.

## Features

- Wired controller HID accepted by **Switch / Switch 2 / Switch Lite** (no authentication setup)
- Bluetooth **keyboard + mouse** via [Bluepad32] (auto-pairs on boot)
- **Fully remappable:** any key / modifier / mouse button / scroll → any Switch button, D-pad, or stick
- **Mouse aim** on a stick, with adjustable sensitivity
- Config **saved to flash** (survives power-off)
- **Web Serial config tool** — edit the mapping in Chrome/Edge, no reflashing
- Builds for both the **Pico 2 W (RP2350)** and the original **Pico W (RP2040)**

## Install (run the firmware)

1. Get `SwitchKMAdapter.uf2` **for your board** (Pico 2 W or Pico W) — from Releases if available, or build it (see below).
2. Hold **BOOTSEL**, plug the Pico 2 W into your PC; it mounts as a drive.
3. Copy `SwitchKMAdapter.uf2` onto that drive. It reboots into controller mode.
4. Plug the Pico 2 W into the Switch (dock USB port, USB-C hub, etc.).
5. Put your Bluetooth keyboard/mouse into pairing mode — they pair automatically.

See the supported [keyboards] and [mice] lists. Devices not on the list may still work.

## Default controls

| Input | Switch |
|---|---|
| `W` `A` `S` `D` | Left stick (move) |
| Mouse move | Right stick (aim) |
| `Q` / `Space` / `E` / `R` | A / B / Y / X |
| `F` / `B` / `I` | D-pad Up / Down / Right |
| `Tab` / `Esc` | Minus (−) / Plus (+) |
| `H` / `C` | Home / Capture |
| `Left Shift` / `Left Ctrl` | L3 / R3 (stick clicks) |
| Left click / Right click | ZR / ZL |
| Middle click | D-pad Left |
| Scroll up / down | L / R |

All of the above is remappable with the config tool.

## Reconfiguring — the Web Serial config tool

1. On the adapter, **triple-click the BOOTSEL button** to toggle into **config mode**; it re-enumerates
   as a USB serial device. Triple-click again to return to controller mode. A power cycle always comes
   back as the controller, so it's always safe to plug into a console.
2. Open the config page in **Chrome or Edge** (Web Serial isn't supported in Firefox/Safari):
   - Serve it locally — `python -m http.server 8770 --directory web`, then open
     <http://localhost:8770/> — **or** host `web/index.html` on GitHub Pages for a permanent URL.
3. Click **Connect**, pick the **SwitchKMAdapter Config** port, edit your bindings, and
   **Save to device** (or **Save & exit to controller**).

Under the hood the page speaks a tiny line protocol over the serial link (`get` / `set` / `save` / …);
the commands are listed at the top of [`src/config_usb.c`](src/config_usb.c) if you want to script it.

## Building from source

Needs the **Raspberry Pi Pico SDK 2.1+** and an ARM GCC toolchain. The easiest way to get both is the
official **Raspberry Pi Pico** VS Code extension, which downloads the SDK, toolchain, CMake, and Ninja
into `~/.pico-sdk`.

```sh
git clone <your-fork-url> SwitchKMAdapter-Switch2
cd SwitchKMAdapter-Switch2
git submodule update --init bluepad32
```

Then either:

- **VS Code:** open the folder, let the Pico extension configure it (board: **Pico 2 W**), and click
  **Compile**.
- **Command line** (using the extension's bundled tools):
  ```sh
  cmake -G Ninja -DPICO_BOARD=pico2_w -S . -B build
  cmake --build build
  ```

The firmware lands at `build/SwitchKMAdapter.uf2`. To build for the original Pico W instead, pass
`-DPICO_BOARD=pico_w`. BTstack is taken from the Pico SDK, so Bluepad32's BTstack submodule is not
needed.

## How it works

- **Two cores:** core 1 runs Bluetooth ([Bluepad32] + BTstack over the CYW43 radio); core 0 runs the
  USB HID controller ([TinyUSB]). They share the gamepad report under a lock.
- **Two USB personalities, chosen at boot:** the HORIPAD HID controller, or a USB CDC serial port for
  the config tool. The choice lives in a watchdog scratch register; triple-clicking BOOTSEL flips it and
  reboots. A cold boot is always the controller.
- **Bindings** live in a single struct persisted to a reserved flash sector; defaults and the action
  table are in [`src/config.c`](src/config.c).

## Acknowledgements

- Forked from [SwitchKMAdapter-Switch2](https://github.com/notsosaelin/SwitchKMAdapter-Switch2)
- Based on [PicoSwitch-WirelessGamepadAdapter](https://github.com/juan518munoz/PicoSwitch-WirelessGamepadAdapter) by juan518munoz
- [Bluepad32] by ricardoquesada
- [TinyUSB] by hathach

[Bluepad32]: https://github.com/ricardoquesada/bluepad32
[TinyUSB]: https://github.com/hathach/tinyusb
[keyboards]: https://bluepad32.readthedocs.io/en/latest/supported_keyboards/
[mice]: https://bluepad32.readthedocs.io/en/latest/supported_mice/
