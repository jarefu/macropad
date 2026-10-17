#ifndef MACROPAD_CONFIG_H
#define MACROPAD_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_PROFILES       16
#define NUM_KEYS           12
#define MAX_LABEL_LEN      7   // 6 chars + null terminator
#define MAX_NAME_LEN       7   // 6 chars + null terminator
#define MAX_KEY_NAME_LEN   8
#define MAX_ACTION_LEN     16
#define MAX_TEXT_LEN       121 // 120 chars + null terminator
#define MAX_MODS           7

#define COLOR_PAGE_DEFAULT (-1)

typedef enum {
    KEY_TYPE_COMBO = 0,
    KEY_TYPE_TEXT,
    KEY_TYPE_MEDIA,
    KEY_TYPE_SPECIAL,
    KEY_TYPE_KNOB
} key_type_t;

typedef enum {
    BEHAVIOR_NORMAL = 0,
    BEHAVIOR_HOLD,
    BEHAVIOR_TOGGLE
} key_behavior_t;

typedef enum {
    OS_PC = 0,
    OS_MAC
} target_os_t;

typedef enum {
    MOD_OS = 0,
    MOD_SHIFT,
    MOD_ALT,
    MOD_CTRL,
    MOD_GUI,
    MOD_OS_SCREEN,
    MOD_OS_SWITCH,
    MOD_COUNT
} mod_id_t;

typedef struct {
    char label[MAX_LABEL_LEN];
    key_type_t type;
    int32_t color; // -1 (COLOR_PAGE_DEFAULT) for null, or 0x000000..0xFFFFFF
    key_behavior_t behavior;

    // Type-specific fields
    uint8_t mod_mask; // bitmask of (1 << mod_id_t)
    uint8_t mod_order[MAX_MODS];
    uint8_t mod_count;
    char key[MAX_KEY_NAME_LEN];
    char text[MAX_TEXT_LEN];
    char action[MAX_ACTION_LEN];
} key_config_t;

typedef struct {
    char name[MAX_NAME_LEN];
    target_os_t os;
    uint32_t theme;
    key_config_t keys[NUM_KEYS];
} profile_config_t;

typedef struct {
    uint8_t active;
    bool sound;
    uint8_t profile_count;
    profile_config_t profiles[MAX_PROFILES];
} macropad_config_t;

typedef struct {
    const char *name;
    int32_t value; // COLOR_PAGE_DEFAULT (-1) for "Page Default"
} color_entry_t;

typedef struct {
    const char *label;
    const char *action_id;
    const char *default_label;
} knob_action_entry_t;

#define NUM_KEY_NAMES      58
#define NUM_COLOR_PALETTE  13
#define NUM_THEME_PALETTE  12
#define NUM_PRESETS        15
#define NUM_KNOB_ACTIONS   8
#define NUM_TEXT_PAGES     7

extern const char * const KEY_NAMES[NUM_KEY_NAMES];
extern const color_entry_t COLOR_PALETTE[NUM_COLOR_PALETTE];
extern const color_entry_t * const THEME_PALETTE;
extern const key_config_t PRESETS[NUM_PRESETS];
extern const knob_action_entry_t KNOB_ACTIONS[NUM_KNOB_ACTIONS];
extern const char TEXT_PAGES_LOWER[NUM_TEXT_PAGES][9];
extern const char TEXT_PAGES_UPPER[NUM_TEXT_PAGES][9];

extern macropad_config_t g_config;

// Configuration management functions
void config_set_defaults(macropad_config_t *cfg);
void config_load(void);
bool config_save(void);

// JSON serialization / deserialization (compatible with docs/index.html & macros_v4.json)
int config_to_json(const macropad_config_t *cfg, char *buf, int max_len);
bool config_from_json(const char *json, macropad_config_t *out_cfg, char *err_buf, int err_len);

// Helper conversions
const char *key_type_to_str(key_type_t t);
key_type_t key_type_from_str(const char *s);
const char *behavior_to_str(key_behavior_t b);
key_behavior_t behavior_from_str(const char *s);
const char *mod_to_str(mod_id_t m);
int mod_from_str(const char *s);
uint8_t key_name_to_hid_keycode(const char *name);

#endif // MACROPAD_CONFIG_H

