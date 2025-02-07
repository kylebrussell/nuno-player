#include "gpio.h"
#include "nuno/ui_state.h"

// Click Wheel state variables
static uint32_t click_wheel_bits = 0;
static uint8_t bit_index = 0;
static uint8_t data_bit = 1;
static uint8_t last_position = 255;
static const uint32_t PACKET_START = 0b01101;

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

    // Configure Click Wheel Clock Pin (added)
    GPIO_InitStruct.Pin = CLICK_WHEEL_CLOCK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CLICK_WHEEL_GPIO_PORT, &GPIO_InitStruct);

    // Configure Click Wheel Data Pin (added)
    GPIO_InitStruct.Pin = CLICK_WHEEL_DATA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CLICK_WHEEL_GPIO_PORT, &GPIO_InitStruct);

    // Enable Click Wheel interrupt
    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
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

// Click Wheel functions (added)
void Process_ClickWheel_Data(uint32_t data) {
    if ((data & PACKET_START) != PACKET_START) {
        return;
    }

    // Get wheel position
    uint8_t wheel_position = (data >> 16) & 0xFF;
    
    // Process button states
    if ((data >> CENTER_BUTTON_BIT) & 1) {
        handleButtonPress(&uiState, BUTTON_CENTER, HAL_GetTick());
    }
    if ((data >> LEFT_BUTTON_BIT) & 1) {
        handleButtonPress(&uiState, BUTTON_MENU, HAL_GetTick());
    }
    if ((data >> RIGHT_BUTTON_BIT) & 1) {
        handleButtonPress(&uiState, BUTTON_PLAY, HAL_GetTick());
    }
    
    // Process rotation
    if (wheel_position != last_position) {
        int8_t direction = ((wheel_position - last_position + 128) % 256) - 128;
        handleRotation(&uiState, direction, HAL_GetTick());
        last_position = wheel_position;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == CLICK_WHEEL_CLOCK_PIN) {
        // Sample data pin
        data_bit = HAL_GPIO_ReadPin(CLICK_WHEEL_GPIO_PORT, CLICK_WHEEL_DATA_PIN);
        
        // Update bits
        if (data_bit) {
            click_wheel_bits |= (1 << bit_index);
        } else {
            click_wheel_bits &= ~(1 << bit_index);
        }
        
        if (++bit_index == 32) {
            Process_ClickWheel_Data(click_wheel_bits);
            bit_index = 0;
            click_wheel_bits = 0;
        }
    }
}