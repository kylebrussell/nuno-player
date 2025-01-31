#include "audio_buffer.h"
#include "platform.h"
#include <stdbool.h>
#include <string.h>

// Buffer states
typedef enum {
    BUFFER_STATE_EMPTY,
    BUFFER_STATE_PRELOADING,
    BUFFER_STATE_READY,
    BUFFER_STATE_PLAYING,
    BUFFER_STATE_UNDERRUN
} BufferState;

// Double buffer structure
typedef struct {
    uint16_t buffer[2][AUDIO_BUFFER_SIZE];
    volatile bool buffer_ready[2];
    volatile uint8_t active_buffer;
    volatile BufferState state;
    volatile size_t samples_played;
    volatile bool gapless_transition;
} DoubleBuffer;

// Static instances
static CircularBuffer circular_buffer;
static DoubleBuffer double_buffer;

// Initialize the audio buffer system
void AudioBuffer_Init(void) {
    // Initialize circular buffer
    circular_buffer.buffer = (uint16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(uint16_t));
    if (circular_buffer.buffer == NULL) {
        // Handle memory allocation failure
        return;
    }
    circular_buffer.size = AUDIO_BUFFER_SIZE;
    circular_buffer.head = 0;
    circular_buffer.tail = 0;
    circular_buffer.is_full = false;

    // Initialize double buffer
    memset(&double_buffer, 0, sizeof(DoubleBuffer));
    double_buffer.active_buffer = 0;
    double_buffer.state = BUFFER_STATE_EMPTY;
    double_buffer.gapless_transition = false;
}

// Get next buffer for DMA
uint16_t* AudioBuffer_GetBuffer(void) {
    return double_buffer.buffer[double_buffer.active_buffer];
}

// Start audio playback with preload
bool AudioBuffer_StartPlayback(void) {
    if (double_buffer.state == BUFFER_STATE_EMPTY) {
        double_buffer.state = BUFFER_STATE_PRELOADING;
        
        // Preload both buffers
        if (!FillBuffer(0) || !FillBuffer(1)) {
            return false;
        }
        
        double_buffer.buffer_ready[0] = true;
        double_buffer.buffer_ready[1] = true;
        double_buffer.state = BUFFER_STATE_READY;
        
        // Start DMA transfer with first buffer
        DMA_StartTransfer(double_buffer.buffer[0], AUDIO_BUFFER_SIZE);
        double_buffer.state = BUFFER_STATE_PLAYING;
        return true;
    }
    return false;
}

// Fill a specific buffer with audio data
static bool FillBuffer(uint8_t buffer_index) {
    size_t samples_needed = AUDIO_BUFFER_SIZE;
    uint16_t* target_buffer = double_buffer.buffer[buffer_index];
    
    while (samples_needed > 0) {
        uint16_t sample;
        if (!CircularBuffer_Remove(&circular_buffer, &sample)) {
            // Buffer underrun
            double_buffer.state = BUFFER_STATE_UNDERRUN;
            return false;
        }
        *target_buffer++ = sample;
        samples_needed--;
    }
    return true;
}

// DMA complete callback
void AudioBuffer_Done(void) {
    uint8_t current_buffer = double_buffer.active_buffer;
    uint8_t next_buffer = (current_buffer + 1) % 2;
    
    // Switch to next buffer
    double_buffer.active_buffer = next_buffer;
    
    // Start filling the just-completed buffer
    if (!FillBuffer(current_buffer)) {
        AudioBuffer_HandleUnderrun();
        return;
    }
    
    double_buffer.buffer_ready[current_buffer] = true;
    double_buffer.samples_played += AUDIO_BUFFER_SIZE;
    
    // Handle gapless transition if needed
    if (double_buffer.gapless_transition) {
        HandleGaplessTransition();
    }
}

// Half transfer callback
void AudioBuffer_HalfDone(void) {
    // Monitor buffer status
    if (AudioBuffer_IsUnderThreshold()) {
        // Request more data from source
        RequestMoreData();
    }
}

// Check buffer status
bool AudioBuffer_IsUnderThreshold(void) {
    size_t available = (circular_buffer.is_full) ? 
        circular_buffer.size : 
        ((circular_buffer.head >= circular_buffer.tail) ? 
            circular_buffer.head - circular_buffer.tail : 
            circular_buffer.size + circular_buffer.head - circular_buffer.tail);
            
    return available < BUFFER_THRESHOLD;
}

// Handle buffer underrun
void AudioBuffer_HandleUnderrun(void) {
    double_buffer.state = BUFFER_STATE_UNDERRUN;
    
    // Fill buffer with silence
    for (size_t i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        double_buffer.buffer[double_buffer.active_buffer][i] = 0;
    }
    
    // Attempt recovery
    RequestMoreData();
}

// Request more data from source
static void RequestMoreData(void) {
    // Trigger async data fetch from filesystem/source
    // Implementation depends on source interface
}

// Handle gapless playback transition
static void HandleGaplessTransition(void) {
    if (double_buffer.gapless_transition) {
        // Seamlessly transition to next track
        double_buffer.gapless_transition = false;
        double_buffer.samples_played = 0;
    }
}

// Get buffer status
BufferState AudioBuffer_GetState(void) {
    return double_buffer.state;
}

// Enable gapless playback for next track
void AudioBuffer_PrepareGaplessTransition(void) {
    double_buffer.gapless_transition = true;
}