#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t note_idx;    /* 0..7 (C4..C5) */
    uint16_t duration_ms;/* default duration in ms */
    const char *lyric;   /* Chinese lyric character */
    const char *note_str;/* e.g. "1", "5", etc. */
} song_note_t;

/** Total count of notes in Little Star melody */
#define STAR_SONG_NOTE_COUNT 42

extern const song_note_t g_star_song[STAR_SONG_NOTE_COUNT];

/**
 * Maps hardware key index (0..7 for S1..S8) to musical note index (0..7 for C4..C5).
 * Layout:
 *   Upper row: S1(5), S2(6), S3(7), S4(i)
 *   Lower row: S5(1), S6(2), S7(3), S8(4)
 */
uint8_t song_key_to_note(uint8_t key_idx);

/**
 * Maps musical note index (0..7) to hardware key index (0..7 for S1..S8).
 */
uint8_t song_note_to_key(uint8_t note_idx);

/**
 * Returns the hardware key name string, e.g. "S5" for 1(Do).
 */
const char *song_key_name(uint8_t key_idx);

#ifdef __cplusplus
}
#endif
