#include "config.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// UI STATE MACHINE DEFINITIONS
// ============================================================================
typedef enum {
    MODE_OVERVIEW = 0,
    MODE_SELECT_KEY,
    MODE_PAGE_MENU,
    MODE_PAGE_OS,
    MODE_PAGE_THEME,
    MODE_PAGE_NAME,
    MODE_CHOOSE_TYPE,
    MODE_PICK_KNOB_ACTION,
    MODE_PICK_PRESET,
    MODE_PICK_COLOR,
    MODE_PICK_BEHAVIOR,
    MODE_BUILD_COMBO_MODS,
    MODE_BUILD_COMBO_KEY,
    MODE_BUILD_TEXT,
    MODE_BUILD_LABEL
} ui_mode_t;

static const char * const EDIT_TYPES[] = {
    "Pick Preset",
    "New Key Combo",
    "New Text String",
    "Knob Function",
    "Set Key Colour",
    "Set Key Behaviour",
    "Cancel"
};
#define NUM_EDIT_TYPES 7

static const char * const BEHAVIOR_OPTIONS[] = {
    "NORMAL (One-shot)",
    "HOLD (Hold down)",
    "TOGGLE (Latch on/off)"
};
#define NUM_BEHAVIOR_OPTIONS 3

static const char * const PAGE_MENU_OPTS[] = {
    "Add New Page",
    "Edit Current Page",
    "Toggle Sound",
    "Delete This Page",
    "Cancel"
};
#define NUM_PAGE_MENU_OPTS 5

static const char * const MOD_OPTIONS[] = {
    "[NEXT -> Key]",
    "OS (Ctrl/Cmd)",
    "SHIFT",
    "ALT/OPT",
    "CTRL",
    "GUI/CMD"
};
#define NUM_MOD_OPTIONS 6

static const char * const OS_OPTIONS[] = {
    "PC (Win/Linux)",
    "MAC (macOS)"
};
#define NUM_OS_OPTIONS 2

// ============================================================================
// RUNTIME STATE VARIABLES
// ============================================================================
static ui_mode_t s_mode = MODE_OVERVIEW;
static int s_selected_key = 0;
static int s_menu_idx = 0;
static int32_t s_last_encoder = 0;
static int s_last_dir = 0;
static uint64_t s_last_turn_time_ms = 0;

static uint8_t s_edit_mods[MAX_MODS];
static uint8_t s_edit_mod_count = 0;
static int s_edit_key_idx = 0;
static char s_edit_string[MAX_TEXT_LEN];
static int s_text_page_idx = 0;
static bool s_text_upper = false;

static target_os_t s_temp_page_os = OS_PC;
static uint32_t s_temp_page_theme = 0x002244;
static bool s_is_creating_page = false;

static uint16_t s_active_toggles_mask = 0; // bitmask of keys 0..11
static int s_active_knob_key = -1;         // -1 for None, 0..11 for active knob key
static uint64_t s_last_activity_ms = 0;
static uint64_t s_last_jiggle_sec = 0;
static bool s_is_sleeping = false;

// WebSerial buffers
#define SERIAL_BUF_SIZE 8192
static char s_serial_buf[SERIAL_BUF_SIZE];
static int s_serial_len = 0;
static char s_json_io_buf[8192];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
static inline profile_config_t *cur_prof(void) {
    if (g_config.active >= g_config.profile_count) {
        g_config.active = 0;
    }
    return &g_config.profiles[g_config.active];
}

static int wrap_index(int val, int count) {
    if (count <= 0) return 0;
    int r = val % count;
    return (r < 0) ? (r + count) : r;
}

static void play_sound(uint32_t freq_hz, float duration_sec) {
    if (g_config.sound) {
        uint32_t ms = (uint32_t)(duration_sec * 1000.0f);
        if (ms < 1) ms = 1;
        hw_play_tone(freq_hz, ms);
    }
}

static void format_cell(const char *label, char out[7]) {
    int len = 0;
    while (label && label[len] && len < 6) {
        out[len] = label[len];
        len++;
    }
    while (len < 6) {
        out[len++] = ' ';
    }
    out[6] = '\0';
}

static void format_char_slot(char ch, char out[4]) {
    if (ch == '\n') {
        strcpy(out, "ENT");
    } else {
        out[0] = ' ';
        out[1] = ch;
        out[2] = ' ';
        out[3] = '\0';
    }
}

static const char (*get_current_text_pages(void))[9] {
    return s_text_upper ? TEXT_PAGES_UPPER : TEXT_PAGES_LOWER;
}

static uint32_t get_key_color(int k_idx, bool has_override, int32_t override_val) {
    profile_config_t *prof = cur_prof();
    uint32_t page_default = prof->theme;
    int32_t c_val = has_override ? override_val : prof->keys[k_idx].color;
    if (c_val == COLOR_PAGE_DEFAULT) {
        return page_default;
    }
    return (uint32_t)c_val;
}

static void update_leds(void) {
    if (s_is_sleeping) {
        for (int i = 0; i < 12; i++) {
            hw_pixel_set(i, 0x000000);
        }
        hw_pixels_show();
        return;
    }

    if (s_mode == MODE_BUILD_TEXT || s_mode == MODE_BUILD_LABEL || s_mode == MODE_PAGE_NAME) {
        for (int i = 0; i < 9; i++) {
            hw_pixel_set(i, 0x003344);
        }
        hw_pixel_set(9,  s_text_upper ? 0x553300 : 0x222200);
        hw_pixel_set(10, 0x001155);
        hw_pixel_set(11, 0x550000);
        hw_pixels_show();
        return;
    }

    for (int i = 0; i < 12; i++) {
        if (s_mode == MODE_OVERVIEW) {
            if (i == s_active_knob_key) {
                hw_pixel_set(i, 0x00FFFF);
            } else if (s_active_toggles_mask & (1u << i)) {
                hw_pixel_set(i, 0xFF5500);
            } else {
                hw_pixel_set(i, get_key_color(i, false, 0));
            }
        } else if (s_mode == MODE_SELECT_KEY) {
            hw_pixel_set(i, 0x442200);
        } else if (s_mode == MODE_PAGE_MENU) {
            hw_pixel_set(i, 0x004444);
        } else if (s_mode == MODE_PAGE_THEME) {
            hw_pixel_set(i, (uint32_t)THEME_PALETTE[s_menu_idx].value);
        } else if (s_mode == MODE_PICK_COLOR) {
            if (i == s_selected_key) {
                int32_t preview_val = COLOR_PALETTE[s_menu_idx].value;
                hw_pixel_set(i, get_key_color(i, true, preview_val));
            } else {
                hw_pixel_set(i, 0x050505);
            }
        } else {
            hw_pixel_set(i, (i == s_selected_key) ? 0x005500 : 0x050505);
        }
    }
    hw_pixels_show();
}

static void build_mods_string(char *out, size_t max_len) {
    out[0] = '\0';
    for (uint8_t i = 0; i < s_edit_mod_count; i++) {
        if (i > 0 && strlen(out) + 1 < max_len) {
            strcat(out, "+");
        }
        const char *mstr = mod_to_str((mod_id_t)s_edit_mods[i]);
        if (strlen(out) + strlen(mstr) < max_len) {
            strcat(out, mstr);
        }
    }
}

static void render_screen(const char *status) {
    if (s_is_sleeping) return;
    profile_config_t *prof = cur_prof();
    char line[64];

    if (s_mode == MODE_OVERVIEW) {
        int total = g_config.profile_count;
        int idx = g_config.active + 1;
        const char *snd = g_config.sound ? "S:ON" : "S:OFF";
        if (status && status[0]) {
            snprintf(line, sizeof(line), "%.21s", status);
        } else if (s_active_knob_key >= 0 && s_active_knob_key < NUM_KEYS) {
            const char *k_act = prof->keys[s_active_knob_key].action[0]
                                ? prof->keys[s_active_knob_key].action
                                : "KNOB";
            snprintf(line, sizeof(line), "%d/%d [%.6s] %s", idx, total, k_act, snd);
        } else {
            snprintf(line, sizeof(line), "%d/%d %s %s", idx, total, prof->name, snd);
        }
        hw_display_set_line(0, line);

        for (int r = 0; r < 4; r++) {
            char c0[7], c1[7], c2[7];
            format_cell(prof->keys[r * 3].label, c0);
            format_cell(prof->keys[r * 3 + 1].label, c1);
            format_cell(prof->keys[r * 3 + 2].label, c2);
            snprintf(line, sizeof(line), "%s %s %s", c0, c1, c2);
            hw_display_set_line(r + 1, line);
        }
    } else if (s_mode == MODE_SELECT_KEY) {
        snprintf(line, sizeof(line), "EDIT [%s]", prof->name);
        hw_display_set_line(0, line);
        hw_display_set_line(1, "Press Key (1-12) Edit");
        hw_display_set_line(2, "Push Knob: Page Menu");
        hw_display_set_line(3, "Turn Knob: Cancel");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_PAGE_MENU) {
        const char *opt = PAGE_MENU_OPTS[s_menu_idx];
        if (s_menu_idx == 2) {
            opt = g_config.sound ? "Sound: ON" : "Sound: OFF";
        }
        snprintf(line, sizeof(line), "PAGE: %s (%s)", prof->name, prof->os == OS_MAC ? "MAC" : "PC");
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "> %s", opt);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Turn:Scroll Push:OK");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_PAGE_OS) {
        hw_display_set_line(0, s_is_creating_page ? "NEW PAGE OS:" : "EDIT PAGE OS:");
        snprintf(line, sizeof(line), "> %s", OS_OPTIONS[s_menu_idx]);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Turn:Scroll Push:Next");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_PAGE_THEME) {
        hw_display_set_line(0, "PAGE DEFAULT COLOUR:");
        snprintf(line, sizeof(line), "> %s", THEME_PALETTE[s_menu_idx].name);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Turn:Scroll Push:Next");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_CHOOSE_TYPE) {
        snprintf(line, sizeof(line), "KEY #%d: %s", s_selected_key + 1, prof->keys[s_selected_key].label);
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "> %s", EDIT_TYPES[s_menu_idx]);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Turn:Scroll Push:OK");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_PICK_KNOB_ACTION) {
        snprintf(line, sizeof(line), "KEY #%d KNOB MODE:", s_selected_key + 1);
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "> %.19s", KNOB_ACTIONS[s_menu_idx].label);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Turn:Scroll Push:Next");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_PICK_PRESET) {
        const key_config_t *p = &PRESETS[s_menu_idx];
        snprintf(line, sizeof(line), "KEY #%d: PRESET", s_selected_key + 1);
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "> %s", p->label);
        hw_display_set_line(1, line);
        snprintf(line, sizeof(line), "(%s)", key_type_to_str(p->type));
        hw_display_set_line(2, line);
        hw_display_set_line(3, "Turn:Scroll Push:Save");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_PICK_COLOR) {
        snprintf(line, sizeof(line), "KEY #%d COLOUR:", s_selected_key + 1);
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "> %s", COLOR_PALETTE[s_menu_idx].name);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Live preview on key");
        hw_display_set_line(3, "Turn:Scroll Push:Save");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_PICK_BEHAVIOR) {
        snprintf(line, sizeof(line), "KEY #%d BEHAVIOR:", s_selected_key + 1);
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "> %s", BEHAVIOR_OPTIONS[s_menu_idx]);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Turn:Scroll Push:Save");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_BUILD_COMBO_MODS) {
        char active_mods[64];
        if (s_edit_mod_count == 0) {
            strcpy(active_mods, "None");
        } else {
            build_mods_string(active_mods, sizeof(active_mods));
        }
        snprintf(line, sizeof(line), "MODS: %.14s", active_mods);
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "> %s", MOD_OPTIONS[s_menu_idx]);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Key1:Bksp  Push:Toggl");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_BUILD_COMBO_KEY) {
        char active_mods[64];
        build_mods_string(active_mods, sizeof(active_mods));
        snprintf(line, sizeof(line), "MODS: %.14s", active_mods);
        hw_display_set_line(0, line);
        snprintf(line, sizeof(line), "KEY:  > %s <", KEY_NAMES[s_edit_key_idx]);
        hw_display_set_line(1, line);
        hw_display_set_line(2, "Turn:Key   Push:Next");
        hw_display_set_line(3, "");
        hw_display_set_line(4, "");
    } else if (s_mode == MODE_BUILD_TEXT || s_mode == MODE_BUILD_LABEL || s_mode == MODE_PAGE_NAME) {
        const char *prefix = (s_mode == MODE_BUILD_TEXT)  ? "TXT:" :
                             (s_mode == MODE_BUILD_LABEL) ? "LBL:" : "PG:";

        // Replace '\n' with literal "\\n" for display
        char disp_str[MAX_TEXT_LEN * 2];
        int d_len = 0;
        for (int i = 0; s_edit_string[i] && d_len < (int)sizeof(disp_str) - 3; i++) {
            if (s_edit_string[i] == '\n') {
                disp_str[d_len++] = '\\';
                disp_str[d_len++] = 'n';
            } else {
                disp_str[d_len++] = s_edit_string[i];
            }
        }
        disp_str[d_len] = '\0';

        int max_len = 21 - (int)strlen(prefix) - 2;
        if (d_len > max_len) {
            snprintf(line, sizeof(line), "%s[..%.*s]", prefix, max_len - 2, &disp_str[d_len - (max_len - 2)]);
        } else {
            snprintf(line, sizeof(line), "%s[%.*s]", prefix, max_len, disp_str);
        }
        hw_display_set_line(0, line);

        const char (*pages)[9] = get_current_text_pages();
        const char *pg = pages[s_text_page_idx];
        for (int r = 0; r < 3; r++) {
            char s0[4], s1[4], s2[4];
            format_char_slot(pg[r * 3], s0);
            format_char_slot(pg[r * 3 + 1], s1);
            format_char_slot(pg[r * 3 + 2], s2);
            if (r == 0) {
                snprintf(line, sizeof(line), "%s %s %s  P%d/%d", s0, s1, s2, s_text_page_idx + 1, NUM_TEXT_PAGES);
                hw_display_set_line(1, line);
            } else if (r == 1) {
                snprintf(line, sizeof(line), "%s %s %s   Turn:Pg", s0, s1, s2);
                hw_display_set_line(2, line);
            } else {
                snprintf(line, sizeof(line), "%s %s %s   Push:OK", s0, s1, s2);
                hw_display_set_line(3, line);
            }
        }

        const char *case_lbl = s_text_upper ? "A->a" : "a->A";
        snprintf(line, sizeof(line), "[%s] [SPC]  [BKSP]", case_lbl);
        hw_display_set_line(4, line);
    }

    hw_display_refresh();
}

static void wake_up(void) {
    s_last_activity_ms = hw_monotonic_ms();
    if (s_is_sleeping) {
        s_is_sleeping = false;
        hw_display_set_sleep(false);
        update_leds();
        render_screen(NULL);
    }
}

// ============================================================================
// ACTION EXECUTION (KEY COMBOS, TEXT, MEDIA, SPECIAL, KNOB)
// ============================================================================
static void execute_action(const key_config_t *item, bool state_active) {
    bool is_mac = (cur_prof()->os == OS_MAC);

    if (item->type == KEY_TYPE_TEXT) {
        if (!state_active) return;
        hw_keyboard_write_text(item->text);
    } else if (item->type == KEY_TYPE_COMBO) {
        uint8_t mods = 0;
        for (int i = 0; i < item->mod_count; i++) {
            mod_id_t m = (mod_id_t)item->mod_order[i];
            if (m == MOD_OS) {
                mods |= is_mac ? HID_MOD_LGUI : HID_MOD_LCTRL;
            } else if (m == MOD_OS_SWITCH) {
                mods |= is_mac ? HID_MOD_LGUI : HID_MOD_LALT;
            } else if (m == MOD_OS_SCREEN) {
                if (is_mac) {
                    if (state_active) {
                        uint8_t k4 = HID_KEY_4;
                        hw_keyboard_send(HID_MOD_LGUI | HID_MOD_LSHIFT, &k4, 1);
                    }
                    return;
                } else {
                    mods |= (HID_MOD_LGUI | HID_MOD_LSHIFT);
                }
            } else if (m == MOD_SHIFT) {
                mods |= HID_MOD_LSHIFT;
            } else if (m == MOD_ALT) {
                mods |= HID_MOD_LALT;
            } else if (m == MOD_CTRL) {
                mods |= HID_MOD_LCTRL;
            } else if (m == MOD_GUI) {
                mods |= HID_MOD_LGUI;
            }
        }
        uint8_t main_k = key_name_to_hid_keycode(item->key);
        if (state_active) {
            if (item->behavior == BEHAVIOR_HOLD) {
                hw_keyboard_press(mods, &main_k, 1);
            } else {
                hw_keyboard_send(mods, &main_k, 1);
            }
        } else if (item->behavior == BEHAVIOR_HOLD) {
            hw_keyboard_release_all();
        }
    } else if (item->type == KEY_TYPE_MEDIA) {
        if (!state_active) return;
        uint16_t code = 0;
        if (strcmp(item->action, "VOL_UP") == 0)          code = CC_VOL_UP;
        else if (strcmp(item->action, "VOL_DOWN") == 0)   code = CC_VOL_DOWN;
        else if (strcmp(item->action, "MUTE") == 0)       code = CC_MUTE;
        else if (strcmp(item->action, "PLAY_PAUSE") == 0) code = CC_PLAY_PAUSE;
        else if (strcmp(item->action, "NEXT") == 0)       code = CC_NEXT_TRACK;
        else if (strcmp(item->action, "PREV") == 0)       code = CC_PREV_TRACK;

        if (code != 0) {
            if (item->behavior == BEHAVIOR_HOLD) {
                hw_consumer_press(code);
                hw_sleep_ms(100);
                hw_consumer_release();
            } else {
                hw_consumer_send(code);
            }
        }
    } else if (item->type == KEY_TYPE_SPECIAL) {
        if (strcmp(item->action, "JIGGLER") == 0 && state_active) {
            hw_mouse_move(2, 0, 0);
            hw_sleep_ms(50);
            hw_mouse_move(-2, 0, 0);
        }
    }
}

static void execute_knob_rotate(int step, int curr_dir) {
    profile_config_t *prof = cur_prof();
    if (s_active_knob_key < 0 || s_active_knob_key >= NUM_KEYS) return;

    const key_config_t *item = &prof->keys[s_active_knob_key];
    const char *action = item->action[0] ? item->action : "VOLUME";
    bool is_mac = (prof->os == OS_MAC);
    int count = abs(step);
    if (count < 1) count = 1;

    if (strcmp(action, "VOLUME") == 0) {
        uint16_t code = (curr_dir > 0) ? CC_VOL_UP : CC_VOL_DOWN;
        for (int i = 0; i < count; i++) {
            hw_consumer_send(code);
        }
        play_sound(curr_dir > 0 ? 900 : 700, 0.015f);
    } else if (strcmp(action, "SCROLL_V") == 0) {
        hw_mouse_move(0, 0, (int8_t)(-step));
    } else if (strcmp(action, "SCROLL_H") == 0) {
        hw_keyboard_press(HID_MOD_LSHIFT, NULL, 0);
        hw_mouse_move(0, 0, (int8_t)(-step));
        hw_keyboard_release_all();
    } else if (strcmp(action, "ZOOM") == 0) {
        uint8_t mod = is_mac ? HID_MOD_LGUI : HID_MOD_LCTRL;
        hw_keyboard_press(mod, NULL, 0);
        hw_mouse_move(0, 0, (int8_t)step);
        hw_keyboard_release_all();
    } else if (strcmp(action, "TRACK") == 0) {
        uint16_t code = (curr_dir > 0) ? CC_NEXT_TRACK : CC_PREV_TRACK;
        hw_consumer_send(code);
        play_sound(curr_dir > 0 ? 1000 : 600, 0.03f);
    } else if (strcmp(action, "TABS") == 0) {
        uint8_t mod = is_mac ? HID_MOD_LGUI : HID_MOD_LCTRL;
        uint8_t ktab = HID_KEY_TAB;
        if (curr_dir > 0) {
            hw_keyboard_send(mod, &ktab, 1);
        } else {
            hw_keyboard_send(mod | HID_MOD_LSHIFT, &ktab, 1);
        }
    } else if (strcmp(action, "UNDO_REDO") == 0) {
        uint8_t mod = is_mac ? HID_MOD_LGUI : HID_MOD_LCTRL;
        if (curr_dir > 0) {
            if (is_mac) {
                uint8_t kz = HID_KEY_Z;
                hw_keyboard_send(mod | HID_MOD_LSHIFT, &kz, 1);
            } else {
                uint8_t ky = HID_KEY_Y;
                hw_keyboard_send(mod, &ky, 1);
            }
        } else {
            uint8_t kz = HID_KEY_Z;
            hw_keyboard_send(mod, &kz, 1);
        }
    } else if (strcmp(action, "BRIGHTNESS") == 0) {
        uint16_t code = (curr_dir > 0) ? CC_BRIGHTNESS_INC : CC_BRIGHTNESS_DEC;
        for (int i = 0; i < count; i++) {
            hw_consumer_send(code);
        }
    }
}

static void handle_knob_push(void) {
    profile_config_t *prof = cur_prof();
    if (s_active_knob_key < 0 || s_active_knob_key >= NUM_KEYS) return;

    const key_config_t *item = &prof->keys[s_active_knob_key];
    const char *action = item->action[0] ? item->action : "VOLUME";
    bool is_mac = (prof->os == OS_MAC);

    if (strcmp(action, "VOLUME") == 0) {
        hw_consumer_send(CC_MUTE);
        play_sound(600, 0.04f);
    } else if (strcmp(action, "SCROLL_V") == 0 || strcmp(action, "SCROLL_H") == 0) {
        hw_mouse_press(MOUSE_BTN_MIDDLE);
        hw_sleep_ms(50);
        hw_mouse_release_all();
        play_sound(800, 0.02f);
    } else if (strcmp(action, "ZOOM") == 0) {
        uint8_t mod = is_mac ? HID_MOD_LGUI : HID_MOD_LCTRL;
        uint8_t k0 = HID_KEY_0;
        hw_keyboard_send(mod, &k0, 1);
        play_sound(1000, 0.04f);
    } else if (strcmp(action, "TRACK") == 0) {
        hw_consumer_send(CC_PLAY_PAUSE);
        play_sound(800, 0.04f);
    } else if (strcmp(action, "TABS") == 0) {
        uint8_t mod = is_mac ? HID_MOD_LGUI : HID_MOD_LCTRL;
        uint8_t kw = HID_KEY_W;
        hw_keyboard_send(mod, &kw, 1);
        play_sound(500, 0.04f);
    } else if (strcmp(action, "UNDO_REDO") == 0) {
        uint8_t kesc = HID_KEY_ESCAPE;
        hw_keyboard_send(0, &kesc, 1);
        play_sound(400, 0.03f);
    }
}

// ============================================================================
// WEBSERIAL CONFIGURATOR PROTOCOL HANDLER
// ============================================================================
static void process_serial_line(const char *cmd) {
    if (!cmd || !cmd[0]) return;

    if (strstr(cmd, "GET_CONFIG") != NULL) {
        wake_up();
        render_screen("Web Connected!");
        play_sound(1000, 0.05f);

        hw_cdc_write_str("\nCONFIG_DATA:");
        int len = config_to_json(&g_config, s_json_io_buf, sizeof(s_json_io_buf));
        hw_cdc_write(s_json_io_buf, (uint32_t)len);
        hw_cdc_write_str("\n");
    } else if (strstr(cmd, "SET_CONFIG:") != NULL) {
        const char *brace = strchr(cmd, '{');
        if (brace) {
            char err_msg[64] = {0};
            macropad_config_t new_cfg;
            if (config_from_json(brace, &new_cfg, err_msg, sizeof(err_msg))) {
                g_config = new_cfg;
                config_save();
                s_active_toggles_mask = 0;
                s_active_knob_key = -1;
                s_mode = MODE_OVERVIEW;
                wake_up();
                update_leds();
                render_screen("Synced via Web!");
                play_sound(1200, 0.08f);
                hw_cdc_write_str("\nSTATUS:OK\n");
            } else {
                char resp[96];
                snprintf(resp, sizeof(resp), "\nSTATUS:ERR:%s\n", err_msg[0] ? err_msg : "Parse error");
                hw_cdc_write_str(resp);
            }
        }
    }
}

static void poll_webserial(void) {
    uint32_t avail = hw_cdc_available();
    if (avail == 0) return;

    char chunk[256];
    while ((avail = hw_cdc_available()) > 0) {
        uint32_t to_read = (avail < sizeof(chunk)) ? avail : (uint32_t)sizeof(chunk);
        uint32_t n = hw_cdc_read(chunk, to_read);
        for (uint32_t i = 0; i < n; i++) {
            char c = chunk[i];
            if (c == '\r') c = '\n';
            if (c == '\n') {
                s_serial_buf[s_serial_len] = '\0';
                // Trim leading whitespace
                char *start = s_serial_buf;
                while (*start == ' ' || *start == '\t') start++;
                if (*start) {
                    process_serial_line(start);
                }
                s_serial_len = 0;
            } else if (s_serial_len < SERIAL_BUF_SIZE - 1) {
                s_serial_buf[s_serial_len++] = c;
            }
        }
    }
}

static void trim_string_copy(char *dst, const char *src, int max_chars, const char *fallback) {
    char tmp[32];
    int n = 0;
    while (src[n] && n < max_chars && n < (int)sizeof(tmp) - 1) {
        tmp[n] = src[n];
        n++;
    }
    tmp[n] = '\0';

    // Trim leading/trailing spaces
    char *start = tmp;
    while (*start == ' ') start++;
    char *end = start + strlen(start);
    while (end > start && *(end - 1) == ' ') {
        *(--end) = '\0';
    }

    if (*start == '\0') {
        strncpy(dst, fallback, (size_t)max_chars);
        dst[max_chars] = '\0';
    } else {
        strncpy(dst, start, (size_t)max_chars);
        dst[max_chars] = '\0';
    }
}

// ============================================================================
// MAIN ENTRY POINT & INTERACTION LOOP
// ============================================================================
int main(void) {
    hw_init();
    config_load();

    s_last_encoder = hw_encoder_get();
    s_last_activity_ms = hw_monotonic_ms();

    update_leds();
    render_screen(NULL);

    bool config_save_pending = false;

    while (true) {
        hw_poll();
        poll_webserial();

        uint64_t now_ms = hw_monotonic_ms();
        profile_config_t *prof = cur_prof();

        // Deferred flash save 1s after page rotation so flash erase never interrupts encoder sampling
        if (config_save_pending && (now_ms - s_last_turn_time_ms > 1000ULL)) {
            config_save_pending = false;
            config_save();
        }

        // --- SLEEP TIMER CHECK (3 MINUTES = 180,000 ms) ---
        if (!s_is_sleeping && (now_ms - s_last_activity_ms > 180000ULL) && s_mode == MODE_OVERVIEW) {
            s_is_sleeping = true;
            update_leds();
            hw_display_set_sleep(true);
        }

        // --- BACKGROUND JIGGLER TICK (EVERY 30 SECONDS) ---
        uint64_t now_sec = now_ms / 1000ULL;
        if (s_active_toggles_mask != 0 && now_sec != s_last_jiggle_sec && (now_sec % 30ULL == 0)) {
            s_last_jiggle_sec = now_sec;
            for (int k = 0; k < NUM_KEYS; k++) {
                if (s_active_toggles_mask & (1u << k)) {
                    const key_config_t *k_item = &prof->keys[k];
                    if (k_item->type == KEY_TYPE_SPECIAL && strcmp(k_item->action, "JIGGLER") == 0) {
                        execute_action(k_item, true);
                    }
                }
            }
        }

        // --- A. ROTARY KNOB PUSH ---
        if (hw_encoder_switch_pressed()) {
            wake_up();
            play_sound(800, 0.03f);
            s_last_encoder = hw_encoder_get();

            if (s_mode == MODE_OVERVIEW) {
                if (s_active_knob_key >= 0) {
                    handle_knob_push();
                } else {
                    s_mode = MODE_SELECT_KEY;
                    update_leds();
                    render_screen(NULL);
                }
            } else if (s_mode == MODE_SELECT_KEY) {
                s_menu_idx = 0;
                s_mode = MODE_PAGE_MENU;
                update_leds();
                render_screen(NULL);
            } else if (s_mode == MODE_PAGE_MENU) {
                if (s_menu_idx == 2) { // Toggle Sound
                    g_config.sound = !g_config.sound;
                    config_save();
                    render_screen(NULL);
                } else if (s_menu_idx == 0) { // Add New Page
                    if (g_config.profile_count < MAX_PROFILES) {
                        s_is_creating_page = true;
                        s_menu_idx = 0;
                        s_mode = MODE_PAGE_OS;
                        update_leds();
                        render_screen(NULL);
                    } else {
                        s_mode = MODE_OVERVIEW;
                        update_leds();
                        render_screen("Max Pages Reached!");
                    }
                } else if (s_menu_idx == 1) { // Edit Current Page
                    s_is_creating_page = false;
                    s_menu_idx = (prof->os == OS_PC) ? 0 : 1;
                    s_mode = MODE_PAGE_OS;
                    update_leds();
                    render_screen(NULL);
                } else if (s_menu_idx == 3) { // Delete This Page
                    if (g_config.profile_count > 1) {
                        for (int i = g_config.active; i < g_config.profile_count - 1; i++) {
                            g_config.profiles[i] = g_config.profiles[i + 1];
                        }
                        g_config.profile_count--;
                        g_config.active = (uint8_t)(g_config.active % g_config.profile_count);
                        config_save();
                        play_sound(300, 0.1f);
                        s_active_toggles_mask = 0;
                        s_active_knob_key = -1;
                        s_mode = MODE_OVERVIEW;
                        update_leds();
                        render_screen("Page Deleted!");
                    } else {
                        s_mode = MODE_OVERVIEW;
                        update_leds();
                        render_screen("Keep at least 1!");
                    }
                } else { // Cancel
                    s_mode = MODE_OVERVIEW;
                    update_leds();
                    render_screen(NULL);
                }
            } else if (s_mode == MODE_PAGE_OS) {
                s_temp_page_os = (s_menu_idx == 0) ? OS_PC : OS_MAC;
                uint32_t target_theme = (s_temp_page_os == OS_PC) ? 0x002244 : 0x330044;
                if (!s_is_creating_page) {
                    target_theme = prof->theme;
                }
                s_menu_idx = 0;
                for (int idx = 0; idx < NUM_THEME_PALETTE; idx++) {
                    if ((uint32_t)THEME_PALETTE[idx].value == target_theme) {
                        s_menu_idx = idx;
                        break;
                    }
                }
                s_mode = MODE_PAGE_THEME;
                update_leds();
                render_screen(NULL);
            } else if (s_mode == MODE_PAGE_THEME) {
                s_temp_page_theme = (uint32_t)THEME_PALETTE[s_menu_idx].value;
                if (s_is_creating_page) {
                    snprintf(s_edit_string, MAX_NAME_LEN, "%s %d",
                             s_temp_page_os == OS_MAC ? "MAC" : "PC",
                             g_config.profile_count + 1);
                } else {
                    strncpy(s_edit_string, prof->name, 6);
                    s_edit_string[6] = '\0';
                }
                s_text_page_idx = 0;
                s_text_upper = true;
                s_mode = MODE_PAGE_NAME;
                update_leds();
                render_screen(NULL);
            } else if (s_mode == MODE_PAGE_NAME) {
                char final_name[MAX_NAME_LEN];
                trim_string_copy(final_name, s_edit_string, 6, s_temp_page_os == OS_MAC ? "MAC" : "PC");
                if (s_is_creating_page && g_config.profile_count < MAX_PROFILES) {
                    profile_config_t *new_p = &g_config.profiles[g_config.profile_count];
                    memset(new_p, 0, sizeof(*new_p));
                    strcpy(new_p->name, final_name);
                    new_p->os = s_temp_page_os;
                    new_p->theme = s_temp_page_theme;
                    for (int i = 0; i < NUM_KEYS; i++) {
                        new_p->keys[i] = PRESETS[i];
                    }
                    g_config.active = g_config.profile_count;
                    g_config.profile_count++;
                } else {
                    strcpy(prof->name, final_name);
                    prof->os = s_temp_page_os;
                    prof->theme = s_temp_page_theme;
                }
                config_save();
                play_sound(1200, 0.08f);
                s_mode = MODE_OVERVIEW;
                update_leds();
                render_screen("Page Saved!");
            } else if (s_mode == MODE_CHOOSE_TYPE) {
                if (s_menu_idx == 0) { // Pick Preset
                    s_menu_idx = 0;
                    s_mode = MODE_PICK_PRESET;
                } else if (s_menu_idx == 1) { // New Key Combo
                    s_edit_mod_count = 0;
                    s_menu_idx = 1;
                    s_mode = MODE_BUILD_COMBO_MODS;
                } else if (s_menu_idx == 2) { // New Text String
                    s_edit_string[0] = '\0';
                    s_text_page_idx = 0;
                    s_text_upper = false;
                    s_mode = MODE_BUILD_TEXT;
                } else if (s_menu_idx == 3) { // Knob Function
                    s_menu_idx = 0;
                    s_mode = MODE_PICK_KNOB_ACTION;
                } else if (s_menu_idx == 4) { // Set Key Colour
                    int32_t curr_c = prof->keys[s_selected_key].color;
                    s_menu_idx = 0;
                    for (int idx = 0; idx < NUM_COLOR_PALETTE; idx++) {
                        if (COLOR_PALETTE[idx].value == curr_c) {
                            s_menu_idx = idx;
                            break;
                        }
                    }
                    s_mode = MODE_PICK_COLOR;
                } else if (s_menu_idx == 5) { // Set Key Behaviour
                    s_menu_idx = (int)prof->keys[s_selected_key].behavior;
                    s_mode = MODE_PICK_BEHAVIOR;
                } else { // Cancel
                    s_mode = MODE_OVERVIEW;
                }
                update_leds();
                render_screen(NULL);
            } else if (s_mode == MODE_PICK_PRESET) {
                int32_t existing_color = prof->keys[s_selected_key].color;
                key_behavior_t existing_behavior = prof->keys[s_selected_key].behavior;
                prof->keys[s_selected_key] = PRESETS[s_menu_idx];
                prof->keys[s_selected_key].color = existing_color;
                prof->keys[s_selected_key].behavior = existing_behavior;
                config_save();
                play_sound(1200, 0.08f);
                s_mode = MODE_OVERVIEW;
                update_leds();
                render_screen("Saved!");
            } else if (s_mode == MODE_PICK_KNOB_ACTION) {
                int32_t existing_color = prof->keys[s_selected_key].color;
                key_config_t *kc = &prof->keys[s_selected_key];
                memset(kc, 0, sizeof(*kc));
                strncpy(kc->label, KNOB_ACTIONS[s_menu_idx].default_label, MAX_LABEL_LEN - 1);
                kc->type = KEY_TYPE_KNOB;
                strncpy(kc->action, KNOB_ACTIONS[s_menu_idx].action_id, MAX_ACTION_LEN - 1);
                kc->color = existing_color;
                kc->behavior = BEHAVIOR_TOGGLE;

                strcpy(s_edit_string, kc->label);
                s_text_page_idx = 0;
                s_text_upper = true;
                s_mode = MODE_BUILD_LABEL;
                update_leds();
                render_screen(NULL);
            } else if (s_mode == MODE_PICK_COLOR) {
                prof->keys[s_selected_key].color = COLOR_PALETTE[s_menu_idx].value;
                config_save();
                play_sound(1200, 0.08f);
                s_mode = MODE_OVERVIEW;
                update_leds();
                render_screen("Colour Saved!");
            } else if (s_mode == MODE_PICK_BEHAVIOR) {
                prof->keys[s_selected_key].behavior = (key_behavior_t)s_menu_idx;
                config_save();
                play_sound(1200, 0.08f);
                s_mode = MODE_OVERVIEW;
                update_leds();
                render_screen("Behavior Saved!");
            } else if (s_mode == MODE_BUILD_COMBO_MODS) {
                if (s_menu_idx == 0) {
                    s_edit_key_idx = 0;
                    s_mode = MODE_BUILD_COMBO_KEY;
                } else {
                    static const mod_id_t mod_map[6] = {
                        MOD_OS, MOD_OS, MOD_SHIFT, MOD_ALT, MOD_CTRL, MOD_GUI
                    };
                    mod_id_t m_val = mod_map[s_menu_idx];
                    int found = -1;
                    for (int i = 0; i < s_edit_mod_count; i++) {
                        if (s_edit_mods[i] == (uint8_t)m_val) {
                            found = i;
                            break;
                        }
                    }
                    if (found >= 0) {
                        for (int i = found; i < s_edit_mod_count - 1; i++) {
                            s_edit_mods[i] = s_edit_mods[i + 1];
                        }
                        s_edit_mod_count--;
                    } else if (s_edit_mod_count < MAX_MODS) {
                        s_edit_mods[s_edit_mod_count++] = (uint8_t)m_val;
                    }
                }
                render_screen(NULL);
            } else if (s_mode == MODE_BUILD_COMBO_KEY) {
                const char *kname = KEY_NAMES[s_edit_key_idx];
                char default_lbl[MAX_LABEL_LEN];
                if (s_edit_mod_count > 0) {
                    char mods_str[32];
                    build_mods_string(mods_str, sizeof(mods_str));
                    snprintf(default_lbl, sizeof(default_lbl), "%.2s%.4s", mods_str, kname);
                } else {
                    snprintf(default_lbl, sizeof(default_lbl), "%.6s", kname);
                }

                int32_t existing_color = prof->keys[s_selected_key].color;
                key_behavior_t existing_behavior = prof->keys[s_selected_key].behavior;
                key_config_t *kc = &prof->keys[s_selected_key];
                memset(kc, 0, sizeof(*kc));
                strcpy(kc->label, default_lbl);
                kc->type = KEY_TYPE_COMBO;
                kc->mod_count = s_edit_mod_count;
                for (int i = 0; i < s_edit_mod_count; i++) {
                    kc->mod_order[i] = s_edit_mods[i];
                    kc->mod_mask |= (1u << s_edit_mods[i]);
                }
                strncpy(kc->key, kname, MAX_KEY_NAME_LEN - 1);
                kc->color = existing_color;
                kc->behavior = existing_behavior;

                strcpy(s_edit_string, default_lbl);
                s_text_page_idx = 0;
                s_text_upper = true;
                s_mode = MODE_BUILD_LABEL;
                update_leds();
                render_screen(NULL);
            } else if (s_mode == MODE_BUILD_TEXT) {
                char clean[MAX_LABEL_LEN];
                int c_idx = 0;
                for (int i = 0; s_edit_string[i] && c_idx < 6; i++) {
                    if (s_edit_string[i] != '\n') {
                        clean[c_idx++] = s_edit_string[i];
                    }
                }
                clean[c_idx] = '\0';

                char default_lbl[MAX_LABEL_LEN];
                trim_string_copy(default_lbl, clean, 6, "Text");

                int32_t existing_color = prof->keys[s_selected_key].color;
                key_behavior_t existing_behavior = prof->keys[s_selected_key].behavior;
                key_config_t *kc = &prof->keys[s_selected_key];
                memset(kc, 0, sizeof(*kc));
                strcpy(kc->label, default_lbl);
                kc->type = KEY_TYPE_TEXT;
                strncpy(kc->text, s_edit_string, MAX_TEXT_LEN - 1);
                kc->color = existing_color;
                kc->behavior = existing_behavior;

                strcpy(s_edit_string, default_lbl);
                s_text_page_idx = 0;
                s_text_upper = true;
                s_mode = MODE_BUILD_LABEL;
                update_leds();
                render_screen(NULL);
            } else if (s_mode == MODE_BUILD_LABEL) {
                trim_string_copy(prof->keys[s_selected_key].label, s_edit_string, 6, "Macro");
                config_save();
                play_sound(1200, 0.08f);
                s_mode = MODE_OVERVIEW;
                update_leds();
                render_screen("Saved!");
            }
        }

        // --- B. ROTARY KNOB ROTATION ---
        int32_t curr_enc = hw_encoder_get();
        if (curr_enc != s_last_encoder) {
            wake_up();
            int32_t delta = curr_enc - s_last_encoder;
            s_last_encoder = curr_enc;
            int curr_dir = (delta > 0) ? 1 : -1;

            if (curr_dir == s_last_dir || (now_ms - s_last_turn_time_ms) >= 50) {
                s_last_dir = curr_dir;
                int step = (abs((int)delta) <= 2) ? curr_dir : (int)delta;

                if (s_mode == MODE_OVERVIEW) {
                    if (s_active_knob_key >= 0) {
                        execute_knob_rotate(step, curr_dir);
                    } else {
                        g_config.active = (uint8_t)wrap_index((int)g_config.active + step, g_config.profile_count);
                        config_save_pending = true;
                        update_leds();
                        render_screen(NULL);
                        play_sound(500 + (g_config.active * 150), 0.06f);
                    }
                } else if (s_mode == MODE_SELECT_KEY) {
                    s_mode = MODE_OVERVIEW;
                    update_leds();
                    render_screen(NULL);
                } else if (s_mode == MODE_PAGE_MENU) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_PAGE_MENU_OPTS);
                    render_screen(NULL);
                } else if (s_mode == MODE_PAGE_OS) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_OS_OPTIONS);
                    render_screen(NULL);
                } else if (s_mode == MODE_PAGE_THEME) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_THEME_PALETTE);
                    update_leds();
                    render_screen(NULL);
                } else if (s_mode == MODE_CHOOSE_TYPE) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_EDIT_TYPES);
                    render_screen(NULL);
                } else if (s_mode == MODE_PICK_KNOB_ACTION) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_KNOB_ACTIONS);
                    render_screen(NULL);
                } else if (s_mode == MODE_PICK_PRESET) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_PRESETS);
                    render_screen(NULL);
                } else if (s_mode == MODE_PICK_COLOR) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_COLOR_PALETTE);
                    update_leds();
                    render_screen(NULL);
                } else if (s_mode == MODE_PICK_BEHAVIOR) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_BEHAVIOR_OPTIONS);
                    render_screen(NULL);
                } else if (s_mode == MODE_BUILD_COMBO_MODS) {
                    s_menu_idx = wrap_index(s_menu_idx + step, NUM_MOD_OPTIONS);
                    render_screen(NULL);
                } else if (s_mode == MODE_BUILD_COMBO_KEY) {
                    s_edit_key_idx = wrap_index(s_edit_key_idx + step, NUM_KEY_NAMES);
                    render_screen(NULL);
                } else if (s_mode == MODE_BUILD_TEXT || s_mode == MODE_BUILD_LABEL || s_mode == MODE_PAGE_NAME) {
                    s_text_page_idx = wrap_index(s_text_page_idx + step, NUM_TEXT_PAGES);
                    render_screen(NULL);
                }

                s_last_turn_time_ms = hw_monotonic_ms();
            }
        }

        // --- C. KEYPAD BUTTON PRESSES ---
        key_event_t key_ev;
        while (hw_key_event_pop(&key_ev)) {
            wake_up();
            int k = key_ev.key_number;
            prof = cur_prof();

            if (key_ev.pressed) {
                if (s_mode == MODE_OVERVIEW) {
                    const key_config_t *item = &prof->keys[k];

                    if (item->type == KEY_TYPE_KNOB) {
                        if (s_active_knob_key == k) {
                            s_active_toggles_mask &= (uint16_t)~(1u << k);
                            s_active_knob_key = -1;
                            play_sound(300, 0.04f);
                        } else {
                            if (s_active_knob_key >= 0) {
                                s_active_toggles_mask &= (uint16_t)~(1u << s_active_knob_key);
                            }
                            s_active_toggles_mask |= (uint16_t)(1u << k);
                            s_active_knob_key = k;
                            play_sound(900, 0.04f);
                        }
                        update_leds();
                        render_screen(NULL);
                    } else if (item->behavior == BEHAVIOR_TOGGLE) {
                        if (s_active_toggles_mask & (1u << k)) {
                            s_active_toggles_mask &= (uint16_t)~(1u << k);
                            execute_action(item, false);
                            play_sound(300, 0.04f);
                        } else {
                            s_active_toggles_mask |= (uint16_t)(1u << k);
                            execute_action(item, true);
                            play_sound(900, 0.04f);
                        }
                        update_leds();
                    } else if (item->behavior == BEHAVIOR_HOLD) {
                        hw_pixel_set(k, 0xFFFFFF);
                        hw_pixels_show();
                        execute_action(item, true);
                    } else {
                        hw_pixel_set(k, 0xFFFFFF);
                        hw_pixels_show();
                        play_sound(700, 0.03f);
                        execute_action(item, true);
                        hw_sleep_ms(80);
                        update_leds();
                    }
                } else if (s_mode == MODE_SELECT_KEY) {
                    s_selected_key = k;
                    s_menu_idx = 0;
                    s_mode = MODE_CHOOSE_TYPE;
                    update_leds();
                    render_screen(NULL);
                } else if (s_mode == MODE_BUILD_COMBO_MODS) {
                    if (k == 0 && s_edit_mod_count > 0) {
                        s_edit_mod_count--;
                        render_screen(NULL);
                    }
                } else if (s_mode == MODE_BUILD_TEXT || s_mode == MODE_BUILD_LABEL || s_mode == MODE_PAGE_NAME) {
                    int max_chars = (s_mode == MODE_BUILD_LABEL || s_mode == MODE_PAGE_NAME) ? 6 : 120;
                    int cur_len = (int)strlen(s_edit_string);

                    if (k < 9) {
                        char ch = get_current_text_pages()[s_text_page_idx][k];
                        if (ch == '\n' && (s_mode == MODE_BUILD_LABEL || s_mode == MODE_PAGE_NAME)) {
                            // Ignore newline in label/page name
                        } else if (cur_len < max_chars) {
                            s_edit_string[cur_len] = ch;
                            s_edit_string[cur_len + 1] = '\0';
                            play_sound(1000, 0.02f);
                        }
                    } else if (k == 9) {
                        s_text_upper = !s_text_upper;
                    } else if (k == 10) {
                        if (cur_len < max_chars) {
                            s_edit_string[cur_len] = ' ';
                            s_edit_string[cur_len + 1] = '\0';
                            play_sound(900, 0.02f);
                        }
                    } else if (k == 11) {
                        if (cur_len > 0) {
                            s_edit_string[cur_len - 1] = '\0';
                        }
                        play_sound(400, 0.03f);
                    }

                    update_leds();
                    render_screen(NULL);
                }
            } else if (!key_ev.pressed && s_mode == MODE_OVERVIEW) {
                const key_config_t *item = &prof->keys[k];
                if (item->type != KEY_TYPE_KNOB && item->behavior == BEHAVIOR_HOLD) {
                    execute_action(item, false);
                    update_leds();
                }
            }
        }
    }

    return 0;
}
