#include "nuno/audio_buffer.h"

#include "nuno/filesystem.h"
#include "nuno/format_decoder.h"
#include "nuno/platform.h"

#include <stdio.h>
#include <string.h>

#define DMA_BUFFER_COUNT 2U

typedef struct {
    uint16_t data[DMA_BUFFER_COUNT][AUDIO_BUFFER_SIZE];
    size_t valid_samples[DMA_BUFFER_COUNT];  // frames (interleaved stereo)
    size_t active;

    BufferState state;
    bool initialised;
    bool end_of_stream;

    bool next_track_available;
    size_t remaining_tracks;

    size_t low_threshold;
    size_t high_threshold;

    AudioBufferStats stats;
    AudioBufferErrorStats errors;

    struct {
        uint32_t timestamp_ms;
        size_t samples_lost;
        uint32_t recovery_ms;
        void (*callback)(void);
    } underrun;

    struct {
        size_t min_bytes;
        size_t max_bytes;
        size_t optimal_bytes;
    } read_cfg;

    struct {
        uint32_t source_rate;
        uint32_t target_rate;
        bool conversion_enabled;
        float ratio;
        uint8_t bits_per_sample;
        uint8_t bytes_per_sample;
        bool is_float;
        bool is_signed;
    } format;

    struct {
        bool enabled;
        bool in_progress;
        uint32_t fade_samples;
    } crossfade;

    FormatDecoder* decoder;
} AudioBufferState;

static AudioBufferState g_buffer;

static void reset_internal_state(void);
static bool fill_buffer(size_t index);
static void update_utilisation(size_t available_samples);

bool AudioBuffer_Init(void) {
    reset_internal_state();
    g_buffer.initialised = true;
    return true;
}

void AudioBuffer_Cleanup(void) {
    if (g_buffer.decoder) {
        format_decoder_close(g_buffer.decoder);
        format_decoder_destroy(g_buffer.decoder);
        g_buffer.decoder = NULL;
    }
    reset_internal_state();
}

bool AudioBuffer_StartPlayback(void) {
    if (!g_buffer.initialised) {
        return false;
    }

    if (!fill_buffer(0U)) {
        g_buffer.state = BUFFER_STATE_END_OF_STREAM;
        return false;
    }

    if (!fill_buffer(1U)) {
        /* We can still play the first buffer, but mark end of stream. */
        g_buffer.end_of_stream = true;
    }

    g_buffer.active = 0U;
    g_buffer.state = BUFFER_STATE_READY;
    return true;
}

uint16_t *AudioBuffer_GetBuffer(void) {
    return g_buffer.data[g_buffer.active];
}

bool AudioBuffer_Done(void) {
    printf("AudioBuffer_Done called\n");
    if (!g_buffer.initialised) {
        printf("Audio buffer not initialized\n");
        return false;
    }

    size_t consumed_index = g_buffer.active;
    size_t next_index = (consumed_index + 1U) % DMA_BUFFER_COUNT;

    printf("Switching from buffer %zu to buffer %zu\n", consumed_index, next_index);

    g_buffer.active = next_index;

    if (g_buffer.valid_samples[next_index] == 0U && g_buffer.end_of_stream) {
        printf("End of stream reached\n");
        g_buffer.state = BUFFER_STATE_END_OF_STREAM;
        return false;
    }

    if (!fill_buffer(consumed_index)) {
        printf("Failed to fill buffer %zu\n", consumed_index);
        g_buffer.state = BUFFER_STATE_END_OF_STREAM;
        return false;
    }

    g_buffer.state = BUFFER_STATE_PLAYING;
    printf("Buffer ready, state: PLAYING\n");
    return true;
}

void AudioBuffer_HalfDone(void) {
    /* No-op for the simple double buffer implementation. */
}

bool AudioBuffer_ProcessComplete(void) {
    return AudioBuffer_Done();
}

void AudioBuffer_Update(void) {
    update_utilisation(g_buffer.valid_samples[g_buffer.active]);
}

bool AudioBuffer_IsUnderThreshold(void) {
    return g_buffer.valid_samples[g_buffer.active] <= g_buffer.low_threshold;
}

void AudioBuffer_HandleUnderrun(void) {
    uint32_t start = platform_get_time_ms();

    g_buffer.state = BUFFER_STATE_UNDERRUN;
    g_buffer.errors.total_underruns++;
    g_buffer.stats.underruns++;

    memset(g_buffer.data[g_buffer.active], 0, AUDIO_BUFFER_BYTES);

    g_buffer.underrun.timestamp_ms = start;
    g_buffer.underrun.samples_lost = AUDIO_BUFFER_FRAMES;

    if (!fill_buffer(g_buffer.active)) {
        g_buffer.end_of_stream = true;
    }

    g_buffer.underrun.recovery_ms = platform_get_time_ms() - start;

    if (g_buffer.underrun.callback) {
        g_buffer.underrun.callback();
    }
}

bool AudioBuffer_Seek(size_t position_in_samples) {
    if (!g_buffer.initialised) {
        return false;
    }

    if (g_buffer.decoder) {
        format_decoder_seek(g_buffer.decoder, position_in_samples);
        if (format_decoder_get_last_error(g_buffer.decoder) != FD_ERROR_NONE) {
            return false;
        }
    } else {
        size_t byte_offset = position_in_samples * sizeof(uint16_t);
        if (!FileSystem_Seek(byte_offset)) {
            return false;
        }
    }

    g_buffer.end_of_stream = false;
    g_buffer.state = BUFFER_STATE_EMPTY;
    return AudioBuffer_StartPlayback();
}

void AudioBuffer_Pause(void) {
    if (g_buffer.state == BUFFER_STATE_PLAYING) {
        g_buffer.state = BUFFER_STATE_READY;
    }
}

BufferState AudioBuffer_GetState(void) {
    return g_buffer.state;
}

void AudioBuffer_ResetBufferStats(void) {
    memset(&g_buffer.stats, 0, sizeof(g_buffer.stats));
}

void AudioBuffer_GetBufferStats(AudioBufferStats *stats) {
    if (!stats) {
        return;
    }
    *stats = g_buffer.stats;
}

void AudioBuffer_GetErrorStats(AudioBufferErrorStats *stats) {
    if (!stats) {
        return;
    }
    *stats = g_buffer.errors;
}

void AudioBuffer_ResetErrorStats(void) {
    memset(&g_buffer.errors, 0, sizeof(g_buffer.errors));
}

void AudioBuffer_GetUnderrunDetails(uint32_t *timestamp_ms,
                                    size_t *samples_lost,
                                    uint32_t *recovery_time_ms) {
    if (timestamp_ms) {
        *timestamp_ms = g_buffer.underrun.timestamp_ms;
    }
    if (samples_lost) {
        *samples_lost = g_buffer.underrun.samples_lost;
    }
    if (recovery_time_ms) {
        *recovery_time_ms = g_buffer.underrun.recovery_ms;
    }
}

void AudioBuffer_RegisterUnderrunCallback(void (*callback)(void)) {
    g_buffer.underrun.callback = callback;
}

void AudioBuffer_ConfigureThresholds(size_t low_threshold, size_t high_threshold) {
    if (low_threshold >= high_threshold || high_threshold > AUDIO_BUFFER_FRAMES) {
        return;
    }
    g_buffer.low_threshold = low_threshold;
    g_buffer.high_threshold = high_threshold;
}

void AudioBuffer_GetThresholdConfig(size_t *low_threshold,
                                    size_t *high_threshold,
                                    float *percentage) {
    if (low_threshold) {
        *low_threshold = g_buffer.low_threshold;
    }
    if (high_threshold) {
        *high_threshold = g_buffer.high_threshold;
    }
    if (percentage) {
        *percentage = (float)g_buffer.low_threshold / (float)AUDIO_BUFFER_FRAMES;
    }
}

void AudioBuffer_ConfigureReadChunks(size_t min_size,
                                     size_t max_size,
                                     size_t optimal_size) {
    if (min_size == 0U || max_size < min_size || optimal_size < min_size) {
        return;
    }
    g_buffer.read_cfg.min_bytes = min_size;
    g_buffer.read_cfg.max_bytes = max_size;
    g_buffer.read_cfg.optimal_bytes = optimal_size;
}

void AudioBuffer_GetReadChunkConfig(size_t *min_size,
                                    size_t *max_size,
                                    size_t *optimal_size) {
    if (min_size) {
        *min_size = g_buffer.read_cfg.min_bytes;
    }
    if (max_size) {
        *max_size = g_buffer.read_cfg.max_bytes;
    }
    if (optimal_size) {
        *optimal_size = g_buffer.read_cfg.optimal_bytes;
    }
}

void AudioBuffer_ConfigureSampleRate(uint32_t source_rate, uint32_t target_rate) {
    g_buffer.format.source_rate = source_rate;
    g_buffer.format.target_rate = target_rate;
    g_buffer.format.conversion_enabled = (source_rate != target_rate) && (source_rate != 0U);
    g_buffer.format.ratio = (source_rate == 0U) ? 1.0f : (float)target_rate / (float)source_rate;
}

void AudioBuffer_GetSampleRateConfig(uint32_t *source_rate,
                                     uint32_t *target_rate,
                                     bool *conversion_enabled,
                                     float *ratio) {
    if (source_rate) {
        *source_rate = g_buffer.format.source_rate;
    }
    if (target_rate) {
        *target_rate = g_buffer.format.target_rate;
    }
    if (conversion_enabled) {
        *conversion_enabled = g_buffer.format.conversion_enabled;
    }
    if (ratio) {
        *ratio = g_buffer.format.ratio;
    }
}

void AudioBuffer_ConfigureSampleFormat(uint8_t bits_per_sample,
                                       bool is_float,
                                       bool is_signed) {
    (void)is_float;
    (void)is_signed;
    /* The simulator currently only handles 16-bit signed PCM. */
    g_buffer.format.bits_per_sample = 16U;
    g_buffer.format.bytes_per_sample = (uint8_t)sizeof(uint16_t);
    g_buffer.format.is_float = false;
    g_buffer.format.is_signed = true;
}

void AudioBuffer_GetSampleFormat(uint8_t *bits_per_sample,
                                 bool *is_float,
                                 bool *is_signed,
                                 uint8_t *bytes_per_sample) {
    if (bits_per_sample) {
        *bits_per_sample = g_buffer.format.bits_per_sample;
    }
    if (is_float) {
        *is_float = g_buffer.format.is_float;
    }
    if (is_signed) {
        *is_signed = g_buffer.format.is_signed;
    }
    if (bytes_per_sample) {
        *bytes_per_sample = g_buffer.format.bytes_per_sample;
    }
}

bool AudioBuffer_Flush(bool reset_stats) {
    memset(g_buffer.data, 0, sizeof(g_buffer.data));
    memset(g_buffer.valid_samples, 0, sizeof(g_buffer.valid_samples));
    g_buffer.active = 0U;
    g_buffer.end_of_stream = false;
    g_buffer.state = BUFFER_STATE_EMPTY;

    if (reset_stats) {
        AudioBuffer_ResetBufferStats();
        AudioBuffer_ResetErrorStats();
    }
    return true;
}

bool AudioBuffer_PrepareCrossfade(uint32_t fade_samples) {
    g_buffer.crossfade.enabled = true;
    g_buffer.crossfade.fade_samples = fade_samples;
    return true;
}

bool AudioBuffer_StartCrossfade(void) {
    if (!g_buffer.crossfade.enabled) {
        return false;
    }
    g_buffer.crossfade.in_progress = true;
    return true;
}

bool AudioBuffer_GetNextTrackSamples(int16_t *buffer, size_t samples) {
    (void)buffer;
    (void)samples;
    return false;
}

bool AudioBuffer_CompleteCrossfade(void) {
    g_buffer.crossfade.in_progress = false;
    return true;
}

bool AudioBuffer_PrepareGaplessTransition(void) {
    g_buffer.next_track_available = true;
    return true;
}

bool AudioBuffer_HasNextTrack(void) {
    return g_buffer.next_track_available;
}

void AudioBuffer_SetNextTrackAvailability(bool available,
                                          size_t remaining_tracks) {
    g_buffer.next_track_available = available;
    g_buffer.remaining_tracks = remaining_tracks;
}

bool AudioBuffer_SetDecoder(FormatDecoder* decoder) {
    if (!g_buffer.initialised) {
        return false;
    }

    // Clean up existing decoder
    if (g_buffer.decoder) {
        format_decoder_close(g_buffer.decoder);
        format_decoder_destroy(g_buffer.decoder);
    }

    g_buffer.decoder = decoder;
    g_buffer.end_of_stream = false;
    g_buffer.state = BUFFER_STATE_EMPTY;
    return true;
}

void AudioBuffer_ClearDecoder(void) {
    if (g_buffer.decoder) {
        format_decoder_close(g_buffer.decoder);
        format_decoder_destroy(g_buffer.decoder);
        g_buffer.decoder = NULL;
    }
    g_buffer.end_of_stream = false;
    g_buffer.state = BUFFER_STATE_EMPTY;
}

FormatDecoder* AudioBuffer_GetDecoder(void) {
    return g_buffer.decoder;
}

static void reset_internal_state(void) {
    memset(&g_buffer, 0, sizeof(g_buffer));
    g_buffer.low_threshold = AUDIO_BUFFER_LOW_WATER_MARK;
    g_buffer.high_threshold = AUDIO_BUFFER_FRAMES;
    g_buffer.read_cfg.min_bytes = AUDIO_BUFFER_BYTES / 4U;
    g_buffer.read_cfg.max_bytes = AUDIO_BUFFER_BYTES;
    g_buffer.read_cfg.optimal_bytes = AUDIO_BUFFER_BYTES / 2U;
    g_buffer.format.bits_per_sample = 16U;
    g_buffer.format.bytes_per_sample = (uint8_t)sizeof(uint16_t);
    g_buffer.format.is_float = false;
    g_buffer.format.is_signed = true;
    g_buffer.format.source_rate = 0U;
    g_buffer.format.target_rate = 0U;
    g_buffer.format.ratio = 1.0f;
    g_buffer.next_track_available = false;
    g_buffer.decoder = NULL;
}

static bool fill_buffer(size_t index) {
    printf("fill_buffer called for index %zu\n", index);

    if (!g_buffer.decoder) {
        printf("No decoder available, using fallback\n");
        // Fallback to raw data reading if no decoder
        size_t bytes_read = FileSystem_ReadAudioData(g_buffer.data[index], AUDIO_BUFFER_BYTES);
        size_t samples_read = bytes_read / sizeof(uint16_t);
        size_t frames_read = samples_read / AUDIO_OUT_CHANNELS;

        if (samples_read < AUDIO_BUFFER_SIZE) {
            size_t remaining = AUDIO_BUFFER_SIZE - samples_read;
            memset(&g_buffer.data[index][samples_read], 0, remaining * sizeof(uint16_t));
            g_buffer.end_of_stream = (frames_read == 0U);
        }

        g_buffer.valid_samples[index] = frames_read;
        g_buffer.stats.total_samples += frames_read;
        printf("Fallback read: %zu frames\n", frames_read);
        return frames_read > 0U;
    }

    // Use format decoder to get decoded audio data (downmix to stereo if needed)
    size_t frames_read_total = 0;
    uint32_t channels = format_decoder_get_channels(g_buffer.decoder);
    if (channels == 0U) {
        channels = AUDIO_OUT_CHANNELS;
    }
    if (channels > 8U) {
        printf("Unsupported channel count: %u\n", channels);
        g_buffer.end_of_stream = true;
        return false;
    }

    static float decode_buffer[AUDIO_BUFFER_FRAMES * 8U];

    printf("Using format decoder...\n");
    while (frames_read_total < AUDIO_BUFFER_FRAMES) {
        size_t frames_to_read = AUDIO_BUFFER_FRAMES - frames_read_total;
        // Read interleaved float frames (channels from decoder)
        size_t frames_read = format_decoder_read(g_buffer.decoder, decode_buffer, frames_to_read);

        if (frames_read == 0) {
            printf("No more frames from decoder\n");
            break;
        }

        for (size_t i = 0; i < frames_read; i++) {
            float left = 0.0f;
            float right = 0.0f;

            if (channels == 1U) {
                left = decode_buffer[i];
                right = decode_buffer[i];
            } else if (channels == 2U) {
                left = decode_buffer[i * channels + 0U];
                right = decode_buffer[i * channels + 1U];
            } else {
                left = decode_buffer[i * channels + 0U];
                right = decode_buffer[i * channels + 1U];
                for (uint32_t ch = 2U; ch < channels; ch++) {
                    float sample = decode_buffer[i * channels + ch];
                    left += sample;
                    right += sample;
                }
                float scale = 1.0f / (float)channels;
                left *= scale;
                right *= scale;
            }

            if (left > 1.0f) left = 1.0f;
            if (left < -1.0f) left = -1.0f;
            if (right > 1.0f) right = 1.0f;
            if (right < -1.0f) right = -1.0f;

            int16_t left_i = (int16_t)(left * 32767.0f);
            int16_t right_i = (int16_t)(right * 32767.0f);
            size_t out_index = (frames_read_total + i) * AUDIO_OUT_CHANNELS;
            g_buffer.data[index][out_index] = (uint16_t)left_i;
            g_buffer.data[index][out_index + 1U] = (uint16_t)right_i;
        }

        frames_read_total += frames_read;
        printf("Read %zu frames, total %zu\n", frames_read, frames_read_total);
    }

    if (frames_read_total < AUDIO_BUFFER_FRAMES) {
        size_t remaining_frames = AUDIO_BUFFER_FRAMES - frames_read_total;
        size_t remaining_samples = remaining_frames * AUDIO_OUT_CHANNELS;
        memset(&g_buffer.data[index][frames_read_total * AUDIO_OUT_CHANNELS],
               0,
               remaining_samples * sizeof(uint16_t));
        g_buffer.end_of_stream = (frames_read_total == 0U);
    }

    g_buffer.valid_samples[index] = frames_read_total;
    g_buffer.stats.total_samples += frames_read_total;
    printf("Buffer filled with %zu frames\n", frames_read_total);
    return frames_read_total > 0U;
}

static void update_utilisation(size_t available_samples) {
    float current = (float)available_samples / (float)AUDIO_BUFFER_FRAMES;
    g_buffer.stats.average_utilisation =
        (g_buffer.stats.average_utilisation * 0.9f) + (current * 0.1f);
}

