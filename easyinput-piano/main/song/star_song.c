#include "star_song.h"

/*
 * Little Star score:
 * 1 1 5 5 | 6 6 5 - | 4 4 3 3 | 2 2 1 - |
 * 5 5 4 4 | 3 3 2 - | 5 5 4 4 | 3 3 2 - |
 * 1 1 5 5 | 6 6 5 - | 4 4 3 3 | 2 2 1 - |
 */
const song_note_t g_star_song[STAR_SONG_NOTE_COUNT] = {
    /* Phrase 1 */
    { 0, 450, "一", "1" },
    { 0, 450, "闪", "1" },
    { 4, 450, "一", "5" },
    { 4, 450, "闪", "5" },
    { 5, 450, "亮", "6" },
    { 5, 450, "晶", "6" },
    { 4, 900, "晶", "5" },

    /* Phrase 2 */
    { 3, 450, "满", "4" },
    { 3, 450, "天", "4" },
    { 2, 450, "都", "3" },
    { 2, 450, "是", "3" },
    { 1, 450, "小", "2" },
    { 1, 450, "星", "2" },
    { 0, 900, "星", "1" },

    /* Phrase 3 */
    { 4, 450, "挂", "5" },
    { 4, 450, "在", "5" },
    { 3, 450, "天", "4" },
    { 3, 450, "空", "4" },
    { 2, 450, "放", "3" },
    { 2, 450, "光", "3" },
    { 1, 900, "明", "2" },

    /* Phrase 4 */
    { 4, 450, "好", "5" },
    { 4, 450, "像", "5" },
    { 3, 450, "千", "4" },
    { 3, 450, "万", "4" },
    { 2, 450, "小", "3" },
    { 2, 450, "眼", "3" },
    { 1, 900, "睛", "2" },

    /* Phrase 5 */
    { 0, 450, "一", "1" },
    { 0, 450, "闪", "1" },
    { 4, 450, "一", "5" },
    { 4, 450, "闪", "5" },
    { 5, 450, "亮", "6" },
    { 5, 450, "晶", "6" },
    { 4, 900, "晶", "5" },

    /* Phrase 6 */
    { 3, 450, "满", "4" },
    { 3, 450, "天", "4" },
    { 2, 450, "都", "3" },
    { 2, 450, "是", "3" },
    { 1, 450, "小", "2" },
    { 1, 450, "星", "2" },
    { 0, 900, "星", "1" },
};

/*
 * Hardware map:
 * S1 -> 5 (G4, idx 4)
 * S2 -> 6 (A4, idx 5)
 * S3 -> 7 (B4, idx 6)
 * S4 -> i (C5, idx 7)
 * S5 -> 1 (C4, idx 0)
 * S6 -> 2 (D4, idx 1)
 * S7 -> 3 (E4, idx 2)
 * S8 -> 4 (F4, idx 3)
 */
static const uint8_t s_key_to_note_map[8] = { 4, 5, 6, 7, 0, 1, 2, 3 };
static const uint8_t s_note_to_key_map[8] = { 4, 5, 6, 7, 0, 1, 2, 3 };

static const char * const s_key_names[8] = {
    "S1", "S2", "S3", "S4",
    "S5", "S6", "S7", "S8",
};

uint8_t song_key_to_note(uint8_t key_idx)
{
    if (key_idx >= 8) return 0;
    return s_key_to_note_map[key_idx];
}

uint8_t song_note_to_key(uint8_t note_idx)
{
    if (note_idx >= 8) return 0;
    return s_note_to_key_map[note_idx];
}

const char *song_key_name(uint8_t key_idx)
{
    if (key_idx >= 8) return "??";
    return s_key_names[key_idx];
}
