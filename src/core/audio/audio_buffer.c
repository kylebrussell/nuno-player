#include "audio_buffer.h"
#include "platform.h"
#include <stdbool.h>

// External DMA Handle
extern DMA_HandleTypeDef hdma_audio;

// Circular Buffer Instance
static CircularBuffer circular_buffer;

// Initialize the audio buffer
void AudioBuffer_Init(void) {
    circular_buffer.buffer = (uint16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(uint16_t));
    if (circular_buffer.buffer == NULL) {
        // Handle memory allocation failure
    }
    circular_buffer.size = AUDIO_BUFFER_SIZE;
    circular_buffer.head = 0;
    circular_buffer.tail = 0;
    circular_buffer.is_full = false;

    // Additional initialization if required
}

// Get pointer to the next audio data chunk
uint16_t* AudioBuffer_GetBuffer(void) {
    return circular_buffer.buffer;
}

// Notify that the DMA has completed a chunk
void AudioBuffer_Done(void) {
    // Refill the buffer with new audio data
    // Example: FetchData(circular_buffer.buffer, AUDIO_BUFFER_SIZE);
    // This function should be implemented to retrieve audio data from the source

    // Check for underrun
    if (AudioBuffer_IsUnderThreshold()) {
        AudioBuffer_HandleUnderrun();
    }
}

// Notify that half of the DMA buffer has been filled
void AudioBuffer_HalfDone(void) {
    // Refill the first half of the buffer
    // Example: FetchData(circular_buffer.buffer, AUDIO_BUFFER_SIZE / 2);
    // This function should be implemented to retrieve audio data from the source

    // Check for underrun
    if (AudioBuffer_IsUnderThreshold()) {
        AudioBuffer_HandleUnderrun();
    }
}

// Check if the buffer is running low on data to prevent underrun
bool AudioBuffer_IsUnderThreshold(void) {
    size_t available = (circular_buffer.is_full) ? circular_buffer.size : (circular_buffer.head >= circular_buffer.tail ? circular_buffer.head - circular_buffer.tail : circular_buffer.size + circular_buffer.head - circular_buffer.tail);
    return available < BUFFER_THRESHOLD;
}

// Handle buffer underrun scenario
void AudioBuffer_HandleUnderrun(void) {
    // Implement underrun handling logic, such as:
    // - Pausing audio playback
    // - Filling buffer with silence
    // - Logging error
    // - Attempting to recover by fetching more data

    // Example: Fill buffer with silence
    for (size_t i = 0; i < BUFFER_THRESHOLD; i++) {
        AudioBuffer_Add(&circular_buffer, 0); // 0 represents silence
    }

    // Optionally notify the system about the underrun
}