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

// Read chunk configuration
static struct {
    size_t min_chunk_size;
    size_t max_chunk_size;
    size_t optimal_chunk_size;
} read_config = {
    .min_chunk_size = 512,      // Minimum bytes to read at once
    .max_chunk_size = 8192,     // Maximum bytes to read at once
    .optimal_chunk_size = 4096  // Default optimal read size
};

// Buffer threshold configuration
static struct {
    size_t low_threshold;
    size_t high_threshold;
    float threshold_percentage;
} buffer_config = {
    .low_threshold = BUFFER_THRESHOLD / 2,  // Default to 50% of BUFFER_THRESHOLD
    .high_threshold = BUFFER_THRESHOLD,     // Default to BUFFER_THRESHOLD
    .threshold_percentage = 0.5f            // Default to 50%
};

// Buffer statistics tracking
static struct {
    size_t total_samples_processed;
    size_t successful_transitions;
    size_t buffer_underruns;
    uint32_t last_transition_timestamp;
    float average_buffer_utilization;
} buffer_stats = {0};

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

void AudioBuffer_ConfigureReadChunks(size_t min_size, size_t max_size, size_t optimal_size) {
    // Validate parameters
    if (min_size > max_size || optimal_size < min_size || optimal_size > max_size) {
        return; // Invalid configuration
    }
    
    // Ensure sizes are multiples of 2 for alignment
    min_size &= ~1;
    max_size &= ~1;
    optimal_size &= ~1;
    
    if (min_size < 32) min_size = 32; // Enforce minimum reasonable size
    
    read_config.min_chunk_size = min_size;
    read_config.max_chunk_size = max_size;
    read_config.optimal_chunk_size = optimal_size;
}

void AudioBuffer_GetReadChunkConfig(size_t* min_size, size_t* max_size, size_t* optimal_size) {
    if (min_size) *min_size = read_config.min_chunk_size;
    if (max_size) *max_size = read_config.max_chunk_size;
    if (optimal_size) *optimal_size = read_config.optimal_chunk_size;
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
        buffer_stats.buffer_underruns++;  // Track underrun
        AudioBuffer_HandleUnderrun();
        return;
    }
    
    double_buffer.buffer_ready[current_buffer] = true;
    double_buffer.samples_played += AUDIO_BUFFER_SIZE;
    buffer_stats.total_samples_processed += AUDIO_BUFFER_SIZE;
    
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
            
    return available < buffer_config.low_threshold;
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

static void RequestMoreData(void) {
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

    // Determine optimal read size based on available space and configuration
    size_t read_size = read_config.optimal_chunk_size;
    
    // Don't read more than available space
    if (read_size > available_space) {
        read_size = available_space;
    }
    
    // Clamp to configured limits
    if (read_size > read_config.max_chunk_size) {
        read_size = read_config.max_chunk_size;
    } else if (read_size < read_config.min_chunk_size) {
        // If we can't read at least the minimum size, wait for more space
        if (available_space < read_config.min_chunk_size) {
            return;
        }
        read_size = read_config.min_chunk_size;
    }

    // Ensure read size is even for 16-bit samples
    read_size &= ~1;
    
    if (read_size >= read_config.min_chunk_size) {
        uint16_t* temp_buffer = (uint16_t*)malloc(read_size);
        if (!temp_buffer) {
            error_stats.read_errors++;
            return;
        }

        size_t bytes_read = 0;
        bool read_success = false;
        uint8_t retry_count = 0;

        while (!read_success && retry_count < MAX_READ_RETRIES) {
            bytes_read = FileSystem_ReadAudioData(temp_buffer, read_size);

            if (bytes_read > 0) {
                read_success = true;
                if (retry_count > 0) {
                    error_stats.retry_successes++;
                }
            } else {
                retry_count++;
                error_stats.read_errors++;
                platform_delay_ms(READ_RETRY_DELAY_MS);
            }
        }

        if (read_success) {
            size_t samples_read = bytes_read / sizeof(uint16_t);
            size_t samples_added = 0;

            for (size_t i = 0; i < samples_read; i++) {
                if (!CircularBuffer_Add(&circular_buffer, temp_buffer[i])) {
                    break;
                }
                samples_added++;
            }

            if (samples_read < (read_size / sizeof(uint16_t))) {
                AudioPipeline_HandleEndOfFile();
            }
        } else {
            error_stats.total_underruns++;
            AudioBuffer_HandleUnderrun();
        }

        free(temp_buffer);
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
        buffer_stats.successful_transitions++;
        buffer_stats.last_transition_timestamp = platform_get_time_ms();
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

// Get buffer statistics
void AudioBuffer_GetBufferStats(size_t* total_samples,
                              size_t* transitions,
                              size_t* underruns,
                              uint32_t* last_transition_time,
                              float* buffer_utilization) {
    if (total_samples) *total_samples = buffer_stats.total_samples_processed;
    if (transitions) *transitions = buffer_stats.successful_transitions;
    if (underruns) *underruns = buffer_stats.buffer_underruns;
    if (last_transition_time) *last_transition_time = buffer_stats.last_transition_timestamp;
    if (buffer_utilization) *buffer_utilization = buffer_stats.average_buffer_utilization;
}

// Reset buffer statistics
void AudioBuffer_ResetBufferStats(void) {
    memset(&buffer_stats, 0, sizeof(buffer_stats));
}

// Update buffer utilization (call this periodically, e.g., in AudioBuffer_HalfDone)
static void UpdateBufferUtilization(void) {
    size_t available = (circular_buffer.is_full) ? 
        circular_buffer.size : 
        ((circular_buffer.head >= circular_buffer.tail) ? 
            circular_buffer.head - circular_buffer.tail : 
            circular_buffer.size + circular_buffer.head - circular_buffer.tail);
    
    float current_utilization = (float)available / circular_buffer.size;
    // Simple moving average
    buffer_stats.average_buffer_utilization = 
        (buffer_stats.average_buffer_utilization * 0.95f) + (current_utilization * 0.05f);
}

// Configure buffer thresholds
void AudioBuffer_ConfigureThresholds(size_t low_threshold, size_t high_threshold) {
    if (low_threshold > high_threshold || high_threshold > AUDIO_BUFFER_SIZE) {
        return; // Invalid configuration
    }
    
    buffer_config.low_threshold = low_threshold;
    buffer_config.high_threshold = high_threshold;
    buffer_config.threshold_percentage = (float)low_threshold / AUDIO_BUFFER_SIZE;
}

// Get current threshold configuration
void AudioBuffer_GetThresholdConfig(size_t* low_threshold, size_t* high_threshold, float* percentage) {
    if (low_threshold) *low_threshold = buffer_config.low_threshold;
    if (high_threshold) *high_threshold = buffer_config.high_threshold;
    if (percentage) *percentage = buffer_config.threshold_percentage;
}