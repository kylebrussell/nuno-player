#include <unity.h>
#include "nuno/es9038q2m.h"
#include "mock_platform.h"

static ES9038Q2M_Config test_config;

void setUp(void) {
    // Initialize default test configuration
    test_config.volume_left = 200;
    test_config.volume_right = 200;
    test_config.filter_type = ES9038Q2M_FILTER_FAST_ROLL_OFF;
    test_config.dsd_mode = false;
    test_config.sample_rate = 44100;
    test_config.bit_depth = 16;
}

void tearDown(void) {
    // Clean up after each test
}

// Initialization Tests
void test_ES9038Q2M_Init_Success(void) {
    // Expect system reset write
    uint8_t reset_data = ES9038Q2M_SYSTEM_RESET;
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, &reset_data, 1, true);
    
    // Expect initial configuration writes
    uint8_t config_data[] = {
        ES9038Q2M_REG_SYSTEM_SETTINGS, 0x00,  // Clear reset bit
        ES9038Q2M_REG_VOLUME_1, 200,         // Left volume
        ES9038Q2M_REG_VOLUME_2, 200,         // Right volume
        ES9038Q2M_REG_FILTER_SETTINGS, ES9038Q2M_FILTER_FAST_ROLL_OFF
    };
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, config_data, sizeof(config_data), true);

    TEST_ASSERT_TRUE(ES9038Q2M_Init(&test_config));
}

void test_ES9038Q2M_Init_Fail_Reset(void) {
    uint8_t reset_data = ES9038Q2M_SYSTEM_RESET;
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, &reset_data, 1, false);

    TEST_ASSERT_FALSE(ES9038Q2M_Init(&test_config));
}

// Configuration Tests
void test_ES9038Q2M_SetProfile_Success(void) {
    uint8_t profile_data[] = {
        ES9038Q2M_REG_GENERAL_CONFIG, 
        ES9038Q2M_PROFILE_HIGH_PERFORMANCE
    };
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, profile_data, sizeof(profile_data), true);

    TEST_ASSERT_TRUE(ES9038Q2M_SetProfile(ES9038Q2M_PROFILE_HIGH_PERFORMANCE));
}

void test_ES9038Q2M_SetFilter_Success(void) {
    uint8_t filter_data[] = {
        ES9038Q2M_REG_FILTER_SETTINGS,
        ES9038Q2M_FILTER_MINIMUM_PHASE
    };
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, filter_data, sizeof(filter_data), true);

    TEST_ASSERT_TRUE(ES9038Q2M_SetFilter(ES9038Q2M_FILTER_MINIMUM_PHASE));
}

void test_ES9038Q2M_SetDSDMode_Success(void) {
    uint8_t dsd_data[] = {
        ES9038Q2M_REG_SYSTEM_SETTINGS,
        ES9038Q2M_DSD_MODE
    };
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, dsd_data, sizeof(dsd_data), true);

    TEST_ASSERT_TRUE(ES9038Q2M_SetDSDMode(true));
}

// Volume Control Tests
void test_ES9038Q2M_SetVolume_Success(void) {
    uint8_t volume_data[] = {
        ES9038Q2M_REG_VOLUME_1, 128,  // Left volume
        ES9038Q2M_REG_VOLUME_2, 128   // Right volume
    };
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, volume_data, sizeof(volume_data), true);

    TEST_ASSERT_TRUE(ES9038Q2M_SetVolume(128, 128));
}

void test_ES9038Q2M_SetVolume_Fail(void) {
    uint8_t volume_data[] = {
        ES9038Q2M_REG_VOLUME_1, 128,
        ES9038Q2M_REG_VOLUME_2, 128
    };
    platform_i2c_write_ExpectAndReturn(ES9038Q2M_I2C_ADDR, volume_data, sizeof(volume_data), false);

    TEST_ASSERT_FALSE(ES9038Q2M_SetVolume(128, 128));
}

int main(void) {
    UNITY_BEGIN();
    
    // Initialization Tests
    RUN_TEST(test_ES9038Q2M_Init_Success);
    RUN_TEST(test_ES9038Q2M_Init_Fail_Reset);
    
    // Configuration Tests
    RUN_TEST(test_ES9038Q2M_SetProfile_Success);
    RUN_TEST(test_ES9038Q2M_SetFilter_Success);
    RUN_TEST(test_ES9038Q2M_SetDSDMode_Success);
    
    // Volume Control Tests
    RUN_TEST(test_ES9038Q2M_SetVolume_Success);
    RUN_TEST(test_ES9038Q2M_SetVolume_Fail);
    
    return UNITY_END();
}
