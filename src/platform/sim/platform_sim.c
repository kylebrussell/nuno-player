#include "nuno/platform.h"
#include "nuno/dma.h"
#include "nuno/audio_buffer.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

static SDL_AudioDeviceID g_audio_device = 0;
static SDL_AudioSpec g_audio_spec;
static bool g_audio_initialised = false;
static size_t g_buffer_offset_samples = 0;  // how many samples consumed in current buffer

/*
 * Decode happens off the audio path. The SDL audio callback (consumer) only
 * copies ready PCM and, via AudioBuffer_Done() -> producer_wake(), posts this
 * semaphore; the producer thread does the decode in AudioBuffer_Service(). This
 * mirrors the intended hardware model (DMA-complete ISR posts a semaphore, a
 * FreeRTOS task decodes) so the two platforms share one playback model.
 */
static SDL_Thread *g_producer_thread = NULL;
static SDL_sem *g_producer_sem = NULL;
static volatile bool g_producer_running = false;

static void audio_callback(void* userdata, Uint8* stream, int len);

/* Consumer/ISR context: just signal the producer. No decode here. */
static void producer_wake(void) {
    if (g_producer_sem) {
        SDL_SemPost(g_producer_sem);
    }
}

static int producer_thread_main(void* arg) {
    (void)arg;
    while (g_producer_running) {
        SDL_SemWait(g_producer_sem);
        if (!g_producer_running) {
            break;
        }
        AudioBuffer_Service();
    }
    return 0;
}

static void start_producer(void) {
    if (g_producer_thread) {
        return;
    }
    g_producer_sem = SDL_CreateSemaphore(0);
    g_producer_running = true;
    g_producer_thread = SDL_CreateThread(producer_thread_main, "nuno-audio-producer", NULL);
    AudioBuffer_SetProducerWake(producer_wake);
}

static void stop_producer(void) {
    if (!g_producer_thread) {
        return;
    }
    AudioBuffer_SetProducerWake(NULL);
    g_producer_running = false;
    SDL_SemPost(g_producer_sem);  // wake it so it can observe the stop flag
    SDL_WaitThread(g_producer_thread, NULL);
    g_producer_thread = NULL;
    if (g_producer_sem) {
        SDL_DestroySemaphore(g_producer_sem);
        g_producer_sem = NULL;
    }
}

static bool open_audio_device(uint32_t sample_rate) {
    SDL_AudioSpec want, have;

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        printf("SDL audio not initialized, trying to init...\n");
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            printf("SDL_InitSubSystem audio failed: %s\n", SDL_GetError());
            return false;
        }
    }

    SDL_zero(want);
    want.freq = (int)sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 2048;
    want.callback = audio_callback;
    want.userdata = NULL;

    printf("Opening audio device...\n");
    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_audio_device == 0) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    printf("Audio device opened successfully, spec: freq=%d, format=%d, channels=%d, samples=%d\n",
           have.freq, have.format, have.channels, have.samples);

    g_audio_spec = have;
    g_audio_initialised = true;
    return true;
}

static void audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;

    int16_t* out = (int16_t*)stream;
    int samples_needed = len / (int)sizeof(int16_t);
    int written = 0;

    while (written < samples_needed) {
        uint16_t* cur = AudioBuffer_GetBuffer();
        if (!cur) {
            // No buffer available, fill rest with silence
            memset(&out[written], 0, (size_t)(samples_needed - written) * sizeof(int16_t));
            break;
        }

        size_t available_in_buffer = (AUDIO_BUFFER_SIZE > g_buffer_offset_samples)
            ? (AUDIO_BUFFER_SIZE - g_buffer_offset_samples)
            : 0;

        if (available_in_buffer == 0) {
            // Move to next buffer
            if (!AudioBuffer_Done()) {
                // End of stream: fill rest with silence
                memset(&out[written], 0, (size_t)(samples_needed - written) * sizeof(int16_t));
                break;
            }
            g_buffer_offset_samples = 0;
            continue;
        }

        int to_copy = samples_needed - written;
        if ((size_t)to_copy > available_in_buffer) {
            to_copy = (int)available_in_buffer;
        }

        memcpy(&out[written], &cur[g_buffer_offset_samples], (size_t)to_copy * sizeof(int16_t));
        written += to_copy;
        g_buffer_offset_samples += (size_t)to_copy;

        if (g_buffer_offset_samples >= AUDIO_BUFFER_SIZE) {
            if (!AudioBuffer_Done()) {
                // End of stream after consuming this buffer; fill remaining with silence
                if (written < samples_needed) {
                    memset(&out[written], 0, (size_t)(samples_needed - written) * sizeof(int16_t));
                }
                break;
            }
            g_buffer_offset_samples = 0;
        }
    }
}

bool DMA_Init(void) {
    if (g_audio_initialised) {
        printf("Audio already initialized\n");
        return true;
    }

    printf("Initializing SDL audio...\n");

    // SDL audio should already be initialized by Display_Init
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        printf("SDL audio not initialized, trying to init...\n");
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            printf("SDL_InitSubSystem audio failed: %s\n", SDL_GetError());
            return false;
        }
    }

    g_buffer_offset_samples = 0;
    if (!open_audio_device(44100U)) {
        return false;
    }
    start_producer();
    return true;
}

bool DMA_Reconfigure(uint32_t sample_rate, uint8_t bit_depth) {
    (void)bit_depth;

    if (g_audio_device != 0) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
    g_audio_initialised = false;
    g_buffer_offset_samples = 0;
    return open_audio_device(sample_rate);
}

bool DMA_StartTransfer(void *buffer, size_t size) {
    printf("DMA_StartTransfer called, size=%zu\n", size);
    if (!g_audio_initialised) {
        printf("Audio not initialized\n");
        return false;
    }

    printf("Starting audio playback...\n");
    SDL_PauseAudioDevice(g_audio_device, 0);  // Start playback
    return true;
}

void DMA_StopTransfer(void) {
    if (!g_audio_initialised) {
        return;
    }

    SDL_PauseAudioDevice(g_audio_device, 1);  // Stop playback
}

void DMA_PauseTransfer(void) {
    if (!g_audio_initialised) {
        return;
    }

    SDL_PauseAudioDevice(g_audio_device, 1);  // Pause playback
}

bool platform_i2c_init(void) {
    return true;
}

bool platform_i2c_write(uint8_t addr, const uint8_t* data, size_t len) {
    return true;
}

bool platform_i2c_read(uint8_t addr, uint8_t* data, size_t len) {
    return true;
}

uint32_t platform_get_time_ms(void) {
    return SDL_GetTicks();
}

void platform_delay_ms(uint32_t ms) {
    SDL_Delay(ms);
}

void platform_audio_cleanup(void) {
    /* Close the device first so the consumer callback stops, then join the
     * producer thread. The caller (SimAudio_Shutdown) invokes this BEFORE
     * AudioBuffer_Cleanup(), so the producer is no longer touching g_buffer when
     * the buffer state is reset. */
    if (g_audio_initialised) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        g_audio_initialised = false;
    }
    stop_producer();
}
