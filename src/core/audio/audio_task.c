#include "FreeRTOS.h"
#include "task.h"
#include "nuno/audio_buffer.h"
#include "nuno/audio_pipeline.h"
#include "nuno/dma.h"
#include "nuno/error.h"

// Task configuration
#define AUDIO_TASK_STACK_SIZE 2048
#define AUDIO_TASK_PRIORITY   (tskIDLE_PRIORITY + 2)

// DMA transfer completion flag
static volatile bool dma_transfer_complete = false;

// Task handle
static TaskHandle_t audio_task_handle = NULL;

// DMA completion callback
void DMA_TransferCompleteCallback(void) {
    dma_transfer_complete = true;
}

static void AudioProcessingTask(void *parameters) {
    (void)parameters;

    if (!DMA_StartTransfer(AudioBuffer_GetBuffer(), AUDIO_BUFFER_SIZE)) {
        Error_Handler();
    }

    for (;;) {
        AudioBuffer_Update();
        AudioPipeline_SynchronizeState();
        
        if (dma_transfer_complete) {
            dma_transfer_complete = false;
            
            if (!AudioBuffer_Done()) {  // Changed from ProcessComplete to Done
                Error_Handler();
            }
            
            if (!DMA_StartTransfer(AudioBuffer_GetBuffer(), AUDIO_BUFFER_SIZE)) {
                Error_Handler();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Create and start the audio processing task
bool AudioTask_Start(void) {
    BaseType_t task_created = xTaskCreate(
        AudioProcessingTask,
        "AudioTask",
        AUDIO_TASK_STACK_SIZE,
        NULL,
        AUDIO_TASK_PRIORITY,
        &audio_task_handle
    );

    return (task_created == pdPASS);
}

// Stop and cleanup the audio processing task
void AudioTask_Stop(void) {
    if (audio_task_handle != NULL) {
        vTaskDelete(audio_task_handle);
        audio_task_handle = NULL;
    }
}

// Get task statistics
void AudioTask_GetStats(TaskStatus_t *stats) {
    if (audio_task_handle != NULL && stats != NULL) {
        vTaskGetInfo(audio_task_handle, stats, pdTRUE, eInvalid);
    }
} 