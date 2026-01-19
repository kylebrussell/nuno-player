#ifndef NUNO_AUDIO_BUFFER_H
#define NUNO_AUDIO_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#endif /* NUNO_AUDIO_BUFFER_H */
