#include "gpio.h"

void GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIO Ports Clock
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure DAC Chip Select Pin (preserved from original)
    GPIO_InitStruct.Pin = DAC_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DAC_CS_GPIO_PORT, &GPIO_InitStruct);

    // Configure DAC Reset Pin (preserved from original)
    GPIO_InitStruct.Pin = DAC_RESET_PIN;
    HAL_GPIO_Init(DAC_RESET_GPIO_PORT, &GPIO_InitStruct);

    // Set DAC Pins to High by default (preserved from original)
    HAL_GPIO_WritePin(DAC_CS_GPIO_PORT, DAC_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC_RESET_GPIO_PORT, DAC_RESET_PIN, GPIO_PIN_SET);

    // Configure trackpad click switch input
    GPIO_InitStruct.Pin = NUNO_TRACKPAD_CLICK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(NUNO_TRACKPAD_CLICK_PORT, &GPIO_InitStruct);

    // Optional trackpad interrupt pin (not used yet)
    GPIO_InitStruct.Pin = NUNO_TRACKPAD_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(NUNO_TRACKPAD_INT_PORT, &GPIO_InitStruct);
}

// Preserved from original
void DAC_ControlLines_Config(void) {
    // Assuming GPIOs are already initialized in GPIO_Init
    // Additional DAC-specific configurations can be added here
}

// Preserved from original
void DAC_Reset(void) {
    HAL_GPIO_WritePin(DAC_RESET_GPIO_PORT, DAC_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(1); // Brief delay for reset pulse
    HAL_GPIO_WritePin(DAC_RESET_GPIO_PORT, DAC_RESET_PIN, GPIO_PIN_SET);
}
