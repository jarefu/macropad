# Adafruit MacroPad RP2040 — CircuitPython Firmware

This directory contains the **CircuitPython** firmware implementation for the **Adafruit MacroPad RP2040**.

It provides a full on-device interactive configuration system as well as real-time **WebSerial** synchronisation with the browser-based configurator in [`../docs/index.html`](../docs/index.html). A 1:1 feature-compatible native C implementation is also available in [`../c/`](../c/).

---

## Features

- **WebSerial Configurator Support (`GET_CONFIG` / `SET_CONFIG:`)**:
  - Exposes a dedicated USB CDC data serial channel (`usb_cdc.data`, falling back to `usb_cdc.console`) compatible with `docs/index.html`.
  - Reads and writes configurations in JSON format (`macros_v4.json`, with automatic migration from legacy `macros_v3.json`).
- **Self-Managed Flash Storage (No Drive Popup)**:
  - `boot.py` hides the `CIRCUITPY` USB mass storage drive during normal operation and mounts the filesystem read/write so the MacroPad can save changes to `/macros_v4.json` directly on the device.
  - Hold **Key 1 (top-left key)** while plugging in USB to expose the `CIRCUITPY` drive to your computer for updating `code.py` or `boot.py`.
- **Composite USB HID (Keyboard + Mouse + Consumer Control)**:
  - **Keyboard (`COMBO` & `TEXT`)**: OS-aware modifiers (`OS` automatically maps to `Ctrl` on PC pages and `Cmd` on Mac pages, plus `SHIFT`, `ALT`, `CTRL`, `GUI`, `OS_SCREEN`, `OS_SWITCH`) and multi-character text string typing.
  - **Media (`MEDIA`)**: Volume Up/Down, Mute, Play/Pause, Next/Previous Track, and Display Brightness.
  - **Mouse & Special (`SPECIAL` & `KNOB`)**: Background Mouse Jiggler (every 30s when toggled), Vertical/Horizontal Scroll, Middle-Click, and Zoom (`Ctrl`/`Cmd` + Scroll Wheel).
- **Full On-Device Interactive Menu System**:
  - Customize any key directly on the OLED screen without a computer:
    - **Pick Preset**: 15 built-in presets (Copy, Paste, Cut, Undo, Redo, Save, Volume, Media, Jiggler, Screenshot, Knob modes).
    - **New Key Combo**: Interactive modifier builder + key selector + custom 6-character label editor.
    - **New Text String**: On-device 7-page 9-key character matrix supporting lowercase, uppercase, digits, symbols, spaces, and newlines (up to 120 characters).
    - **Knob Function**: Assign a key to toggle the rotary encoder into Volume, Vertical Scroll, Horizontal Scroll, Zoom, Media Track, Next/Prev Tab, Undo/Redo, or Brightness mode.
    - **Set Key Colour**: Per-key custom LED color override or Page Default from a 13-option palette.
    - **Set Key Behaviour**: `NORMAL` (one-shot), `HOLD` (active while held), or `TOGGLE` (latch on/off with green indicator LED).
  - Manage profiles/pages on-device (`Add New Page`, `Edit Current Page`, `Toggle Sound`, `Delete This Page`).
- **Power Saving**:
  - **3-Minute Auto-Sleep Timer** blanks the OLED display when idle in `OVERVIEW` mode to prevent OLED burn-in while keeping NeoPixel key lighting active.

---

## Source Files

| File | Description |
| :--- | :--- |
| `boot.py` | Runs at startup: disables USB MIDI, enables dual USB CDC (`console` + `data`), and hides the `CIRCUITPY` USB drive mounts unless Key 1 is held down |
| `code.py` | Main application: configuration persistence (`/macros_v4.json`), OLED UI state machine, NeoPixel LED manager, HID macro dispatcher, and WebSerial protocol handler |

---

## Installation & Setup

### 1. Install CircuitPython on the MacroPad RP2040

1. Download the latest **CircuitPython `.uf2`** for the **Adafruit MacroPad RP2040** from [circuitpython.org/board/adafruit_macropad_rp2040](https://circuitpython.org/board/adafruit_macropad_rp2040/).
2. Enter the RP2040 bootloader: hold down the **rotary encoder button** (top-right knob switch) while pressing the **Reset** button (or while plugging in the USB-C cable).
3. Copy the CircuitPython `.uf2` file onto the **`RPI-RP2`** USB drive that appears. The device will reboot and mount as **`CIRCUITPY`**.

### 2. Install Required CircuitPython Libraries

Ensure the following libraries from the [Adafruit CircuitPython Bundle](https://circuitpython.org/libraries) are present in the `CIRCUITPY/lib/` folder:

- `adafruit_macropad.mpy`
- `adafruit_hid/`
- `adafruit_display_text/`
- `adafruit_debouncer.mpy`
- `adafruit_ticks.mpy`
- `adafruit_simple_text_display.mpy`
- `neopixel.mpy`

### 3. Copy the Firmware Files

1. Copy `boot.py` and `code.py` from this `python/` directory to the root of your **`CIRCUITPY`** drive.
2. Unplug and replug the MacroPad (without holding any keys) so `boot.py` runs and enables the WebSerial data port and writable internal storage.

---

## Maintenance Mode (Re-enabling the `CIRCUITPY` Drive)

Because `boot.py` hides the `CIRCUITPY` USB drive in normal operation so the MacroPad can write to `/macros_v4.json` directly:

1. Unplug the MacroPad USB-C cable.
2. **Hold down Key 1 (the top-left mechanical key)** while plugging the USB-C cable back in.
3. Keep holding Key 1 for ~1 second until the **`CIRCUITPY`** drive mounts on your computer.
4. Edit or replace `code.py` / `boot.py`, then safely eject and replug normally.

