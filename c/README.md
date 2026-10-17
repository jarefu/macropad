# Adafruit MacroPad RP2040 — Native C Firmware (Pico SDK)

This directory contains a complete native C firmware implementation for the **Adafruit MacroPad RP2040** using the **Raspberry Pi Pico C/C++ SDK (`pico-sdk`)** and **TinyUSB**.

It provides full 1:1 feature parity with the CircuitPython firmware in `../python/` and works out of the box with the WebSerial configurator in `../docs/index.html`.

---

## Features

- **WebSerial Configurator Support (`GET_CONFIG` / `SET_CONFIG:`)**:
  - Exposes a USB CDC serial interface compatible with `docs/index.html`.
  - Full recursive-descent JSON parser and serializer supporting the `macros_v4.json` schema (`active`, `sound`, `profiles`, `keys`, `mods`, `color`, `behavior`, `text`, `action`).
- **Persistent On-Chip QSPI Flash Storage**:
  - Saves configurations directly to the RP2040 flash with a `"MPV4"` magic header and CRC32 integrity check (no exposed `CIRCUITPY` USB mass storage popup).
- **Composite USB HID (Keyboard + Mouse + Consumer Control)**:
  - **Keyboard (`COMBO` & `TEXT`)**: Full modifier support (`OS`, `SHIFT`, `ALT`, `CTRL`, `GUI`, `OS_SCREEN`, `OS_SWITCH`) and US-QWERTY ASCII-to-HID text typing with newline (`Enter`) support.
  - **Media (`MEDIA`)**: Volume Up/Down, Mute, Play/Pause, Next/Previous Track, Display Brightness.
  - **Mouse (`SPECIAL` & `KNOB`)**: Background Mouse Jiggler (every 30s when toggled), Vertical/Horizontal Scroll, Middle-Click, and Zoom (`Ctrl`/`Cmd` + Wheel).
- **Full On-Device Interactive Menu System**:
  - Edit keys directly on the MacroPad OLED (`Pick Preset`, `New Key Combo`, `New Text String`, `Knob Function`, `Set Key Colour`, `Set Key Behaviour`), manage pages (`Add New Page`, `Edit Current Page`, `Toggle Sound`, `Delete This Page`), and type strings/labels using the 7-page 9-key character entry matrix.
- **Native Hardware Drivers**:
  - **SH1106 128x64 SPI OLED** (`spi1` @ 10MHz) with a custom 5x8 font rendering 5 lines of 21 characters.
  - **12x WS2812B NeoPixels** driven via RP2040 **PIO0** state machine on `GP19`.
  - **1kHz Interrupt-Driven Quadrature Encoder** on `GP17`/`GP18` + debounced push switch on `GP0`.
  - **12x Debounced Mechanical Keys** on `GP1`–`GP12`.
  - **Hardware PWM Audio Tone Generator** on `GP16` with Class-D amplifier enable on `GP14`.
  - **3-Minute Auto-Sleep Timer** turning off the OLED and NeoPixels when idle in `OVERVIEW` mode.

---

## Source Files

| File | Description |
| :--- | :--- |
| `CMakeLists.txt` | CMake build configuration targeting RP2040 (`adafruit_macropad_rp2040`) |
| `pico_sdk_import.cmake` | Standard Raspberry Pi Pico SDK locator/fetcher |
| `main.c` | UI state machine, WebSerial protocol handler, key/knob dispatch, and main loop |
| `config.h` / `config.c` | Presets, palettes, key maps, JSON parser/serializer, and QSPI flash persistence |
| `hardware.h` / `hardware.c` | SH1106 OLED driver, PIO NeoPixels, quadrature encoder ISR, key scanner, PWM audio, and TinyUSB HID/CDC wrappers |
| `font5x8.h` | 5x8 ASCII bitmap font for the 128x64 OLED display |
| `ws2812.pio` | RP2040 PIO assembly program for WS2812B NeoPixel timing |
| `tusb_config.h` | TinyUSB device stack configuration (CDC + HID) |
| `usb_descriptors.h` / `usb_descriptors.c` | USB composite descriptors (Adafruit VID `0x239A`, PID `0x8108`, CDC + HID Keyboard/Mouse/Consumer) |

---

## Building the Firmware (`.uf2`)

### Prerequisites

Install the ARM embedded toolchain, CMake, and the Raspberry Pi Pico SDK:

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git python3
```

### Build Steps

1. Clone the `pico-sdk` (or let CMake fetch it automatically with `-DPICO_SDK_FETCH_FROM_GIT=ON`):

```bash
cd c
mkdir -p build && cd build
cmake .. -DPICO_SDK_FETCH_FROM_GIT=ON
make -j$(nproc)
```

*(If you already have `pico-sdk` installed locally, pass `-DPICO_SDK_PATH=/path/to/pico-sdk` instead.)*

2. When the build completes, it produces **`build/macropad.uf2`**.

---

## Flashing to the MacroPad RP2040

1. Hold down the **rotary encoder button** (top-right knob switch, `BOOT`) and press the **Reset** button on the side of the MacroPad (or plug in the USB-C cable while holding the knob down).
2. A USB drive named **`RPI-RP2`** will appear on your computer.
3. Copy `build/macropad.uf2` onto the **`RPI-RP2`** drive.
4. The MacroPad will automatically reboot into the native C firmware and is immediately ready for use with `docs/index.html` via **Connect via USB**.

