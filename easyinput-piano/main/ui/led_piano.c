#include "led_piano.h"
#include "board_pins.h"
#include "board_power.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "led_strip.h"

#include <math.h>

static const char *TAG = "led_piano";

static led_strip_handle_t s_strip;
static int64_t s_vol_preview_until = 0;
static uint8_t s_vol_level = 0;
static int64_t s_victory_until = 0;

static uint8_t s_last_hit_note = 255;
static int64_t s_hit_time_us = 0;
static uint8_t s_guide_note = 0;

/* Color palette for 8 notes: C4 .. C5 (Red, Orange, Yellow, Green, Cyan, Blue, Purple, White) */
static const uint8_t s_note_colors[8][3] = {
    { 255,   0,   0 }, /* 1 (Do)  - Red */
    { 255,  80,   0 }, /* 2 (Re)  - Orange */
    { 255, 200,   0 }, /* 3 (Mi)  - Yellow */
    {   0, 255,  20 }, /* 4 (Fa)  - Green */
    {   0, 200, 255 }, /* 5 (Sol) - Cyan */
    {   0,  30, 255 }, /* 6 (La)  - Blue */
    { 180,   0, 255 }, /* 7 (Si)  - Purple */
    { 255, 255, 255 }, /* i (Do5) - White */
};

esp_err_t led_piano_init(void)
{
    ESP_RETURN_ON_FALSE(board_power_peripherals_enabled(), ESP_ERR_INVALID_STATE, TAG,
                        "GPIO8 must be enabled before initializing WS2812");

    led_strip_config_t strip_cfg = {
        .strip_gpio_num = BOARD_GPIO_LED_DIN,
        .max_leds = BOARD_WS2812_COUNT,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip), TAG, "rmt init failed");
    ESP_RETURN_ON_ERROR(led_strip_clear(s_strip), TAG, "led_strip_clear failed");

    ESP_LOGI(TAG, "WS2812 LED strip ready (%d LEDs)", BOARD_WS2812_COUNT);
    return ESP_OK;
}

void led_piano_on_note_hit(uint8_t note_idx)
{
    s_last_hit_note = note_idx;
    s_hit_time_us = esp_timer_get_time();
}

void led_piano_show_guide(uint8_t target_note)
{
    s_guide_note = target_note;
}

void led_piano_show_volume(uint8_t volume)
{
    s_vol_level = volume;
    s_vol_preview_until = esp_timer_get_time() + 600000; /* 600ms preview */
}

void led_piano_show_victory(void)
{
    s_victory_until = esp_timer_get_time() + 2500000; /* 2.5s celebration */
}

void led_piano_update(int64_t now_us, piano_play_mode_t mode)
{
    if (!s_strip) return;

    /* 1. Victory animation takes priority */
    if (now_us < s_victory_until) {
        int64_t progress = (now_us / 40000) % 360;
        for (int i = 0; i < BOARD_WS2812_COUNT; ++i) {
            float hue = (float)((progress + i * 50) % 360);
            /* Simple rainbow hue generator */
            float r = fmaxf(0.0f, cosf(hue * 0.01745f)) * 180.0f;
            float g = fmaxf(0.0f, cosf((hue - 120.0f) * 0.01745f)) * 180.0f;
            float b = fmaxf(0.0f, cosf((hue - 240.0f) * 0.01745f)) * 180.0f;
            led_strip_set_pixel(s_strip, i, (uint8_t)r, (uint8_t)g, (uint8_t)b);
        }
        led_strip_refresh(s_strip);
        return;
    }

    /* 2. Volume bar preview */
    if (now_us < s_vol_preview_until) {
        led_strip_clear(s_strip);
        int lit_count = (s_vol_level * BOARD_WS2812_COUNT + 10) / 100;
        if (lit_count < 1 && s_vol_level > 0) lit_count = 1;
        for (int i = 0; i < lit_count && i < BOARD_WS2812_COUNT; ++i) {
            led_strip_set_pixel(s_strip, i, 0, 160, 220);
        }
        led_strip_refresh(s_strip);
        return;
    }

    /* 3. Note strike splash */
    if (s_last_hit_note < 8 && (now_us - s_hit_time_us < 200000)) {
        float decay = 1.0f - (float)(now_us - s_hit_time_us) / 200000.0f;
        const uint8_t *c = s_note_colors[s_last_hit_note];
        uint8_t r = (uint8_t)(c[0] * decay * 0.8f);
        uint8_t g = (uint8_t)(c[1] * decay * 0.8f);
        uint8_t b = (uint8_t)(c[2] * decay * 0.8f);
        for (int i = 0; i < BOARD_WS2812_COUNT; ++i) {
            led_strip_set_pixel(s_strip, i, r, g, b);
        }
        led_strip_refresh(s_strip);
        return;
    }

    /* 4. Normal Mode Idle / Guide Animation */
    led_strip_clear(s_strip);

    if (mode == PIANO_MODE_GUIDE) {
        /* Gently breathe the target note's corresponding LED index */
        /* Map 8 notes to 5 LEDs: note 0..4 -> led 0..4; note 5..7 -> led 2..4 */
        uint8_t target_led = s_guide_note < 5 ? s_guide_note : (s_guide_note - 3);
        if (target_led >= BOARD_WS2812_COUNT) target_led = BOARD_WS2812_COUNT - 1;

        float breath = 0.5f + 0.5f * sinf((float)now_us * 0.000006f);
        const uint8_t *c = s_note_colors[s_guide_note];
        led_strip_set_pixel(s_strip, target_led,
                            (uint8_t)(c[0] * breath * 0.7f),
                            (uint8_t)(c[1] * breath * 0.7f),
                            (uint8_t)(c[2] * breath * 0.7f));
    } else if (mode == PIANO_MODE_FREE) {
        /* Soft warm ambient breathing */
        float breath = 0.2f + 0.15f * sinf((float)now_us * 0.000003f);
        for (int i = 0; i < BOARD_WS2812_COUNT; ++i) {
            led_strip_set_pixel(s_strip, i, (uint8_t)(60 * breath), (uint8_t)(30 * breath), (uint8_t)(100 * breath));
        }
    } else if (mode == PIANO_MODE_MAGIC) {
        /* Sparkling starlight idle */
        float phase = sinf((float)now_us * 0.000004f);
        int sparkle_led = ((now_us / 500000) % BOARD_WS2812_COUNT);
        led_strip_set_pixel(s_strip, sparkle_led, (uint8_t)(80 * fabsf(phase)), (uint8_t)(100 * fabsf(phase)), 20);
    } else if (mode == PIANO_MODE_AUTO) {
        /* Moving beacon */
        int beacon_led = ((now_us / 200000) % BOARD_WS2812_COUNT);
        led_strip_set_pixel(s_strip, beacon_led, 0, 150, 150);
    }

    led_strip_refresh(s_strip);
}
