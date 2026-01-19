#ifndef GPIO_H
#define GPIO_H

#include "stm32h7xx_hal.h"
#include "nuno/board_config.h"

// DAC Control Pins (preserved from original)
#define DAC_CS_PIN           GPIO_PIN_4
#define DAC_CS_GPIO_PORT     GPIOA
#define DAC_RESET_PIN        GPIO_PIN_5
#define DAC_RESET_GPIO_PORT  GPIOA

#ifdef __cplusplus
extern "C" {
#endif

// Existing DAC function declarations
void GPIO_Init(void);
void DAC_ControlLines_Config(void);
void DAC_Reset(void);

// Trackpad click input is configured in GPIO_Init()

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */