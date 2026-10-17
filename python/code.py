import json
import time
import supervisor
import usb_cdc
from adafruit_macropad import MacroPad

# CRITICAL: Disable auto-reload so saving JSON files doesn't trigger a half-baked reboot
supervisor.runtime.autoreload = False
serial_buf = ""

macropad = MacroPad()
CONFIG_FILE = "/macros_v4.json"
LEGACY_FILE = "/macros_v3.json"
K = macropad.Keycode

# ============================================================================
# 1. KEYCODE DICTIONARY, COLOURS & PRESETS
# ============================================================================
KEY_MAP = {
    "A": K.A, "B": K.B, "C": K.C, "D": K.D, "E": K.E, "F": K.F,
    "G": K.G, "H": K.H, "I": K.I, "J": K.J, "K": K.K, "L": K.L,
    "M": K.M, "N": K.N, "O": K.O, "P": K.P, "Q": K.Q, "R": K.R,
    "S": K.S, "T": K.T, "U": K.U, "V": K.V, "W": K.W, "X": K.X,
    "Y": K.Y, "Z": K.Z,
    "0": K.ZERO, "1": K.ONE, "2": K.TWO, "3": K.THREE, "4": K.FOUR,
    "5": K.FIVE, "6": K.SIX, "7": K.SEVEN, "8": K.EIGHT, "9": K.NINE,
    "F1": K.F1, "F2": K.F2, "F3": K.F3, "F4": K.F4, "F5": K.F5, "F6": K.F6,
    "F7": K.F7, "F8": K.F8, "F9": K.F9, "F10": K.F10, "F11": K.F11, "F12": K.F12,
    "ENTER": K.ENTER, "ESC": K.ESCAPE, "BACKSP": K.BACKSPACE, "TAB": K.TAB,
    "SPACE": K.SPACE, "DELETE": K.DELETE,
    "UP": K.UP_ARROW, "DOWN": K.DOWN_ARROW, "LEFT": K.LEFT_ARROW, "RIGHT": K.RIGHT_ARROW,
}

KEY_NAMES = [
    "A","B","C","D","E","F","G","H","I","J","K","L","M",
    "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    "0","1","2","3","4","5","6","7","8","9",
    "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
    "ENTER","ESC","BACKSP","TAB","SPACE","DELETE","UP","DOWN","LEFT","RIGHT"
]

COLOR_PALETTE = [
    ("Page Default", None),
    ("Soft Blue",    0x002244),
    ("Purple",       0x330044),
    ("Red",          0x550000),
    ("Green",        0x005500),
    ("Deep Blue",    0x001166),
    ("Yellow",       0x444400),
    ("Cyan",         0x004444),
    ("Magenta",      0x440044),
    ("Orange",       0x551800),
    ("Pink",         0x550822),
    ("White",        0x333333),
    ("Off",          0x000000),
]

THEME_PALETTE = COLOR_PALETTE[1:]

PRESETS = [
    {"label": "Copy",   "type": "COMBO",   "mods": ["OS"],          "key": "C",   "color": None, "behavior": "NORMAL"},
    {"label": "Paste",  "type": "COMBO",   "mods": ["OS"],          "key": "V",   "color": None, "behavior": "NORMAL"},
    {"label": "Cut",    "type": "COMBO",   "mods": ["OS"],          "key": "X",   "color": None, "behavior": "NORMAL"},
    {"label": "Undo",   "type": "COMBO",   "mods": ["OS"],          "key": "Z",   "color": None, "behavior": "NORMAL"},
    {"label": "Redo",   "type": "COMBO",   "mods": ["OS", "SHIFT"], "key": "Z",   "color": None, "behavior": "NORMAL"},
    {"label": "Save",   "type": "COMBO",   "mods": ["OS"],          "key": "S",   "color": None, "behavior": "NORMAL"},
    {"label": "Vol+",   "type": "MEDIA",   "action": "VOL_UP",      "color": None, "behavior": "HOLD"},
    {"label": "Vol-",   "type": "MEDIA",   "action": "VOL_DOWN",    "color": None, "behavior": "HOLD"},
    {"label": "Mute",   "type": "MEDIA",   "action": "MUTE",        "color": None, "behavior": "NORMAL"},
    {"label": "Play",   "type": "MEDIA",   "action": "PLAY_PAUSE",  "color": None, "behavior": "NORMAL"},
    {"label": "Jiggle", "type": "SPECIAL","action": "JIGGLER",     "color": 0x004400, "behavior": "TOGGLE"},
    {"label": "Screen", "type": "COMBO",   "mods": ["OS_SCREEN"],   "key": "S",   "color": None, "behavior": "NORMAL"},
    {"label": "K-Vol",  "type": "KNOB",    "action": "VOLUME",      "color": 0x004444, "behavior": "TOGGLE"},
    {"label": "K-ScV",  "type": "KNOB",    "action": "SCROLL_V",    "color": 0x440044, "behavior": "TOGGLE"},
    {"label": "K-Zoom", "type": "KNOB",    "action": "ZOOM",        "color": 0x444400, "behavior": "TOGGLE"},
]

TEXT_PAGES_LOWER = [
    ["a", "b", "c", "d", "e", "f", "g", "h", "i"],
    ["j", "k", "l", "m", "n", "o", "p", "q", "r"],
    ["s", "t", "u", "v", "w", "x", "y", "z", "\n"],
    ["0", "1", "2", "3", "4", "5", "6", "7", "8"],
    ["9", ".", ",", "!", "?", "@", "#", "$", "%"],
    ["-", "_", "+", "=", "/", "\\", ":", ";", "&"],
    ["(", ")", "[", "]", "'", "\"", "*", "~", "`"],
]

TEXT_PAGES_UPPER = [
    ["A", "B", "C", "D", "E", "F", "G", "H", "I"],
    ["J", "K", "L", "M", "N", "O", "P", "Q", "R"],
    ["S", "T", "U", "V", "W", "X", "Y", "Z", "\n"],
    ["0", "1", "2", "3", "4", "5", "6", "7", "8"],
    ["9", ".", ",", "!", "?", "@", "#", "$", "%"],
    ["-", "_", "+", "=", "/", "\\", ":", ";", "&"],
    ["(", ")", "[", "]", "'", "\"", "*", "~", "`"],
]

# ============================================================================
# 2. CONFIGURATION LOAD & SAVE
# ============================================================================
def clone_keys(key_list):
    out = []
    for k in key_list:
        d = dict(k)
        if "mods" in d:
            d["mods"] = list(d["mods"])
        if "color" not in d:
            d["color"] = None
        if "behavior" not in d:
            d["behavior"] = "NORMAL"
        out.append(d)
    return out

def default_config():
    return {
        "active": 0,
        "sound": True,
        "profiles": [
            {
                "name": "PC 1",
                "os": "PC",
                "theme": 0x002244,
                "keys": clone_keys(PRESETS[:12])
            },
            {
                "name": "MAC 1",
                "os": "MAC",
                "theme": 0x330044,
                "keys": clone_keys(PRESETS[:12])
            }
        ]
    }

config = default_config()

def load_config():
    global config
    try:
        with open(CONFIG_FILE, "r") as f:
            loaded = json.load(f)
            if "profiles" in loaded and len(loaded["profiles"]) > 0:
                config = loaded
                if "sound" not in config:
                    config["sound"] = True
                if config.get("active", 0) >= len(config["profiles"]):
                    config["active"] = 0
                return
    except Exception:
        pass

    try:
        with open(LEGACY_FILE, "r") as f:
            old = json.load(f)
            if "profiles" in old:
                config = old
                if "sound" not in config:
                    config["sound"] = True
                save_config()
    except Exception:
        pass

def save_config():
    try:
        with open(CONFIG_FILE, "w") as f:
            json.dump(config, f)
        return True
    except Exception:
        return False

def cur_prof():
    return config["profiles"][config["active"]]

load_config()

# ============================================================================
# 3. UI & AUDIO HELPERS
# ============================================================================
text_lines = macropad.display_text()
text_lines.show()

mode = "OVERVIEW"
selected_key = 0
menu_idx = 0
last_encoder = macropad.encoder
last_dir = 0
last_turn_time = 0.0

edit_mods = []
edit_key_idx = 0
edit_string = ""
text_page_idx = 0
text_upper = False

temp_page_os = "PC"
temp_page_theme = 0x002244
is_creating_page = False

active_toggles = set()
active_knob_key = None
last_activity_time = time.monotonic()
is_sleeping = False

EDIT_TYPES = ["Pick Preset", "New Key Combo", "New Text String", "Knob Function", "Set Key Colour", "Set Key Behaviour", "Cancel"]
BEHAVIOR_OPTIONS = ["NORMAL (One-shot)", "HOLD (Hold down)", "TOGGLE (Latch on/off)"]
PAGE_MENU_OPTS = ["Add New Page", "Edit Current Page", "Toggle Sound", "Delete This Page", "Cancel"]
MOD_OPTIONS = ["[NEXT -> Key]", "OS (Ctrl/Cmd)", "SHIFT", "ALT/OPT", "CTRL", "GUI/CMD"]
OS_OPTIONS = ["PC (Win/Linux)", "MAC (macOS)"]

KNOB_ACTIONS = [
    ("Volume Control", "VOLUME",     "K-Vol"),
    ("Vert Scroll",    "SCROLL_V",   "K-ScV"),
    ("Horiz Scroll",   "SCROLL_H",   "K-ScH"),
    ("Zoom In/Out",    "ZOOM",       "K-Zoom"),
    ("Media Track",    "TRACK",      "K-Trk"),
    ("Next/Prev Tab",  "TABS",       "K-Tabs"),
    ("Undo / Redo",    "UNDO_REDO",  "K-Undo"),
    ("Brightness",     "BRIGHTNESS", "K-Brt"),
]

def play_sound(freq, duration):
    if config.get("sound", True):
        try:
            macropad.play_tone(freq, duration)
        except Exception:
            pass

def format_cell(label):
    s = label[:6]
    return s + (" " * (6 - len(s)))

def format_char_slot(ch):
    if ch == "\n":
        return "ENT"
    return f" {ch} "

def get_current_text_pages():
    return TEXT_PAGES_UPPER if text_upper else TEXT_PAGES_LOWER

def get_key_color(k_idx, override_val="NO_OVERRIDE"):
    prof = cur_prof()
    page_default = prof.get("theme", 0x002244 if prof["os"] == "PC" else 0x330044)
    c_val = prof["keys"][k_idx].get("color", None) if override_val == "NO_OVERRIDE" else override_val
    if c_val is None:
        return page_default
    return c_val

def update_leds():
    if mode in ("BUILD_TEXT", "BUILD_LABEL", "PAGE_NAME"):
        for i in range(9):
            macropad.pixels[i] = 0x003344
        macropad.pixels[9]  = 0x553300 if text_upper else 0x222200
        macropad.pixels[10] = 0x001155
        macropad.pixels[11] = 0x550000
        return

    for i in range(12):
        if mode == "OVERVIEW":
            if i == active_knob_key:
                macropad.pixels[i] = 0x00FFFF
            elif i in active_toggles:
                macropad.pixels[i] = 0xFF5500
            else:
                macropad.pixels[i] = get_key_color(i)
        elif mode == "SELECT_KEY":
            macropad.pixels[i] = 0x442200
        elif mode == "PAGE_MENU":
            macropad.pixels[i] = 0x004444
        elif mode == "PAGE_THEME":
            macropad.pixels[i] = THEME_PALETTE[menu_idx][1]
        elif mode == "PICK_COLOR":
            if i == selected_key:
                preview_val = COLOR_PALETTE[menu_idx][1]
                macropad.pixels[i] = get_key_color(i, override_val=preview_val)
            else:
                macropad.pixels[i] = 0x050505
        else:
            macropad.pixels[i] = 0x005500 if i == selected_key else 0x050505

def set_line(idx, text):
    if text_lines[idx].text != text:
        text_lines[idx].text = text

def render_screen(status=None):
    if is_sleeping:
        return
    macropad.display.auto_refresh = False
    prof = cur_prof()

    if mode == "OVERVIEW":
        total = len(config["profiles"])
        idx = config["active"] + 1
        snd = "S:ON" if config.get("sound", True) else "S:OFF"
        if status:
            hdr = status
        elif active_knob_key is not None and active_knob_key < len(prof["keys"]):
            k_act = prof["keys"][active_knob_key].get("action", "KNOB")
            hdr = f"{idx}/{total} [{k_act[:6]}] {snd}"
        else:
            hdr = f"{idx}/{total} {prof['name']} {snd}"
        set_line(0, hdr[:21])
        for r in range(4):
            c0 = format_cell(prof["keys"][r * 3]["label"])
            c1 = format_cell(prof["keys"][r * 3 + 1]["label"])
            c2 = format_cell(prof["keys"][r * 3 + 2]["label"])
            set_line(r + 1, f"{c0} {c1} {c2}")

    elif mode == "SELECT_KEY":
        set_line(0, f"EDIT [{prof['name']}]")
        set_line(1, "Press Key (1-12) Edit")
        set_line(2, "Push Knob: Page Menu")
        set_line(3, "Turn Knob: Cancel")
        set_line(4, "")

    elif mode == "PAGE_MENU":
        snd_label = "Sound: ON" if config.get("sound", True) else "Sound: OFF"
        opts = list(PAGE_MENU_OPTS)
        opts[2] = snd_label
        set_line(0, f"PAGE: {prof['name']} ({prof['os']})")
        set_line(1, f"> {opts[menu_idx]}")
        set_line(2, "Turn:Scroll Push:OK")
        set_line(3, "")
        set_line(4, "")

    elif mode == "PAGE_OS":
        title = "NEW PAGE OS:" if is_creating_page else "EDIT PAGE OS:"
        set_line(0, title)
        set_line(1, f"> {OS_OPTIONS[menu_idx]}")
        set_line(2, "Turn:Scroll Push:Next")
        set_line(3, "")
        set_line(4, "")

    elif mode == "PAGE_THEME":
        cname, _ = THEME_PALETTE[menu_idx]
        set_line(0, "PAGE DEFAULT COLOUR:")
        set_line(1, f"> {cname}")
        set_line(2, "Turn:Scroll Push:Next")
        set_line(3, "")
        set_line(4, "")

    elif mode == "CHOOSE_TYPE":
        set_line(0, f"KEY #{selected_key + 1}: {prof['keys'][selected_key]['label']}")
        set_line(1, f"> {EDIT_TYPES[menu_idx]}")
        set_line(2, "Turn:Scroll Push:OK")
        set_line(3, "")
        set_line(4, "")

    elif mode == "PICK_KNOB_ACTION":
        act_label, _, _ = KNOB_ACTIONS[menu_idx]
        set_line(0, f"KEY #{selected_key + 1} KNOB MODE:")
        set_line(1, f"> {act_label[:19]}")
        set_line(2, "Turn:Scroll Push:Next")
        set_line(3, "")
        set_line(4, "")

    elif mode == "PICK_PRESET":
        p = PRESETS[menu_idx]
        set_line(0, f"KEY #{selected_key + 1}: PRESET")
        set_line(1, f"> {p['label']}")
        set_line(2, f"({p.get('type', 'COMBO')})")
        set_line(3, "Turn:Scroll Push:Save")
        set_line(4, "")

    elif mode == "PICK_COLOR":
        cname, _ = COLOR_PALETTE[menu_idx]
        set_line(0, f"KEY #{selected_key + 1} COLOUR:")
        set_line(1, f"> {cname}")
        set_line(2, "Live preview on key")
        set_line(3, "Turn:Scroll Push:Save")
        set_line(4, "")

    elif mode == "PICK_BEHAVIOR":
        set_line(0, f"KEY #{selected_key + 1} BEHAVIOR:")
        set_line(1, f"> {BEHAVIOR_OPTIONS[menu_idx]}")
        set_line(2, "Turn:Scroll Push:Save")
        set_line(3, "")
        set_line(4, "")

    elif mode == "BUILD_COMBO_MODS":
        opt = MOD_OPTIONS[menu_idx]
        active = "+".join(edit_mods) if edit_mods else "None"
        set_line(0, f"MODS: {active[:14]}")
        set_line(1, f"> {opt}")
        set_line(2, "Key1:Bksp  Push:Toggl")
        set_line(3, "")
        set_line(4, "")

    elif mode == "BUILD_COMBO_KEY":
        kname = KEY_NAMES[edit_key_idx]
        active = "+".join(edit_mods)
        set_line(0, f"MODS: {active[:14]}")
        set_line(1, f"KEY:  > {kname} <")
        set_line(2, "Turn:Key   Push:Next")
        set_line(3, "")
        set_line(4, "")

    elif mode in ("BUILD_TEXT", "BUILD_LABEL", "PAGE_NAME"):
        if mode == "BUILD_TEXT":
            prefix = "TXT:"
        elif mode == "BUILD_LABEL":
            prefix = "LBL:"
        else:
            prefix = "PG:"

        disp_str = edit_string.replace("\n", "\\n")
        max_len = 21 - len(prefix) - 2
        if len(disp_str) > max_len:
            disp_str = ".." + disp_str[-(max_len - 2):]
        set_line(0, f"{prefix}[{disp_str}]")

        pages = get_current_text_pages()
        pg = pages[text_page_idx]
        for r in range(3):
            s0 = format_char_slot(pg[r * 3])
            s1 = format_char_slot(pg[r * 3 + 1])
            s2 = format_char_slot(pg[r * 3 + 2])
            if r == 0:
                p_info = f"P{text_page_idx + 1}/{len(pages)}"
                set_line(1, f"{s0} {s1} {s2}  {p_info}")
            elif r == 1:
                set_line(2, f"{s0} {s1} {s2}   Turn:Pg")
            else:
                set_line(3, f"{s0} {s1} {s2}   Push:OK")

        case_lbl = "A->a" if text_upper else "a->A"
        set_line(4, f"[{case_lbl}] [SPC]  [BKSP]")

    macropad.display.auto_refresh = True
    macropad.display.refresh()

def wake_up():
    global is_sleeping, last_activity_time
    last_activity_time = time.monotonic()
    if is_sleeping:
        is_sleeping = False
        text_lines.show()
        update_leds()
        render_screen()

def execute_action(item, state_active=True):
    is_mac = (cur_prof()["os"] == "MAC")
    atype = item.get("type", "COMBO")

    if atype == "TEXT":
        txt = item.get("text", "")
        parts = txt.split("\n")
        for idx, part in enumerate(parts):
            if part:
                macropad.keyboard_layout.write(part)
            if idx < len(parts) - 1:
                macropad.keyboard.send(K.ENTER)

    elif atype == "COMBO":
        codes = []
        for m in item.get("mods", []):
            if m == "OS":
                codes.append(K.GUI if is_mac else K.CONTROL)
            elif m == "OS_SWITCH":
                codes.append(K.GUI if is_mac else K.ALT)
            elif m == "OS_SCREEN":
                if is_mac:
                    macropad.keyboard.send(K.GUI, K.SHIFT, K.FOUR)
                    return
                else:
                    codes.extend([K.GUI, K.SHIFT])
            elif m == "SHIFT":
                codes.append(K.SHIFT)
            elif m == "ALT":
                codes.append(K.ALT)
            elif m == "CTRL":
                codes.append(K.CONTROL)
            elif m == "GUI":
                codes.append(K.GUI)

        main_k = KEY_MAP.get(item.get("key", "A"), K.A)
        codes.append(main_k)
        if state_active:
            macropad.keyboard.send(*codes)

    elif atype == "MEDIA":
        action = item.get("action")
        cc = macropad.consumer_control
        cc_codes = {
            "VOL_UP": macropad.ConsumerControlCode.VOLUME_INCREMENT,
            "VOL_DOWN": macropad.ConsumerControlCode.VOLUME_DECREMENT,
            "MUTE": macropad.ConsumerControlCode.MUTE,
            "PLAY_PAUSE": macropad.ConsumerControlCode.PLAY_PAUSE,
            "NEXT": macropad.ConsumerControlCode.SCAN_NEXT_TRACK,
            "PREV": macropad.ConsumerControlCode.SCAN_PREVIOUS_TRACK,
        }
        if action in cc_codes:
            code = cc_codes[action]
            if item.get("behavior") == "HOLD":
                cc.press(code)
                time.sleep(0.1)
                cc.release()
            else:
                cc.send(code)

    elif atype == "SPECIAL":
        action = item.get("action")
        if action == "JIGGLER":
            if state_active:
                macropad.mouse.move(x=2)
                time.sleep(0.05)
                macropad.mouse.move(x=-2)

def execute_knob_rotate(step, curr_dir):
    prof = cur_prof()
    if active_knob_key is None or active_knob_key >= len(prof["keys"]):
        return
    item = prof["keys"][active_knob_key]
    action = item.get("action", "VOLUME")
    is_mac = (prof.get("os") == "MAC")
    cc = macropad.consumer_control

    if action == "VOLUME":
        code = (
            macropad.ConsumerControlCode.VOLUME_INCREMENT
            if curr_dir > 0
            else macropad.ConsumerControlCode.VOLUME_DECREMENT
        )
        for _ in range(abs(step)):
            cc.send(code)
        play_sound(900 if curr_dir > 0 else 700, 0.015)

    elif action == "SCROLL_V":
        wheel_delta = -step
        macropad.mouse.move(wheel=wheel_delta)

    elif action == "SCROLL_H":
        wheel_delta = -step
        macropad.keyboard.press(K.SHIFT)
        macropad.mouse.move(wheel=wheel_delta)
        macropad.keyboard.release(K.SHIFT)

    elif action == "ZOOM":
        mod = K.GUI if is_mac else K.CONTROL
        wheel_delta = step
        macropad.keyboard.press(mod)
        macropad.mouse.move(wheel=wheel_delta)
        macropad.keyboard.release(mod)

    elif action == "TRACK":
        code = (
            macropad.ConsumerControlCode.SCAN_NEXT_TRACK
            if curr_dir > 0
            else macropad.ConsumerControlCode.SCAN_PREVIOUS_TRACK
        )
        cc.send(code)
        play_sound(1000 if curr_dir > 0 else 600, 0.03)

    elif action == "TABS":
        mod = K.GUI if is_mac else K.CONTROL
        if curr_dir > 0:
            macropad.keyboard.send(mod, K.TAB)
        else:
            macropad.keyboard.send(mod, K.SHIFT, K.TAB)

    elif action == "UNDO_REDO":
        mod = K.GUI if is_mac else K.CONTROL
        if curr_dir > 0:
            if is_mac:
                macropad.keyboard.send(mod, K.SHIFT, K.Z)
            else:
                macropad.keyboard.send(mod, K.Y)
        else:
            macropad.keyboard.send(mod, K.Z)

    elif action == "BRIGHTNESS":
        bright_inc = getattr(macropad.ConsumerControlCode, "BRIGHTNESS_INCREMENT", None)
        bright_dec = getattr(macropad.ConsumerControlCode, "BRIGHTNESS_DECREMENT", None)
        if bright_inc and bright_dec:
            code = bright_inc if curr_dir > 0 else bright_dec
            for _ in range(abs(step)):
                cc.send(code)

def handle_knob_push():
    prof = cur_prof()
    if active_knob_key is None or active_knob_key >= len(prof["keys"]):
        return
    item = prof["keys"][active_knob_key]
    action = item.get("action", "VOLUME")
    is_mac = (prof.get("os") == "MAC")
    cc = macropad.consumer_control

    if action == "VOLUME":
        cc.send(macropad.ConsumerControlCode.MUTE)
        play_sound(600, 0.04)

    elif action in ("SCROLL_V", "SCROLL_H"):
        mid_btn = getattr(macropad, "Mouse", None)
        mid_btn_val = getattr(mid_btn, "MIDDLE_BUTTON", 4) if mid_btn else 4
        macropad.mouse.press(mid_btn_val)
        time.sleep(0.05)
        macropad.mouse.release_all()
        play_sound(800, 0.02)

    elif action == "ZOOM":
        mod = K.GUI if is_mac else K.CONTROL
        macropad.keyboard.send(mod, K.ZERO)
        play_sound(1000, 0.04)

    elif action == "TRACK":
        cc.send(macropad.ConsumerControlCode.PLAY_PAUSE)
        play_sound(800, 0.04)

    elif action == "TABS":
        mod = K.GUI if is_mac else K.CONTROL
        macropad.keyboard.send(mod, K.W)
        play_sound(500, 0.04)

    elif action == "UNDO_REDO":
        macropad.keyboard.send(K.ESCAPE)
        play_sound(400, 0.03)

update_leds()
render_screen()

# ============================================================================
# 4. MAIN INTERACTION LOOP
# ============================================================================
while True:
# --- WEB SERIAL CONFIGURATOR LISTENER ---
    for s_port in (getattr(usb_cdc, "data", None), getattr(usb_cdc, "console", None)):
        if s_port is not None and s_port.in_waiting > 0:
            try:
                raw = s_port.read(s_port.in_waiting)
                # Convert incoming bytes directly to a string and replace \r with \n
                chunk = raw.decode("utf-8", "ignore").replace("\r", "\n")
                serial_buf += chunk

                while "\n" in serial_buf:
                    cut = serial_buf.find("\n")
                    cmd = serial_buf[:cut].strip()
                    serial_buf = serial_buf[cut + 1:]

                    if not cmd:
                        continue

                    if "GET_CONFIG" in cmd:
                        wake_up()
                        render_screen("Web Connected!")
                        play_sound(1000, 0.05)
                        payload = "\nCONFIG_DATA:" + json.dumps(config) + "\n"
                        s_port.write(payload.encode("utf-8"))

                    elif "SET_CONFIG:" in cmd and "{" in cmd:
                        try:
                            json_part = cmd[cmd.index("{"):]
                            new_cfg = json.loads(json_part)
                            if "profiles" in new_cfg and len(new_cfg["profiles"]) > 0:
                                config = new_cfg
                                if config.get("active", 0) >= len(config["profiles"]):
                                    config["active"] = 0
                                save_config()
                                active_toggles.clear()
                                active_knob_key = None
                                mode = "OVERVIEW"
                                wake_up()
                                update_leds()
                                render_screen("Synced via Web!")
                                play_sound(1200, 0.08)
                                s_port.write(b"\nSTATUS:OK\n")
                        except Exception as e:
                            err_msg = f"\nSTATUS:ERR:{e}\n"
                            s_port.write(err_msg.encode("utf-8"))
            except Exception as e:
                set_line(4, f"ERR:{str(e)[:16]}")
#####################################
            
    now = time.monotonic()
    prof = cur_prof()

    # --- SLEEP TIMER CHECK (3 MINUTES) ---
    if not is_sleeping and (now - last_activity_time > 180.0) and mode == "OVERVIEW":
        is_sleeping = True
        macropad.display.root_group = None
        macropad.display.refresh()

    # --- BACKGROUND JIGGLER TICK ---
    for t_idx in list(active_toggles):
        if t_idx < len(prof["keys"]):
            k_item = prof["keys"][t_idx]
            if k_item.get("type") == "SPECIAL" and k_item.get("action") == "JIGGLER":
                if int(now) % 30 == 0:
                    execute_action(k_item, state_active=True)

    # --- A. ROTARY KNOB PUSH ---
    macropad.encoder_switch_debounced.update()
    if macropad.encoder_switch_debounced.pressed:
        wake_up()
        play_sound(800, 0.03)
        last_encoder = macropad.encoder

        if mode == "OVERVIEW":
            if active_knob_key is not None:
                handle_knob_push()
            else:
                mode = "SELECT_KEY"
                update_leds()
                render_screen()

        elif mode == "SELECT_KEY":
            menu_idx = 0
            mode = "PAGE_MENU"
            update_leds()
            render_screen()

        elif mode == "PAGE_MENU":
            choice = PAGE_MENU_OPTS[menu_idx]
            if menu_idx == 2:  # Toggle Sound
                config["sound"] = not config.get("sound", True)
                save_config()
                render_screen()
                continue

            if choice == "Add New Page":
                is_creating_page = True
                menu_idx = 0
                mode = "PAGE_OS"
            elif choice == "Edit Current Page":
                is_creating_page = False
                menu_idx = 0 if prof["os"] == "PC" else 1
                mode = "PAGE_OS"
            elif choice == "Delete This Page":
                if len(config["profiles"]) > 1:
                    config["profiles"].pop(config["active"])
                    config["active"] = config["active"] % len(config["profiles"])
                    save_config()
                    play_sound(300, 0.1)
                    active_toggles.clear()
                    active_knob_key = None
                    mode = "OVERVIEW"
                    update_leds()
                    render_screen("Page Deleted!")
                    continue
                else:
                    mode = "OVERVIEW"
                    update_leds()
                    render_screen("Keep at least 1!")
                    continue
            else:
                mode = "OVERVIEW"
            update_leds()
            render_screen()

        elif mode == "PAGE_OS":
            temp_page_os = "PC" if menu_idx == 0 else "MAC"
            target_theme = 0x002244 if temp_page_os == "PC" else 0x330044
            if not is_creating_page:
                target_theme = prof.get("theme", target_theme)
            menu_idx = 0
            for idx, (_, c_val) in enumerate(THEME_PALETTE):
                if c_val == target_theme:
                    menu_idx = idx
                    break
            mode = "PAGE_THEME"
            update_leds()
            render_screen()

        elif mode == "PAGE_THEME":
            temp_page_theme = THEME_PALETTE[menu_idx][1]
            if is_creating_page:
                edit_string = f"{temp_page_os} {len(config['profiles']) + 1}"[:6]
            else:
                edit_string = prof["name"][:6]
            text_page_idx = 0
            text_upper = True
            mode = "PAGE_NAME"
            update_leds()
            render_screen()

        elif mode == "PAGE_NAME":
            final_name = edit_string[:6].strip() or temp_page_os
            if is_creating_page:
                new_p = {
                    "name": final_name,
                    "os": temp_page_os,
                    "theme": temp_page_theme,
                    "keys": clone_keys(PRESETS[:12])
                }
                config["profiles"].append(new_p)
                config["active"] = len(config["profiles"]) - 1
            else:
                prof["name"] = final_name
                prof["os"] = temp_page_os
                prof["theme"] = temp_page_theme
            save_config()
            play_sound(1200, 0.08)
            mode = "OVERVIEW"
            update_leds()
            render_screen("Page Saved!")

        elif mode == "CHOOSE_TYPE":
            choice = EDIT_TYPES[menu_idx]
            if choice == "Pick Preset":
                menu_idx = 0
                mode = "PICK_PRESET"
            elif choice == "New Key Combo":
                edit_mods = []
                menu_idx = 1
                mode = "BUILD_COMBO_MODS"
            elif choice == "New Text String":
                edit_string = ""
                text_page_idx = 0
                text_upper = False
                mode = "BUILD_TEXT"
            elif choice == "Knob Function":
                menu_idx = 0
                mode = "PICK_KNOB_ACTION"
            elif choice == "Set Key Colour":
                curr_c = prof["keys"][selected_key].get("color", None)
                menu_idx = 0
                for idx, (_, c_val) in enumerate(COLOR_PALETTE):
                    if c_val == curr_c:
                        menu_idx = idx
                        break
                mode = "PICK_COLOR"
            elif choice == "Set Key Behaviour":
                curr_b = prof["keys"][selected_key].get("behavior", "NORMAL")
                menu_idx = 0
                for idx, b_name in enumerate(BEHAVIOR_OPTIONS):
                    if b_name.split()[0] == curr_b:
                        menu_idx = idx
                        break
                mode = "PICK_BEHAVIOR"
            else:
                mode = "OVERVIEW"
            update_leds()
            render_screen()

        elif mode == "PICK_PRESET":
            existing_color = prof["keys"][selected_key].get("color", None)
            existing_behavior = prof["keys"][selected_key].get("behavior", "NORMAL")
            new_key_cfg = dict(PRESETS[menu_idx])
            new_key_cfg["color"] = existing_color
            new_key_cfg["behavior"] = existing_behavior
            prof["keys"][selected_key] = new_key_cfg
            save_config()
            play_sound(1200, 0.08)
            mode = "OVERVIEW"
            update_leds()
            render_screen("Saved!")

        elif mode == "PICK_KNOB_ACTION":
            act_label, act_id, default_lbl = KNOB_ACTIONS[menu_idx]
            existing_color = prof["keys"][selected_key].get("color", None)
            prof["keys"][selected_key] = {
                "label": default_lbl,
                "type": "KNOB",
                "action": act_id,
                "color": existing_color,
                "behavior": "TOGGLE"
            }
            edit_string = default_lbl
            text_page_idx = 0
            text_upper = True
            mode = "BUILD_LABEL"
            update_leds()
            render_screen()

        elif mode == "PICK_COLOR":
            prof["keys"][selected_key]["color"] = COLOR_PALETTE[menu_idx][1]
            save_config()
            play_sound(1200, 0.08)
            mode = "OVERVIEW"
            update_leds()
            render_screen("Colour Saved!")

        elif mode == "PICK_BEHAVIOR":
            b_type = BEHAVIOR_OPTIONS[menu_idx].split()[0]
            prof["keys"][selected_key]["behavior"] = b_type
            save_config()
            play_sound(1200, 0.08)
            mode = "OVERVIEW"
            update_leds()
            render_screen("Behavior Saved!")

        elif mode == "BUILD_COMBO_MODS":
            if menu_idx == 0:
                edit_key_idx = 0
                mode = "BUILD_COMBO_KEY"
            else:
                mod_map = {1: "OS", 2: "SHIFT", 3: "ALT", 4: "CTRL", 5: "GUI"}
                m_val = mod_map[menu_idx]
                if m_val in edit_mods:
                    edit_mods.remove(m_val)
                else:
                    edit_mods.append(m_val)
            render_screen()

        elif mode == "BUILD_COMBO_KEY":
            kname = KEY_NAMES[edit_key_idx]
            default_lbl = ("+".join(edit_mods)[:2] + kname)[:6] if edit_mods else kname[:6]
            existing_color = prof["keys"][selected_key].get("color", None)
            existing_behavior = prof["keys"][selected_key].get("behavior", "NORMAL")
            prof["keys"][selected_key] = {
                "label": default_lbl,
                "type": "COMBO",
                "mods": list(edit_mods),
                "key": kname,
                "color": existing_color,
                "behavior": existing_behavior
            }
            edit_string = default_lbl
            text_page_idx = 0
            text_upper = True
            mode = "BUILD_LABEL"
            update_leds()
            render_screen()

        elif mode == "BUILD_TEXT":
            default_lbl = edit_string.replace("\n", "")[:6].strip() or "Text"
            existing_color = prof["keys"][selected_key].get("color", None)
            existing_behavior = prof["keys"][selected_key].get("behavior", "NORMAL")
            prof["keys"][selected_key] = {
                "label": default_lbl,
                "type": "TEXT",
                "text": edit_string,
                "color": existing_color,
                "behavior": existing_behavior
            }
            edit_string = default_lbl
            text_page_idx = 0
            text_upper = True
            mode = "BUILD_LABEL"
            update_leds()
            render_screen()

        elif mode == "BUILD_LABEL":
            prof["keys"][selected_key]["label"] = (edit_string[:6].strip() or "Macro")
            save_config()
            play_sound(1200, 0.08)
            mode = "OVERVIEW"
            update_leds()
            render_screen("Saved!")

    # --- B. ROTARY KNOB ROTATION ---
    curr_enc = macropad.encoder
    if curr_enc != last_encoder:
        wake_up()
        delta = curr_enc - last_encoder
        last_encoder = curr_enc
        curr_dir = 1 if delta > 0 else -1

        if curr_dir != last_dir and (now - last_turn_time) < 0.04:
            continue

        last_dir = curr_dir
        last_turn_time = now
        step = curr_dir if abs(delta) <= 2 else delta

        if mode == "OVERVIEW":
            if active_knob_key is not None:
                execute_knob_rotate(step, curr_dir)
            else:
                config["active"] = (config["active"] + step) % len(config["profiles"])
                save_config()
                update_leds()
                render_screen()
                play_sound(500 + (config["active"] * 150), 0.06)
        elif mode == "SELECT_KEY":
            mode = "OVERVIEW"
            update_leds()
            render_screen()
        elif mode == "PAGE_MENU":
            menu_idx = (menu_idx + step) % len(PAGE_MENU_OPTS)
            render_screen()
        elif mode == "PAGE_OS":
            menu_idx = (menu_idx + step) % len(OS_OPTIONS)
            render_screen()
        elif mode == "PAGE_THEME":
            menu_idx = (menu_idx + step) % len(THEME_PALETTE)
            update_leds()
            render_screen()
        elif mode == "CHOOSE_TYPE":
            menu_idx = (menu_idx + step) % len(EDIT_TYPES)
            render_screen()
        elif mode == "PICK_KNOB_ACTION":
            menu_idx = (menu_idx + step) % len(KNOB_ACTIONS)
            render_screen()
        elif mode == "PICK_PRESET":
            menu_idx = (menu_idx + step) % len(PRESETS)
            render_screen()
        elif mode == "PICK_COLOR":
            menu_idx = (menu_idx + step) % len(COLOR_PALETTE)
            update_leds()
            render_screen()
        elif mode == "PICK_BEHAVIOR":
            menu_idx = (menu_idx + step) % len(BEHAVIOR_OPTIONS)
            render_screen()
        elif mode == "BUILD_COMBO_MODS":
            menu_idx = (menu_idx + step) % len(MOD_OPTIONS)
            render_screen()
        elif mode == "BUILD_COMBO_KEY":
            edit_key_idx = (edit_key_idx + step) % len(KEY_NAMES)
            render_screen()
        elif mode in ("BUILD_TEXT", "BUILD_LABEL", "PAGE_NAME"):
            text_page_idx = (text_page_idx + step) % len(get_current_text_pages())
            render_screen()

    # --- C. KEYPAD BUTTON PRESSES ---
    key_event = macropad.keys.events.get()
    if key_event:
        wake_up()
        k = key_event.key_number

        if key_event.pressed:
            if mode == "OVERVIEW":
                item = prof["keys"][k]
                atype = item.get("type", "COMBO")
                beh = item.get("behavior", "NORMAL")

                if atype == "KNOB":
                    if active_knob_key == k:
                        active_toggles.discard(k)
                        active_knob_key = None
                        play_sound(300, 0.04)
                    else:
                        if active_knob_key is not None:
                            active_toggles.discard(active_knob_key)
                        active_toggles.add(k)
                        active_knob_key = k
                        play_sound(900, 0.04)
                    update_leds()
                    render_screen()
                elif beh == "TOGGLE":
                    if k in active_toggles:
                        active_toggles.remove(k)
                        execute_action(item, state_active=False)
                        play_sound(300, 0.04)
                    else:
                        active_toggles.add(k)
                        execute_action(item, state_active=True)
                        play_sound(900, 0.04)
                    update_leds()
                elif beh == "HOLD":
                    macropad.pixels[k] = 0xFFFFFF
                    execute_action(item, state_active=True)
                else:
                    macropad.pixels[k] = 0xFFFFFF
                    play_sound(700, 0.03)
                    execute_action(item, state_active=True)
                    time.sleep(0.08)
                    update_leds()

            elif mode == "SELECT_KEY":
                selected_key = k
                menu_idx = 0
                mode = "CHOOSE_TYPE"
                update_leds()
                render_screen()

            elif mode == "BUILD_COMBO_MODS":
                if k == 0 and edit_mods:
                    edit_mods.pop()
                    render_screen()

            elif mode in ("BUILD_TEXT", "BUILD_LABEL", "PAGE_NAME"):
                max_chars = 6 if mode in ("BUILD_LABEL", "PAGE_NAME") else 120
                if k < 9:
                    ch = get_current_text_pages()[text_page_idx][k]
                    if ch == "\n" and mode in ("BUILD_LABEL", "PAGE_NAME"):
                        pass
                    elif len(edit_string) < max_chars:
                        edit_string += ch
                        play_sound(1000, 0.02)
                elif k == 9:
                    text_upper = not text_upper
                elif k == 10:
                    if len(edit_string) < max_chars:
                        edit_string += " "
                        play_sound(900, 0.02)
                elif k == 11:
                    edit_string = edit_string[:-1]
                    play_sound(400, 0.03)

                update_leds()
                render_screen()

        elif not key_event.pressed and mode == "OVERVIEW":
            item = prof["keys"][k]
            if item.get("type") != "KNOB" and item.get("behavior", "NORMAL") == "HOLD":
                execute_action(item, state_active=False)
                update_leds()