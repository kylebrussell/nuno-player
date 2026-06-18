#include "nuno/audio_codec.h"
#include "nuno/es9038q2m.h"

/*
 * ES9038Q2M codec adapter - maps the portable AudioCodec_* HAL onto the
 * ES9038Q2M_* driver. Selected at build time with NUNO_CODEC=ES9038Q2M.
 *
 * The ES9038Q2M is the premium production DAC (flagship ESS SABRE DAC +
 * headphone amp): far higher dynamic range and a 32-bit hardware volume, at the
 * cost of a clean analog supply, a real master clock (the plan's dual CCHD-575
 * oscillators) and careful PCB layout. The WM8960 (wm8960.c) is the cheaper
 * prototype alternative. The audio pipeline talks only to AudioCodec_*, so the
 * two are interchangeable behind this HAL.
 */

static bool g_codec_ready = false;

/* ES9038Q2M master clock per sample-rate family - the NUNO plan pairs a
 * 22.5792 MHz oscillator for the 44.1 kHz family with a 24.576 MHz oscillator
 * for the 48 kHz family. */
static uint32_t master_clock_for(uint32_t sample_rate) {
    return (sample_rate % 11025u == 0u) ? 22579200u : 24576000u;
}

bool AudioCodec_Init(uint32_t sample_rate, uint8_t bit_depth) {
    ES9038Q2M_Config cfg = {
        /* Start at 0 dB attenuation (loudest). The pipeline's software master
         * volume rides on top; AudioCodec_SetVolume() can also drive the DAC's
         * hardware volume below. */
        .volume_left  = 0u,
        .volume_right = 0u,
        .filter_type  = ES9038Q2M_FILTER_FAST_ROLL_OFF,
        .dsd_mode     = false,
        .sample_rate  = sample_rate,
        .bit_depth    = bit_depth,
        .master_clock = master_clock_for(sample_rate),
    };

    g_codec_ready = ES9038Q2M_Init(&cfg);
    return g_codec_ready;
}

bool AudioCodec_PowerUp(void) {
    if (!g_codec_ready) {
        return false;
    }
    return ES9038Q2M_PowerUp();
}

bool AudioCodec_PowerDown(void) {
    if (!g_codec_ready) {
        return false;
    }
    return ES9038Q2M_PowerDown();
}

bool AudioCodec_SetVolume(uint8_t volume_percent) {
    if (!g_codec_ready) {
        return false;
    }
    if (volume_percent > 100u) {
        volume_percent = 100u;
    }
    /* The ES9038Q2M volume registers are attenuation: 0 == 0 dB (loudest),
     * higher == quieter, 255 ~= mute. Map a 0..100% level onto that (100% -> 0
     * attenuation). Linear for now; the perceptual curve should be tuned on
     * hardware. */
    uint8_t attenuation = (uint8_t)(((100u - volume_percent) * 255u) / 100u);
    return ES9038Q2M_SetVolume(attenuation, attenuation);
}
