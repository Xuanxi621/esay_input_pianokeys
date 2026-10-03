#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Main keys S1–S8, active low (internal pull-up) */
#define BOARD_GPIO_S1                 2
#define BOARD_GPIO_S2                47
#define BOARD_GPIO_S3                38
#define BOARD_GPIO_S4                41
#define BOARD_GPIO_S5                 1
#define BOARD_GPIO_S6                 6
#define BOARD_GPIO_S7                 7
#define BOARD_GPIO_S8                48

/* Encoder: A/B quadrature + press (S9), press active low */
#define BOARD_GPIO_ENC_A             17
#define BOARD_GPIO_ENC_B             16
#define BOARD_GPIO_ENC_PRESS         18

#define BOARD_GPIO_KEY_WAKE          21

/* Shared peripheral rail for WS2812 / SPK (MAX98357A), active high */
#define BOARD_GPIO_PWR_EN             8

/* 5x WS2812 on one data line */
#define BOARD_GPIO_LED_DIN           12
#define BOARD_WS2812_COUNT            5

#define BOARD_GPIO_STATUS_LED        42

/* Speaker: MAX98357A I2S DAC */
#define BOARD_GPIO_SPK_BCLK          14
#define BOARD_GPIO_SPK_WS            13
#define BOARD_GPIO_SPK_DOUT          15

#define BOARD_PWR_SETTLE_MS          50

#ifdef __cplusplus
}
#endif
