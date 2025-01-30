#include "mock_platform.h"

// CMock will automatically generate the implementation of these functions
// based on the header file declarations. The generated code will be placed
// in a file named mock_platform.c by CMock during the build process.

// If you need any custom mock implementations, you can add them here:

void platform_i2c_write_ExpectWithArrayAndReturn(uint8_t addr, const uint8_t* data, size_t len, bool retval) {
    platform_i2c_write_ExpectAndReturn(addr, data, len, retval);
    platform_i2c_write_AddExpectedData(data, len);
}

void platform_i2c_read_ExpectWithArrayAndReturn(uint8_t addr, uint8_t* data, size_t len, bool retval) {
    platform_i2c_read_ExpectAndReturn(addr, data, len, retval);
    platform_i2c_read_ReturnArrayThruPtr_data(data, len);
}