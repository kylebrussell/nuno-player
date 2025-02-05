#include "nuno/audio_buffer.h"
#include "nuno/platform.h"
#include "nuno/filesystem.h"
#include "nuno/audio_pipeline.h"
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

// Maximum number of read retries before giving up
#define MAX_READ_RETRIES 3

// Read retry delay in milliseconds
#define READ_RETRY_DELAY_MS 5

// Error tracking
static struct {
    size_t read_errors;
    size_t retry_successes;
    size_t total_underruns;
} error_stats = {0};

// Detailed underrun tracking
static struct {
    uint32_t last_underrun_timestamp;
    size_t samples_lost;
    uint32_t recovery_time_ms;
    void (*underrun_callback)(void);
} underrun_details = {0};

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
    uint32_t start_time = platform_get_time_ms();
    
    // Update state and stats
    double_buffer.state = BUFFER_STATE_UNDERRUN;
    error_stats.total_underruns++;
    
    // Record underrun details
    underrun_details.last_underrun_timestamp = start_time;
    underrun_details.samples_lost = AUDIO_BUFFER_SIZE; // At least one buffer worth
    
    // Fill buffer with silence
    for (size_t i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        double_buffer.buffer[double_buffer.active_buffer][i] = 0;
    }
    
    // Attempt recovery
    RequestMoreData();
    
    // Calculate recovery time
    underrun_details.recovery_time_ms = platform_get_time_ms() - start_time;
    
    // Notify via callback if registered
    if (underrun_details.underrun_callback != NULL) {
        underrun_details.underrun_callback();
    }
}

void AudioBuffer_GetUnderrunDetails(uint32_t* timestamp, 
                                  size_t* samples_lost,
                                  uint32_t* recovery_time_ms) {
    if (timestamp) *timestamp = underrun_details.last_underrun_timestamp;
    if (samples_lost) *samples_lost = underrun_details.samples_lost;
    if (recovery_time_ms) *recovery_time_ms = underrun_details.recovery_time_ms;
}

void AudioBuffer_RegisterUnderrunCallback(void (*callback)(void)) {
    underrun_details.underrun_callback = callback;
}

// Request more data from source with retry logic
static void RequestMoreData(void) {
    // Only request more data if we have space available
    if (circular_buffer.is_full) {
        return;
    }

    // Calculate available space
    size_t available_space;
    if (circular_buffer.head >= circular_buffer.tail) {
        available_space = circular_buffer.size - (circular_buffer.head - circular_buffer.tail);
    } else {
        available_space = circular_buffer.tail - circular_buffer.head;
    }

    // Only trigger read if we have enough space
    if (available_space >= BUFFER_THRESHOLD) {
        uint16_t temp_buffer[BUFFER_THRESHOLD];
        size_t bytes_read = 0;
        bool read_success = false;
        uint8_t retry_count = 0;

        // Retry loop for reading data
        while (!read_success && retry_count < MAX_READ_RETRIES) {
            bytes_read = FileSystem_ReadAudioData(
                temp_buffer,
                BUFFER_THRESHOLD * sizeof(uint16_t)
            );

            if (bytes_read > 0) {
                read_success = true;
                if (retry_count > 0) {
                    error_stats.retry_successes++;
                }
            } else {
                retry_count++;
                error_stats.read_errors++;
                
                // Add delay between retries to allow system recovery
                platform_delay_ms(READ_RETRY_DELAY_MS);
            }
        }

        if (read_success) {
            // Convert bytes read to number of samples
            size_t samples_read = bytes_read / sizeof(uint16_t);

            // Add samples to circular buffer
            size_t samples_added = 0;
            for (size_t i = 0; i < samples_read; i++) {
                if (!CircularBuffer_Add(&circular_buffer, temp_buffer[i])) {
                    // Buffer is full
                    break;
                }
                samples_added++;
            }

            // If we couldn't read enough data, we may be at end of file
            if (samples_read < BUFFER_THRESHOLD) {
                AudioPipeline_HandleEndOfFile();
            }
        } else {
            // All retries failed
            error_stats.total_underruns++;
            AudioBuffer_HandleUnderrun();
        }
    }
}

// Get error statistics
void AudioBuffer_GetErrorStats(size_t* read_errors, size_t* retry_successes, size_t* total_underruns) {
    if (read_errors) *read_errors = error_stats.read_errors;
    if (retry_successes) *retry_successes = error_stats.retry_successes;
    if (total_underruns) *total_underruns = error_stats.total_underruns;
}

// Reset error statistics
void AudioBuffer_ResetErrorStats(void) {
    memset(&error_stats, 0, sizeof(error_stats));
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