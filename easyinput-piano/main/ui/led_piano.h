#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIANO_MODE_FREE = 0,     /* Free playing (chromatic/diatonic keyboard) */
    PIANO_MODE_GUIDE,        /* Guide mode: practice Little Star note by note */
    PIANO_MODE_MAGIC,        /* One-key play: any button plays next note */
    PIANO_MODE_AUTO,         /* Auto demo playback */
    PIANO_MODE_MAX,
} piano_play_mode_t;

esp_err_t led_piano_init(void);

/**
 * Visual feedback when a note is pressed.
 * @param note_idx 0..7
 */
void led_piano_on_note_hit(uint8_t note_idx);

/**
 * Visual guide for target note in Guide Mode.
 * @param target_note 0..7
 */
void led_piano_show_guide(uint8_t target_note);

/**
 * Show volume bar on 5 LEDs temporarily.
 * @param volume 0..100
 */
void led_piano_show_volume(uint8_t volume);

/**
 * Show victory rainbow effect upon finishing song.
 */
void led_piano_show_victory(void);

/**
 * Visual error hint when wrong key is pressed in game.
 */
void led_piano_on_miss(void);

/**
 * Update tick for animations (call periodically in loop).
 */
void led_piano_update(int64_t now_us, piano_play_mode_t mode);

/**
 * Set LED brightness percentage (0..100, default is soft 8%).
 */
void led_piano_set_brightness(uint8_t percent);

/**
 * Toggle LEDs ON/OFF (silent mute mode).
 */
void led_piano_toggle_enable(void);

/**
 * Check if LEDs are enabled.
 */
bool led_piano_is_enabled(void);

#ifdef __cplusplus
}
#endif
