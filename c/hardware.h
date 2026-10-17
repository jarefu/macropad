#ifndef MACROPAD_HARDWARE_H
#define MACROPAD_HARDWARE_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// ADAFRUIT MACROPAD RP2040 PIN DEFINITIONS
// ============================================================================
#define PIN_ENCODER_SWITCH 0
#define PIN_KEY1           1   // Keys 1..12 are on GPIO 1..12
#define PIN_RED_LED        13
#define PIN_SPEAKER_ENABLE 14
#define PIN_SPEAKER        16
#define PIN_ROTA           17
#define PIN_ROTB           18
#define PIN_NEOPIXEL       19
#define PIN_OLED_CS        22
#define PIN_OLED_RESET     23
#define PIN_OLED_DC        24
#define PIN_OLED_SCK       26
#define PIN_OLED_MOSI      27

// ============================================================================
// USB HID MODIFIER BITS & CONSUMER CONTROL CODES
// ============================================================================
#define HID_MOD_LCTRL      0x01
#define HID_MOD_LSHIFT     0x02
#define HID_MOD_LALT       0x04
#define HID_MOD_LGUI       0x08

#define HID_KEY_NONE       0x00
#define HID_KEY_A          0x04
#define HID_KEY_S          0x16
#define HID_KEY_W          0x1A
#define HID_KEY_Y          0x1C
#define HID_KEY_Z          0x1D
#define HID_KEY_4          0x21
#define HID_KEY_0          0x27
#define HID_KEY_ENTER      0x28
#define HID_KEY_ESCAPE     0x29
#define HID_KEY_BACKSPACE  0x2A
#define HID_KEY_TAB        0x2B
#define HID_KEY_SPACE      0x2C

// Consumer Control Usage Codes (16-bit)
#define CC_BRIGHTNESS_INC  0x006F
#define CC_BRIGHTNESS_DEC  0x0070
#define CC_NEXT_TRACK      0x00B5
#define CC_PREV_TRACK      0x00B6
#define CC_PLAY_PAUSE      0x00CD
#define CC_MUTE            0x00E2
#define CC_VOL_UP          0x00E9
#define CC_VOL_DOWN        0x00EA

// Mouse button mask
#define MOUSE_BTN_LEFT     0x01
#define MOUSE_BTN_RIGHT    0x02
#define MOUSE_BTN_MIDDLE   0x04

typedef struct {
    uint8_t key_number; // 0..11
    bool pressed;
} key_event_t;

// ============================================================================
// HARDWARE API
// ============================================================================
void hw_init(void);
void hw_poll(void);
uint64_t hw_monotonic_ms(void);
void hw_sleep_ms(uint32_t ms);

// Display (SH1106 128x64 OLED, 5 lines x 21 chars)
void hw_display_set_line(int line_idx, const char *text);
void hw_display_refresh(void);
void hw_display_set_sleep(bool sleep);

// NeoPixels (12 WS2812B LEDs)
void hw_pixel_set(int idx, uint32_t rgb);
void hw_pixels_show(void);

// Audio (Speaker PWM on GP16 + Enable on GP14)
void hw_play_tone(uint32_t freq_hz, uint32_t duration_ms);

// Input: Rotary Encoder & 12 Keys
int32_t hw_encoder_get(void);
bool hw_encoder_switch_pressed(void);
bool hw_key_event_pop(key_event_t *ev);

// USB HID Actions
void hw_keyboard_press(uint8_t modifiers, const uint8_t *keycodes, uint8_t count);
void hw_keyboard_release_all(void);
void hw_keyboard_send(uint8_t modifiers, const uint8_t *keycodes, uint8_t count);
void hw_keyboard_write_text(const char *text);

void hw_consumer_press(uint16_t usage_code);
void hw_consumer_release(void);
void hw_consumer_send(uint16_t usage_code);

void hw_mouse_move(int8_t x, int8_t y, int8_t wheel);
void hw_mouse_press(uint8_t buttons);
void hw_mouse_release_all(void);

// USB CDC (WebSerial)
uint32_t hw_cdc_available(void);
uint32_t hw_cdc_read(char *buf, uint32_t max_len);
void hw_cdc_write(const char *data, uint32_t len);
void hw_cdc_write_str(const char *str);

#endif // MACROPAD_HARDWARE_H

