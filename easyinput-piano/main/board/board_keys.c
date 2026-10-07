#include "board_keys.h"
#include "board_pins.h"

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "board_keys";

static const int s_key_gpios[8] = {
    BOARD_GPIO_S1, BOARD_GPIO_S2, BOARD_GPIO_S3, BOARD_GPIO_S4,
    BOARD_GPIO_S5, BOARD_GPIO_S6, BOARD_GPIO_S7, BOARD_GPIO_S8,
};

static pcnt_unit_handle_t s_encoder_unit;
static int s_encoder_consumed_count;

static bool s_keys_last[8] = {false};
static bool s_enc_last = false;

#define PRESS_DEBOUNCE_US 25000
#define ENCODER_COUNTS_PER_DETENT 4

static esp_err_t encoder_pcnt_init(void)
{
    const pcnt_unit_config_t unit_config = {
        .high_limit = 32767,
        .low_limit = -32768,
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_config, &s_encoder_unit), TAG, "pcnt unit");

    const pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_encoder_unit, &filter_config), TAG, "pcnt filter");

    const pcnt_chan_config_t channel_a_config = {
        .edge_gpio_num = BOARD_GPIO_ENC_A,
        .level_gpio_num = BOARD_GPIO_ENC_B,
    };
    const pcnt_chan_config_t channel_b_config = {
        .edge_gpio_num = BOARD_GPIO_ENC_B,
        .level_gpio_num = BOARD_GPIO_ENC_A,
    };
    pcnt_channel_handle_t channel_a = NULL;
    pcnt_channel_handle_t channel_b = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_encoder_unit, &channel_a_config, &channel_a), TAG, "pcnt channel A");
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_encoder_unit, &channel_b_config, &channel_b), TAG, "pcnt channel B");

    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_edge_action(channel_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                     PCNT_CHANNEL_EDGE_ACTION_INCREASE),
        TAG, "pcnt A edge");
    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_level_action(channel_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
        TAG, "pcnt A level");
    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_edge_action(channel_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                     PCNT_CHANNEL_EDGE_ACTION_DECREASE),
        TAG, "pcnt B edge");
    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_level_action(channel_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
        TAG, "pcnt B level");

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_encoder_unit), TAG, "pcnt enable");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_encoder_unit), TAG, "pcnt clear");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(s_encoder_unit), TAG, "pcnt start");
    s_encoder_consumed_count = 0;
    return ESP_OK;
}

esp_err_t board_keys_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < 8; ++i) {
        mask |= 1ULL << s_key_gpios[i];
    }
    mask |= 1ULL << BOARD_GPIO_ENC_A;
    mask |= 1ULL << BOARD_GPIO_ENC_B;
    mask |= 1ULL << BOARD_GPIO_ENC_PRESS;

    gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config");
    ESP_RETURN_ON_ERROR(encoder_pcnt_init(), TAG, "encoder pcnt init");
    ESP_LOGI(TAG, "keys S1..S8 and encoder ready");
    return ESP_OK;
}

static int64_t s_key_debounce_us[8] = {0};
static int64_t s_enc_debounce_us = 0;

esp_err_t board_keys_poll(board_input_snapshot_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    const int64_t now_us = esp_timer_get_time();

    /* 8 physical keys: active low with 25ms software debounce filter */
    for (int i = 0; i < 8; ++i) {
        bool raw = (gpio_get_level(s_key_gpios[i]) == 0);
        out->pressed[i] = false;
        out->released[i] = false;

        if (raw != s_keys_last[i]) {
            if (now_us - s_key_debounce_us[i] >= 25000) {
                s_key_debounce_us[i] = now_us;
                s_keys_last[i] = raw;
                out->pressed[i] = raw;
                out->released[i] = !raw;
            }
        }
        out->s[i] = s_keys_last[i];
    }

    /* Encoder press (S9): active low with debounce */
    bool raw_enc = (gpio_get_level(BOARD_GPIO_ENC_PRESS) == 0);
    out->enc_just_pressed = false;
    if (raw_enc != s_enc_last) {
        if (now_us - s_enc_debounce_us >= 25000) {
            s_enc_debounce_us = now_us;
            s_enc_last = raw_enc;
            out->enc_just_pressed = raw_enc;
        }
    }
    out->enc_press = s_enc_last;

    /* Rotary delta from hardware PCNT */
    int raw_count = 0;
    ESP_RETURN_ON_ERROR(pcnt_unit_get_count(s_encoder_unit, &raw_count), TAG, "pcnt read");
    int diff = raw_count - s_encoder_consumed_count;
    int steps = diff / ENCODER_COUNTS_PER_DETENT;
    if (steps != 0) {
        s_encoder_consumed_count += steps * ENCODER_COUNTS_PER_DETENT;
        out->enc_delta = (int8_t)steps;
    } else {
        out->enc_delta = 0;
    }

    return ESP_OK;
}
