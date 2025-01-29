#ifndef ES9038Q2M_H
#define ES9038Q2M_H

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"

// I2C Device Address (7-bit)
#define ES9038Q2M_I2C_ADDR           0x48

// Register Map
#define ES9038Q2M_REG_SYSTEM_SETTINGS    0x00
#define ES9038Q2M_REG_INPUT_CONFIG       0x01
#define ES9038Q2M_REG_MIXING_CONFIG      0x02
#define ES9038Q2M_REG_CLOCK_DIVIDER      0x03
#define ES9038Q2M_REG_NCO_CONFIG         0x04
#define ES9038Q2M_REG_VOLUME_1           0x05
#define ES9038Q2M_REG_VOLUME_2           0x06
#define ES9038Q2M_REG_GENERAL_CONFIG     0x07
#define ES9038Q2M_REG_GPIO_CONFIG        0x08
#define ES9038Q2M_REG_MASTER_MODE        0x09
#define ES9038Q2M_REG_CHANNEL_MAP        0x0A
#define ES9038Q2M_REG_DPLL_SETTINGS      0x0B
#define ES9038Q2M_REG_FILTER_SETTINGS    0x0C
#define ES9038Q2M_REG_DSD_CONFIG         0x0D
#define ES9038Q2M_REG_SOFT_START         0x0E
#define ES9038Q2M_REG_STATUS             0x0F

// System Settings Register Bits
#define ES9038Q2M_SYSTEM_RESET           (1 << 0)
#define ES9038Q2M_POWER_DOWN             (1 << 1)
#define ES9038Q2M_SERIAL_MODE            (1 << 2)
#define ES9038Q2M_DSD_MODE               (1 << 3)

// Configuration Structures
typedef enum {
    ES9038Q2M_PROFILE_NORMAL,
    ES9038Q2M_PROFILE_HIGH_PERFORMANCE,
    ES9038Q2M_PROFILE_POWER_SAVING
} ES9038Q2M_Profile;

typedef enum {
    ES9038Q2M_FILTER_FAST_ROLL_OFF,
    ES9038Q2M_FILTER_SLOW_ROLL_OFF,
    ES9038Q2M_FILTER_MINIMUM_PHASE
} ES9038Q2M_FilterType;

typedef struct {
    uint8_t volume_left;
    uint8_t volume_right;
    ES9038Q2M_FilterType filter_type;
    bool dsd_mode;
    uint32_t sample_rate;
    uint8_t bit_depth;
} ES9038Q2M_Config;

// Function Prototypes

/**
 * @brief Initialize the ES9038Q2M DAC
 * @param config Initial configuration settings
 * @return true if initialization successful, false otherwise
 */
bool ES9038Q2M_Init(const ES9038Q2M_Config* config);

/**
 * @brief Set the DAC volume
 * @param left Left channel volume (0-255)
 * @param right Right channel volume (0-255)
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_SetVolume(uint8_t left, uint8_t right);

/**
 * @brief Set the DAC operating profile
 * @param profile Selected operating profile
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_SetProfile(ES9038Q2M_Profile profile);

/**
 * @brief Set the digital filter type
 * @param filter Selected filter type
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_SetFilter(ES9038Q2M_FilterType filter);

/**
 * @brief Switch between PCM and DSD modes
 * @param dsd_mode true for DSD mode, false for PCM mode
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_SetDSDMode(bool dsd_mode);

/**
 * @brief Configure the DAC clock source
 * @param sample_rate Sample rate in Hz
 * @param master_clock Master clock frequency in Hz
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_ConfigureClock(uint32_t sample_rate, uint32_t master_clock);

/**
 * @brief Put the DAC into power down mode
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_PowerDown(void);

/**
 * @brief Wake up the DAC from power down mode
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_PowerUp(void);

/**
 * @brief Perform a soft reset of the DAC
 * @return true if successful, false otherwise
 */
bool ES9038Q2M_Reset(void);

/**
 * @brief Get the current DAC status
 * @return Status register value
 */
uint8_t ES9038Q2M_GetStatus(void);

#endif // ES9038Q2M_H