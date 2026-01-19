#ifndef NUNO_BOARD_CONFIG_H
#define NUNO_BOARD_CONFIG_H

#include "nuno/stm32h7xx_hal.h"

/*
 * Prototype target: STM32 NUCLEO-H743ZI2
 * - Trackpad + audio codec control share I2C1
 * - Audio data uses I2S2 (SPI2 in I2S mode)
 * - Trackpad click uses a single GPIO input
 */

/* Shared I2C bus (trackpad + codec control) */
#define NUNO_I2C_INSTANCE        I2C1
#define NUNO_I2C_SCL_GPIO_PORT   GPIOB
#define NUNO_I2C_SCL_PIN         GPIO_PIN_8
#define NUNO_I2C_SDA_GPIO_PORT   GPIOB
#define NUNO_I2C_SDA_PIN         GPIO_PIN_9

/* Trackpad module (Azoteq IQS550 family) */
#define NUNO_TRACKPAD_I2C_ADDR   0x74u /* 7-bit default address */
#define NUNO_TRACKPAD_INT_PORT   GPIOA
#define NUNO_TRACKPAD_INT_PIN    GPIO_PIN_1 /* optional */

/* Trackpad mechanical click switch */
#define NUNO_TRACKPAD_CLICK_PORT GPIOC
#define NUNO_TRACKPAD_CLICK_PIN  GPIO_PIN_13 /* pull-up, active low */

/* Audio output (WM8960 I2S codec module) */
#define NUNO_I2S_INSTANCE        SPI2
#define NUNO_I2S_WS_PORT         GPIOB
#define NUNO_I2S_WS_PIN          GPIO_PIN_12
#define NUNO_I2S_SCK_PORT        GPIOB
#define NUNO_I2S_SCK_PIN         GPIO_PIN_13
#define NUNO_I2S_SD_PORT         GPIOB
#define NUNO_I2S_SD_PIN          GPIO_PIN_15
#define NUNO_I2S_MCK_PORT        GPIOC
#define NUNO_I2S_MCK_PIN         GPIO_PIN_6

/* WM8960 I2C address (7-bit) */
#define NUNO_CODEC_I2C_ADDR      0x1Au

#endif /* NUNO_BOARD_CONFIG_H */
