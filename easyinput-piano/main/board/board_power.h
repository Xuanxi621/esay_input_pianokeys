#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_power_init(void);
esp_err_t board_power_enable_peripherals(bool enable);
bool board_power_peripherals_enabled(void);

#ifdef __cplusplus
}
#endif
