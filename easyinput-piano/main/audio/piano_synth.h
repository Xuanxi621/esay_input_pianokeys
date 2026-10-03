#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIANO_NOTE_C4   0  /* 1 (Do)  - 261.63 Hz */
#define PIANO_NOTE_D4   1  /* 2 (Re)  - 293.66 Hz */
#define PIANO_NOTE_E4   2  /* 3 (Mi)  - 329.63 Hz */
#define PIANO_NOTE_F4   3  /* 4 (Fa)  - 349.23 Hz */
#define PIANO_NOTE_G4   4  /* 5 (Sol) - 392.00 Hz */
#define PIANO_NOTE_A4   5  /* 6 (La)  - 440.00 Hz */
#define PIANO_NOTE_B4   6  /* 7 (Si)  - 493.88 Hz */
#define PIANO_NOTE_C5   7  /* i (Do5) - 523.25 Hz */
#define PIANO_TOTAL_NOTES 8

/* Note frequency array in Hz */
extern const float g_piano_freqs[PIANO_TOTAL_NOTES];
extern const char * const g_piano_note_names[PIANO_TOTAL_NOTES];

/**
 * Initialize I2S hardware peripheral and piano synth audio task.
 * Note: GPIO8 power rail must be enabled before calling this.
 */
esp_err_t piano_synth_init(void);

/**
 * Trigger note on.
 * @param note_idx Note index 0..7
 * @param velocity Note velocity 1..127 (controls strike loudness)
 */
void piano_synth_note_on(uint8_t note_idx, uint8_t velocity);

/**
 * Release note (damper pads string, fast decay).
 * @param note_idx Note index 0..7
 */
void piano_synth_note_off(uint8_t note_idx);

/**
 * Set master volume in percentage [0..100].
 */
void piano_synth_set_volume(uint8_t volume);

/**
 * Get current volume percentage.
 */
uint8_t piano_synth_get_volume(void);

/**
 * Silence all active voices immediately.
 */
void piano_synth_all_notes_off(void);

#ifdef __cplusplus
}
#endif
