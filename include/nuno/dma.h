#ifndef DMA_H
#define DMA_H

#include "nuno/hal_types.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Initialize DMA for audio streaming
bool DMA_Init(void);

// Start DMA Transfer
bool DMA_StartTransfer(void *data, size_t len);

// Start Audio Streaming
bool DMA_StartAudioStreaming(void);

// Stop current DMA transfer
void DMA_StopTransfer(void);

// Temporarily pause an active DMA transfer
void DMA_PauseTransfer(void);

// External callback declarations
void HAL_DMA_TxCpltCallback(DMA_HandleTypeDef *hdma);
void HAL_DMA_TxHalfCpltCallback(DMA_HandleTypeDef *hdma);

#endif // DMA_H
