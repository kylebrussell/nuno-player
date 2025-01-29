#include "platform.h"
#include "stm32h7xx_hal.h"
#include "audio_buffer.h"
#include <stdbool.h>

// DMA Handle
DMA_HandleTypeDef hdma_audio;

// Audio buffer configuration
#define AUDIO_BUFFER_SIZE 4096
uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
volatile bool dma_transfer_complete = false;

// Initialize DMA for audio streaming
bool DMA_Init(void) {
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
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    return true;
}

// Start DMA transfer
bool DMA_Start(void *data, size_t size) {
    if (HAL_DMA_Start_IT(&hdma_audio, (uint32_t)data, (uint32_t)&SPI2->DR, size) != HAL_OK) {
        return false;
    }
    // Enable the DMA stream
    __HAL_DMA_ENABLE(&hdma_audio);
    return true;
}

// DMA interrupt handler
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_audio);
}

// DMA transfer complete callback
void HAL_DMA_TxCpltCallback(DMA_HandleTypeDef *hdma) {
    if (hdma->Instance == DMA1_Stream0) {
        dma_transfer_complete = true;
        // Refill buffer or handle next chunk
        AudioBuffer_Done();
    }
}

// DMA half transfer complete callback
void HAL_DMA_TxHalfCpltCallback(DMA_HandleTypeDef *hdma) {
    if (hdma->Instance == DMA1_Stream0) {
        // Handle half-buffer transfer if using double buffering
        AudioBuffer_HalfDone();
    }
}