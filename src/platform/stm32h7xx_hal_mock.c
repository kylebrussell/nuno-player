#include "nuno/stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

// Mock state variables
static struct {
    // GPIO state
    GPIO_PinState pin_states[16];  // Track state for 16 pins per port
    
    // I2C state
    HAL_StatusTypeDef i2c_init_return;
    HAL_StatusTypeDef i2c_transmit_return;
    HAL_StatusTypeDef i2c_receive_return;
    I2C_InitTypeDef i2c_init_params;
    struct {
        uint16_t DevAddress;
        uint8_t* pData;
        uint16_t Size;
    } i2c_transmit_params, i2c_receive_params;
    uint8_t i2c_receive_data[32];  // Buffer for mock received data
} mock_state = {
    .i2c_init_return = HAL_OK,
    .i2c_transmit_return = HAL_OK,
    .i2c_receive_return = HAL_OK
};

// Reset mock state
void HAL_Mock_Reset(void) {
    memset(&mock_state, 0, sizeof(mock_state));
    mock_state.i2c_init_return = HAL_OK;
    mock_state.i2c_transmit_return = HAL_OK;
    mock_state.i2c_receive_return = HAL_OK;
}

// HAL Core Functions
void HAL_Init(void) {
    printf("HAL_Init called\n");
}

void SystemClock_Config(void) {
    printf("SystemClock_Config called\n");
}

// GPIO Functions
void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init) {
    printf("HAL_GPIO_Init called for GPIOx: %p, Pin: %lu\n", GPIOx, GPIO_Init->Pin);
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState) {
    printf("HAL_GPIO_WritePin called for GPIOx: %p, Pin: %u, State: %d\n", 
           GPIOx, GPIO_Pin, PinState);
    
    // Store pin state
    uint8_t pin_index = 0;
    while (GPIO_Pin > 1) {
        GPIO_Pin >>= 1;
        pin_index++;
    }
    mock_state.pin_states[pin_index] = PinState;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    printf("HAL_GPIO_ReadPin called for GPIOx: %p, Pin: %u\n", GPIOx, GPIO_Pin);
    
    // Return stored pin state
    uint8_t pin_index = 0;
    while (GPIO_Pin > 1) {
        GPIO_Pin >>= 1;
        pin_index++;
    }
    return mock_state.pin_states[pin_index];
}

void HAL_Delay(uint32_t Delay) {
    printf("HAL_Delay called for %u ms\n", Delay);
}

// I2C Functions
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *hi2c) {
    printf("HAL_I2C_Init called\n");
    mock_state.i2c_init_params = hi2c->Init;
    return mock_state.i2c_init_return;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, 
                                         uint16_t DevAddress, 
                                         uint8_t *pData, 
                                         uint16_t Size, 
                                         uint32_t Timeout) {
    printf("HAL_I2C_Master_Transmit called - Address: 0x%X, Size: %u\n", 
           DevAddress, Size);
    
    mock_state.i2c_transmit_params.DevAddress = DevAddress;
    mock_state.i2c_transmit_params.pData = pData;
    mock_state.i2c_transmit_params.Size = Size;
    
    return mock_state.i2c_transmit_return;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c, 
                                        uint16_t DevAddress,
                                        uint8_t *pData, 
                                        uint16_t Size, 
                                        uint32_t Timeout) {
    printf("HAL_I2C_Master_Receive called - Address: 0x%X, Size: %u\n", 
           DevAddress, Size);
    
    mock_state.i2c_receive_params.DevAddress = DevAddress;
    mock_state.i2c_receive_params.pData = pData;
    mock_state.i2c_receive_params.Size = Size;
    
    // Copy mock data to output buffer
    memcpy(pData, mock_state.i2c_receive_data, Size);
    
    return mock_state.i2c_receive_return;
}

// Mock Control Functions
void HAL_Mock_SetI2CInitReturn(HAL_StatusTypeDef status) {
    mock_state.i2c_init_return = status;
}

void HAL_Mock_SetI2CTransmitReturn(HAL_StatusTypeDef status) {
    mock_state.i2c_transmit_return = status;
}

void HAL_Mock_SetI2CReceiveReturn(HAL_StatusTypeDef status) {
    mock_state.i2c_receive_return = status;
}

void HAL_Mock_SetI2CReceiveData(const uint8_t* data, size_t len) {
    if (len > sizeof(mock_state.i2c_receive_data)) {
        len = sizeof(mock_state.i2c_receive_data);
    }
    memcpy(mock_state.i2c_receive_data, data, len);
}

// Mock State Access Functions
const I2C_InitTypeDef* HAL_Mock_GetI2CInitParams(void) {
    return &mock_state.i2c_init_params;
}

void HAL_Mock_GetI2CTransmitParams(uint16_t* addr, uint8_t** data, uint16_t* size) {
    if (addr) *addr = mock_state.i2c_transmit_params.DevAddress;
    if (data) *data = mock_state.i2c_transmit_params.pData;
    if (size) *size = mock_state.i2c_transmit_params.Size;
}

void HAL_Mock_GetI2CReceiveParams(uint16_t* addr, uint8_t** data, uint16_t* size) {
    if (addr) *addr = mock_state.i2c_receive_params.DevAddress;
    if (data) *data = mock_state.i2c_receive_params.pData;
    if (size) *size = mock_state.i2c_receive_params.Size;
}