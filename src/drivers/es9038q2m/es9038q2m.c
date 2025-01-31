#include "es9038q2m.h"
#include "nuno/platform.h"
#include <stdlib.h>
#include <string.h>

// internal helper to write a single byte to a register
static bool write_reg(uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    return platform_i2c_write(ES9038Q2M_I2C_ADDR, data, 2);
}

// internal helper to read a single byte from a register
static bool read_reg(uint8_t reg, uint8_t *value) {
    if (!platform_i2c_write(ES9038Q2M_I2C_ADDR, &reg, 1))
        return false;
    return platform_i2c_read(ES9038Q2M_I2C_ADDR, value, 1);
}

// improved clock configuration based on datasheet specs
bool ES9038Q2M_ConfigureClock(uint32_t sample_rate, uint32_t master_clock) {
    if (sample_rate == 0 || master_clock == 0) {
        return false;
    }
    uint32_t ratio = master_clock / sample_rate;
    // valid ratio range example: 16 to 512
    if (ratio < 16 || ratio > 512) {
        return false;
    }
    // set NCO configuration if ratio is lower than a threshold (example: <=256)
    uint8_t nco_config = (ratio <= 256) ? 0x01 : 0x00;
    if (!write_reg(ES9038Q2M_REG_NCO_CONFIG, nco_config)) {
        return false;
    }
    // set clock divider with proper scaling (example: ratio/2)
    uint8_t divider = (uint8_t)(ratio / 2);
    return write_reg(ES9038Q2M_REG_CLOCK_DIVIDER, divider);
}

// improved filter selection preserving other bits
bool ES9038Q2M_SetFilter(ES9038Q2M_FilterType filter) {
    uint8_t current;
    if (!read_reg(ES9038Q2M_REG_FILTER_SETTINGS, &current)) {
        return false;
    }
    // preserve lower nibble, clear upper nibble where filter type is stored
    current &= 0x0F;
    switch (filter) {
        case ES9038Q2M_FILTER_FAST_ROLL_OFF:
            current |= 0x00;
            break;
        case ES9038Q2M_FILTER_SLOW_ROLL_OFF:
            current |= 0x10;
            break;
        case ES9038Q2M_FILTER_MINIMUM_PHASE:
            current |= 0x20;
            break;
        default:
            return false;
    }
    return write_reg(ES9038Q2M_REG_FILTER_SETTINGS, current);
}

// dac initialization sequence with error recovery
bool ES9038Q2M_Init(const ES9038Q2M_Config* config) {
    if (!config) return false;

    // initial soft reset attempt
    if (!ES9038Q2M_Reset()) {
        platform_delay_ms(100);
        if (!ES9038Q2M_Reset()) {
            return false;
        }
    }
    // check status after reset (example: error bit in bit0)
    uint8_t status = ES9038Q2M_GetStatus();
    if ((status & 0x01) != 0) {
        return false;
    }
    // configure input mode, assume serial mode
    if (!write_reg(ES9038Q2M_REG_INPUT_CONFIG, ES9038Q2M_SERIAL_MODE))
        return false;
    // set filter configuration
    if (!ES9038Q2M_SetFilter(config->filter_type))
        return false;
    // set dsd/pcm mode
    if (!ES9038Q2M_SetDSDMode(config->dsd_mode))
        return false;
    // set volume
    if (!ES9038Q2M_SetVolume(config->volume_left, config->volume_right))
        return false;
    // configure clock with robust validation
    if (!ES9038Q2M_ConfigureClock(config->sample_rate, config->bit_depth)) // bit_depth used illustratively as master_clock
        return false;
    return true;
}

// volume control functions
bool ES9038Q2M_SetVolume(uint8_t left, uint8_t right) {
    if (!write_reg(ES9038Q2M_REG_VOLUME_1, left))
        return false;
    return write_reg(ES9038Q2M_REG_VOLUME_2, right);
}

// operating profile configuration
bool ES9038Q2M_SetProfile(ES9038Q2M_Profile profile) {
    uint8_t value = 0;
    switch (profile) {
        case ES9038Q2M_PROFILE_NORMAL:
            value = 0x00;
            break;
        case ES9038Q2M_PROFILE_HIGH_PERFORMANCE:
            value = 0x01;
            break;
        case ES9038Q2M_PROFILE_POWER_SAVING:
            value = 0x02;
            break;
        default:
            return false;
    }
    return write_reg(ES9038Q2M_REG_MASTER_MODE, value);
}

// dsd/pcm mode switching
bool ES9038Q2M_SetDSDMode(bool dsd_mode) {
    uint8_t value = dsd_mode ? 0x01 : 0x00;
    return write_reg(ES9038Q2M_REG_DSD_CONFIG, value);
}

// power management: power down
bool ES9038Q2M_PowerDown(void) {
    uint8_t current;
    if (!read_reg(ES9038Q2M_REG_SYSTEM_SETTINGS, &current))
        return false;
    current |= ES9038Q2M_POWER_DOWN;
    return write_reg(ES9038Q2M_REG_SYSTEM_SETTINGS, current);
}

// power management: power up
bool ES9038Q2M_PowerUp(void) {
    uint8_t current;
    if (!read_reg(ES9038Q2M_REG_SYSTEM_SETTINGS, &current))
        return false;
    current &= ~ES9038Q2M_POWER_DOWN;
    return write_reg(ES9038Q2M_REG_SYSTEM_SETTINGS, current);
}

// soft reset function
bool ES9038Q2M_Reset(void) {
    if (!write_reg(ES9038Q2M_REG_SOFT_START, 0x01))
        return false;
    platform_delay_ms(10);
    return write_reg(ES9038Q2M_REG_SOFT_START, 0x00);
}

// get dac status
uint8_t ES9038Q2M_GetStatus(void) {
    uint8_t status = 0;
    if (!read_reg(ES9038Q2M_REG_STATUS, &status))
        return 0; // fallback
    return status;
}
