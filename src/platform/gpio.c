#include "gpio.h"
#include "nuno/stm32h7xx_hal.h"

// DAC Control Lines Configuration
#define DAC_CS_PIN          GPIO_PIN_4
#define DAC_CS_GPIO_PORT    GPIOA
#define DAC_RESET_PIN       GPIO_PIN_5
#define DAC_RESET_GPIO_PORT  GPIOA

// Click Wheel Configuration
#define CLICK_WHEEL_PIN     GPIO_PIN_0
#define CLICK_WHEEL_GPIO_PORT GPIOB

/**
 * @brief Initialize GPIOs for DAC control and Click Wheel input
 */
void GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIO Ports Clock
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure DAC Chip Select Pin
    GPIO_InitStruct.Pin = DAC_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DAC_CS_GPIO_PORT, &GPIO_InitStruct);

    // Configure DAC Reset Pin
    GPIO_InitStruct.Pin = DAC_RESET_PIN;
    HAL_GPIO_Init(DAC_RESET_GPIO_PORT, &GPIO_InitStruct);

    // Set DAC Pins to High by default
    HAL_GPIO_WritePin(DAC_CS_GPIO_PORT, DAC_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC_RESET_GPIO_PORT, DAC_RESET_PIN, GPIO_PIN_SET);

    // Configure Click Wheel Pin as Input with Interrupt
    GPIO_InitStruct.Pin = CLICK_WHEEL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CLICK_WHEEL_GPIO_PORT, &GPIO_InitStruct);

    // Enable and set Click Wheel EXTI Interrupt to the lowest priority
    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/**
 * @brief Configure DAC control lines
 */
void DAC_ControlLines_Config(void) {
    // Assuming GPIOs are already initialized in GPIO_Init
    // Additional DAC-specific configurations can be added here
}

/**
 * @brief Reset the DAC module
 */
void DAC_Reset(void) {
    // Pull Reset pin low
    HAL_GPIO_WritePin(DAC_RESET_GPIO_PORT, DAC_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(10); // Delay for reset duration
    // Pull Reset pin high
    HAL_GPIO_WritePin(DAC_RESET_GPIO_PORT, DAC_RESET_PIN, GPIO_PIN_SET);
}

/**
 * @brief EXTI line interrupt handler for Click Wheel
 */
void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(CLICK_WHEEL_PIN);
}

/**
 * @brief Callback function executed on Click Wheel interrupt
 * @param GPIO_Pin The pin number that triggered the interrupt
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if(GPIO_Pin == CLICK_WHEEL_PIN) {
        // Handle Click Wheel input
        // For example, read the rotation direction or button press
        Process_ClickWheel_Input();
    }
}

/**
 * @brief Process Click Wheel input events
 */
void Process_ClickWheel_Input(void) {
    // Implement debouncing if necessary
    // Read the state of the Click Wheel and perform actions
}