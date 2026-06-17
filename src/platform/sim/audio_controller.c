#include "platform/sim/audio_controller.h"

#include "nuno/audio_buffer.h"
#include "nuno/audio_pipeline.h"
#include "nuno/dma.h"
#include "nuno/music_library.h"

#include <stdio.h>

static bool g_audio_initialised = false;

bool SimAudio_Init(void) {
    printf("SimAudio_Init called\n");
    if (g_audio_initialised) {
        printf("Audio already initialized\n");
        return true;
    }

    printf("Initializing audio pipeline...\n");
    if (!AudioPipeline_Init()) {
        printf("AudioPipeline_Init failed\n");
        return false;
    }

    printf("Initializing DMA/audio backend...\n");
    if (!DMA_Init()) {
        printf("DMA_Init failed\n");
        return false;
    }

    printf("Audio pipeline initialized successfully\n");
    g_audio_initialised = true;
    return true;
}

bool SimAudio_PlayTrack(void *context, size_t track_index) {
    (void)context;

    if (!g_audio_initialised) {
        if (!SimAudio_Init()) {
            return false;
        }
    }

    printf("Playing track %zu\n", track_index);
    if (!AudioPipeline_PlayTrack(track_index)) {
        return false;
    }

    // Start streaming to the audio device
    if (!DMA_StartTransfer(AudioBuffer_GetBuffer(), AUDIO_BUFFER_SIZE)) {
        printf("DMA_StartTransfer failed\n");
        return false;
    }

    return true;
}

void SimAudio_Shutdown(void) {
    if (!g_audio_initialised) {
        return;
    }

    AudioPipeline_Stop();

    // Stop the audio device + join the producer thread BEFORE resetting the
    // buffer, so the producer is no longer in fill_buffer() when g_buffer is
    // cleared by AudioBuffer_Cleanup().
    extern void platform_audio_cleanup(void);
    platform_audio_cleanup();

    AudioBuffer_Cleanup();

    g_audio_initialised = false;
}
