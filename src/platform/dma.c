#include "nuno/platform.h"
#include "nuno/stm32h7xx_hal.h"
#include "nuno/audio_buffer.h"
#include <stdbool.h>

// DMA Handle
DMA_HandleTypeDef hdma_audio;

// Audio buffer configuration
#define AUDIO_BUFFER_SIZE 4096
uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
volatile bool dma_transfer_complete = false;

static CircularBuffer circular_buffer;

// Initialize Circular Buffer
void CircularBuffer_Init(CircularBuffer *cb, uint16_t *buffer, size_t size) {
    cb->buffer = buffer;
    cb->size = size;
    cb->head = 0;
    cb->tail = 0;
    cb->is_full = false;
}

// Add data to Circular Buffer
bool CircularBuffer_Add(CircularBuffer *cb, uint16_t data) {
    if (cb->is_full) {
        // Buffer is full
        return false;
    }
    cb->buffer[cb->head] = data;
    cb->head = (cb->head + 1) % cb->size;

    if (cb->head == cb->tail) {
        cb->is_full = true;
    }

    return true;
}

// Remove data from Circular Buffer
bool CircularBuffer_Remove(CircularBuffer *cb, uint16_t *data) {
    if (cb->head == cb->tail && !cb->is_full) {
        // Buffer is empty
        return false;
    }

    *data = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % cb->size;
    cb->is_full = false;

    return true;
}

// Initialize DMA for audio streaming
bool DMA_Init(void) {
    // Initialize Circular Buffer
    CircularBuffer_Init(&circular_buffer, audio_buffer, AUDIO_BUFFER_SIZE);

    // Enable DMA clock
    __HAL_RCC_DMA1_CLK_ENABLE();

    // Configure DMA parameters
    hdma_audio.Instance = DMA1_Stream0;
    hdma_audio.Init.Channel = DMA_CHANNEL_0;
    hdma_audio.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_audio.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_audio.Init.MemInc = DMA_MINC_ENABLE;
    hdma_audio.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_audio.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_audio.Init.Mode = DMA_CIRCULAR;
    hdma_audio.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_audio.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_audio) != HAL_OK) {
        return false;
    }

    // Link DMA handle to SPI handle (Assuming SPI2 is used for DAC)
    extern SPI_HandleTypeDef hspi2;
    __HAL_LINKDMA(&hspi2, hdmatx, hdma_audio);

    // Configure DMA interrupt
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    return true;
}

// Start DMA Transfer
bool DMA_StartTransfer(void *data, size_t len) {
    if (HAL_DMA_Start(&hdma_audio, (uint32_t)data, (uint32_t)&SPI2->DR, len) != HAL_OK) {
        return false;
    }

    // Enable DMA transfer complete and half-transfer interrupts
    __HAL_DMA_ENABLE_IT(&hdma_audio, DMA_IT_TC | DMA_IT_HT);

    // Start DMA
    if (HAL_SPI_Transmit_DMA(&hspi2, data, len) != HAL_OK) {
        return false;
    }

    return true;
}

// DMA Interrupt Handler
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_audio);
}

// DMA Transfer Complete Callback
void HAL_DMA_TxCpltCallback(DMA_HandleTypeDef *hdma) {
    if (hdma->Instance == DMA1_Stream0) {
        dma_transfer_complete = true;
        // Notify audio buffer that DMA transfer is complete
        AudioBuffer_Done();
    }
}

// DMA Half Transfer Complete Callback
void HAL_DMA_TxHalfCpltCallback(DMA_HandleTypeDef *hdma) {
    if (hdma->Instance == DMA1_Stream0) {
        // Notify audio buffer that half of the DMA buffer has been transferred
        AudioBuffer_HalfDone();
    }
}

// Start Audio Streaming
bool DMA_StartAudioStreaming(void) {
    // Get initial buffer data
    uint16_t *buffer = AudioBuffer_GetBuffer();

    // Start DMA transfer
    return DMA_StartTransfer(buffer, AUDIO_BUFFER_SIZE);
}