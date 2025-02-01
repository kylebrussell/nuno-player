#include "nuno/platform.h"
#include "nuno/stm32h7xx_hal.h"

static I2C_HandleTypeDef hi2c1;

bool platform_i2c_init(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x20B0CCFF;  // 100kHz at PCLK 54MHz
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        return false;
    }

    return true;
}

bool platform_i2c_write(uint8_t addr, const uint8_t* data, size_t len) {
    return HAL_I2C_Master_Transmit(&hi2c1, addr << 1, (uint8_t*)data, len, 100) == HAL_OK;
}

bool platform_i2c_read(uint8_t addr, uint8_t* data, size_t len) {
    return HAL_I2C_Master_Receive(&hi2c1, addr << 1, data, len, 100) == HAL_OK;
}
