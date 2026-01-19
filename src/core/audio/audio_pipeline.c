#include "nuno/audio_pipeline.h"

#include "nuno/audio_buffer.h"
#include "nuno/dma.h"
#include "nuno/audio_codec.h"
#include "nuno/format_decoder.h"
#include "nuno/music_library.h"
#include "nuno/platform.h"

#include <stdio.h>
#include <string.h>

#if !defined(PATH_MAX)
#define PATH_MAX 512
#endif

typedef struct {
    AudioPipelineConfig config;
    PipelineState state;
    PipelineStateCallback state_callback;
    EndOfPlaylistCallback playlist_callback;
    bool end_of_playlist;
    bool transition_pending;
} AudioPipelineContext;

static AudioPipelineContext g_pipeline;

static void set_state(PipelineState new_state);
static bool ensure_buffer_ready(void);
static void configure_codec(uint32_t sample_rate, uint8_t bit_depth);
static void update_next_track_status(void);

bool AudioPipeline_Init(void) {
    printf("AudioPipeline_Init starting...\n");

    memset(&g_pipeline, 0, sizeof(g_pipeline));

    g_pipeline.config.sample_rate = SAMPLE_RATE;
    g_pipeline.config.bit_depth = 16U;
    g_pipeline.config.gapless_enabled = false;
    g_pipeline.config.crossfade_enabled = false;

    printf("Initializing audio buffer...\n");
    if (!AudioBuffer_Init()) {
        printf("AudioBuffer_Init failed\n");
        return false;
    }
    printf("Audio buffer initialized\n");

    printf("Initializing audio codec...\n");
    if (!AudioCodec_Init(g_pipeline.config.sample_rate, g_pipeline.config.bit_depth)) {
        printf("AudioCodec_Init failed\n");
        return false;
    }
    printf("Audio codec initialized\n");

    printf("Initializing music library with path: %s\n", NUNO_DEFAULT_LIBRARY_PATH);
    if (!MusicLibrary_Init(NUNO_DEFAULT_LIBRARY_PATH)) {
        printf("MusicLibrary_Init failed\n");
        return false;
    }
    printf("Music library initialized\n");

    update_next_track_status();

    set_state(PIPELINE_STATE_STOPPED);
    printf("AudioPipeline_Init completed successfully\n");
    return true;
}

bool AudioPipeline_Play(void) {
    if (g_pipeline.state == PIPELINE_STATE_PLAYING) {
        return true;
    }

    if (!MusicLibrary_GetCurrentTrack()) {
        if (!MusicLibrary_OpenTrack(0U)) {
            return false;
        }
    }

    // Create and set up format decoder for the current track if not already done
    const MusicLibraryTrack* track = MusicLibrary_GetCurrentTrack();
    if (track && !AudioBuffer_GetDecoder()) {  // Check if decoder is already set
        FormatDecoder* decoder = format_decoder_create();
        if (decoder) {
            char full_path[PATH_MAX];
            if (MusicLibrary_GetRoot() && strlen(MusicLibrary_GetRoot()) > 0) {
                snprintf(full_path, sizeof(full_path), "%s/%s", MusicLibrary_GetRoot(), track->filename);
            } else {
                strncpy(full_path, track->filename, sizeof(full_path) - 1);
                full_path[sizeof(full_path) - 1] = '\0';
            }

            if (format_decoder_open(decoder, full_path)) {
                AudioBuffer_SetDecoder(decoder);
            } else {
                format_decoder_destroy(decoder);
            }
        }
    }

    if (!ensure_buffer_ready()) {
        return false;
    }

    if (!AudioCodec_PowerUp()) {
        return false;
    }

    // Ensure audio streaming is (re)started when transitioning to PLAYING
    (void)DMA_StartTransfer(AudioBuffer_GetBuffer(), AUDIO_BUFFER_SIZE);

    set_state(PIPELINE_STATE_PLAYING);
    return true;
}

bool AudioPipeline_Pause(void) {
    if (g_pipeline.state != PIPELINE_STATE_PLAYING) {
        return true;
    }

    DMA_PauseTransfer();
    AudioBuffer_Pause();
    AudioCodec_PowerDown();

    set_state(PIPELINE_STATE_PAUSED);
    return true;
}

bool AudioPipeline_Stop(void) {
    if (g_pipeline.state == PIPELINE_STATE_STOPPED) {
        return true;
    }

    DMA_StopTransfer();
    AudioBuffer_Flush(false);
    AudioBuffer_ClearDecoder();
    AudioCodec_PowerDown();

    set_state(PIPELINE_STATE_STOPPED);
    g_pipeline.transition_pending = false;
    return true;
}

bool AudioPipeline_Skip(void) {
    g_pipeline.transition_pending = true;

    if (!MusicLibrary_OpenNextTrack()) {
        g_pipeline.transition_pending = false;
        g_pipeline.end_of_playlist = true;
        return false;
    }

    update_next_track_status();

    if (!AudioBuffer_Flush(false)) {
        return false;
    }

    g_pipeline.end_of_playlist = false;
    return ensure_buffer_ready();
}

bool AudioPipeline_Previous(void) {
    g_pipeline.transition_pending = true;

    if (!MusicLibrary_OpenPreviousTrack()) {
        g_pipeline.transition_pending = false;
        return false;
    }

    update_next_track_status();

    if (!AudioBuffer_Flush(false)) {
        return false;
    }

    return ensure_buffer_ready();
}

bool AudioPipeline_PlayTrack(size_t track_index) {
    if (g_pipeline.state == PIPELINE_STATE_PLAYING) {
        AudioPipeline_Stop();
    }

    printf("Trying to open track %zu...\n", track_index);
    if (!MusicLibrary_OpenTrack(track_index)) {
        printf("Failed to open track %zu\n", track_index);
        return false;
    }
    printf("Successfully opened track %zu\n", track_index);

    // Create and set up format decoder for the track
    const MusicLibraryTrack* track = MusicLibrary_GetCurrentTrack();
    if (track) {
        printf("Track: %s - %s by %s\n", track->title, track->album, track->artist);
        char full_path[PATH_MAX];
        if (MusicLibrary_GetRoot() && strlen(MusicLibrary_GetRoot()) > 0) {
            snprintf(full_path, sizeof(full_path), "%s/%s", MusicLibrary_GetRoot(), track->filename);
        } else {
            strncpy(full_path, track->filename, sizeof(full_path) - 1);
            full_path[sizeof(full_path) - 1] = '\0';
        }
        printf("Opening file: %s\n", full_path);

        FormatDecoder* decoder = format_decoder_create();
        if (decoder) {
            if (format_decoder_open(decoder, full_path)) {
                printf("Successfully opened decoder\n");
                AudioBuffer_SetDecoder(decoder);
            } else {
                printf("Failed to open decoder\n");
                format_decoder_destroy(decoder);
            }
        } else {
            printf("Failed to create decoder\n");
        }
    } else {
        printf("No track found for index %zu\n", track_index);
    }

    update_next_track_status();
    AudioPipeline_ResetEndOfPlaylistFlag();

    if (!AudioBuffer_Flush(true)) {
        printf("Failed to flush audio buffer\n");
        return false;
    }

    return AudioPipeline_Play();
}

bool AudioPipeline_SetVolume(uint8_t volume) {
    return AudioCodec_SetVolume(volume);
}

bool AudioPipeline_Configure(const AudioPipelineConfig *config) {
    if (!config) {
        return false;
    }

    g_pipeline.config = *config;
    configure_codec(config->sample_rate, config->bit_depth);

    if (g_pipeline.state == PIPELINE_STATE_PLAYING) {
        ensure_buffer_ready();
    }

    return true;
}

PipelineState AudioPipeline_GetState(void) {
    return g_pipeline.state;
}

void AudioPipeline_HandleUnderrun(void) {
    AudioBuffer_HandleUnderrun();
    set_state(PIPELINE_STATE_TRANSITIONING);
}

void AudioPipeline_HandleEndOfFile(void) {
    g_pipeline.transition_pending = false;
    if (MusicLibrary_OpenNextTrack()) {
        update_next_track_status();
        if (!AudioBuffer_Flush(false)) {
            g_pipeline.end_of_playlist = true;
            set_state(PIPELINE_STATE_STOPPED);
            if (g_pipeline.playlist_callback) {
                g_pipeline.playlist_callback();
            }
            return;
        }

        if (!ensure_buffer_ready()) {
            g_pipeline.end_of_playlist = true;
            set_state(PIPELINE_STATE_STOPPED);
            if (g_pipeline.playlist_callback) {
                g_pipeline.playlist_callback();
            }
            return;
        }

        g_pipeline.end_of_playlist = false;
        set_state(PIPELINE_STATE_PLAYING);
        return;
    }

    g_pipeline.end_of_playlist = true;
    set_state(PIPELINE_STATE_STOPPED);
    if (g_pipeline.playlist_callback) {
        g_pipeline.playlist_callback();
    }
}

void AudioPipeline_RegisterStateCallback(PipelineStateCallback callback) {
    g_pipeline.state_callback = callback;
}

void AudioPipeline_UnregisterStateCallback(void) {
    g_pipeline.state_callback = NULL;
}

void AudioPipeline_ProcessCrossfade(int16_t *buffer, size_t samples) {
    (void)buffer;
    (void)samples;
    /* Crossfade is not implemented in the simplified pipeline. */
}

bool AudioPipeline_Seek(size_t sample_position) {
    bool was_playing = (g_pipeline.state == PIPELINE_STATE_PLAYING);
    if (was_playing) {
        AudioPipeline_Pause();
    }

    if (!AudioBuffer_Seek(sample_position)) {
        return false;
    }

    if (was_playing) {
        return AudioPipeline_Play();
    }
    return true;
}

bool AudioPipeline_ReconfigureFormat(uint32_t new_sample_rate, uint8_t new_bit_depth) {
    configure_codec(new_sample_rate, new_bit_depth);
    AudioBuffer_ConfigureSampleRate(new_sample_rate, new_sample_rate);
    AudioBuffer_ConfigureSampleFormat(new_bit_depth, false, true);

    if (!AudioCodec_Init(new_sample_rate, new_bit_depth)) {
        return false;
    }
    return true;
}

void AudioPipeline_RegisterEndOfPlaylistCallback(EndOfPlaylistCallback callback) {
    g_pipeline.playlist_callback = callback;
}

void AudioPipeline_UnregisterEndOfPlaylistCallback(void) {
    g_pipeline.playlist_callback = NULL;
}

void AudioPipeline_ResetEndOfPlaylistFlag(void) {
    g_pipeline.end_of_playlist = false;
}

bool AudioPipeline_IsEndOfPlaylistReached(void) {
    return g_pipeline.end_of_playlist;
}

void AudioPipeline_SynchronizeState(void) {
    switch (AudioBuffer_GetState()) {
        case BUFFER_STATE_PLAYING:
            set_state(PIPELINE_STATE_PLAYING);
            break;
        case BUFFER_STATE_READY:
            if (g_pipeline.state == PIPELINE_STATE_PLAYING) {
                set_state(PIPELINE_STATE_TRANSITIONING);
            }
            break;
        case BUFFER_STATE_UNDERRUN:
            AudioPipeline_HandleUnderrun();
            break;
        case BUFFER_STATE_END_OF_STREAM:
            AudioPipeline_HandleEndOfFile();
            break;
        case BUFFER_STATE_EMPTY:
        default:
            break;
    }
}

void AudioPipeline_NotifyTransitionComplete(void) {
    g_pipeline.transition_pending = false;
    set_state(PIPELINE_STATE_PLAYING);
}

void AudioPipeline_NotifyCrossfadeComplete(void) {
    g_pipeline.transition_pending = false;
    set_state(PIPELINE_STATE_PLAYING);
}

static void set_state(PipelineState new_state) {
    PipelineState previous = g_pipeline.state;
    if (previous == new_state) {
        return;
    }
    g_pipeline.state = new_state;
    if (g_pipeline.state_callback) {
        g_pipeline.state_callback(previous, new_state);
    }
}

static bool ensure_buffer_ready(void) {
    BufferState buffer_state = AudioBuffer_GetState();
    if (buffer_state == BUFFER_STATE_EMPTY || buffer_state == BUFFER_STATE_END_OF_STREAM) {
        if (!AudioBuffer_StartPlayback()) {
            return false;
        }
    }
    return true;
}

static void configure_codec(uint32_t sample_rate, uint8_t bit_depth) {
    (void)AudioCodec_Init(sample_rate, bit_depth);
}

static void update_next_track_status(void) {
    bool has_next = MusicLibrary_HasNextTrack();
    size_t remaining = MusicLibrary_GetRemainingTracks();
    AudioBuffer_SetNextTrackAvailability(has_next, remaining);
}
