#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool s[8];             /* Level: true = pressed (down) */
    bool pressed[8];       /* Rising edge: just pressed down this tick */
    bool released[8];      /* Falling edge: just released this tick */
    bool enc_press;        /* Encoder button pressed level */
    bool enc_just_pressed; /* Encoder button edge: just pressed */
    int8_t enc_delta;      /* Rotary encoder steps (+1 clockwise, -1 counter-clockwise) */
} board_input_snapshot_t;

esp_err_t board_keys_init(void);
esp_err_t board_keys_poll(board_input_snapshot_t *out);

#ifdef __cplusplus
}
#endif
