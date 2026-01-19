#include "nuno/audio_i2s.h"

#include "nuno/audio_codec.h"
#include "nuno/board_config.h"
#include "nuno/platform.h"

#include <string.h>

#define WM8960_REG_RESET          0x0Fu
#define WM8960_REG_CLOCKING1      0x04u
#define WM8960_REG_AUDIO_IFACE    0x07u
#define WM8960_REG_L_DAC_VOL      0x0Au
#define WM8960_REG_R_DAC_VOL      0x0Bu
#define WM8960_REG_POWER1         0x19u
#define WM8960_REG_POWER2         0x1Au
#define WM8960_REG_POWER3         0x2Fu

static I2S_HandleTypeDef g_i2s_handle;
static bool g_i2s_ready = false;
static bool g_codec_ready = false;

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

    uint16_t scaled = (uint16_t)((volume_percent * 0xFFu) / 100u);
    uint16_t reg_value = (uint16_t)(0x100u | (scaled & 0xFFu));
    if (!wm8960_write(WM8960_REG_L_DAC_VOL, reg_value)) {
        return false;
    }
    return wm8960_write(WM8960_REG_R_DAC_VOL, reg_value);
}

static uint32_t map_audio_freq(uint32_t sample_rate) {
    switch (sample_rate) {
        case 44100u:
            return I2S_AUDIOFREQ_44K;
        case 48000u:
            return I2S_AUDIOFREQ_48K;
        default:
            return I2S_AUDIOFREQ_44K;
    }
}

bool AudioI2S_Init(uint32_t sample_rate, uint8_t bit_depth) {
    memset(&g_i2s_handle, 0, sizeof(g_i2s_handle));
    g_i2s_handle.Instance = NUNO_I2S_INSTANCE;
    g_i2s_handle.Init.Mode = I2S_MODE_MASTER_TX;
    g_i2s_handle.Init.Standard = I2S_STANDARD_PHILIPS;
    g_i2s_handle.Init.DataFormat = (bit_depth >= 24U) ? I2S_DATAFORMAT_24B : I2S_DATAFORMAT_16B;
    g_i2s_handle.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
    g_i2s_handle.Init.AudioFreq = map_audio_freq(sample_rate);
    g_i2s_handle.Init.CPOL = I2S_CPOL_LOW;
    g_i2s_handle.Init.ClockSource = I2S_CLOCK_PLL;
    g_i2s_handle.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;

    if (HAL_I2S_Init(&g_i2s_handle) != HAL_OK) {
        return false;
    }

    if (!AudioCodec_Init(sample_rate, bit_depth)) {
        return false;
    }

    g_i2s_ready = true;
    return true;
}

I2S_HandleTypeDef *AudioI2S_GetHandle(void) {
    return &g_i2s_handle;
}

void HAL_I2S_MspInit(I2S_HandleTypeDef *hi2s) {
    if (!hi2s || hi2s->Instance != SPI2) {
        return;
    }

    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI2;

    gpio.Pin = NUNO_I2S_WS_PIN | NUNO_I2S_SCK_PIN | NUNO_I2S_SD_PIN;
    HAL_GPIO_Init(NUNO_I2S_WS_PORT, &gpio);

    gpio.Pin = NUNO_I2S_MCK_PIN;
    HAL_GPIO_Init(NUNO_I2S_MCK_PORT, &gpio);
}
