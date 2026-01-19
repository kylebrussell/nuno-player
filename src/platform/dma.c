#include "nuno/platform.h"
#include "nuno/stm32h7xx_hal.h"
#include "nuno/audio_buffer.h"
#include "nuno/audio_i2s.h"

#include <stdbool.h>

// DMA Handle
DMA_HandleTypeDef hdma_i2s_tx;
volatile bool dma_transfer_complete = false;
static bool dma_active = false;

// Initialize DMA for audio streaming
bool DMA_Init(void) {
    if (!AudioI2S_Init(44100U, 16U)) {
        return false;
    }

    I2S_HandleTypeDef *hi2s = AudioI2S_GetHandle();
    if (!hi2s) {
        return false;
    }

    // Enable DMA clock
    __HAL_RCC_DMA1_CLK_ENABLE();

    // Configure DMA parameters
    hdma_i2s_tx.Instance = DMA1_Stream0;
    hdma_i2s_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_i2s_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_i2s_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2s_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2s_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_i2s_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_i2s_tx.Init.Mode = DMA_CIRCULAR;
    hdma_i2s_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_i2s_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_i2s_tx) != HAL_OK) {
        return false;
    }

    // Link DMA handle to I2S handle
    __HAL_LINKDMA(hi2s, hdmatx, hdma_i2s_tx);

    // Configure DMA interrupt
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

    return true;
}

// Start DMA Transfer
bool DMA_StartTransfer(void *data, size_t len) {
    I2S_HandleTypeDef *hi2s = AudioI2S_GetHandle();
    if (!hi2s) {
        return false;
    }

    if (HAL_DMA_Start(&hdma_i2s_tx, (uint32_t)data, (uint32_t)&hi2s->Instance->DR, len) != HAL_OK) {
        return false;
    }

    // Enable DMA transfer complete and half-transfer interrupts
    __HAL_DMA_ENABLE_IT(&hdma_i2s_tx, DMA_IT_TC | DMA_IT_HT);

    // Start I2S DMA transfer (length is number of 16-bit samples)
    if (HAL_I2S_Transmit_DMA(hi2s, (uint16_t *)data, (uint16_t)len) != HAL_OK) {
        return false;
    }

    dma_active = true;
    return true;
}

// DMA Interrupt Handler
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_i2s_tx);
}

// I2S Transfer Complete Callback
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s && hi2s->Instance == SPI2) {
        dma_transfer_complete = true;
        // Notify audio buffer that DMA transfer is complete
        AudioBuffer_Done();
    }
}

// I2S Half Transfer Complete Callback
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s && hi2s->Instance == SPI2) {
        // Notify audio buffer that half of the DMA buffer has been transferred
        AudioBuffer_HalfDone();
    }
}

void DMA_StopTransfer(void) {
    if (!dma_active) {
        return;
    }

    HAL_DMA_Abort(&hdma_i2s_tx);
    __HAL_DMA_DISABLE_IT(&hdma_i2s_tx, DMA_IT_TC | DMA_IT_HT);
    dma_transfer_complete = false;
    dma_active = false;
}

void DMA_PauseTransfer(void) {
    if (!dma_active) {
        return;
    }

    __HAL_DMA_DISABLE(&hdma_i2s_tx);
}

// Start Audio Streaming
bool DMA_StartAudioStreaming(void) {
    // Get initial buffer data
    uint16_t *buffer = AudioBuffer_GetBuffer();

    // Start DMA transfer
    return DMA_StartTransfer(buffer, AUDIO_BUFFER_SIZE);
}
