#include "audio_pipeline.h"
#include "audio_buffer.h"
#include "es9038q2m.h"
#include "platform.h"
#include <string.h>


// Pipeline configuration
typedef struct {
    ES9038Q2M_Config dac_config;
    uint32_t sample_rate;
    uint8_t bit_depth;
    bool gapless_enabled;
    PipelineState state;
    volatile bool transition_pending;
    PipelineStateCallback state_callback;
} AudioPipeline;

static AudioPipeline pipeline;

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
        
        // Prepare for gapless transition if enabled
        if (pipeline.gapless_enabled) {
            AudioBuffer_PrepareGaplessTransition();
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

// Configure pipeline parameters
bool AudioPipeline_Configure(const AudioPipelineConfig* config) {
    if (!config) return false;

    pipeline.sample_rate = config->sample_rate;
    pipeline.bit_depth = config->bit_depth;
    pipeline.gapless_enabled = config->gapless_enabled;

    // Update DAC configuration
    pipeline.dac_config.sample_rate = config->sample_rate;
    pipeline.dac_config.bit_depth = config->bit_depth;
    
    // Apply new DAC settings
    return ES9038Q2M_ConfigureClock(config->sample_rate, config->bit_depth);
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

    // If gapless playback is enabled, prepare for smooth transition
    if (pipeline.gapless_enabled) {
        AudioBuffer_PrepareGaplessTransition();
        
        // Let the buffer system handle the transition timing
        return;
    }

    // Standard (non-gapless) end of file handling
    AudioPipeline_Stop();

    // Attempt to start playing the next track
    if (AudioPipeline_Play()) {
        updatePipelineState(PIPELINE_STATE_PLAYING);
    } else {
        // If we couldn't start the next track, remain stopped
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

// Helper function to handle state transitions
static void updatePipelineState(PipelineState newState) {
    if (pipeline.state != newState) {
        PipelineState oldState = pipeline.state;
        pipeline.state = newState;
        
        // Notify callback if registered
        if (pipeline.state_callback != NULL) {
            pipeline.state_callback(oldState, newState);
        }
    }
}