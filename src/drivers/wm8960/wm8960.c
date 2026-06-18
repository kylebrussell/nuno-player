#include "nuno/audio_codec.h"
#include "nuno/board_config.h"
#include "nuno/platform.h"

/*
 * WM8960 codec driver - implements the portable AudioCodec_* HAL over I2C.
 *
 * The WM8960 is a low-cost integrated codec (DAC + ADC + headphone/speaker amp)
 * used for the 2026 prototype board: simple I2C control, 3.3 V logic, runs off
 * the dev board. Selected at build time with NUNO_CODEC=WM8960 (the default).
 * The premium production target is the ES9038Q2M (see es9038q2m_codec.c).
 *
 * This file owns only codec control; the I2S transport lives in
 * src/platform/audio_i2s.c, which calls AudioCodec_Init() through this HAL.
 */

#define WM8960_REG_RESET          0x0Fu
#define WM8960_REG_CLOCKING1      0x04u
#define WM8960_REG_AUDIO_IFACE    0x07u
#define WM8960_REG_L_DAC_VOL      0x0Au
#define WM8960_REG_R_DAC_VOL      0x0Bu
#define WM8960_REG_POWER1         0x19u
#define WM8960_REG_POWER2         0x1Au
#define WM8960_REG_POWER3         0x2Fu

static bool g_codec_ready = false;

/* WM8960 control: 7-bit register address + 9-bit value, MSB of the value rides
 * in the low bit of the address byte. */
static bool wm8960_write(uint8_t reg, uint16_t value) {
    uint8_t payload[2];
    payload[0] = (uint8_t)((reg << 1) | ((value >> 8) & 0x01u));
    payload[1] = (uint8_t)(value & 0xFFu);
    return platform_i2c_write(NUNO_CODEC_I2C_ADDR, payload, sizeof(payload));
}

bool AudioCodec_Init(uint32_t sample_rate, uint8_t bit_depth) {
    (void)sample_rate;
    (void)bit_depth;

    if (!wm8960_write(WM8960_REG_RESET, 0x000u)) {
        return false;
    }
    platform_delay_ms(10);

    // Power up core blocks (conservative defaults).
    if (!wm8960_write(WM8960_REG_POWER1, 0x1C0u)) {
        return false;
    }
    if (!wm8960_write(WM8960_REG_POWER2, 0x1F8u)) {
        return false;
    }
    if (!wm8960_write(WM8960_REG_POWER3, 0x0Fu)) {
        return false;
    }

    // Clocking: default dividers, MCLK based.
    if (!wm8960_write(WM8960_REG_CLOCKING1, 0x000u)) {
        return false;
    }

    // Audio interface: I2S, 16-bit.
    if (!wm8960_write(WM8960_REG_AUDIO_IFACE, 0x002u)) {
        return false;
    }

    // Set DAC volumes to 0 dB (approx) and enable update bit.
    if (!wm8960_write(WM8960_REG_L_DAC_VOL, 0x1FFu)) {
        return false;
    }
    if (!wm8960_write(WM8960_REG_R_DAC_VOL, 0x1FFu)) {
        return false;
    }

    g_codec_ready = true;
    return true;
}

bool AudioCodec_PowerUp(void) {
    if (!g_codec_ready) {
        return false;
    }
    return wm8960_write(WM8960_REG_POWER2, 0x1F8u);
}

bool AudioCodec_PowerDown(void) {
    if (!g_codec_ready) {
        return false;
    }
    return wm8960_write(WM8960_REG_POWER2, 0x000u);
}

bool AudioCodec_SetVolume(uint8_t volume_percent) {
    if (!g_codec_ready) {
        return false;
    }
    if (volume_percent > 100u) {
        volume_percent = 100u;
    }

    uint16_t scaled = (uint16_t)((volume_percent * 0xFFu) / 100u);
    uint16_t reg_value = (uint16_t)(0x100u | (scaled & 0xFFu));
    if (!wm8960_write(WM8960_REG_L_DAC_VOL, reg_value)) {
        return false;
    }
    return wm8960_write(WM8960_REG_R_DAC_VOL, reg_value);
}
