#include <unity.h>
#include "nuno/platform.h"
#include "nuno/stm32h7xx_hal.h"

// Test fixture setup and teardown
void setUp(void) {
    // Reset any mock state before each test
}

void tearDown(void) {
    // Clean up after each test
}

// Test I2C initialization
void test_i2c_init_success(void) {
    // Arrange
    // Mock HAL_I2C_Init to return HAL_OK
    mock_hal_i2c_init_return = HAL_OK;

    // Act
    bool result = platform_i2c_init();

    // Assert
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(I2C1, mock_hal_i2c_init_params.Instance);
    TEST_ASSERT_EQUAL(I2C_ADDRESSINGMODE_7BIT, mock_hal_i2c_init_params.Init.AddressingMode);
    TEST_ASSERT_EQUAL(0x20B0CCFF, mock_hal_i2c_init_params.Init.Timing);
}

void test_i2c_init_failure(void) {
    // Arrange
    mock_hal_i2c_init_return = HAL_ERROR;

    // Act
    bool result = platform_i2c_init();

    // Assert
    TEST_ASSERT_FALSE(result);
}

void test_i2c_write_success(void) {
    // Arrange
    const uint8_t addr = 0x48;  // Example device address
    const uint8_t data[] = {0x01, 0x02, 0x03};
    mock_hal_i2c_transmit_return = HAL_OK;

    // Act
    bool result = platform_i2c_write(addr, data, sizeof(data));

    // Assert
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(addr << 1, mock_hal_i2c_transmit_params.DevAddress);
    TEST_ASSERT_EQUAL_MEMORY(data, mock_hal_i2c_transmit_params.pData, sizeof(data));
    TEST_ASSERT_EQUAL(sizeof(data), mock_hal_i2c_transmit_params.Size);
}

void test_i2c_write_failure(void) {
    // Arrange
    const uint8_t addr = 0x48;
    const uint8_t data[] = {0x01};
    mock_hal_i2c_transmit_return = HAL_ERROR;

    // Act
    bool result = platform_i2c_write(addr, data, sizeof(data));

    // Assert
    TEST_ASSERT_FALSE(result);
}

void test_i2c_read_success(void) {
    // Arrange
    const uint8_t addr = 0x48;
    uint8_t data[3];
    mock_hal_i2c_receive_return = HAL_OK;
    mock_hal_i2c_receive_data[0] = 0x0A;
    mock_hal_i2c_receive_data[1] = 0x0B;
    mock_hal_i2c_receive_data[2] = 0x0C;

    // Act
    bool result = platform_i2c_read(addr, data, sizeof(data));

    // Assert
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(addr << 1, mock_hal_i2c_receive_params.DevAddress);
    TEST_ASSERT_EQUAL_MEMORY(mock_hal_i2c_receive_data, data, sizeof(data));
    TEST_ASSERT_EQUAL(sizeof(data), mock_hal_i2c_receive_params.Size);
}

void test_i2c_read_failure(void) {
    // Arrange
    const uint8_t addr = 0x48;
    uint8_t data[1];
    mock_hal_i2c_receive_return = HAL_ERROR;

    // Act
    bool result = platform_i2c_read(addr, data, sizeof(data));

    // Assert
    TEST_ASSERT_FALSE(result);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_i2c_init_success);
    RUN_TEST(test_i2c_init_failure);
    RUN_TEST(test_i2c_write_success);
    RUN_TEST(test_i2c_write_failure);
    RUN_TEST(test_i2c_read_success);
    RUN_TEST(test_i2c_read_failure);
    
    return UNITY_END();
}