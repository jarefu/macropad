#include "hardware.h"
#include "font5x8.h"
#include <string.h>
#include <stdio.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "ws2812.pio.h"
#endif

// ============================================================================
// DISPLAY STATE (5 lines x 21 chars on 128x64 SH1106)
// ============================================================================
#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_PAGES  (OLED_HEIGHT / 8)
#define TEXT_LINES  5
#define TEXT_COLS   21

static char s_lines[TEXT_LINES][TEXT_COLS + 1];
static bool s_display_dirty = true;
static bool s_display_sleeping = false;
static uint8_t s_oled_fb[OLED_PAGES][OLED_WIDTH];

// ============================================================================
// NEOPIXEL STATE
// ============================================================================
static uint32_t s_pixels[12];
static bool s_pixels_dirty = true;

// ============================================================================
// INPUT STATE (12 KEYS + ROTARY ENCODER + SWITCH)
// ============================================================================
#define KEY_QUEUE_SIZE 32
static key_event_t s_key_queue[KEY_QUEUE_SIZE];
static uint8_t s_kq_head = 0;
static uint8_t s_kq_tail = 0;

static bool s_enc_sw_pressed_edge = false;
static volatile int32_t s_enc_substeps = 0;
static volatile int32_t s_enc_position = 0;

#ifdef PICO_BUILD
static bool s_key_state[12];
static uint64_t s_key_last_change[12];
static bool s_enc_sw_state = false;
static uint64_t s_enc_sw_last_change = 0;
static volatile uint8_t s_enc_prev_ab = 0x03;
#endif

// ============================================================================
// ASCII TO USB HID KEYCODE + SHIFT LOOKUP TABLE (US QWERTY)
// ============================================================================
typedef struct {
    uint8_t keycode;
    uint8_t modifier;
} ascii_hid_entry_t;

static ascii_hid_entry_t ascii_to_hid(char c) {
    ascii_hid_entry_t res = {0, 0};
    if (c >= 'a' && c <= 'z') {
        res.keycode = (uint8_t)(0x04 + (c - 'a'));
        return res;
    }
    if (c >= 'A' && c <= 'Z') {
        res.keycode = (uint8_t)(0x04 + (c - 'A'));
        res.modifier = HID_MOD_LSHIFT;
        return res;
    }
    if (c >= '1' && c <= '9') {
        res.keycode = (uint8_t)(0x1E + (c - '1'));
        return res;
    }
    if (c == '0') {
        res.keycode = 0x27;
        return res;
    }

    switch (c) {
        case '\n': case '\r': res.keycode = 0x28; break; // ENTER
        case '\t': res.keycode = 0x2B; break;            // TAB
        case ' ':  res.keycode = 0x2C; break;            // SPACE
        case '!':  res.keycode = 0x1E; res.modifier = HID_MOD_LSHIFT; break;
        case '@':  res.keycode = 0x1F; res.modifier = HID_MOD_LSHIFT; break;
        case '#':  res.keycode = 0x20; res.modifier = HID_MOD_LSHIFT; break;
        case '$':  res.keycode = 0x21; res.modifier = HID_MOD_LSHIFT; break;
        case '%':  res.keycode = 0x22; res.modifier = HID_MOD_LSHIFT; break;
        case '^':  res.keycode = 0x23; res.modifier = HID_MOD_LSHIFT; break;
        case '&':  res.keycode = 0x24; res.modifier = HID_MOD_LSHIFT; break;
        case '*':  res.keycode = 0x25; res.modifier = HID_MOD_LSHIFT; break;
        case '(':  res.keycode = 0x26; res.modifier = HID_MOD_LSHIFT; break;
        case ')':  res.keycode = 0x27; res.modifier = HID_MOD_LSHIFT; break;
        case '-':  res.keycode = 0x2D; break;
        case '_':  res.keycode = 0x2D; res.modifier = HID_MOD_LSHIFT; break;
        case '=':  res.keycode = 0x2E; break;
        case '+':  res.keycode = 0x2E; res.modifier = HID_MOD_LSHIFT; break;
        case '[':  res.keycode = 0x2F; break;
        case '{':  res.keycode = 0x2F; res.modifier = HID_MOD_LSHIFT; break;
        case ']':  res.keycode = 0x30; break;
        case '}':  res.keycode = 0x30; res.modifier = HID_MOD_LSHIFT; break;
        case '\\': res.keycode = 0x31; break;
        case '|':  res.keycode = 0x31; res.modifier = HID_MOD_LSHIFT; break;
        case ';':  res.keycode = 0x33; break;
        case ':':  res.keycode = 0x33; res.modifier = HID_MOD_LSHIFT; break;
        case '\'': res.keycode = 0x34; break;
        case '\"': res.keycode = 0x34; res.modifier = HID_MOD_LSHIFT; break;
        case '`':  res.keycode = 0x35; break;
        case '~':  res.keycode = 0x35; res.modifier = HID_MOD_LSHIFT; break;
        case ',':  res.keycode = 0x36; break;
        case '<':  res.keycode = 0x36; res.modifier = HID_MOD_LSHIFT; break;
        case '.':  res.keycode = 0x37; break;
        case '>':  res.keycode = 0x37; res.modifier = HID_MOD_LSHIFT; break;
        case '/':  res.keycode = 0x38; break;
        case '?':  res.keycode = 0x38; res.modifier = HID_MOD_LSHIFT; break;
        default:   res.keycode = 0x00; break;
    }
    return res;
}

// ============================================================================
// FRAMEBUFFER TEXT RENDERER
// ============================================================================
static void fb_set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int page = y >> 3;
    uint8_t bit = (uint8_t)(1u << (y & 7));
    if (on) {
        s_oled_fb[page][x] |= bit;
    } else {
        s_oled_fb[page][x] &= (uint8_t)~bit;
    }
}

static void fb_draw_char(int x, int y, char c) {
    if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) c = ' ';
    const uint8_t *glyph = FONT_5X8[(unsigned char)c - 0x20];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 8; row++) {
            if (bits & (1u << row)) {
                fb_set_pixel(x + col, y + row, true);
            }
        }
    }
}

static void fb_render_lines(void) {
    memset(s_oled_fb, 0, sizeof(s_oled_fb));
    if (s_display_sleeping) return;

    // 5 lines spaced cleanly across 64 vertical pixels: y = 0, 14, 27, 40, 53
    static const int line_y[TEXT_LINES] = { 0, 14, 27, 40, 53 };
    for (int row = 0; row < TEXT_LINES; row++) {
        const char *str = s_lines[row];
        int x = 1;
        for (int col = 0; col < TEXT_COLS && str[col]; col++) {
            fb_draw_char(x, line_y[row], str[col]);
            x += 6;
        }
    }
}

void hw_display_set_line(int line_idx, const char *text) {
    if (line_idx < 0 || line_idx >= TEXT_LINES) return;
    char formatted[TEXT_COLS + 1];
    memset(formatted, 0, sizeof(formatted));
    if (text) {
        strncpy(formatted, text, TEXT_COLS);
    }
    if (strcmp(s_lines[line_idx], formatted) != 0) {
        strcpy(s_lines[line_idx], formatted);
        s_display_dirty = true;
    }
}

void hw_pixel_set(int idx, uint32_t rgb) {
    if (idx < 0 || idx >= 12) return;
    if (s_pixels[idx] != rgb) {
        s_pixels[idx] = rgb;
        s_pixels_dirty = true;
    }
}

bool hw_key_event_pop(key_event_t *ev) {
    if (s_kq_head == s_kq_tail) return false;
    if (ev) *ev = s_key_queue[s_kq_tail];
    s_kq_tail = (uint8_t)((s_kq_tail + 1) % KEY_QUEUE_SIZE);
    return true;
}

static void key_event_push(uint8_t key_num, bool pressed) {
    uint8_t next = (uint8_t)((s_kq_head + 1) % KEY_QUEUE_SIZE);
    if (next != s_kq_tail) {
        s_key_queue[s_kq_head].key_number = key_num;
        s_key_queue[s_kq_head].pressed = pressed;
        s_kq_head = next;
    }
}

#ifdef PICO_BUILD

static PIO s_neo_pio = pio0;
static uint s_neo_sm = 0;
static repeating_timer_t s_enc_timer;

// ============================================================================
// SH1106 SPI OLED DRIVER
// ============================================================================
static void sh1106_write_cmd(uint8_t cmd) {
    gpio_put(PIN_OLED_DC, 0);
    gpio_put(PIN_OLED_CS, 0);
    spi_write_blocking(spi1, &cmd, 1);
    gpio_put(PIN_OLED_CS, 1);
}

static void sh1106_write_data(const uint8_t *data, size_t len) {
    gpio_put(PIN_OLED_DC, 1);
    gpio_put(PIN_OLED_CS, 0);
    spi_write_blocking(spi1, data, len);
    gpio_put(PIN_OLED_CS, 1);
}

static void sh1106_init(void) {
    spi_init(spi1, 10 * 1000 * 1000); // 10 MHz SPI
    gpio_set_function(PIN_OLED_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_OLED_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_OLED_CS);
    gpio_set_dir(PIN_OLED_CS, GPIO_OUT);
    gpio_put(PIN_OLED_CS, 1);

    gpio_init(PIN_OLED_DC);
    gpio_set_dir(PIN_OLED_DC, GPIO_OUT);
    gpio_put(PIN_OLED_DC, 0);

    gpio_init(PIN_OLED_RESET);
    gpio_set_dir(PIN_OLED_RESET, GPIO_OUT);
    gpio_put(PIN_OLED_RESET, 1);
    sleep_ms(1);
    gpio_put(PIN_OLED_RESET, 0);
    sleep_ms(10);
    gpio_put(PIN_OLED_RESET, 1);
    sleep_ms(10);

    static const uint8_t init_cmds[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Clock divide ratio / oscillator freq
        0xA8, 0x3F, // Multiplex ratio (1/64)
        0xD3, 0x00, // Display offset = 0
        0x40,       // Start line = 0
        0xAD, 0x8B, // DC-DC converter ON
        0xA1,       // Segment re-map (horizontal flip)
        0xC8,       // COM output scan direction (vertical flip)
        0xDA, 0x12, // COM pins hardware config
        0x81, 0xCF, // Contrast control
        0xD9, 0xF1, // Pre-charge period
        0xDB, 0x40, // VCOMH deselect level
        0xA4,       // Entire display ON from RAM
        0xA6,       // Normal display (not inverted)
        0xAF        // Display ON
    };
    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        sh1106_write_cmd(init_cmds[i]);
    }
}

void hw_display_refresh(void) {
    if (!s_display_dirty) return;
    fb_render_lines();
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        sh1106_write_cmd((uint8_t)(0xB0 | page)); // Page address
        sh1106_write_cmd(0x02);                   // Lower column = 2 (SH1106 132x64 -> 128x64 offset)
        sh1106_write_cmd(0x10);                   // Upper column = 0
        sh1106_write_data(s_oled_fb[page], OLED_WIDTH);
    }
    s_display_dirty = false;
}

void hw_display_set_sleep(bool sleep) {
    if (s_display_sleeping != sleep) {
        s_display_sleeping = sleep;
        sh1106_write_cmd(sleep ? 0xAE : 0xAF);
        s_display_dirty = true;
        hw_display_refresh();
    }
}

// ============================================================================
// WS2812 NEOPIXEL DRIVER
// ============================================================================
void hw_pixels_show(void) {
    if (!s_pixels_dirty) return;
    for (int i = 0; i < 12; i++) {
        uint32_t rgb = s_pixels[i];
        uint8_t r = (uint8_t)((rgb >> 16) & 0xFF);
        uint8_t g = (uint8_t)((rgb >> 8) & 0xFF);
        uint8_t b = (uint8_t)(rgb & 0xFF);
        uint32_t grb = ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
        pio_sm_put_blocking(s_neo_pio, s_neo_sm, grb);
    }
    s_pixels_dirty = false;
}

// ============================================================================
// QUADRATURE ROTARY ENCODER (1kHz TIMER ISR)
// ============================================================================
static const int8_t ENC_TABLE[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0
};

static bool encoder_timer_cb(repeating_timer_t *rt) {
    (void)rt;
    uint8_t a = (uint8_t)gpio_get(PIN_ROTA);
    uint8_t b = (uint8_t)gpio_get(PIN_ROTB);
    uint8_t ab = (uint8_t)((a << 1) | b);
    if (ab == s_enc_prev_ab) {
        return true;
    }

    uint8_t idx = (uint8_t)((s_enc_prev_ab << 2) | ab);
    int8_t step = ENC_TABLE[idx];
    if (step != 0) {
        s_enc_substeps += step;
        s_enc_prev_ab = ab;

        // Physical detents rest at AB == 0b11 (both switches open / pulled high).
        // Latch one step only when settling back into the detent after >= 2 transitions,
        // and zero out substeps so contact bounce around the detent cannot step backward.
        if (ab == 0x03) {
            if (s_enc_substeps >= 2) {
                s_enc_position++;
            } else if (s_enc_substeps <= -2) {
                s_enc_position--;
            }
            s_enc_substeps = 0;
        }
    }
    return true;
}

int32_t hw_encoder_get(void) {
    return s_enc_position;
}

bool hw_encoder_switch_pressed(void) {
    bool res = s_enc_sw_pressed_edge;
    s_enc_sw_pressed_edge = false;
    return res;
}

// ============================================================================
// AUDIO / TONE GENERATOR (PWM ON GP16 + SPEAKER ENABLE ON GP14)
// ============================================================================
void hw_play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0 || duration_ms == 0) return;

    uint slice_num = pwm_gpio_to_slice_num(PIN_SPEAKER);
    uint chan = pwm_gpio_to_channel(PIN_SPEAKER);

    uint32_t sys_clk = clock_get_hz(clk_sys);
    uint32_t div16 = (sys_clk * 16) / (freq_hz * 4096);
    if (div16 < 16) div16 = 16;
    uint32_t top = (sys_clk * 16) / (div16 * freq_hz) - 1;
    if (top > 65534) top = 65534;

    pwm_set_clkdiv_int_frac(slice_num, (uint8_t)(div16 >> 4), (uint8_t)(div16 & 0x0F));
    pwm_set_wrap(slice_num, (uint16_t)top);
    pwm_set_chan_level(slice_num, chan, (uint16_t)(top / 2));

    gpio_put(PIN_SPEAKER_ENABLE, 1);
    pwm_set_enabled(slice_num, true);

    hw_sleep_ms(duration_ms);

    pwm_set_enabled(slice_num, false);
    gpio_put(PIN_SPEAKER_ENABLE, 0);
}

// ============================================================================
// INITIALIZATION & POLLING
// ============================================================================
void hw_init(void) {
    // 1. Keys GP1..GP12 (active-low with pull-up)
    for (int i = 0; i < 12; i++) {
        uint pin = PIN_KEY1 + i;
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        s_key_state[i] = false;
        s_key_last_change[i] = 0;
    }

    // 2. Encoder switch GP0 & quadrature pins GP17, GP18
    gpio_init(PIN_ENCODER_SWITCH);
    gpio_set_dir(PIN_ENCODER_SWITCH, GPIO_IN);
    gpio_pull_up(PIN_ENCODER_SWITCH);

    gpio_init(PIN_ROTA);
    gpio_set_dir(PIN_ROTA, GPIO_IN);
    gpio_pull_up(PIN_ROTA);

    gpio_init(PIN_ROTB);
    gpio_set_dir(PIN_ROTB, GPIO_IN);
    gpio_pull_up(PIN_ROTB);

    s_enc_prev_ab = (uint8_t)(((gpio_get(PIN_ROTA) ? 1 : 0) << 1) | (gpio_get(PIN_ROTB) ? 1 : 0));
    add_repeating_timer_us(-500, encoder_timer_cb, NULL, &s_enc_timer);

    // 3. Speaker amplifier enable (GP14) & PWM pin (GP16)
    gpio_init(PIN_SPEAKER_ENABLE);
    gpio_set_dir(PIN_SPEAKER_ENABLE, GPIO_OUT);
    gpio_put(PIN_SPEAKER_ENABLE, 0);
    gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);

    // 4. NeoPixels via PIO0
    uint offset = pio_add_program(s_neo_pio, &ws2812_program);
    ws2812_program_init(s_neo_pio, s_neo_sm, offset, PIN_NEOPIXEL, 800000.0f, false);

    // 5. SH1106 128x64 OLED Display
    sh1106_init();

    // 6. TinyUSB Device Stack
    tusb_init();
}

uint64_t hw_monotonic_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void hw_sleep_ms(uint32_t ms) {
    uint64_t start = hw_monotonic_ms();
    while (hw_monotonic_ms() - start < ms) {
        tud_task();
        sleep_us(250);
    }
}

void hw_poll(void) {
    tud_task();

    uint64_t now = hw_monotonic_ms();
    const uint64_t debounce_ms = 20;

    // Poll encoder switch (active-low on GP0)
    bool sw_raw = !gpio_get(PIN_ENCODER_SWITCH);
    if (sw_raw != s_enc_sw_state && (now - s_enc_sw_last_change) >= debounce_ms) {
        s_enc_sw_state = sw_raw;
        s_enc_sw_last_change = now;
        if (sw_raw) {
            s_enc_sw_pressed_edge = true;
        }
    }

    // Poll 12 mechanical keys (active-low on GP1..GP12)
    for (uint8_t i = 0; i < 12; i++) {
        bool k_raw = !gpio_get(PIN_KEY1 + i);
        if (k_raw != s_key_state[i] && (now - s_key_last_change[i]) >= debounce_ms) {
            s_key_state[i] = k_raw;
            s_key_last_change[i] = now;
            key_event_push(i, k_raw);
        }
    }
}

// ============================================================================
// USB HID REPORTS (KEYBOARD, CONSUMER CONTROL, MOUSE)
// ============================================================================
static void wait_hid_ready(void) {
    uint64_t start = hw_monotonic_ms();
    while (!tud_hid_ready()) {
        tud_task();
        if (hw_monotonic_ms() - start > 50) break;
    }
}

void hw_keyboard_press(uint8_t modifiers, const uint8_t *keycodes, uint8_t count) {
    if (!tud_mounted()) return;
    uint8_t report[6] = {0};
    for (uint8_t i = 0; i < count && i < 6; i++) {
        report[i] = keycodes[i];
    }
    wait_hid_ready();
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifiers, report);
    hw_sleep_ms(4);
}

void hw_keyboard_release_all(void) {
    if (!tud_mounted()) return;
    wait_hid_ready();
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
    hw_sleep_ms(4);
}

void hw_keyboard_send(uint8_t modifiers, const uint8_t *keycodes, uint8_t count) {
    hw_keyboard_press(modifiers, keycodes, count);
    hw_keyboard_release_all();
}

void hw_keyboard_write_text(const char *text) {
    if (!text) return;
    while (*text) {
        ascii_hid_entry_t e = ascii_to_hid(*text++);
        if (e.keycode != 0) {
            hw_keyboard_send(e.modifier, &e.keycode, 1);
        }
    }
}

void hw_consumer_press(uint16_t usage_code) {
    if (!tud_mounted()) return;
    wait_hid_ready();
    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &usage_code, sizeof(usage_code));
    hw_sleep_ms(4);
}

void hw_consumer_release(void) {
    if (!tud_mounted()) return;
    uint16_t zero = 0;
    wait_hid_ready();
    tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &zero, sizeof(zero));
    hw_sleep_ms(4);
}

void hw_consumer_send(uint16_t usage_code) {
    hw_consumer_press(usage_code);
    hw_consumer_release();
}

void hw_mouse_move(int8_t x, int8_t y, int8_t wheel) {
    if (!tud_mounted()) return;
    wait_hid_ready();
    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, x, y, wheel, 0);
    hw_sleep_ms(4);
}

void hw_mouse_press(uint8_t buttons) {
    if (!tud_mounted()) return;
    wait_hid_ready();
    tud_hid_mouse_report(REPORT_ID_MOUSE, buttons, 0, 0, 0, 0);
    hw_sleep_ms(4);
}

void hw_mouse_release_all(void) {
    if (!tud_mounted()) return;
    wait_hid_ready();
    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, 0, 0, 0, 0);
    hw_sleep_ms(4);
}

// ============================================================================
// USB CDC (WEBSERIAL)
// ============================================================================
uint32_t hw_cdc_available(void) {
    return tud_cdc_available();
}

uint32_t hw_cdc_read(char *buf, uint32_t max_len) {
    return tud_cdc_read(buf, max_len);
}

void hw_cdc_write(const char *data, uint32_t len) {
    uint32_t sent = 0;
    uint64_t start = hw_monotonic_ms();
    while (sent < len) {
        uint32_t avail = tud_cdc_write_available();
        if (avail > 0) {
            uint32_t chunk = (len - sent < avail) ? (len - sent) : avail;
            uint32_t w = tud_cdc_write(data + sent, chunk);
            sent += w;
            tud_cdc_write_flush();
            start = hw_monotonic_ms();
        }
        tud_task();
        if (hw_monotonic_ms() - start > 250) break;
    }
    tud_cdc_write_flush();
}

void hw_cdc_write_str(const char *str) {
    if (!str) return;
    hw_cdc_write(str, (uint32_t)strlen(str));
}

// TinyUSB HID callbacks
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

#else
// ============================================================================
// HOST STUB IMPLEMENTATION (FOR SYNTAX / UNIT TESTING ON LINUX)
// ============================================================================
void hw_init(void) {
    (void)ascii_to_hid;
    (void)key_event_push;
}
void hw_poll(void) {}
uint64_t hw_monotonic_ms(void) { return 0; }
void hw_sleep_ms(uint32_t ms) { (void)ms; }
void hw_display_refresh(void) { fb_render_lines(); s_display_dirty = false; }
void hw_display_set_sleep(bool sleep) { s_display_sleeping = sleep; }
void hw_pixels_show(void) { s_pixels_dirty = false; }
void hw_play_tone(uint32_t freq_hz, uint32_t duration_ms) { (void)freq_hz; (void)duration_ms; }
int32_t hw_encoder_get(void) { (void)s_enc_substeps; return s_enc_position; }
bool hw_encoder_switch_pressed(void) { return s_enc_sw_pressed_edge; }
void hw_keyboard_press(uint8_t modifiers, const uint8_t *keycodes, uint8_t count) { (void)modifiers; (void)keycodes; (void)count; }
void hw_keyboard_release_all(void) {}
void hw_keyboard_send(uint8_t modifiers, const uint8_t *keycodes, uint8_t count) { (void)modifiers; (void)keycodes; (void)count; }
void hw_keyboard_write_text(const char *text) { (void)text; }
void hw_consumer_press(uint16_t usage_code) { (void)usage_code; }
void hw_consumer_release(void) {}
void hw_consumer_send(uint16_t usage_code) { (void)usage_code; }
void hw_mouse_move(int8_t x, int8_t y, int8_t wheel) { (void)x; (void)y; (void)wheel; }
void hw_mouse_press(uint8_t buttons) { (void)buttons; }
void hw_mouse_release_all(void) {}
uint32_t hw_cdc_available(void) { return 0; }
uint32_t hw_cdc_read(char *buf, uint32_t max_len) { (void)buf; (void)max_len; return 0; }
void hw_cdc_write(const char *data, uint32_t len) { (void)data; (void)len; }
void hw_cdc_write_str(const char *str) { (void)str; }
#endif
