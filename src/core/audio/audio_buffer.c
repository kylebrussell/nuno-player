#include "nuno/audio_buffer.h"

#include "nuno/filesystem.h"
#include "nuno/format_decoder.h"
#include "nuno/platform.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define DMA_BUFFER_COUNT 2U

/*
 * Software volume curve. The public target is a 0..100 percentage; the applied
 * gain is (percent/100)^2, a mild perceptual curve that keeps 100% bit-exact
 * (gain == 1.0) so the default does not alter output. VOLUME_RAMP_STEP bounds
 * how far the per-block applied gain may move toward the target each fill, which
 * removes zipper noise on abrupt volume changes (one fill == AUDIO_BUFFER_FRAMES
 * frames, ~46 ms at 44.1 kHz, so a full 0<->1 sweep takes a handful of blocks).
 */
#define VOLUME_MAX_PERCENT 100U
#define VOLUME_RAMP_STEP   0.25f

/*
 * Unit conventions for this module (read before touching counts):
 *   - data[]            : interleaved S16 PCM. Indexed in *samples* (uint16_t
 *                         elements); holds AUDIO_BUFFER_SIZE samples ==
 *                         AUDIO_BUFFER_FRAMES frames (AUDIO_OUT_CHANNELS per frame).
 *   - valid_frames[]    : number of decoded *frames* (stereo sample pairs)
 *                         currently valid in the matching data[] buffer.
 *   - low/high_threshold: *frame* counts (see AUDIO_BUFFER_FRAMES / LOW_WATER_MARK).
 * One frame == AUDIO_OUT_CHANNELS samples == AUDIO_OUT_CHANNELS * sizeof(uint16_t) bytes.
 * The DMA layer is driven elsewhere with a length of AUDIO_BUFFER_SIZE (samples),
 * matching the uint16_t element count of data[].
 */
typedef struct {
    /*
     * data[]/valid_frames[]/active are shared between the producer (fill_buffer,
     * driven from the DMA-complete / audio-task path) and the audio output
     * callback that consumes the active buffer. The double-buffer scheme keeps
     * them on separate indices so the producer fills the just-consumed buffer
     * while the callback drains the other; 'active' is flipped only from the
     * consume path. This is intentionally lock-free and single-writer per index.
     * Keep that invariant if you add producers.
     */
    uint16_t data[DMA_BUFFER_COUNT][AUDIO_BUFFER_SIZE];
    /*
     * Shared producer/consumer scalars are atomic - see the contract in
     * audio_buffer.h. 'active' is published with release ordering from the
     * consume path and read with acquire ordering from the producer/consumer;
     * the others are single-writer (producer) with relaxed loads on the read
     * side, which is sufficient because 'active' is the synchronising handoff.
     */
    _Atomic size_t valid_frames[DMA_BUFFER_COUNT];  // decoded frames (interleaved stereo pairs)
    _Atomic size_t active;

    _Atomic int state;        // BufferState, stored as int for atomic ops
    bool initialised;
    _Atomic bool end_of_stream;

    bool next_track_available;
    size_t remaining_tracks;

    /* Software master volume. target_percent is the control input (any thread);
     * applied_gain is producer-local and ramps toward the target per block. */
    _Atomic uint8_t volume_percent;
    float applied_gain;

    /* Gapless: provider that yields the next track's decoder on EOF, plus a
     * monotonic transition counter the UI can poll. */
    AudioBufferNextTrackProvider next_track_provider;
    void* next_track_user_data;
    _Atomic uint32_t track_change_count;
    uint32_t track_change_seen;  // last value latched by ConsumeTrackChanged

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
static void update_utilisation(size_t available_frames);

/* Map a 0..100 volume percentage to a linear gain via a mild quadratic curve.
 * 100% -> 1.0 exactly (bit-exact passthrough); 0% -> 0.0 (silence). */
static inline float volume_percent_to_gain(uint8_t percent) {
    if (percent >= VOLUME_MAX_PERCENT) {
        return 1.0f;
    }
    float norm = (float)percent / (float)VOLUME_MAX_PERCENT;
    return norm * norm;
}

/* Producer-side: nudge the applied gain toward the volume target. Returns the
 * gain in effect for the block being produced. Clamping the per-block step
 * removes zipper noise; once the target is reached this is a no-op. */
static float advance_volume_gain(void) {
    uint8_t percent = atomic_load_explicit(&g_buffer.volume_percent,
                                           memory_order_relaxed);
    float target = volume_percent_to_gain(percent);
    float current = g_buffer.applied_gain;
    float delta = target - current;
    if (delta > VOLUME_RAMP_STEP) {
        current += VOLUME_RAMP_STEP;
    } else if (delta < -VOLUME_RAMP_STEP) {
        current -= VOLUME_RAMP_STEP;
    } else {
        current = target;
    }
    g_buffer.applied_gain = current;
    return current;
}

static inline void set_state(BufferState state) {
    atomic_store_explicit(&g_buffer.state, (int)state, memory_order_relaxed);
}

static inline BufferState get_state(void) {
    return (BufferState)atomic_load_explicit(&g_buffer.state, memory_order_relaxed);
}

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
        set_state(BUFFER_STATE_END_OF_STREAM);
        return false;
    }

    if (!fill_buffer(1U)) {
        /* We can still play the first buffer, but mark end of stream. */
        atomic_store_explicit(&g_buffer.end_of_stream, true, memory_order_relaxed);
    }

    /* Publish the initial active index with release ordering so both filled
     * buffers' data[] writes are visible to a consumer that acquires it. */
    atomic_store_explicit(&g_buffer.active, 0U, memory_order_release);
    set_state(BUFFER_STATE_READY);
    return true;
}

uint16_t *AudioBuffer_GetBuffer(void) {
    /* Acquire the active index published by AudioBuffer_Done() so that the
     * producer's data[] writes for that buffer are visible to this consumer. */
    size_t active = atomic_load_explicit(&g_buffer.active, memory_order_acquire);
    return g_buffer.data[active];
}

bool AudioBuffer_Done(void) {
    if (!g_buffer.initialised) {
        printf("Audio buffer not initialized\n");
        return false;
    }

    size_t consumed_index = atomic_load_explicit(&g_buffer.active,
                                                 memory_order_relaxed);
    size_t next_index = (consumed_index + 1U) % DMA_BUFFER_COUNT;

    size_t next_valid = atomic_load_explicit(&g_buffer.valid_frames[next_index],
                                             memory_order_relaxed);
    bool eos = atomic_load_explicit(&g_buffer.end_of_stream, memory_order_relaxed);
    if (next_valid == 0U && eos) {
        set_state(BUFFER_STATE_END_OF_STREAM);
        /* Still publish the flip so the consumer sees the (silent) buffer. */
        atomic_store_explicit(&g_buffer.active, next_index, memory_order_release);
        return false;
    }

    /* Publish the flip to the next buffer with release ordering. The next
     * buffer was filled by a prior fill_buffer() whose writes are ordered
     * before this store, so the consumer's acquire load sees valid data. */
    atomic_store_explicit(&g_buffer.active, next_index, memory_order_release);

    if (!fill_buffer(consumed_index)) {
        /* The buffer we just refilled came up empty: no more audio. We do not
         * fail here because the freshly-activated next_index buffer is still
         * valid to play; end_of_stream is now set so the *following* Done()
         * will report EOS once that buffer drains. */
        set_state(BUFFER_STATE_PLAYING);
        return true;
    }

    set_state(BUFFER_STATE_PLAYING);
    return true;
}

void AudioBuffer_HalfDone(void) {
    /* No-op for the simple double buffer implementation. */
}

bool AudioBuffer_ProcessComplete(void) {
    return AudioBuffer_Done();
}

void AudioBuffer_Update(void) {
    size_t active = atomic_load_explicit(&g_buffer.active, memory_order_relaxed);
    update_utilisation(atomic_load_explicit(&g_buffer.valid_frames[active],
                                            memory_order_relaxed));
}

bool AudioBuffer_IsUnderThreshold(void) {
    size_t active = atomic_load_explicit(&g_buffer.active, memory_order_relaxed);
    return atomic_load_explicit(&g_buffer.valid_frames[active],
                                memory_order_relaxed) <= g_buffer.low_threshold;
}

void AudioBuffer_HandleUnderrun(void) {
    uint32_t start = platform_get_time_ms();

    set_state(BUFFER_STATE_UNDERRUN);
    g_buffer.errors.total_underruns++;
    g_buffer.stats.underruns++;

    size_t active = atomic_load_explicit(&g_buffer.active, memory_order_relaxed);
    memset(g_buffer.data[active], 0, AUDIO_BUFFER_BYTES);

    g_buffer.underrun.timestamp_ms = start;
    /* One full buffer was zero-filled; report the loss as a frame count
     * (the public field is historically named "samples_lost"). */
    g_buffer.underrun.samples_lost = AUDIO_BUFFER_FRAMES;

    if (!fill_buffer(active)) {
        atomic_store_explicit(&g_buffer.end_of_stream, true, memory_order_relaxed);
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

    atomic_store_explicit(&g_buffer.end_of_stream, false, memory_order_relaxed);
    set_state(BUFFER_STATE_EMPTY);
    return AudioBuffer_StartPlayback();
}

void AudioBuffer_Pause(void) {
    if (get_state() == BUFFER_STATE_PLAYING) {
        set_state(BUFFER_STATE_READY);
    }
}

BufferState AudioBuffer_GetState(void) {
    return get_state();
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
    for (size_t i = 0; i < DMA_BUFFER_COUNT; i++) {
        atomic_store_explicit(&g_buffer.valid_frames[i], 0U, memory_order_relaxed);
    }
    atomic_store_explicit(&g_buffer.active, 0U, memory_order_release);
    atomic_store_explicit(&g_buffer.end_of_stream, false, memory_order_relaxed);
    set_state(BUFFER_STATE_EMPTY);

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
    atomic_store_explicit(&g_buffer.end_of_stream, false, memory_order_relaxed);
    set_state(BUFFER_STATE_EMPTY);
    return true;
}

void AudioBuffer_ClearDecoder(void) {
    if (g_buffer.decoder) {
        format_decoder_close(g_buffer.decoder);
        format_decoder_destroy(g_buffer.decoder);
        g_buffer.decoder = NULL;
    }
    atomic_store_explicit(&g_buffer.end_of_stream, false, memory_order_relaxed);
    set_state(BUFFER_STATE_EMPTY);
}

FormatDecoder* AudioBuffer_GetDecoder(void) {
    return g_buffer.decoder;
}

void AudioBuffer_SetVolume(uint8_t percent) {
    if (percent > VOLUME_MAX_PERCENT) {
        percent = VOLUME_MAX_PERCENT;
    }
    /* Atomic target; the producer reads it once per block and ramps toward it. */
    atomic_store_explicit(&g_buffer.volume_percent, percent, memory_order_relaxed);
}

uint8_t AudioBuffer_GetVolume(void) {
    return atomic_load_explicit(&g_buffer.volume_percent, memory_order_relaxed);
}

void AudioBuffer_SetNextTrackProvider(AudioBufferNextTrackProvider provider,
                                      void* user_data) {
    /* Set on the control thread before playback; read by the producer on EOF. */
    g_buffer.next_track_provider = provider;
    g_buffer.next_track_user_data = user_data;
}

uint32_t AudioBuffer_GetTrackChangeCount(void) {
    return atomic_load_explicit(&g_buffer.track_change_count, memory_order_relaxed);
}

bool AudioBuffer_ConsumeTrackChanged(void) {
    uint32_t now = atomic_load_explicit(&g_buffer.track_change_count,
                                        memory_order_relaxed);
    if (now != g_buffer.track_change_seen) {
        g_buffer.track_change_seen = now;
        return true;
    }
    return false;
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

    /* Default master volume is 100% == bit-exact passthrough so nothing
     * regresses; start the ramp already at unity to avoid a fade-in. */
    atomic_store_explicit(&g_buffer.volume_percent, VOLUME_MAX_PERCENT,
                          memory_order_relaxed);
    g_buffer.applied_gain = 1.0f;

    g_buffer.next_track_provider = NULL;
    g_buffer.next_track_user_data = NULL;
    atomic_store_explicit(&g_buffer.track_change_count, 0U, memory_order_relaxed);
    g_buffer.track_change_seen = 0U;
}

/*
 * Gapless transition: the active decoder has hit EOF. Ask the registered
 * provider for the next track's decoder. On success, the old decoder is
 * destroyed, the new one installed, the track-change counter bumped, and true
 * is returned so the caller keeps filling the *same* output buffer from the new
 * decoder - no silence gap. Returns false when there is no provider or no next
 * track (true end of library), leaving the caller to zero-pad and stop.
 */
static bool advance_to_next_track(void) {
    if (!g_buffer.next_track_provider) {
        return false;
    }

    FormatDecoder* next = g_buffer.next_track_provider(g_buffer.next_track_user_data);
    if (!next) {
        return false;  // end of library: drain cleanly
    }

    if (g_buffer.decoder) {
        format_decoder_close(g_buffer.decoder);
        format_decoder_destroy(g_buffer.decoder);
    }
    g_buffer.decoder = next;

    /* Publish the transition so a UI poll loop can refresh "Now Playing". */
    atomic_fetch_add_explicit(&g_buffer.track_change_count, 1U,
                              memory_order_relaxed);
    return true;
}

static bool fill_buffer(size_t index) {
    printf("fill_buffer called for index %zu\n", index);

    /* Snapshot the master-volume gain once per block (ramped toward target). */
    const float gain = advance_volume_gain();

    if (!g_buffer.decoder) {
        printf("No decoder available, using fallback\n");
        // Fallback to raw data reading if no decoder
        size_t bytes_read = FileSystem_ReadAudioData(g_buffer.data[index], AUDIO_BUFFER_BYTES);
        size_t samples_read = bytes_read / sizeof(uint16_t);
        size_t frames_read = samples_read / AUDIO_OUT_CHANNELS;

        /* Apply master volume to the raw S16 stream as well. Skip the scan at
         * unity gain so the default path stays bit-exact. */
        if (gain < 1.0f) {
            for (size_t s = 0; s < samples_read; s++) {
                int16_t v = (int16_t)g_buffer.data[index][s];
                g_buffer.data[index][s] = (uint16_t)(int16_t)((float)v * gain);
            }
        }

        if (samples_read < AUDIO_BUFFER_SIZE) {
            size_t remaining = AUDIO_BUFFER_SIZE - samples_read;
            memset(&g_buffer.data[index][samples_read], 0, remaining * sizeof(uint16_t));
            atomic_store_explicit(&g_buffer.end_of_stream, (frames_read == 0U),
                                  memory_order_relaxed);
        }

        atomic_store_explicit(&g_buffer.valid_frames[index], frames_read,
                              memory_order_relaxed);
        g_buffer.stats.total_samples += frames_read;
        printf("Fallback read: %zu frames\n", frames_read);
        return frames_read > 0U;
    }

    // Use format decoder to get decoded audio data (downmix to stereo if needed)
    size_t frames_read_total = 0;

    static float decode_buffer[AUDIO_BUFFER_FRAMES * 8U];

    printf("Using format decoder...\n");
    while (frames_read_total < AUDIO_BUFFER_FRAMES) {
        // Channel count is re-read every iteration because a gapless transition
        // mid-fill can swap in a decoder with a different channel layout.
        uint32_t channels = format_decoder_get_channels(g_buffer.decoder);
        if (channels == 0U) {
            channels = AUDIO_OUT_CHANNELS;
        }
        if (channels > 8U) {
            printf("Unsupported channel count: %u\n", channels);
            break;  // stop filling; zero-pad below
        }

        size_t frames_to_read = AUDIO_BUFFER_FRAMES - frames_read_total;
        // Read interleaved float frames (channels from decoder)
        size_t frames_read = format_decoder_read(g_buffer.decoder, decode_buffer, frames_to_read);

        if (frames_read == 0) {
            // Current decoder is exhausted. Try to advance to the next track and
            // keep filling this same buffer for gapless playback. If there is no
            // next track, fall through and zero-pad the remainder.
            printf("Decoder EOF; attempting gapless advance\n");
            if (advance_to_next_track()) {
                printf("Gapless: advanced to next track, continuing fill\n");
                continue;
            }
            printf("No next track; ending stream\n");
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

            // Apply master volume in the float domain before clamping/quantising.
            left *= gain;
            right *= gain;

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
        atomic_store_explicit(&g_buffer.end_of_stream, (frames_read_total == 0U),
                              memory_order_relaxed);
    }

    atomic_store_explicit(&g_buffer.valid_frames[index], frames_read_total,
                          memory_order_relaxed);
    g_buffer.stats.total_samples += frames_read_total;
    printf("Buffer filled with %zu frames\n", frames_read_total);
    return frames_read_total > 0U;
}

static void update_utilisation(size_t available_frames) {
    /* frames / frames -> 0..1 fill ratio (both operands are frame counts). */
    float current = (float)available_frames / (float)AUDIO_BUFFER_FRAMES;
    g_buffer.stats.average_utilisation =
        (g_buffer.stats.average_utilisation * 0.9f) + (current * 0.1f);
}

