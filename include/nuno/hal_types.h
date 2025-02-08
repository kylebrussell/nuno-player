#ifndef HAL_TYPES_H
#define HAL_TYPES_H

#ifdef BUILD_SIM
// Mock types for simulator
typedef struct {
    void* Instance;
    uint32_t Init;
    uint32_t State;
} DMA_HandleTypeDef;

// Add other HAL types as needed
#else
// Include actual STM32 HAL headers for embedded build
#include "stm32h7xx_hal.h"
#endif

#endif // HAL_TYPES_H
