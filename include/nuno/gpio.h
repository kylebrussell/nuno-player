#ifndef GPIO_H
#define GPIO_H

#include "stm32h7xx_hal.h"

// DAC Control Pins (preserved from original)
#define DAC_CS_PIN           GPIO_PIN_4
#define DAC_CS_GPIO_PORT     GPIOA
#define DAC_RESET_PIN        GPIO_PIN_5
#define DAC_RESET_GPIO_PORT  GPIOA

// Click Wheel Pins (added)
#define CLICK_WHEEL_CLOCK_PIN     GPIO_PIN_0
#define CLICK_WHEEL_DATA_PIN      GPIO_PIN_1
#define CLICK_WHEEL_GPIO_PORT     GPIOB

// Click Wheel Button Bits
#define CENTER_BUTTON_BIT  7
#define LEFT_BUTTON_BIT    9
#define RIGHT_BUTTON_BIT   8
#define UP_BUTTON_BIT      11
#define DOWN_BUTTON_BIT    10
#define WHEEL_TOUCH_BIT    29

#ifdef __cplusplus
extern "C" {
#endif

// Existing DAC function declarations
void GPIO_Init(void);
void DAC_ControlLines_Config(void);
void DAC_Reset(void);

// Click Wheel function declarations (added)
void Process_ClickWheel_Data(uint32_t data);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */