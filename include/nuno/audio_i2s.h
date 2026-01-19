#ifndef NUNO_AUDIO_I2S_H
#define NUNO_AUDIO_I2S_H

#include "nuno/stm32h7xx_hal.h"

bool AudioI2S_Init(uint32_t sample_rate, uint8_t bit_depth);
I2S_HandleTypeDef *AudioI2S_GetHandle(void);

#endif /* NUNO_AUDIO_I2S_H */
