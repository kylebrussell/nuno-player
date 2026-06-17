#include "nuno/platform.h"
#include "nuno/stm32h7xx_hal.h"
#include "nuno/audio_buffer.h"
#include "nuno/audio_i2s.h"
#include "nuno/audio_task.h"

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

    /* Start the audio producer task and register its ISR-safe wake. After this,
     * the transfer-complete ISR's AudioBuffer_Done() only flips the buffer and
     * wakes the producer (no decode/filesystem in the ISR); the producer task
     * does the decode via AudioBuffer_Service(). */
    if (!AudioTask_Start()) {
        return false;
    }

    return true;
}

bool DMA_Reconfigure(uint32_t sample_rate, uint8_t bit_depth) {
    if (dma_active) {
        DMA_StopTransfer();
    }

    I2S_HandleTypeDef *hi2s = AudioI2S_GetHandle();
    if (hi2s) {
        (void)HAL_I2S_DeInit(hi2s);
    }

    if (!AudioI2S_Init(sample_rate, bit_depth)) {
        return false;
    }

    hi2s = AudioI2S_GetHandle();
    if (!hi2s) {
        return false;
    }

    __HAL_LINKDMA(hi2s, hdmatx, hdma_i2s_tx);
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

// I2S Transfer Complete Callback (CONSUMER signal: a buffer finished playing).
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
    if (hi2s && hi2s->Instance == SPI2) {
        /* AudioBuffer_Done() flips the active buffer and wakes the audio
         * producer task to refill the freed one. It does NOT decode here - a
         * producer wake was registered in DMA_Init (AudioTask_Start), so the
         * decode/filesystem work runs in the task, not this interrupt. */
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
