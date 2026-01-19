#ifndef NUNO_AUDIO_CODEC_H
#define NUNO_AUDIO_CODEC_H

#include <stdbool.h>
#include <stdint.h>

bool AudioCodec_Init(uint32_t sample_rate, uint8_t bit_depth);
bool AudioCodec_PowerUp(void);
bool AudioCodec_PowerDown(void);
bool AudioCodec_SetVolume(uint8_t volume_percent);

#endif /* NUNO_AUDIO_CODEC_H */
