#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_BUFFER_SIZE 4096

// Initialize the audio buffer
void AudioBuffer_Init(void);

// Get pointer to the next audio data chunk
uint16_t* AudioBuffer_GetBuffer(void);

// Notify that the DMA has completed a chunk
void AudioBuffer_Done(void);

// Notify that half of the DMA buffer has been filled
void AudioBuffer_HalfDone(void);

#endif // AUDIO_BUFFER_H