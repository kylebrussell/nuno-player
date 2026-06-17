#include "nuno/audio_buffer.h"

#include "nuno/filesystem.h"
#include "nuno/format_decoder.h"
#include "nuno/platform.h"

#include <math.h>
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
 * Crossfade tail ring. The producer keeps the last CROSSFADE_MAX_FRAMES frames
 * of decoded (post-volume, float, downmixed-to-stereo) outgoing audio so that
 * when the outgoing decoder hits EOF it can overlap-mix that already-emitted
 * tail against the incoming track's head. The ring is a fixed producer-local
 * buffer (no per-call allocation) sized to the maximum supported fade length;
 * any requested fade is clamped to this. At 44.1 kHz, 4 s == 176400 frames.
 * Stored as interleaved stereo floats: CROSSFADE_MAX_FRAMES * AUDIO_OUT_CHANNELS.
 */
#define CROSSFADE_MAX_FRAMES (4U * 44100U)

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

    /*
     * Producer decoupling (see the contract in audio_buffer.h). When
     * producer_wake is non-NULL, AudioBuffer_Done() (consumer/ISR) marks the
     * freed buffer in fill_pending and calls producer_wake instead of decoding
     * inline; AudioBuffer_Service() (producer thread/task) performs the fill.
     * fill_pending is a bitmask of buffer indices awaiting refill.
     */
    _Atomic uint32_t fill_pending;
    void (*producer_wake)(void);

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

    /*
     * Crossfade state.
     *
     *   target_frames  - control input (any thread, atomic). Desired fade
     *                    length in frames; 0 == disabled. Snapshotted into
     *                    'active_frames' when a transition begins.
     *   in_progress    - producer-local: a fade is currently being mixed.
     *   active_frames  - producer-local: the fade length in effect for the
     *                    current fade (clamped to CROSSFADE_MAX_FRAMES and to
     *                    however much outgoing tail was actually captured).
     *   pos            - producer-local: frames already emitted into the fade.
     *   incoming       - producer-local: the pre-opened decoder for the track
     *                    being faded in. Owned here for the fade's duration and
     *                    closed/destroyed when the fade completes or aborts.
     *   tail[]/tail_count/tail_head - producer-local ring of recently emitted
     *                    outgoing audio (interleaved stereo floats, already
     *                    volume-scaled). tail_head is the index just past the
     *                    most-recently written frame; tail_count saturates at
     *                    CROSSFADE_MAX_FRAMES. The producer pushes here while a
     *                    fade is ARMED but not yet in progress.
     *   tail_anchor/tail_window - the FROZEN tail window for the in-flight fade:
     *                    'tail_window' frames starting at ring index
     *                    'tail_anchor'. Captured in crossfade_begin(); the
     *                    producer stops pushing during the fade so this window
     *                    stays valid and is read by absolute offset.
     */
    struct {
        _Atomic uint32_t target_frames;
        bool in_progress;
        uint32_t active_frames;
        uint32_t pos;
        FormatDecoder* incoming;
        float tail[CROSSFADE_MAX_FRAMES * AUDIO_OUT_CHANNELS];
        size_t tail_count;
        size_t tail_head;
        size_t tail_anchor;
        uint32_t tail_window;
    } crossfade;

    FormatDecoder* decoder;
} AudioBufferState;

static AudioBufferState g_buffer;

static void reset_internal_state(void);
static bool fill_buffer(size_t index);
static void update_utilisation(size_t available_frames);
static void crossfade_release_incoming(void);
static void crossfade_abort(void);

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
    crossfade_abort();
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
    set_state(BUFFER_STATE_PLAYING);

    if (g_buffer.producer_wake) {
        /* Decoupled producer path (the only safe option in an ISR / audio
         * callback): mark the just-freed buffer and wake the producer to refill
         * it off the audio path. Decode does NOT happen here. */
        atomic_fetch_or_explicit(&g_buffer.fill_pending,
                                 (uint32_t)(1U << consumed_index),
                                 memory_order_relaxed);
        g_buffer.producer_wake();
    } else {
        /* No producer registered: refill inline (synchronous fallback). If it
         * comes up empty, end_of_stream is now set so the following Done() will
         * report EOS once the freshly-activated buffer drains. */
        (void)fill_buffer(consumed_index);
    }
    return true;
}

void AudioBuffer_SetProducerWake(void (*wake)(void)) {
    g_buffer.producer_wake = wake;
}

void AudioBuffer_Service(void) {
    if (!g_buffer.initialised) {
        return;
    }
    /* Claim and clear the pending set atomically so a Done() concurrent with
     * this fill can re-arm the next round without losing a request. */
    uint32_t pending = atomic_exchange_explicit(&g_buffer.fill_pending, 0U,
                                                memory_order_relaxed);
    for (size_t i = 0; i < DMA_BUFFER_COUNT; i++) {
        if (pending & (1U << i)) {
            (void)fill_buffer(i);
        }
    }
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
    /* A flush discards in-flight audio (Skip / Previous / track change), so any
     * fade in progress and its captured tail must go too - otherwise the new
     * track would inherit a stale fade or leak the incoming decoder. */
    crossfade_abort();
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
    /* Legacy entry point. Interpret the historical "samples" argument as a
     * stereo-frame count and route it through the new atomic target so callers
     * that still use it stay coherent with AudioBuffer_SetCrossfadeFrames. */
    AudioBuffer_SetCrossfadeFrames(fade_samples);
    return true;
}

bool AudioBuffer_StartCrossfade(void) {
    /* The fade is driven automatically by the producer at EOF; there is no
     * separate "start" trigger now. Report whether crossfade is armed. */
    return atomic_load_explicit(&g_buffer.crossfade.target_frames,
                                memory_order_relaxed) > 0U;
}

bool AudioBuffer_GetNextTrackSamples(int16_t *buffer, size_t samples) {
    (void)buffer;
    (void)samples;
    return false;
}

bool AudioBuffer_CompleteCrossfade(void) {
    return !g_buffer.crossfade.in_progress;
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

    /* Swapping the primary decoder invalidates any in-flight fade. */
    crossfade_abort();

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
    crossfade_abort();
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

void AudioBuffer_SetCrossfadeFrames(uint32_t frames) {
    if (frames > CROSSFADE_MAX_FRAMES) {
        frames = CROSSFADE_MAX_FRAMES;
    }
    /* Atomic target; the producer snapshots it when a transition begins. A
     * value of 0 disables crossfade and keeps the hard-cut gapless path. */
    atomic_store_explicit(&g_buffer.crossfade.target_frames, frames,
                          memory_order_relaxed);
}

uint32_t AudioBuffer_GetCrossfadeFrames(void) {
    return atomic_load_explicit(&g_buffer.crossfade.target_frames,
                                memory_order_relaxed);
}

static void reset_internal_state(void) {
    /* The producer wake is a platform binding, not buffer state - keep it across
     * resets (Init/Cleanup/Flush) so the producer stays wired. fill_pending is
     * cleared by the memset, which is what we want. */
    void (*saved_wake)(void) = g_buffer.producer_wake;
    memset(&g_buffer, 0, sizeof(g_buffer));
    g_buffer.producer_wake = saved_wake;
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

/*
 * Downmix one decoded interleaved frame (any channel count, already validated
 * <= 8) to a stereo float pair. Mirrors the per-frame mixing in fill_buffer so
 * the gapless and crossfade paths share one definition.
 */
static inline void downmix_frame(const float* frame, uint32_t channels,
                                 float* out_left, float* out_right) {
    float left;
    float right;
    if (channels == 1U) {
        left = frame[0];
        right = frame[0];
    } else if (channels == 2U) {
        left = frame[0];
        right = frame[1];
    } else {
        left = frame[0];
        right = frame[1];
        for (uint32_t ch = 2U; ch < channels; ch++) {
            float sample = frame[ch];
            left += sample;
            right += sample;
        }
        float scale = 1.0f / (float)channels;
        left *= scale;
        right *= scale;
    }
    *out_left = left;
    *out_right = right;
}

/* Clamp a stereo float pair to [-1,1], quantise to S16 and write it at the
 * given frame offset of the destination buffer. */
static inline void write_stereo_frame(size_t index, size_t frame_offset,
                                      float left, float right) {
    if (left > 1.0f) left = 1.0f;
    if (left < -1.0f) left = -1.0f;
    if (right > 1.0f) right = 1.0f;
    if (right < -1.0f) right = -1.0f;

    int16_t left_i = (int16_t)(left * 32767.0f);
    int16_t right_i = (int16_t)(right * 32767.0f);
    size_t out_index = frame_offset * AUDIO_OUT_CHANNELS;
    g_buffer.data[index][out_index] = (uint16_t)left_i;
    g_buffer.data[index][out_index + 1U] = (uint16_t)right_i;
}

/* Append one already-volume-scaled stereo frame to the crossfade tail ring so
 * it is available as outgoing-track tail if the decoder hits EOF soon. Only
 * worth maintaining while a fade length is armed. */
static inline void tail_push(float left, float right) {
    float* slot = &g_buffer.crossfade.tail[g_buffer.crossfade.tail_head * AUDIO_OUT_CHANNELS];
    slot[0] = left;
    slot[1] = right;
    g_buffer.crossfade.tail_head =
        (g_buffer.crossfade.tail_head + 1U) % CROSSFADE_MAX_FRAMES;
    if (g_buffer.crossfade.tail_count < CROSSFADE_MAX_FRAMES) {
        g_buffer.crossfade.tail_count++;
    }
}

/*
 * Read frame 'i' of the frozen outgoing tail window (0 == oldest of the window).
 * The window of 'tail_window' frames is anchored at 'tail_anchor' (the ring
 * index of its oldest frame), both captured when the fade began. Because the
 * producer stops pushing to the ring for the duration of the fade, this window
 * is stable and can be read by absolute offset with wrap-around - no copy, no
 * second buffer. Out-of-range reads yield silence.
 */
static inline void tail_window_at(uint32_t i, float* left, float* right) {
    if (i >= g_buffer.crossfade.tail_window) {
        *left = 0.0f;
        *right = 0.0f;
        return;
    }
    size_t idx = (g_buffer.crossfade.tail_anchor + i) % CROSSFADE_MAX_FRAMES;
    const float* slot = &g_buffer.crossfade.tail[idx * AUDIO_OUT_CHANNELS];
    *left = slot[0];
    *right = slot[1];
}

/* Discard the incoming decoder held for a fade, if any. */
static void crossfade_release_incoming(void) {
    if (g_buffer.crossfade.incoming) {
        format_decoder_close(g_buffer.crossfade.incoming);
        format_decoder_destroy(g_buffer.crossfade.incoming);
        g_buffer.crossfade.incoming = NULL;
    }
}

/*
 * Tear down any in-flight fade and reset the producer-local crossfade state
 * (incoming decoder, fade window, captured tail). Called whenever playback is
 * flushed, the primary decoder is swapped, or the buffer is cleaned up - e.g. a
 * Skip during a fade - so the second decoder never leaks and stale tail does
 * not bleed into the next track. Does NOT clear the armed fade length, which is
 * a persistent control setting. Producer-thread paths only.
 */
static void crossfade_abort(void) {
    crossfade_release_incoming();
    g_buffer.crossfade.in_progress = false;
    g_buffer.crossfade.active_frames = 0U;
    g_buffer.crossfade.pos = 0U;
    g_buffer.crossfade.tail_count = 0U;
    g_buffer.crossfade.tail_head = 0U;
    g_buffer.crossfade.tail_anchor = 0U;
    g_buffer.crossfade.tail_window = 0U;
}

/*
 * Begin a crossfade after the outgoing decoder hit EOF. Pulls the incoming
 * decoder from the next-track provider and arms the fade window. Returns true
 * if a fade was started (incoming decoder installed, track-change published),
 * false if there is no next track or no captured tail to fade against - in
 * which case the caller falls back to the plain gapless swap / drain.
 *
 * NOTE: the next-track provider both advances the music library AND opens the
 * decoder, so calling it here performs the same library advance the gapless
 * path would. The fade then plays the incoming head while fading the captured
 * outgoing tail; afterwards g_buffer.decoder is swapped to this incoming
 * decoder and normal filling resumes.
 */
static bool crossfade_begin(uint32_t fade_frames) {
    if (fade_frames == 0U || g_buffer.crossfade.tail_count == 0U) {
        return false;
    }
    if (!g_buffer.next_track_provider) {
        return false;
    }

    FormatDecoder* incoming = g_buffer.next_track_provider(g_buffer.next_track_user_data);
    if (!incoming) {
        return false;  // end of library: no fade, drain cleanly
    }

    uint32_t channels = format_decoder_get_channels(incoming);
    if (channels == 0U) {
        channels = AUDIO_OUT_CHANNELS;
    }
    if (channels > 8U) {
        /* Cannot mix an unsupported layout; abandon the fade and let the plain
         * gapless path take over by swapping straight to this decoder. */
        if (g_buffer.decoder) {
            format_decoder_close(g_buffer.decoder);
            format_decoder_destroy(g_buffer.decoder);
        }
        g_buffer.decoder = incoming;
        atomic_fetch_add_explicit(&g_buffer.track_change_count, 1U,
                                  memory_order_relaxed);
        return false;
    }

    /* Clamp the fade to however much outgoing tail we actually captured (a
     * track shorter than the window simply gets a shorter fade). */
    if (fade_frames > g_buffer.crossfade.tail_count) {
        fade_frames = (uint32_t)g_buffer.crossfade.tail_count;
    }

    /* Freeze the outgoing-tail window: its oldest frame sits 'fade_frames' back
     * from the current ring head. The producer stops pushing to the ring for
     * the fade's duration, so this anchor stays valid and is read by absolute
     * offset in crossfade_emit(). */
    g_buffer.crossfade.tail_anchor =
        (g_buffer.crossfade.tail_head + CROSSFADE_MAX_FRAMES - fade_frames)
        % CROSSFADE_MAX_FRAMES;
    g_buffer.crossfade.tail_window = fade_frames;

    g_buffer.crossfade.incoming = incoming;
    g_buffer.crossfade.active_frames = fade_frames;
    g_buffer.crossfade.pos = 0U;
    g_buffer.crossfade.in_progress = true;

    /* Publish the transition now so the UI refreshes "Now Playing" at the
     * point the incoming track becomes audible (start of the fade). */
    atomic_fetch_add_explicit(&g_buffer.track_change_count, 1U,
                              memory_order_relaxed);
    return true;
}

/*
 * Emit crossfade-mixed frames into the destination buffer starting at
 * out_frame. Mixes the captured outgoing tail (faded out) with freshly decoded
 * incoming-head frames (faded in) using an equal-power cos/sin curve:
 *
 *     t in [0,1]:  gain_out = cos(t * PI/2),  gain_in = sin(t * PI/2)
 *     gain_out^2 + gain_in^2 == 1  ->  constant summed power, no mid-fade dip.
 *
 * Incoming frames are decoded one block at a time into 'scratch' and already
 * have master volume applied (the tail was captured post-volume too, so both
 * sides share the same gain). Writes up to 'max_frames' frames; returns the
 * number written. Sets *fade_done when the window is exhausted. If the incoming
 * decoder unexpectedly ends mid-fade, the fade is cut short and *fade_done set.
 */
static size_t crossfade_emit(size_t index, size_t out_frame, size_t max_frames,
                             float gain, float* scratch, bool* fade_done) {
    *fade_done = false;
    size_t written = 0U;
    uint32_t total = g_buffer.crossfade.active_frames;

    uint32_t channels = format_decoder_get_channels(g_buffer.crossfade.incoming);
    if (channels == 0U) {
        channels = AUDIO_OUT_CHANNELS;
    }

    while (written < max_frames && g_buffer.crossfade.pos < total) {
        size_t want = max_frames - written;
        uint32_t remaining = total - g_buffer.crossfade.pos;
        if (want > remaining) {
            want = remaining;
        }

        size_t got = format_decoder_read(g_buffer.crossfade.incoming, scratch, want);
        if (got == 0U) {
            /* Incoming track is shorter than the fade window: end the fade
             * early; the outgoing tail simply finishes faded out. */
            *fade_done = true;
            break;
        }

        for (size_t i = 0; i < got; i++) {
            float in_l;
            float in_r;
            downmix_frame(&scratch[i * channels], channels, &in_l, &in_r);
            in_l *= gain;
            in_r *= gain;

            /* Outgoing tail frame at this fade position (frozen window). */
            float out_l;
            float out_r;
            tail_window_at(g_buffer.crossfade.pos, &out_l, &out_r);

            /* Equal-power (constant energy) fade: gain_out=cos, gain_in=sin,
             * so gain_out^2 + gain_in^2 == 1 and the mixed power stays flat. */
            float t = (total > 1U)
                          ? (float)g_buffer.crossfade.pos / (float)(total - 1U)
                          : 1.0f;
            float g_out = cosf(t * 1.57079632679f);
            float g_in = sinf(t * 1.57079632679f);

            float left = out_l * g_out + in_l * g_in;
            float right = out_r * g_out + in_r * g_in;

            /* Do NOT push to the tail ring during a fade: the frozen window
             * anchored in crossfade_begin() must stay intact. Normal tail
             * capture resumes once the incoming decoder becomes primary. */

            write_stereo_frame(index, out_frame + written + i, left, right);
            g_buffer.crossfade.pos++;
        }
        written += got;
    }

    if (g_buffer.crossfade.pos >= total) {
        *fade_done = true;
    }
    return written;
}

static bool fill_buffer(size_t index) {
    /* Snapshot the master-volume gain once per block (ramped toward target). */
    const float gain = advance_volume_gain();

    if (!g_buffer.decoder) {
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
        return frames_read > 0U;
    }

    // Use format decoder to get decoded audio data (downmix to stereo if needed)
    size_t frames_read_total = 0;

    static float decode_buffer[AUDIO_BUFFER_FRAMES * 8U];

    /* Snapshot the armed fade length once per fill. While > 0 the producer
     * captures a tail ring so it can crossfade on EOF; 0 keeps the existing
     * hard-cut gapless behaviour with no tail bookkeeping. */
    const uint32_t fade_frames = atomic_load_explicit(
        &g_buffer.crossfade.target_frames, memory_order_relaxed);
    const bool crossfade_armed = (fade_frames > 0U);

    while (frames_read_total < AUDIO_BUFFER_FRAMES) {
        /* If a crossfade is mid-flight, finish (or advance) its window before
         * decoding any more of the incoming track normally. */
        if (g_buffer.crossfade.in_progress) {
            bool fade_done = false;
            size_t emitted = crossfade_emit(index, frames_read_total,
                                            AUDIO_BUFFER_FRAMES - frames_read_total,
                                            gain, decode_buffer, &fade_done);
            frames_read_total += emitted;
            if (fade_done) {
                /* Fade complete: promote the incoming decoder to the primary
                 * decoder and resume normal filling from where the head left
                 * off. The outgoing decoder was already closed in
                 * crossfade_begin()'s provider call path; close nothing else. */
                if (g_buffer.decoder) {
                    format_decoder_close(g_buffer.decoder);
                    format_decoder_destroy(g_buffer.decoder);
                }
                g_buffer.decoder = g_buffer.crossfade.incoming;
                g_buffer.crossfade.incoming = NULL;
                g_buffer.crossfade.in_progress = false;
                g_buffer.crossfade.active_frames = 0U;
                g_buffer.crossfade.pos = 0U;
                printf("Crossfade complete; resuming normal fill\n");
            }
            if (emitted == 0U && !fade_done) {
                break;  // could not make progress; avoid spinning
            }
            continue;
        }

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
            // Current decoder is exhausted. With crossfade armed, overlap-mix the
            // captured tail with the incoming head; otherwise fall back to the
            // hard-cut gapless swap. If neither yields a next track, zero-pad.
            printf("Decoder EOF; attempting transition\n");
            if (crossfade_armed && crossfade_begin(fade_frames)) {
                printf("Crossfade: started fade to next track\n");
                continue;  // fade is now in_progress; handled at loop top
            }
            if (advance_to_next_track()) {
                printf("Gapless: advanced to next track, continuing fill\n");
                continue;
            }
            printf("No next track; ending stream\n");
            break;
        }

        for (size_t i = 0; i < frames_read; i++) {
            float left;
            float right;
            downmix_frame(&decode_buffer[i * channels], channels, &left, &right);

            // Apply master volume in the float domain before clamping/quantising.
            left *= gain;
            right *= gain;

            /* Capture the post-volume tail so a crossfade on the next EOF can
             * fade this track out against the incoming head. */
            if (crossfade_armed) {
                tail_push(left, right);
            }

            write_stereo_frame(index, frames_read_total + i, left, right);
        }

        frames_read_total += frames_read;
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
    return frames_read_total > 0U;
}

static void update_utilisation(size_t available_frames) {
    /* frames / frames -> 0..1 fill ratio (both operands are frame counts). */
    float current = (float)available_frames / (float)AUDIO_BUFFER_FRAMES;
    g_buffer.stats.average_utilisation =
        (g_buffer.stats.average_utilisation * 0.9f) + (current * 0.1f);
}

