#ifndef MOCK_PLATFORM_H
#define MOCK_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "unity.h"
#include "cmock.h"

// I2C Functions to mock
bool platform_i2c_init(void);
bool platform_i2c_write(uint8_t addr, const uint8_t* data, size_t len);
bool platform_i2c_read(uint8_t addr, uint8_t* data, size_t len);

// Mock function declarations
void platform_i2c_init_ExpectAndReturn(bool retval);
void platform_i2c_write_ExpectAndReturn(uint8_t addr, const uint8_t* data, size_t len, bool retval);
void platform_i2c_read_ExpectAndReturn(uint8_t addr, uint8_t* data, size_t len, bool retval);

// Optional: Add parameter checking for more detailed test verification
void platform_i2c_write_ExpectWithArrayAndReturn(uint8_t addr, const uint8_t* data, size_t len, bool retval);
void platform_i2c_read_ExpectWithArrayAndReturn(uint8_t addr, uint8_t* data, size_t len, bool retval);

#endif /* MOCK_PLATFORM_H */