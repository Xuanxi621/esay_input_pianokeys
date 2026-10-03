#include "board_power.h"
#include "board_pins.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board_power";
static bool s_peripherals_enabled = false;

esp_err_t board_power_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_GPIO_PWR_EN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "failed to configure GPIO%d", BOARD_GPIO_PWR_EN);
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_GPIO_PWR_EN, 0), TAG, "failed to set GPIO%d low", BOARD_GPIO_PWR_EN);
    s_peripherals_enabled = false;
    ESP_LOGI(TAG, "power rail ready; GPIO%d held low at boot", BOARD_GPIO_PWR_EN);
    return ESP_OK;
}

esp_err_t board_power_enable_peripherals(bool enable)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_GPIO_PWR_EN, enable ? 1 : 0), TAG,
                        "failed to toggle GPIO%d", BOARD_GPIO_PWR_EN);
    if (enable) {
        vTaskDelay(pdMS_TO_TICKS(BOARD_PWR_SETTLE_MS));
    }
    s_peripherals_enabled = enable;
    ESP_LOGI(TAG, "GPIO%d -> %s (settle %d ms)", BOARD_GPIO_PWR_EN,
             enable ? "ENABLED" : "DISABLED", enable ? BOARD_PWR_SETTLE_MS : 0);
    return ESP_OK;
}

bool board_power_peripherals_enabled(void)
{
    return s_peripherals_enabled;
}
