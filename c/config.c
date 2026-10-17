#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#endif

// ============================================================================
// 1. CONSTANTS, KEY MAPS, COLOURS & PRESETS
// ============================================================================

const char * const KEY_NAMES[NUM_KEY_NAMES] = {
    "A","B","C","D","E","F","G","H","I","J","K","L","M",
    "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    "0","1","2","3","4","5","6","7","8","9",
    "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
    "ENTER","ESC","BACKSP","TAB","SPACE","DELETE","UP","DOWN","LEFT","RIGHT"
};

// Standard USB HID Usage IDs (Keyboard/Keypad Page 0x07)
static const uint8_t KEY_HID_CODES[NUM_KEY_NAMES] = {
    // A - Z (0x04 - 0x1D)
    0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,
    0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,
    // 0, 1 - 9
    0x27,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
    // F1 - F12 (0x3A - 0x45)
    0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42,0x43,0x44,0x45,
    // ENTER, ESC, BACKSP, TAB, SPACE, DELETE, UP, DOWN, LEFT, RIGHT
    0x28,0x29,0x2A,0x2B,0x2C,0x4C,0x52,0x51,0x50,0x4F
};

uint8_t key_name_to_hid_keycode(const char *name) {
    if (!name) return 0x04; // Default 'A'
    for (int i = 0; i < NUM_KEY_NAMES; i++) {
        if (strcmp(name, KEY_NAMES[i]) == 0) {
            return KEY_HID_CODES[i];
        }
    }
    return 0x04;
}

const color_entry_t COLOR_PALETTE[NUM_COLOR_PALETTE] = {
    { "Page Default", COLOR_PAGE_DEFAULT },
    { "Soft Blue",    0x002244 },
    { "Purple",       0x330044 },
    { "Red",          0x550000 },
    { "Green",        0x005500 },
    { "Deep Blue",    0x001166 },
    { "Yellow",       0x444400 },
    { "Cyan",         0x004444 },
    { "Magenta",      0x440044 },
    { "Orange",       0x551800 },
    { "Pink",         0x550822 },
    { "White",        0x333333 },
    { "Off",          0x000000 }
};

const color_entry_t * const THEME_PALETTE = &COLOR_PALETTE[1];

const key_config_t PRESETS[NUM_PRESETS] = {
    { .label = "Copy",   .type = KEY_TYPE_COMBO,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .mod_mask = (1 << MOD_OS), .mod_order = {MOD_OS}, .mod_count = 1, .key = "C" },
    { .label = "Paste",  .type = KEY_TYPE_COMBO,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .mod_mask = (1 << MOD_OS), .mod_order = {MOD_OS}, .mod_count = 1, .key = "V" },
    { .label = "Cut",    .type = KEY_TYPE_COMBO,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .mod_mask = (1 << MOD_OS), .mod_order = {MOD_OS}, .mod_count = 1, .key = "X" },
    { .label = "Undo",   .type = KEY_TYPE_COMBO,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .mod_mask = (1 << MOD_OS), .mod_order = {MOD_OS}, .mod_count = 1, .key = "Z" },
    { .label = "Redo",   .type = KEY_TYPE_COMBO,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .mod_mask = (1 << MOD_OS) | (1 << MOD_SHIFT), .mod_order = {MOD_OS, MOD_SHIFT}, .mod_count = 2, .key = "Z" },
    { .label = "Save",   .type = KEY_TYPE_COMBO,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .mod_mask = (1 << MOD_OS), .mod_order = {MOD_OS}, .mod_count = 1, .key = "S" },
    { .label = "Vol+",   .type = KEY_TYPE_MEDIA,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_HOLD,
      .action = "VOL_UP" },
    { .label = "Vol-",   .type = KEY_TYPE_MEDIA,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_HOLD,
      .action = "VOL_DOWN" },
    { .label = "Mute",   .type = KEY_TYPE_MEDIA,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .action = "MUTE" },
    { .label = "Play",   .type = KEY_TYPE_MEDIA,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .action = "PLAY_PAUSE" },
    { .label = "Jiggle", .type = KEY_TYPE_SPECIAL, .color = 0x004400,           .behavior = BEHAVIOR_TOGGLE,
      .action = "JIGGLER" },
    { .label = "Screen", .type = KEY_TYPE_COMBO,   .color = COLOR_PAGE_DEFAULT, .behavior = BEHAVIOR_NORMAL,
      .mod_mask = (1 << MOD_OS_SCREEN), .mod_order = {MOD_OS_SCREEN}, .mod_count = 1, .key = "S" },
    { .label = "K-Vol",  .type = KEY_TYPE_KNOB,    .color = 0x004444,           .behavior = BEHAVIOR_TOGGLE,
      .action = "VOLUME" },
    { .label = "K-ScV",  .type = KEY_TYPE_KNOB,    .color = 0x440044,           .behavior = BEHAVIOR_TOGGLE,
      .action = "SCROLL_V" },
    { .label = "K-Zoom", .type = KEY_TYPE_KNOB,    .color = 0x444400,           .behavior = BEHAVIOR_TOGGLE,
      .action = "ZOOM" }
};

const knob_action_entry_t KNOB_ACTIONS[NUM_KNOB_ACTIONS] = {
    { "Volume Control", "VOLUME",     "K-Vol"  },
    { "Vert Scroll",    "SCROLL_V",   "K-ScV"  },
    { "Horiz Scroll",   "SCROLL_H",   "K-ScH"  },
    { "Zoom In/Out",    "ZOOM",       "K-Zoom" },
    { "Media Track",    "TRACK",      "K-Trk"  },
    { "Next/Prev Tab",  "TABS",       "K-Tabs" },
    { "Undo / Redo",    "UNDO_REDO",  "K-Undo" },
    { "Brightness",     "BRIGHTNESS", "K-Brt"  }
};

const char TEXT_PAGES_LOWER[NUM_TEXT_PAGES][9] = {
    { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i' },
    { 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r' },
    { 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '\n' },
    { '0', '1', '2', '3', '4', '5', '6', '7', '8' },
    { '9', '.', ',', '!', '?', '@', '#', '$', '%' },
    { '-', '_', '+', '=', '/', '\\', ':', ';', '&' },
    { '(', ')', '[', ']', '\'', '\"', '*', '~', '`' }
};

const char TEXT_PAGES_UPPER[NUM_TEXT_PAGES][9] = {
    { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I' },
    { 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R' },
    { 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '\n' },
    { '0', '1', '2', '3', '4', '5', '6', '7', '8' },
    { '9', '.', ',', '!', '?', '@', '#', '$', '%' },
    { '-', '_', '+', '=', '/', '\\', ':', ';', '&' },
    { '(', ')', '[', ']', '\'', '\"', '*', '~', '`' }
};

macropad_config_t g_config;

// ============================================================================
// 2. STRING / ENUM CONVERSION HELPERS
// ============================================================================

const char *key_type_to_str(key_type_t t) {
    switch (t) {
        case KEY_TYPE_COMBO:   return "COMBO";
        case KEY_TYPE_TEXT:    return "TEXT";
        case KEY_TYPE_MEDIA:   return "MEDIA";
        case KEY_TYPE_SPECIAL: return "SPECIAL";
        case KEY_TYPE_KNOB:    return "KNOB";
        default:               return "COMBO";
    }
}

key_type_t key_type_from_str(const char *s) {
    if (!s) return KEY_TYPE_COMBO;
    if (strcmp(s, "TEXT") == 0)    return KEY_TYPE_TEXT;
    if (strcmp(s, "MEDIA") == 0)   return KEY_TYPE_MEDIA;
    if (strcmp(s, "SPECIAL") == 0) return KEY_TYPE_SPECIAL;
    if (strcmp(s, "KNOB") == 0)    return KEY_TYPE_KNOB;
    return KEY_TYPE_COMBO;
}

const char *behavior_to_str(key_behavior_t b) {
    switch (b) {
        case BEHAVIOR_NORMAL: return "NORMAL";
        case BEHAVIOR_HOLD:   return "HOLD";
        case BEHAVIOR_TOGGLE: return "TOGGLE";
        default:              return "NORMAL";
    }
}

key_behavior_t behavior_from_str(const char *s) {
    if (!s) return BEHAVIOR_NORMAL;
    if (strcmp(s, "HOLD") == 0)   return BEHAVIOR_HOLD;
    if (strcmp(s, "TOGGLE") == 0) return BEHAVIOR_TOGGLE;
    return BEHAVIOR_NORMAL;
}

const char *mod_to_str(mod_id_t m) {
    switch (m) {
        case MOD_OS:        return "OS";
        case MOD_SHIFT:     return "SHIFT";
        case MOD_ALT:       return "ALT";
        case MOD_CTRL:      return "CTRL";
        case MOD_GUI:       return "GUI";
        case MOD_OS_SCREEN: return "OS_SCREEN";
        case MOD_OS_SWITCH: return "OS_SWITCH";
        default:            return "OS";
    }
}

int mod_from_str(const char *s) {
    if (!s) return -1;
    if (strcmp(s, "OS") == 0)        return MOD_OS;
    if (strcmp(s, "SHIFT") == 0)     return MOD_SHIFT;
    if (strcmp(s, "ALT") == 0)       return MOD_ALT;
    if (strcmp(s, "CTRL") == 0)      return MOD_CTRL;
    if (strcmp(s, "GUI") == 0)       return MOD_GUI;
    if (strcmp(s, "OS_SCREEN") == 0) return MOD_OS_SCREEN;
    if (strcmp(s, "OS_SWITCH") == 0) return MOD_OS_SWITCH;
    return -1;
}

// ============================================================================
// 3. DEFAULT CONFIGURATION
// ============================================================================

void config_set_defaults(macropad_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->active = 0;
    cfg->sound = true;
    cfg->profile_count = 2;

    // Profile 0: PC 1
    strncpy(cfg->profiles[0].name, "PC 1", MAX_NAME_LEN - 1);
    cfg->profiles[0].os = OS_PC;
    cfg->profiles[0].theme = 0x002244;
    for (int i = 0; i < NUM_KEYS; i++) {
        cfg->profiles[0].keys[i] = PRESETS[i];
    }

    // Profile 1: MAC 1
    strncpy(cfg->profiles[1].name, "MAC 1", MAX_NAME_LEN - 1);
    cfg->profiles[1].os = OS_MAC;
    cfg->profiles[1].theme = 0x330044;
    for (int i = 0; i < NUM_KEYS; i++) {
        cfg->profiles[1].keys[i] = PRESETS[i];
    }
}

// ============================================================================
// 4. JSON SERIALIZER & PARSER
// ============================================================================

static void append_str(char *buf, int max_len, int *pos, const char *s) {
    while (*s && *pos < max_len - 1) {
        buf[(*pos)++] = *s++;
    }
    buf[*pos] = '\0';
}

static void append_json_escaped(char *buf, int max_len, int *pos, const char *s) {
    append_str(buf, max_len, pos, "\"");
    while (*s && *pos < max_len - 3) {
        unsigned char c = (unsigned char)*s++;
        if (c == '\"') {
            append_str(buf, max_len, pos, "\\\"");
        } else if (c == '\\') {
            append_str(buf, max_len, pos, "\\\\");
        } else if (c == '\n') {
            append_str(buf, max_len, pos, "\\n");
        } else if (c == '\r') {
            append_str(buf, max_len, pos, "\\r");
        } else if (c == '\t') {
            append_str(buf, max_len, pos, "\\t");
        } else if (c >= 0x20) {
            buf[(*pos)++] = (char)c;
            buf[*pos] = '\0';
        }
    }
    append_str(buf, max_len, pos, "\"");
}

int config_to_json(const macropad_config_t *cfg, char *buf, int max_len) {
    if (!cfg || !buf || max_len <= 0) return 0;
    int pos = 0;
    buf[0] = '\0';

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "{\"active\":%u,\"sound\":%s,\"profiles\":[",
             cfg->active, cfg->sound ? "true" : "false");
    append_str(buf, max_len, &pos, tmp);

    for (int p = 0; p < cfg->profile_count; p++) {
        if (p > 0) append_str(buf, max_len, &pos, ",");
        const profile_config_t *prof = &cfg->profiles[p];

        append_str(buf, max_len, &pos, "{\"name\":");
        append_json_escaped(buf, max_len, &pos, prof->name);
        snprintf(tmp, sizeof(tmp), ",\"os\":\"%s\",\"theme\":%u,\"keys\":[",
                 prof->os == OS_MAC ? "MAC" : "PC", (unsigned int)prof->theme);
        append_str(buf, max_len, &pos, tmp);

        for (int k = 0; k < NUM_KEYS; k++) {
            if (k > 0) append_str(buf, max_len, &pos, ",");
            const key_config_t *kc = &prof->keys[k];

            append_str(buf, max_len, &pos, "{\"label\":");
            append_json_escaped(buf, max_len, &pos, kc->label);

            snprintf(tmp, sizeof(tmp), ",\"type\":\"%s\"", key_type_to_str(kc->type));
            append_str(buf, max_len, &pos, tmp);

            if (kc->color == COLOR_PAGE_DEFAULT) {
                append_str(buf, max_len, &pos, ",\"color\":null");
            } else {
                snprintf(tmp, sizeof(tmp), ",\"color\":%d", (int)kc->color);
                append_str(buf, max_len, &pos, tmp);
            }

            snprintf(tmp, sizeof(tmp), ",\"behavior\":\"%s\"", behavior_to_str(kc->behavior));
            append_str(buf, max_len, &pos, tmp);

            if (kc->type == KEY_TYPE_COMBO) {
                append_str(buf, max_len, &pos, ",\"mods\":[");
                for (int m = 0; m < kc->mod_count; m++) {
                    if (m > 0) append_str(buf, max_len, &pos, ",");
                    append_json_escaped(buf, max_len, &pos, mod_to_str((mod_id_t)kc->mod_order[m]));
                }
                append_str(buf, max_len, &pos, "],\"key\":");
                append_json_escaped(buf, max_len, &pos, kc->key[0] ? kc->key : "A");
            } else if (kc->type == KEY_TYPE_TEXT) {
                append_str(buf, max_len, &pos, ",\"text\":");
                append_json_escaped(buf, max_len, &pos, kc->text);
            } else if (kc->type == KEY_TYPE_MEDIA || kc->type == KEY_TYPE_SPECIAL || kc->type == KEY_TYPE_KNOB) {
                append_str(buf, max_len, &pos, ",\"action\":");
                append_json_escaped(buf, max_len, &pos, kc->action);
            }

            append_str(buf, max_len, &pos, "}");
        }

        append_str(buf, max_len, &pos, "]}");
    }

    append_str(buf, max_len, &pos, "]}");
    return pos;
}

// Lightweight recursive-descent JSON parser
typedef struct {
    const char *src;
    int pos;
    const char *err;
} json_parser_t;

static void skip_ws(json_parser_t *p) {
    while (p->src[p->pos] && isspace((unsigned char)p->src[p->pos])) {
        p->pos++;
    }
}

static bool match_char(json_parser_t *p, char expected) {
    skip_ws(p);
    if (p->src[p->pos] == expected) {
        p->pos++;
        return true;
    }
    return false;
}

static bool parse_string(json_parser_t *p, char *out, int max_len) {
    skip_ws(p);
    if (p->src[p->pos] != '\"') {
        p->err = "Expected string";
        return false;
    }
    p->pos++; // skip opening quote
    int out_idx = 0;
    while (p->src[p->pos] && p->src[p->pos] != '\"') {
        char c = p->src[p->pos++];
        if (c == '\\') {
            char esc = p->src[p->pos];
            if (!esc) {
                p->err = "Unterminated escape";
                return false;
            }
            p->pos++;
            switch (esc) {
                case '\"': c = '\"'; break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u': {
                    // Parse 4 hex digits
                    unsigned int code = 0;
                    for (int i = 0; i < 4 && isxdigit((unsigned char)p->src[p->pos]); i++) {
                        char hc = p->src[p->pos++];
                        code = (code << 4) | (isdigit((unsigned char)hc) ? (hc - '0') : (tolower((unsigned char)hc) - 'a' + 10));
                    }
                    c = (code < 0x80 && code > 0) ? (char)code : '?';
                    break;
                }
                default: c = esc; break;
            }
        }
        if (out && out_idx < max_len - 1) {
            out[out_idx++] = c;
        }
    }
    if (p->src[p->pos] != '\"') {
        p->err = "Unterminated string";
        return false;
    }
    p->pos++; // skip closing quote
    if (out && max_len > 0) {
        out[out_idx] = '\0';
    }
    return true;
}

static bool parse_int(json_parser_t *p, int32_t *out) {
    skip_ws(p);
    char *endptr = NULL;
    long val = strtol(&p->src[p->pos], &endptr, 10);
    if (endptr == &p->src[p->pos]) {
        p->err = "Expected integer";
        return false;
    }
    p->pos = (int)(endptr - p->src);
    if (out) *out = (int32_t)val;
    return true;
}

static bool parse_bool(json_parser_t *p, bool *out) {
    skip_ws(p);
    if (strncmp(&p->src[p->pos], "true", 4) == 0) {
        p->pos += 4;
        if (out) *out = true;
        return true;
    }
    if (strncmp(&p->src[p->pos], "false", 5) == 0) {
        p->pos += 5;
        if (out) *out = false;
        return true;
    }
    p->err = "Expected boolean";
    return false;
}

static bool skip_value(json_parser_t *p);

static bool skip_array(json_parser_t *p) {
    if (!match_char(p, '[')) return false;
    if (match_char(p, ']')) return true;
    while (true) {
        if (!skip_value(p)) return false;
        if (match_char(p, ',')) continue;
        if (match_char(p, ']')) return true;
        p->err = "Malformed array";
        return false;
    }
}

static bool skip_object(json_parser_t *p) {
    if (!match_char(p, '{')) return false;
    if (match_char(p, '}')) return true;
    while (true) {
        if (!parse_string(p, NULL, 0)) return false;
        if (!match_char(p, ':')) return false;
        if (!skip_value(p)) return false;
        if (match_char(p, ',')) continue;
        if (match_char(p, '}')) return true;
        p->err = "Malformed object";
        return false;
    }
}

static bool skip_value(json_parser_t *p) {
    skip_ws(p);
    char c = p->src[p->pos];
    if (c == '\"') return parse_string(p, NULL, 0);
    if (c == '{')  return skip_object(p);
    if (c == '[')  return skip_array(p);
    if (c == 't' || c == 'f') return parse_bool(p, NULL);
    if (strncmp(&p->src[p->pos], "null", 4) == 0) {
        p->pos += 4;
        return true;
    }
    if (c == '-' || isdigit((unsigned char)c)) return parse_int(p, NULL);
    p->err = "Unexpected token";
    return false;
}

static bool parse_key_object(json_parser_t *p, key_config_t *kc) {
    memset(kc, 0, sizeof(*kc));
    strncpy(kc->label, "Macro", MAX_LABEL_LEN - 1);
    kc->type = KEY_TYPE_COMBO;
    kc->color = COLOR_PAGE_DEFAULT;
    kc->behavior = BEHAVIOR_NORMAL;
    strncpy(kc->key, "A", MAX_KEY_NAME_LEN - 1);

    if (!match_char(p, '{')) {
        p->err = "Expected key object";
        return false;
    }
    if (match_char(p, '}')) return true;

    char field[32];
    char str_val[MAX_TEXT_LEN];
    while (true) {
        if (!parse_string(p, field, sizeof(field))) return false;
        if (!match_char(p, ':')) {
            p->err = "Expected ':' in key";
            return false;
        }
        skip_ws(p);

        if (strcmp(field, "label") == 0) {
            if (!parse_string(p, kc->label, MAX_LABEL_LEN)) return false;
        } else if (strcmp(field, "type") == 0) {
            if (!parse_string(p, str_val, sizeof(str_val))) return false;
            kc->type = key_type_from_str(str_val);
        } else if (strcmp(field, "color") == 0) {
            if (strncmp(&p->src[p->pos], "null", 4) == 0) {
                p->pos += 4;
                kc->color = COLOR_PAGE_DEFAULT;
            } else {
                int32_t c_val = 0;
                if (!parse_int(p, &c_val)) return false;
                kc->color = c_val;
            }
        } else if (strcmp(field, "behavior") == 0) {
            if (!parse_string(p, str_val, sizeof(str_val))) return false;
            kc->behavior = behavior_from_str(str_val);
        } else if (strcmp(field, "key") == 0) {
            if (!parse_string(p, kc->key, MAX_KEY_NAME_LEN)) return false;
        } else if (strcmp(field, "text") == 0) {
            if (!parse_string(p, kc->text, MAX_TEXT_LEN)) return false;
        } else if (strcmp(field, "action") == 0) {
            if (!parse_string(p, kc->action, MAX_ACTION_LEN)) return false;
        } else if (strcmp(field, "mods") == 0) {
            if (!match_char(p, '[')) {
                p->err = "Expected mods array";
                return false;
            }
            kc->mod_mask = 0;
            kc->mod_count = 0;
            if (!match_char(p, ']')) {
                while (true) {
                    if (!parse_string(p, str_val, sizeof(str_val))) return false;
                    int mid = mod_from_str(str_val);
                    if (mid >= 0 && !(kc->mod_mask & (1 << mid)) && kc->mod_count < MAX_MODS) {
                        kc->mod_mask |= (1 << mid);
                        kc->mod_order[kc->mod_count++] = (uint8_t)mid;
                    }
                    if (match_char(p, ',')) continue;
                    if (match_char(p, ']')) break;
                    p->err = "Expected ']' in mods";
                    return false;
                }
            }
        } else {
            if (!skip_value(p)) return false;
        }

        if (match_char(p, ',')) continue;
        if (match_char(p, '}')) break;
        p->err = "Expected '}' in key";
        return false;
    }
    return true;
}

static bool parse_profile_object(json_parser_t *p, profile_config_t *prof) {
    memset(prof, 0, sizeof(*prof));
    strncpy(prof->name, "PAGE", MAX_NAME_LEN - 1);
    prof->os = OS_PC;
    prof->theme = 0x002244;
    for (int i = 0; i < NUM_KEYS; i++) {
        prof->keys[i] = PRESETS[i];
    }

    if (!match_char(p, '{')) {
        p->err = "Expected profile object";
        return false;
    }
    if (match_char(p, '}')) return true;

    char field[32];
    char str_val[32];
    while (true) {
        if (!parse_string(p, field, sizeof(field))) return false;
        if (!match_char(p, ':')) {
            p->err = "Expected ':' in profile";
            return false;
        }
        skip_ws(p);

        if (strcmp(field, "name") == 0) {
            if (!parse_string(p, prof->name, MAX_NAME_LEN)) return false;
        } else if (strcmp(field, "os") == 0) {
            if (!parse_string(p, str_val, sizeof(str_val))) return false;
            prof->os = (strcmp(str_val, "MAC") == 0) ? OS_MAC : OS_PC;
        } else if (strcmp(field, "theme") == 0) {
            int32_t t_val = 0x002244;
            if (!parse_int(p, &t_val)) return false;
            prof->theme = (uint32_t)t_val;
        } else if (strcmp(field, "keys") == 0) {
            if (!match_char(p, '[')) {
                p->err = "Expected keys array";
                return false;
            }
            int k_idx = 0;
            if (!match_char(p, ']')) {
                while (true) {
                    if (k_idx < NUM_KEYS) {
                        if (!parse_key_object(p, &prof->keys[k_idx])) return false;
                        k_idx++;
                    } else {
                        if (!skip_value(p)) return false;
                    }
                    if (match_char(p, ',')) continue;
                    if (match_char(p, ']')) break;
                    p->err = "Expected ']' in keys";
                    return false;
                }
            }
        } else {
            if (!skip_value(p)) return false;
        }

        if (match_char(p, ',')) continue;
        if (match_char(p, '}')) break;
        p->err = "Expected '}' in profile";
        return false;
    }
    return true;
}

bool config_from_json(const char *json, macropad_config_t *out_cfg, char *err_buf, int err_len) {
    if (!json || !out_cfg) return false;
    json_parser_t p = { .src = json, .pos = 0, .err = NULL };

    macropad_config_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.active = 0;
    tmp.sound = true;
    tmp.profile_count = 0;

    if (!match_char(&p, '{')) {
        if (err_buf && err_len > 0) snprintf(err_buf, err_len, "Root must be object");
        return false;
    }

    char field[32];
    while (!match_char(&p, '}')) {
        if (!parse_string(&p, field, sizeof(field))) {
            if (err_buf && err_len > 0) snprintf(err_buf, err_len, "%s", p.err ? p.err : "Key error");
            return false;
        }
        if (!match_char(&p, ':')) {
            if (err_buf && err_len > 0) snprintf(err_buf, err_len, "Expected ':'");
            return false;
        }
        skip_ws(&p);

        if (strcmp(field, "active") == 0) {
            int32_t act = 0;
            if (!parse_int(&p, &act)) {
                if (err_buf && err_len > 0) snprintf(err_buf, err_len, "Bad active index");
                return false;
            }
            tmp.active = (act >= 0) ? (uint8_t)act : 0;
        } else if (strcmp(field, "sound") == 0) {
            if (!parse_bool(&p, &tmp.sound)) {
                if (err_buf && err_len > 0) snprintf(err_buf, err_len, "Bad sound bool");
                return false;
            }
        } else if (strcmp(field, "profiles") == 0) {
            if (!match_char(&p, '[')) {
                if (err_buf && err_len > 0) snprintf(err_buf, err_len, "Expected profiles array");
                return false;
            }
            if (!match_char(&p, ']')) {
                while (true) {
                    if (tmp.profile_count < MAX_PROFILES) {
                        if (!parse_profile_object(&p, &tmp.profiles[tmp.profile_count])) {
                            if (err_buf && err_len > 0) snprintf(err_buf, err_len, "%s", p.err ? p.err : "Profile error");
                            return false;
                        }
                        tmp.profile_count++;
                    } else {
                        if (!skip_value(&p)) return false;
                    }
                    if (match_char(&p, ',')) continue;
                    if (match_char(&p, ']')) break;
                    if (err_buf && err_len > 0) snprintf(err_buf, err_len, "Expected ']' in profiles");
                    return false;
                }
            }
        } else {
            if (!skip_value(&p)) {
                if (err_buf && err_len > 0) snprintf(err_buf, err_len, "%s", p.err ? p.err : "Skip error");
                return false;
            }
        }

        if (match_char(&p, ',')) continue;
        if (match_char(&p, '}')) break;
        if (err_buf && err_len > 0) snprintf(err_buf, err_len, "Expected '}' at root");
        return false;
    }

    if (tmp.profile_count == 0) {
        if (err_buf && err_len > 0) snprintf(err_buf, err_len, "No profiles in config");
        return false;
    }
    if (tmp.active >= tmp.profile_count) {
        tmp.active = 0;
    }

    *out_cfg = tmp;
    return true;
}

// ============================================================================
// 5. PERSISTENT FLASH STORAGE (RP2040 QSPI FLASH)
// ============================================================================

#define FLASH_CONFIG_MAGIC 0x4D505634u // "MPV4" (macros_v4)

typedef struct {
    uint32_t magic;
    uint32_t crc32;
    macropad_config_t config;
} flash_storage_blob_t;

static uint32_t calc_crc32(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

#ifdef PICO_BUILD
// Store in the final 64KB block of the 2MB/8MB flash (offset 0x1F0000 fits both 2MB Pico and 8MB MacroPad)
#define FLASH_TARGET_OFFSET (0x200000u - 0x10000u)
#define FLASH_STORAGE_SECS  ((sizeof(flash_storage_blob_t) + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE)
#define FLASH_STORAGE_PAGES ((sizeof(flash_storage_blob_t) + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE)

void config_load(void) {
    const flash_storage_blob_t *blob = (const flash_storage_blob_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (blob->magic == FLASH_CONFIG_MAGIC) {
        uint32_t expected_crc = calc_crc32(&blob->config, sizeof(macropad_config_t));
        if (blob->crc32 == expected_crc &&
            blob->config.profile_count > 0 &&
            blob->config.profile_count <= MAX_PROFILES) {
            g_config = blob->config;
            if (g_config.active >= g_config.profile_count) {
                g_config.active = 0;
            }
            return;
        }
    }
    config_set_defaults(&g_config);
}

bool config_save(void) {
    static uint8_t page_buf[FLASH_STORAGE_PAGES * FLASH_PAGE_SIZE] __attribute__((aligned(4)));
    memset(page_buf, 0xFF, sizeof(page_buf));

    flash_storage_blob_t *blob = (flash_storage_blob_t *)page_buf;
    blob->magic = FLASH_CONFIG_MAGIC;
    blob->config = g_config;
    blob->crc32 = calc_crc32(&blob->config, sizeof(macropad_config_t));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_STORAGE_SECS * FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, page_buf, FLASH_STORAGE_PAGES * FLASH_PAGE_SIZE);
    restore_interrupts(ints);
    return true;
}
#else
// Host fallback for testing
void config_load(void) {
    (void)calc_crc32;
    config_set_defaults(&g_config);
}

bool config_save(void) {
    return true;
}
#endif

