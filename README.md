# Adafruit MacroPad RP2040 — Custom Firmware & Web Configurator

Complete custom firmware and browser-based **WebSerial Configurator** for the **Adafruit MacroPad RP2040** (3×4 mechanical keypad, rotary encoder with push switch, 128×64 OLED display, 12× addressable RGB NeoPixels, and speaker).

Customize your keypad, pages, OS-specific shortcuts, rotary encoder modes, and RGB lighting **directly on the MacroPad's built-in OLED screen** or **via the WebSerial browser interface**—with no desktop software installation required.

---

## Repository Structure

| Directory / File | Description |
| :--- | :--- |
| [`c/`](./c/) | **Native C Firmware** built with the Raspberry Pi Pico C/C++ SDK (`pico-sdk`) and TinyUSB. Fast boot, zero external dependencies, and direct on-chip QSPI flash persistence. See [`c/README.md`](./c/README.md). |
| [`python/`](./python/) | **CircuitPython Firmware** (`boot.py` + `code.py`) using the `adafruit_macropad` library. Easy to inspect and tweak in Python. See [`python/README.md`](./python/README.md). |
| [`docs/index.html`](./docs/index.html) | **WebSerial Configurator** — standalone single-file web app that connects to either the C or Python firmware over USB to visually edit profiles, keys, colors, and presets. |

Both the **Native C** (`c/`) and **CircuitPython** (`python/`) firmwares share **1:1 feature parity** and use the exact same `macros_v4.json` configuration schema and WebSerial protocol.

---

## Key Features

- **Dual Configuration Workflow**:
  - **On-Device OLED Menu**: Edit any key, build multi-modifier shortcuts, type custom text macros or labels using the 9-key multi-page character matrix, change LED colors, or add/edit/delete pages directly on the device.
  - **WebSerial Browser Configurator (`docs/index.html`)**: Live hardware preview, one-click presets, visual key editor, and JSON backup import/export via Chrome, Edge, or Opera.
- **Multi-Page / Multi-OS Profiles**:
  - Create multiple pages (up to 8 in C firmware, unlimited in CircuitPython) with per-page OS targeting (**PC (Win/Linux)** or **MAC (macOS)**) and page theme colors.
  - Smart **`OS` Modifier**: Automatically sends `Ctrl` on PC pages and `Cmd` (`GUI`) on Mac pages, plus cross-platform screenshot (`OS_SCREEN`) and app-switching (`OS_SWITCH`) combos.
- **Rich Action Types**:
  - **Key Combo (`COMBO`)**: Any key combined with `OS`, `SHIFT`, `ALT`, `CTRL`, or `GUI`.
  - **Text String (`TEXT`)**: Types out custom strings up to 120 characters (including `Enter`/newlines).
  - **Media Control (`MEDIA`)**: Volume Up/Down, Mute, Play/Pause, Next/Prev Track, Brightness.
  - **Rotary Knob Mode (`KNOB`)**: Turn any key into a toggle that remaps the rotary encoder to **Volume Control**, **Vertical Scroll**, **Horizontal Scroll**, **Zoom In/Out**, **Media Track**, **Next/Prev Tab**, **Undo/Redo**, or **Screen Brightness**.
  - **Special (`SPECIAL`)**: Built-in background **Mouse Jiggler** (nudges the cursor every 30 seconds while toggled on).
- **Flexible Key Behaviors**:
  - `NORMAL` (One-shot press & release), `HOLD` (Held down while the physical key is pressed), and `TOGGLE` (Latches on/off and illuminates the key green while active).
- **Quality-of-Life Details**:
  - **No Unwanted USB Drive Popups**: Both firmwares store settings internally without popping up a mass storage window every time you plug in the pad.
  - **Audio Feedback**: Piezo speaker tone feedback on keypresses, page changes, and saves (can be toggled on/off).
  - **OLED Auto-Sleep**: Automatically turns off the OLED display after 3 minutes of inactivity in `OVERVIEW` mode to prevent screen burn-in.

---

## Quick Start

### Option A: Native C Firmware (Recommended for speed & standalone UF2)

Produces a single self-contained `macropad.uf2` binary with no external libraries required.

```bash
cd c
mkdir -p build && cd build
cmake .. -DPICO_SDK_FETCH_FROM_GIT=ON
make -j$(nproc)
```

Hold the **rotary encoder button** while resetting/plugging in the MacroPad, then copy `c/build/macropad.uf2` onto the **`RPI-RP2`** drive.
See [`c/README.md`](./c/README.md) for full prerequisites and build details.

### Option B: CircuitPython Firmware

1. Flash CircuitPython for the Adafruit MacroPad RP2040 and install the required `adafruit_macropad` bundle libraries in `CIRCUITPY/lib/`.
2. Copy [`python/boot.py`](./python/boot.py) and [`python/code.py`](./python/code.py) to the root of the **`CIRCUITPY`** drive and replug the device.
See [`python/README.md`](./python/README.md) for detailed installation and maintenance mode instructions.

---

## Using the Web Configurator (`docs/index.html`)

1. Open [`docs/index.html`](./docs/index.html) in a browser with **WebSerial API** support (**Google Chrome**, **Microsoft Edge**, or **Opera**)—either locally from disk or hosted via GitHub Pages / a local HTTP server.
2. Click **Connect via USB** and select your MacroPad serial port from the browser prompt.
   - Upon connection, the configurator automatically fetches your current configuration from the pad.
3. Click any key on the **Live Hardware Preview** to change its action type, label, key combination, text payload, LED color, or press behavior, or click the simulated encoder knob to switch/manage pages.
4. Click **Save to MacroPad** to push your updated configuration back to the device's persistent flash storage, or use **Export JSON** / **Import JSON** to back up and share profiles.

---

## On-Device Controls Reference

When using the MacroPad in normal (`OVERVIEW`) mode:

| Input | Action |
| :--- | :--- |
| **Press Key 1–12** | Trigger the assigned macro, media key, toggle, or knob mode |
| **Rotate Knob** | Cycle between profile pages *(or execute the active Knob Function if a `KNOB` key is toggled on)* |
| **Press Knob (Once)** | Enter **Key Select Mode** (`Click Key to Edit`) — press any of the 12 keys to customize it *(or execute the secondary knob click action if a `KNOB` key is toggled on)* |
| **Press Knob (Twice)** | Enter **Page Menu** (`Add New Page`, `Edit Current Page`, `Toggle Sound`, `Delete This Page`) |
| **Rotate Knob in Key Select** | Cancel and return to `OVERVIEW` mode |

### On-Device Text Entry Matrix

When typing a **New Text String**, **Key Label**, or **Page Name** on the device:
- **Rotate the Knob** to flip through the 7 character pages (`a–z`, `0–9`, punctuation, and symbols).
- **Keys 1–9** (top 3 rows, cyan LEDs) type the corresponding character shown in the 3×3 grid on the OLED.
- **Key 10** (bottom-left, purple/orange LED) toggles **CAPS / lowercase**.
- **Key 11** (bottom-middle, blue LED) inserts a **Space**.
- **Key 12** (bottom-right, red LED) acts as **Backspace**.
- **Press the Knob** to confirm and save.
