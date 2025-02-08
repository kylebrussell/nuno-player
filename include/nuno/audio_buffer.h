#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

// Buffer Configuration
#define AUDIO_BUFFER_SIZE 4096
#define BUFFER_THRESHOLD (AUDIO_BUFFER_SIZE / 4) // Example threshold for underrun protection

// Circular Buffer Structure
typedef struct {
    uint16_t *buffer;
    size_t size;
    size_t head;
    size_t tail;
    bool is_full;
} CircularBuffer;

// Initialize the audio buffer
void AudioBuffer_Init(void);

// Get pointer to the next audio data chunk
uint16_t* AudioBuffer_GetBuffer(void);

// Notify that the DMA has completed a chunk
void AudioBuffer_Done(void);

// Notify that half of the DMA buffer has been filled
void AudioBuffer_HalfDone(void);

// Underrun Protection Functions

/**
 * @brief Check if the buffer is running low on data to prevent underrun
 * @return true if under threshold, false otherwise
 */
bool AudioBuffer_IsUnderThreshold(void);

/**
 * @brief Handle buffer underrun scenario
 */
void AudioBuffer_HandleUnderrun(void);

// Get error statistics for audio buffer operations
void AudioBuffer_GetErrorStats(size_t* read_errors, size_t* retry_successes, size_t* total_underruns);

// Reset error statistics counters
void AudioBuffer_ResetErrorStats(void);

/**
 * @brief Get detailed information about the last underrun event
 * @param timestamp Pointer to store the timestamp of the last underrun
 * @param samples_lost Pointer to store number of samples lost during underrun
 * @param recovery_time_ms Pointer to store how long recovery took in ms
 */
void AudioBuffer_GetUnderrunDetails(uint32_t* timestamp, 
                                  size_t* samples_lost,
                                  uint32_t* recovery_time_ms);

/**
 * @brief Register a callback function to be notified of underrun events
 * @param callback Function pointer to call when underrun occurs
 */
void AudioBuffer_RegisterUnderrunCallback(void (*callback)(void));

/**
 * @brief Get buffer performance statistics
 * @param total_samples Pointer to store total samples processed
 * @param transitions Pointer to store number of successful transitions
 * @param underruns Pointer to store number of buffer underruns
 * @param last_transition_time Pointer to store timestamp of last transition
 * @param buffer_utilization Pointer to store average buffer utilization (0.0-1.0)
 */
void AudioBuffer_GetBufferStats(size_t* total_samples,
                              size_t* transitions,
                              size_t* underruns,
                              uint32_t* last_transition_time,
                              float* buffer_utilization);

/**
 * @brief Reset all buffer statistics to zero
 */
void AudioBuffer_ResetBufferStats(void);

// Configure read chunk sizes
void AudioBuffer_ConfigureReadChunks(size_t min_size, size_t max_size, size_t optimal_size);

// Get current read chunk configuration
void AudioBuffer_GetReadChunkConfig(size_t* min_size, size_t* max_size, size_t* optimal_size);

/**
 * @brief Configure buffer threshold values
 * @param low_threshold Minimum buffer level before triggering underrun protection
 * @param high_threshold Buffer level required before requesting more data
 * @note Both thresholds must be less than AUDIO_BUFFER_SIZE, and low_threshold must be less than high_threshold
 */
void AudioBuffer_ConfigureThresholds(size_t low_threshold, size_t high_threshold);

/**
 * @brief Get current buffer threshold configuration
 * @param low_threshold Pointer to store current low threshold value (can be NULL)
 * @param high_threshold Pointer to store current high threshold value (can be NULL)
 * @param percentage Pointer to store current threshold percentage of total buffer (can be NULL)
 */
void AudioBuffer_GetThresholdConfig(size_t* low_threshold, size_t* high_threshold, float* percentage);

/**
 * @brief Configure crossfade parameters
 * 
 * @param fade_length Length of crossfade in samples
 * @param curve_factor Curve factor for fade (1.0 = linear, >1 = exponential)
 * @param enable Enable/disable crossfade functionality
 */
void AudioBuffer_ConfigureCrossfade(size_t fade_length, float curve_factor, bool enable);

/**
 * @brief Start a crossfade transition
 * 
 * Initiates a crossfade from the current audio to the next audio segment.
 * Must be called after crossfade has been configured and enabled.
 * 
 * @return true if crossfade started successfully, false if crossfade
 *         is disabled, already in progress, or system not ready
 */
bool AudioBuffer_StartCrossfade(void);

/**
 * @brief Clean up audio buffer resources
 * 
 * Frees all dynamically allocated memory used by the audio buffer system.
 * Should be called when shutting down the audio system.
 */
void AudioBuffer_Cleanup(void);

/**
 * @brief Prepare buffer system for crossfade transition
 * 
 * Sets up the buffer system to handle crossfade between current and next track.
 * This includes allocating temporary buffers and preparing for parallel reads.
 * 
 * @param fade_samples Number of samples over which to perform the crossfade
 * @return true if preparation successful, false if resources unavailable
 */
bool AudioBuffer_PrepareCrossfade(uint32_t fade_samples);

/**
 * @brief Get samples from the next track's buffer for crossfading
 * 
 * Retrieves samples from the next track's buffer during a crossfade transition.
 * Should only be called after AudioBuffer_PrepareCrossfade().
 * 
 * @param buffer Buffer to store the samples from next track
 * @param samples Number of samples to retrieve
 * @return true if samples retrieved successfully, false otherwise
 */
bool AudioBuffer_GetNextTrackSamples(int16_t* buffer, size_t samples);

/**
 * @brief Complete the crossfade transition
 * 
 * Finalizes the crossfade transition, cleaning up temporary buffers and
 * switching to the next track's buffer as the primary buffer.
 * 
 * @return true if transition completed successfully, false otherwise
 */
bool AudioBuffer_CompleteCrossfade(void);

/**
 * @brief Prepare for gapless transition between tracks
 * 
 * Sets up the buffer system for a gapless transition without crossfading.
 * This ensures continuous playback between tracks without mixing.
 * 
 * @return true if preparation successful, false otherwise
 */
bool AudioBuffer_PrepareGaplessTransition(void);

/**
 * @brief Start playback from the buffer
 * 
 * Initializes DMA transfers and starts audio playback from the current buffer position.
 * 
 * @return true if playback started successfully, false otherwise
 */
bool AudioBuffer_StartPlayback(void);

/**
 * Flushes all audio buffers and resets the buffer state.
 * Used when seeking to a new position in the audio file.
 * 
 * @param reset_stats If true, also resets buffer statistics
 * @return true if flush was successful, false otherwise
 */
bool AudioBuffer_Flush(bool reset_stats);

// Add these function declarations
void AudioBuffer_ConfigureSampleRate(uint32_t source_rate, uint32_t target_rate);
void AudioBuffer_GetSampleRateConfig(uint32_t* source_rate, uint32_t* target_rate, 
                                   bool* conversion_enabled, float* ratio);

#endif // AUDIO_BUFFER_H