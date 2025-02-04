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

#endif // AUDIO_BUFFER_H