#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SAMPLE_RATE 44100  // or whatever your sample rate is

// Pipeline state enumeration
typedef enum {
    PIPELINE_STATE_STOPPED,
    PIPELINE_STATE_PLAYING,
    PIPELINE_STATE_PAUSED,
    PIPELINE_STATE_TRANSITIONING
} PipelineState;

// Pipeline configuration structure
typedef struct {
    uint32_t sample_rate;    // Sample rate in Hz (e.g., 44100, 48000)
    uint8_t bit_depth;       // Bit depth (e.g., 16, 24, 32)
    bool gapless_enabled;    // Enable gapless playback transitions
    bool crossfade_enabled;  // Enable crossfade during track transitions
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

bool AudioPipeline_PlayTrack(size_t track_index);

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

/**
 * @brief Handle end of file condition
 * 
 * This function is called by the audio buffer system when it detects
 * the end of the current audio file. It should handle transitioning
 * to the next track if available, or stopping playback if at the end
 * of the playlist.
 */
void AudioPipeline_HandleEndOfFile(void);

// Add these declarations at the end of the header file, before the #endif

// Callback function type for state changes
typedef void (*PipelineStateCallback)(PipelineState oldState, PipelineState newState);

/**
 * @brief Register a callback for pipeline state changes
 * @param callback Function to be called when pipeline state changes
 */
void AudioPipeline_RegisterStateCallback(PipelineStateCallback callback);

/**
 * @brief Unregister the state change callback
 */
void AudioPipeline_UnregisterStateCallback(void);

/**
 * @brief Process crossfade between current and next track
 * 
 * This function handles the mixing of audio samples during track transitions
 * when crossfade is enabled. It should be called by the audio processing loop
 * for each buffer of samples.
 * 
 * @param buffer Pointer to the current audio buffer
 * @param samples Number of samples in the buffer
 */
void AudioPipeline_ProcessCrossfade(int16_t* buffer, size_t samples);

/**
 * @brief Seek to a specific sample position in the audio stream
 * 
 * Performs a coordinated seek operation that:
 * 1. Safely stops ongoing playback
 * 2. Seeks the underlying buffer system
 * 3. Restarts playback if previously playing
 * 
 * @param sample_position Target sample position to seek to
 * @return true if seek operation was successful, false otherwise
 */
bool AudioPipeline_Seek(size_t sample_position);

/**
 * @brief Reconfigure audio format during playback
 * 
 * This function handles real-time changes to sample rate or bit depth,
 * ensuring proper buffer draining and DAC reconfiguration.
 * 
 * @param new_sample_rate New sample rate in Hz
 * @param new_bit_depth New bit depth (8, 16, 24, or 32)
 * @return true if reconfiguration successful, false otherwise
 */
bool AudioPipeline_ReconfigureFormat(uint32_t new_sample_rate, uint8_t new_bit_depth);

// Add this after the PipelineStateCallback definition
// Callback function type for end of playlist notification
typedef void (*EndOfPlaylistCallback)(void);

/**
 * @brief Register a callback for end of playlist notification
 * @param callback Function to be called when the end of playlist is reached
 */
void AudioPipeline_RegisterEndOfPlaylistCallback(EndOfPlaylistCallback callback);

/**
 * @brief Unregister the end of playlist callback
 */
void AudioPipeline_UnregisterEndOfPlaylistCallback(void);

/**
 * @brief Reset end of playlist flag
 * 
 * Call this when loading a new playlist to clear the end-of-playlist state
 */
void AudioPipeline_ResetEndOfPlaylistFlag(void);

/**
 * @brief Check if end of playlist has been reached
 * @return true if the end of playlist has been reached, false otherwise
 */
bool AudioPipeline_IsEndOfPlaylistReached(void);

void AudioPipeline_SynchronizeState(void);
void AudioPipeline_NotifyTransitionComplete(void);
void AudioPipeline_NotifyCrossfadeComplete(void);

#endif /* AUDIO_PIPELINE_H */
