#ifndef GPIO_H
#define GPIO_H

#include "stm32h7xx_hal.h"

// DAC Control Pins
#define DAC_CS_PIN           GPIO_PIN_4
#define DAC_CS_GPIO_PORT     GPIOA
#define DAC_RESET_PIN        GPIO_PIN_5
#define DAC_RESET_GPIO_PORT  GPIOA

// Click Wheel Pin
#define CLICK_WHEEL_PIN      GPIO_PIN_0
#define CLICK_WHEEL_GPIO_PORT GPIOB

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize GPIOs for DAC control and Click Wheel input
 */
void GPIO_Init(void);

/**
 * @brief Configure DAC control lines
 */
void DAC_ControlLines_Config(void);

/**
 * @brief Reset the DAC module
 */
void DAC_Reset(void);

/**
 * @brief EXTI line interrupt handler for Click Wheel
 */
void EXTI0_IRQHandler(void);

/**
 * @brief Callback function executed on Click Wheel interrupt
 * @param GPIO_Pin The pin number that triggered the interrupt
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/**
 * @brief Process Click Wheel input events
 */
void Process_ClickWheel_Input(void);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */