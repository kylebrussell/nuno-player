#include "../../../include/nuno/audio_pipeline.h"
#include "../../../include/nuno/audio_buffer.h"
#include "../../../include/nuno/es9038q2m.h"
#include "../../../include/nuno/platform.h"
#include "../../../include/nuno/dma.h"
#include <string.h>

typedef enum {
    PIPELINE_STATE_STOPPED,
    PIPELINE_STATE_PLAYING,
    PIPELINE_STATE_PAUSED,
    PIPELINE_STATE_CROSSFADE_IN_PROGRESS
} PipelineState;

// Pipeline configuration
typedef struct {
    ES9038Q2M_Config dac_config;
    uint32_t sample_rate;
    uint8_t bit_depth;
    bool gapless_enabled;
    PipelineState state;
    volatile bool transition_pending;
    PipelineStateCallback state_callback;
    uint32_t transition_fade_samples;
    float transition_crossfade_ratio;
    bool crossfade_enabled;
    bool end_of_playlist_reached;  // New flag to track end of playlist
    EndOfPlaylistCallback end_of_playlist_callback;  // New callback for end of playlist
} AudioPipeline;

static AudioPipeline pipeline = {
    .state = PIPELINE_STATE_STOPPED,
    .crossfade_enabled = false,
    .transition_pending = false,
    .transition_crossfade_ratio = 0.0f,
    .end_of_playlist_reached = false,
    .end_of_playlist_callback = NULL
};

#define FADE_OUT_DURATION_MS 50  // 50ms fade out
#define FADE_OUT_SAMPLES (FADE_OUT_DURATION_MS * SAMPLE_RATE / 1000)

// Initialize the audio pipeline
bool AudioPipeline_Init(void) {
    // Initialize pipeline state
    memset(&pipeline, 0, sizeof(AudioPipeline));
    updatePipelineState(PIPELINE_STATE_STOPPED);
    pipeline.sample_rate = 44100;  // Default to CD quality
    pipeline.bit_depth = 16;
    
    // Configure DAC
    pipeline.dac_config.volume_left = 200;
    pipeline.dac_config.volume_right = 200;
    pipeline.dac_config.filter_type = ES9038Q2M_FILTER_FAST_ROLL_OFF;
    pipeline.dac_config.dsd_mode = false;
    pipeline.dac_config.sample_rate = pipeline.sample_rate;
    pipeline.dac_config.bit_depth = pipeline.bit_depth;

    // Initialize DAC
    if (!ES9038Q2M_Init(&pipeline.dac_config)) {
        return false;
    }

    // Initialize audio buffer system
    AudioBuffer_Init();

    return true;
}

// Start playback
bool AudioPipeline_Play(void) {
    if (pipeline.state == PIPELINE_STATE_STOPPED || 
        pipeline.state == PIPELINE_STATE_PAUSED) {
        
        // Start the audio buffer system
        if (!AudioBuffer_StartPlayback()) {
            return false;
        }

        // Power up DAC if needed
        if (pipeline.state == PIPELINE_STATE_STOPPED) {
            if (!ES9038Q2M_PowerUp()) {
                return false;
            }
        }

        updatePipelineState(PIPELINE_STATE_PLAYING);
        return true;
    }
    return false;
}

// Pause playback
bool AudioPipeline_Pause(void) {
    if (pipeline.state == PIPELINE_STATE_PLAYING) {
        // Stop DMA transfers but keep buffers intact
        DMA_PauseTransfer();
        
        updatePipelineState(PIPELINE_STATE_PAUSED);
        return true;
    }
    return false;
}

// Stop playback
bool AudioPipeline_Stop(void) {
    if (pipeline.state != PIPELINE_STATE_STOPPED) {
        // Stop DMA transfers
        DMA_StopTransfer();
        
        // Power down DAC
        ES9038Q2M_PowerDown();
        
        // Reset buffer system
        AudioBuffer_Init();
        
        updatePipelineState(PIPELINE_STATE_STOPPED);
        return true;
    }
    return false;
}

// Skip to next track
bool AudioPipeline_Skip(void) {
    if (pipeline.state == PIPELINE_STATE_PLAYING) {
        pipeline.transition_pending = true;
        
        if (pipeline.gapless_enabled) {
            if (pipeline.crossfade_enabled) {
                // Initialize crossfade
                pipeline.transition_crossfade_ratio = 0.0f;
                AudioBuffer_PrepareCrossfade(pipeline.transition_fade_samples);
            } else {
                AudioBuffer_PrepareGaplessTransition();
            }
        } else {
            // Standard skip - stop current playback
            AudioPipeline_Stop();
            // Start new track
            AudioPipeline_Play();
        }
        return true;
    }
    return false;
}

// Set volume
bool AudioPipeline_SetVolume(uint8_t volume) {
    // Scale volume from 0-100 to DAC range
    uint8_t dac_volume = (volume * 255) / 100;
    return ES9038Q2M_SetVolume(dac_volume, dac_volume);
}

// Add this new function to handle format changes
bool AudioPipeline_ReconfigureFormat(uint32_t new_sample_rate, uint8_t new_bit_depth) {
    // Check if format is actually changing
    if (pipeline.sample_rate == new_sample_rate && pipeline.bit_depth == new_bit_depth) {
        return true; // No change needed
    }
    
    // Save current state
    PipelineState previous_state = pipeline.state;
    
    // Pause playback during reconfiguration
    if (previous_state == PIPELINE_STATE_PLAYING) {
        AudioPipeline_Pause();
    }
    
    // Stop DMA transfers
    DMA_StopTransfer();
    
    // Flush audio buffers to prepare for new format
    AudioBuffer_Flush(false);
    
    // Update pipeline configuration
    pipeline.sample_rate = new_sample_rate;
    pipeline.bit_depth = new_bit_depth;
    
    // Update DAC configuration
    pipeline.dac_config.sample_rate = new_sample_rate;
    pipeline.dac_config.bit_depth = new_bit_depth;
    
    // Reconfigure DAC clock for new format
    if (!ES9038Q2M_ConfigureClock(new_sample_rate, new_bit_depth)) {
        return false;
    }
    
    // Configure audio buffer for new format
    AudioBuffer_ConfigureSampleRate(new_sample_rate, new_sample_rate);
    AudioBuffer_ConfigureSampleFormat(new_bit_depth, false, true);
    
    // If we were previously playing, restart playback
    if (previous_state == PIPELINE_STATE_PLAYING) {
        if (!AudioBuffer_StartPlayback()) {
            return false;
        }
        updatePipelineState(PIPELINE_STATE_PLAYING);
    }
    
    return true;
}

// Modify AudioPipeline_Configure to use the new function
bool AudioPipeline_Configure(const AudioPipelineConfig* config) {
    if (!config) return false;
    
    // Handle sample rate and bit depth changes
    if (pipeline.sample_rate != config->sample_rate || 
        pipeline.bit_depth != config->bit_depth) {
        if (!AudioPipeline_ReconfigureFormat(config->sample_rate, config->bit_depth)) {
            return false;
        }
    }
    
    // Configure other parameters
    pipeline.gapless_enabled = config->gapless_enabled;
    pipeline.crossfade_enabled = config->crossfade_enabled;
    
    // Calculate fade duration in samples (default 50ms crossfade)
    pipeline.transition_fade_samples = (pipeline.sample_rate * 50) / 1000;
    pipeline.transition_crossfade_ratio = 0.0f;
    
    return true;
}

// Get current pipeline state
PipelineState AudioPipeline_GetState(void) {
    return pipeline.state;
}

// Handle buffer underrun
void AudioPipeline_HandleUnderrun(void) {
    // Notify buffer system
    AudioBuffer_HandleUnderrun();
    
    if (pipeline.state == PIPELINE_STATE_PLAYING) {
        // Attempt to recover playback
        AudioPipeline_Play();
    }
}

void AudioPipeline_HandleEndOfFile(void) {
    // If we're already transitioning or not playing, ignore
    if (pipeline.state != PIPELINE_STATE_PLAYING || pipeline.transition_pending) {
        return;
    }

    pipeline.transition_pending = true;

    // Check if there's another track available
    bool has_next_track = AudioBuffer_HasNextTrack();
    
    if (!has_next_track) {
        // We've reached the end of the playlist
        pipeline.end_of_playlist_reached = true;
        
        // Notify via callback if registered
        if (pipeline.end_of_playlist_callback != NULL) {
            pipeline.end_of_playlist_callback();
        }
        
        // Stop playback
        AudioPipeline_Stop();
        return;
    }

    // Continue with normal track transition
    if (pipeline.gapless_enabled) {
        if (pipeline.crossfade_enabled) {
            // Initialize crossfade
            pipeline.transition_crossfade_ratio = 0.0f;
            AudioBuffer_PrepareCrossfade(pipeline.transition_fade_samples);
        } else {
            AudioBuffer_PrepareGaplessTransition();
        }
        return;
    }

    // Standard (non-gapless) end of file handling
    AudioPipeline_Stop();

    if (AudioPipeline_Play()) {
        updatePipelineState(PIPELINE_STATE_PLAYING);
    } else {
        updatePipelineState(PIPELINE_STATE_STOPPED);
        pipeline.transition_pending = false;
    }
}

void AudioPipeline_RegisterStateCallback(PipelineStateCallback callback) {
    pipeline.state_callback = callback;
}

void AudioPipeline_UnregisterStateCallback(void) {
    pipeline.state_callback = NULL;
}

// Register callback for end of playlist notification
void AudioPipeline_RegisterEndOfPlaylistCallback(EndOfPlaylistCallback callback) {
    pipeline.end_of_playlist_callback = callback;
}

// Unregister end of playlist callback
void AudioPipeline_UnregisterEndOfPlaylistCallback(void) {
    pipeline.end_of_playlist_callback = NULL;
}

// Reset end of playlist flag (call when loading a new playlist)
void AudioPipeline_ResetEndOfPlaylistFlag(void) {
    pipeline.end_of_playlist_reached = false;
}

// Check if end of playlist has been reached
bool AudioPipeline_IsEndOfPlaylistReached(void) {
    return pipeline.end_of_playlist_reached;
}

// Helper function to handle state transitions
static void updatePipelineState(PipelineState newState) {
    if (pipeline.state != newState) {
        // Validate state transition
        bool valid_transition = true;
        
        switch (pipeline.state) {
            case PIPELINE_STATE_STOPPED:
                // Can only go to PLAYING or PAUSED from STOPPED
                valid_transition = (newState == PIPELINE_STATE_PLAYING || 
                                  newState == PIPELINE_STATE_PAUSED);
                break;
                
            case PIPELINE_STATE_PLAYING:
                // Can go to any state from PLAYING
                break;
                
            case PIPELINE_STATE_PAUSED:
                // Cannot go directly to CROSSFADE from PAUSED
                valid_transition = (newState != PIPELINE_STATE_CROSSFADE_IN_PROGRESS);
                break;
                
            case PIPELINE_STATE_CROSSFADE_IN_PROGRESS:
                // Can only go to PLAYING or STOPPED from CROSSFADE
                valid_transition = (newState == PIPELINE_STATE_PLAYING || 
                                  newState == PIPELINE_STATE_STOPPED);
                break;
        }
        
        if (!valid_transition) {
            // Log invalid transition attempt
            return;
        }

        PipelineState oldState = pipeline.state;
        pipeline.state = newState;
        
        // Notify callback if registered
        if (pipeline.state_callback != NULL) {
            pipeline.state_callback(oldState, newState);
        }
    }
}

// Add new function to handle crossfade processing
void AudioPipeline_ProcessCrossfade(int16_t* buffer, size_t samples) {
    if (!pipeline.transition_pending || !pipeline.crossfade_enabled) {
        return;
    }

    // Get samples from next track's buffer
    int16_t next_buffer[samples];
    if (!AudioBuffer_GetNextTrackSamples(next_buffer, samples)) {
        return;
    }

    // Apply crossfade
    for (size_t i = 0; i < samples; i++) {
        float fade_out = 1.0f - pipeline.transition_crossfade_ratio;
        float fade_in = pipeline.transition_crossfade_ratio;
        
        buffer[i] = (int16_t)(
            (float)buffer[i] * fade_out +
            (float)next_buffer[i] * fade_in
        );

        // Update crossfade ratio
        pipeline.transition_crossfade_ratio += 1.0f / pipeline.transition_fade_samples;
        
        // Check if crossfade is complete
        if (pipeline.transition_crossfade_ratio >= 1.0f) {
            AudioBuffer_CompleteCrossfade();
            pipeline.transition_pending = false;
            pipeline.transition_crossfade_ratio = 0.0f;
            break;
        }
    }
}

void AudioPipeline_SynchronizeState(void) {
    BufferState bufferState = AudioBuffer_GetState();
    
    // Validate current state combinations
    bool state_mismatch = false;
    
    // Check for invalid state combinations
    if (pipeline.state == PIPELINE_STATE_PLAYING && 
        (bufferState == BUFFER_STATE_EMPTY || bufferState == BUFFER_STATE_PRELOADING)) {
        state_mismatch = true;
    }
    
    if (pipeline.state == PIPELINE_STATE_STOPPED && 
        (bufferState == BUFFER_STATE_PLAYING || bufferState == BUFFER_STATE_READY)) {
        state_mismatch = true;
    }
    
    // Force realignment if states are invalid
    if (state_mismatch) {
        DMA_StopTransfer();
        AudioBuffer_Flush(false);
        updatePipelineState(PIPELINE_STATE_STOPPED);
        return;
    }

    switch (bufferState) {
        case BUFFER_STATE_EMPTY:
            if (pipeline.state != PIPELINE_STATE_STOPPED) {
                updatePipelineState(PIPELINE_STATE_STOPPED);
            }
            break;
            
        case BUFFER_STATE_UNDERRUN:
            if (pipeline.state == PIPELINE_STATE_PLAYING) {
                AudioPipeline_HandleUnderrun();
            } else {
                // If we're not playing, treat underrun as a stop condition
                updatePipelineState(PIPELINE_STATE_STOPPED);
            }
            break;
            
        case BUFFER_STATE_PLAYING:
            if (pipeline.state == PIPELINE_STATE_PAUSED) {
                DMA_PauseTransfer();
                AudioBuffer_Pause();  // Add this function to audio_buffer.h
            } else if (pipeline.state == PIPELINE_STATE_STOPPED) {
                updatePipelineState(PIPELINE_STATE_PLAYING);
            }
            break;
            
        case BUFFER_STATE_PRELOADING:
            // Ensure we're not in an active state while preloading
            if (pipeline.state == PIPELINE_STATE_PLAYING || 
                pipeline.state == PIPELINE_STATE_CROSSFADE_IN_PROGRESS) {
                updatePipelineState(PIPELINE_STATE_PAUSED);
            }
            break;
            
        case BUFFER_STATE_READY:
            // If we were paused and buffer is ready, we can resume
            if (pipeline.state == PIPELINE_STATE_PAUSED && !pipeline.transition_pending) {
                updatePipelineState(PIPELINE_STATE_PLAYING);
            }
            break;
    }
    
    if (pipeline.crossfade_enabled && crossfade_config.in_progress) {
        if (pipeline.state != PIPELINE_STATE_CROSSFADE_IN_PROGRESS) {
            updatePipelineState(PIPELINE_STATE_CROSSFADE_IN_PROGRESS);
        }
    } else if (pipeline.state == PIPELINE_STATE_CROSSFADE_IN_PROGRESS) {
        // Crossfade ended but state wasn't updated
        updatePipelineState(PIPELINE_STATE_PLAYING);
    }
}

void AudioPipeline_NotifyTransitionComplete(void) {
    pipeline.transition_pending = false;
    
    if (pipeline.state == PIPELINE_STATE_CROSSFADE_IN_PROGRESS) {
        updatePipelineState(PIPELINE_STATE_PLAYING);
    }
}

void AudioPipeline_NotifyCrossfadeComplete(void) {
    pipeline.transition_pending = false;
    pipeline.transition_crossfade_ratio = 0.0f;
    
    if (pipeline.state == PIPELINE_STATE_CROSSFADE_IN_PROGRESS) {
        updatePipelineState(PIPELINE_STATE_PLAYING);
    }
}

bool AudioPipeline_Seek(size_t sample_position) {
    // Save current state
    PipelineState previous_state = pipeline.state;
    
    // Stop DMA transfers
    DMA_StopTransfer();
    
    // If we were in a crossfade, cancel it
    if (pipeline.state == PIPELINE_STATE_CROSSFADE_IN_PROGRESS) {
        pipeline.transition_pending = false;
        pipeline.transition_crossfade_ratio = 0.0f;
    }
    
    // Temporarily set state to paused during seek
    updatePipelineState(PIPELINE_STATE_PAUSED);
    
    // Perform the seek operation in the buffer system
    if (!AudioBuffer_Seek(sample_position)) {
        // If seek failed, try to restore previous state
        if (previous_state == PIPELINE_STATE_PLAYING) {
            AudioPipeline_Play();
        }
        return false;
    }
    
    // If we were previously playing, restart playback
    if (previous_state == PIPELINE_STATE_PLAYING) {
        if (!AudioBuffer_StartPlayback()) {
            return false;
        }
        updatePipelineState(PIPELINE_STATE_PLAYING);
    }
    
    return true;
}