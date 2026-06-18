#include "nuno/audio_i2s.h"

#include "nuno/audio_codec.h"
#include "nuno/board_config.h"
#include "nuno/platform.h"

#include <string.h>

/*
 * I2S transport for the audio DAC. Codec control (the AudioCodec_* HAL) lives in
 * the selected codec driver - src/drivers/wm8960/wm8960.c or
 * src/drivers/es9038q2m/es9038q2m_codec.c, chosen at build time via NUNO_CODEC.
 * AudioI2S_Init() brings the codec up through that HAL once the I2S peripheral
 * is initialised, so this file stays codec-agnostic.
 */

static I2S_HandleTypeDef g_i2s_handle;
static bool g_i2s_ready = false;

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
