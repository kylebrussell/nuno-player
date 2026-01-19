#include "nuno/audio_codec.h"

bool AudioCodec_Init(uint32_t sample_rate, uint8_t bit_depth) {
    (void)sample_rate;
    (void)bit_depth;
    return true;
}

bool AudioCodec_PowerUp(void) {
    return true;
}

bool AudioCodec_PowerDown(void) {
    return true;
}

bool AudioCodec_SetVolume(uint8_t volume_percent) {
    (void)volume_percent;
    return true;
}
