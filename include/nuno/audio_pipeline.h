#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>

// Pipeline configuration structure
typedef struct {
    uint32_t sample_rate;    // Sample rate in Hz (e.g., 44100, 48000)
    uint8_t bit_depth;       // Bit depth (e.g., 16, 24, 32)
    bool gapless_enabled;    // Enable gapless playback transitions
} AudioPipelineConfig;

/**
 * @brief Initialize the audio pipeline
 * @return true if initialization successful, false otherwise
 */
bool AudioPipeline_Init(void);

/**
 * @brief Start audio playback
 * @return true if playback started successfully, false otherwise
 */
bool AudioPipeline_Play(void);

/**
 * @brief Pause audio playback
 * @return true if paused successfully, false otherwise
 */
bool AudioPipeline_Pause(void);

/**
 * @brief Stop audio playback
 * @return true if stopped successfully, false otherwise
 */
bool AudioPipeline_Stop(void);

/**
 * @brief Skip to next track
 * @return true if skip initiated successfully, false otherwise
 */
bool AudioPipeline_Skip(void);

/**
 * @brief Set audio volume
 * @param volume Volume level (0-100)
 * @return true if volume set successfully, false otherwise
 */
bool AudioPipeline_SetVolume(uint8_t volume);

/**
 * @brief Configure pipeline parameters
 * @param config Pointer to configuration structure
 * @return true if configuration successful, false otherwise
 */
bool AudioPipeline_Configure(const AudioPipelineConfig* config);

/**
 * @brief Get current pipeline state
 * @return Current state of the pipeline
 */
PipelineState AudioPipeline_GetState(void);

/**
 * @brief Handle buffer underrun condition
 */
void AudioPipeline_HandleUnderrun(void);

#endif /* AUDIO_PIPELINE_H */