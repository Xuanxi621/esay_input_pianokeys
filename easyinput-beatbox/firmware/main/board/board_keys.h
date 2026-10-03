#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool s[8];
    bool enc_press;
    int8_t enc_delta; /* +1 / -1 steps since last poll */
} board_input_snapshot_t;

esp_err_t board_keys_init(void);
esp_err_t board_keys_poll(board_input_snapshot_t *out);

#ifdef __cplusplus
}
#endif
