#ifndef NUNO_AUDIO_BUFFER_H
#define NUNO_AUDIO_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Producer / consumer contract (read before touching the double buffer)
 * --------------------------------------------------------------------
 * Two parties touch g_buffer concurrently:
 *
 *   PRODUCER  - fill_buffer(), reached from the audio-task / DMA-complete
 *               path (in the sim: whoever calls AudioBuffer_Done()). It is the
 *               SOLE writer of data[], valid_frames[], end_of_stream and the
 *               buffer state. It only ever writes the *inactive* index.
 *   CONSUMER  - the audio output callback / future DMA ISR. In the sim this is
 *               the SDL audio thread, which calls AudioBuffer_GetBuffer() to
 *               read the *active* index, then AudioBuffer_Done() to advance.
 *
 * Synchronisation is lock-free (no blocking in the consumer path). The shared
 * scalars that cross the producer/consumer boundary - 'active', the per-index
 * 'valid_frames', 'end_of_stream' and 'state' - are C11 atomics. The publishing
 * store of 'active' in AudioBuffer_Done() uses release ordering and the
 * matching load in AudioBuffer_GetBuffer() uses acquire ordering, so a buffer's
 * data[] writes are guaranteed visible before the index that exposes them.
 *
 * Single-writer-per-index is the load-bearing invariant: the producer fills the
 * just-consumed (inactive) buffer while the consumer drains the other, so the
 * raw data[] arrays never need per-sample atomics. Preserve that if you add a
 * second producer.
 *
 * The volume target (AudioBuffer_SetVolume) may be set from a control thread
 * different from the producer; it is an atomic scalar read by the producer.
 */

struct FormatDecoder;  // Forward declaration
typedef struct FormatDecoder FormatDecoder;

#define AUDIO_OUT_CHANNELS 2U
#define AUDIO_BUFFER_SIZE 4096U
#define AUDIO_BUFFER_FRAMES (AUDIO_BUFFER_SIZE / AUDIO_OUT_CHANNELS)
#define AUDIO_BUFFER_BYTES (AUDIO_BUFFER_SIZE * sizeof(uint16_t))
#define AUDIO_BUFFER_LOW_WATER_MARK (AUDIO_BUFFER_FRAMES / 4U)

typedef enum {
    BUFFER_STATE_EMPTY,
    BUFFER_STATE_READY,
    BUFFER_STATE_PLAYING,
    BUFFER_STATE_UNDERRUN,
    BUFFER_STATE_END_OF_STREAM
} BufferState;

typedef struct {
    size_t total_samples;
    size_t underruns;
    uint32_t last_transition_time_ms;
    float average_utilisation;
} AudioBufferStats;

typedef struct {
    size_t read_errors;
    size_t retry_successes;
    size_t total_underruns;
} AudioBufferErrorStats;

bool AudioBuffer_Init(void);
void AudioBuffer_Cleanup(void);

bool AudioBuffer_StartPlayback(void);
uint16_t *AudioBuffer_GetBuffer(void);

bool AudioBuffer_Done(void);
void AudioBuffer_HalfDone(void);
bool AudioBuffer_ProcessComplete(void);

void AudioBuffer_Update(void);
bool AudioBuffer_IsUnderThreshold(void);
void AudioBuffer_HandleUnderrun(void);

bool AudioBuffer_Seek(size_t position_in_samples);
void AudioBuffer_Pause(void);

BufferState AudioBuffer_GetState(void);
void AudioBuffer_ResetBufferStats(void);
void AudioBuffer_GetBufferStats(AudioBufferStats *stats);

void AudioBuffer_GetErrorStats(AudioBufferErrorStats *stats);
void AudioBuffer_ResetErrorStats(void);

void AudioBuffer_GetUnderrunDetails(uint32_t *timestamp_ms,
                                    size_t *samples_lost,
                                    uint32_t *recovery_time_ms);
void AudioBuffer_RegisterUnderrunCallback(void (*callback)(void));

void AudioBuffer_ConfigureThresholds(size_t low_threshold, size_t high_threshold);
void AudioBuffer_GetThresholdConfig(size_t *low_threshold,
                                    size_t *high_threshold,
                                    float *percentage);

void AudioBuffer_ConfigureReadChunks(size_t min_size,
                                     size_t max_size,
                                     size_t optimal_size);
void AudioBuffer_GetReadChunkConfig(size_t *min_size,
                                    size_t *max_size,
                                    size_t *optimal_size);

void AudioBuffer_ConfigureSampleRate(uint32_t source_rate, uint32_t target_rate);
void AudioBuffer_GetSampleRateConfig(uint32_t *source_rate,
                                     uint32_t *target_rate,
                                     bool *conversion_enabled,
                                     float *ratio);

void AudioBuffer_ConfigureSampleFormat(uint8_t bits_per_sample,
                                       bool is_float,
                                       bool is_signed);
void AudioBuffer_GetSampleFormat(uint8_t *bits_per_sample,
                                 bool *is_float,
                                 bool *is_signed,
                                 uint8_t *bytes_per_sample);

bool AudioBuffer_Flush(bool reset_stats);

bool AudioBuffer_PrepareCrossfade(uint32_t fade_samples);
bool AudioBuffer_StartCrossfade(void);
bool AudioBuffer_GetNextTrackSamples(int16_t *buffer, size_t samples);
bool AudioBuffer_CompleteCrossfade(void);

bool AudioBuffer_PrepareGaplessTransition(void);

bool AudioBuffer_HasNextTrack(void);
void AudioBuffer_SetNextTrackAvailability(bool available,
                                          size_t remaining_tracks);

bool AudioBuffer_SetDecoder(FormatDecoder* decoder);
void AudioBuffer_ClearDecoder(void);
FormatDecoder* AudioBuffer_GetDecoder(void);

/* ----------------------------------------------------------------------------
 * Software master volume
 *
 * Gain is applied by the producer (fill_buffer) as float frames are converted
 * to interleaved S16. The target is set as a 0..100 percentage and mapped
 * through a mild perceptual (quadratic) curve: gain = (percent/100)^2. 100%
 * is bit-exact passthrough so the default does not regress output. The target
 * is an atomic scalar so it may be set from a thread other than the producer;
 * the producer ramps the applied gain toward the target across each block to
 * avoid zipper noise on abrupt changes.
 * -------------------------------------------------------------------------- */
void AudioBuffer_SetVolume(uint8_t percent);
uint8_t AudioBuffer_GetVolume(void);

/* ----------------------------------------------------------------------------
 * Gapless playback
 *
 * When the active decoder reaches EOF mid-fill, the producer asks the
 * registered provider for the next track's decoder and keeps filling the same
 * output buffer from it - no silence gap, no underrun-stop. The provider
 * (installed by the pipeline) is responsible for advancing the music library
 * and returning an opened FormatDecoder for the next track, or NULL at the end
 * of the library. Returning NULL lets the buffer drain cleanly to end-of-stream.
 *
 * Each successful transition increments an atomic sequence counter so the
 * UI can poll for "Now Playing" changes without the buffer touching UI code.
 * -------------------------------------------------------------------------- */
typedef FormatDecoder* (*AudioBufferNextTrackProvider)(void* user_data);
void AudioBuffer_SetNextTrackProvider(AudioBufferNextTrackProvider provider,
                                      void* user_data);

/* Monotonic count of gapless track transitions performed by the producer. */
uint32_t AudioBuffer_GetTrackChangeCount(void);

/*
 * Returns true exactly once per gapless transition: it reports whether the
 * track-change counter advanced since the previous call and latches the new
 * value. Intended for a UI poll loop to refresh "Now Playing".
 */
bool AudioBuffer_ConsumeTrackChanged(void);

#endif /* NUNO_AUDIO_BUFFER_H */
