#include "nuno/gpio.h"
#include "nuno/platform.h"
#include "nuno/audio_buffer.h"
#include "nuno/dma.h"
#include "FreeRTOS.h"
#include "task.h"

// Error handler function prototype
static void Error_Handler(void);

// Audio processing task prototype
static void AudioProcessingTask(void *parameters);

// Constants for task configuration
#define AUDIO_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 2)
#define AUDIO_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)

int main(void) {
    // Initialize HAL Library
    if (HAL_Init() != HAL_OK) {
        Error_Handler();
    }

    // Configure the system clock
    SystemClock_Config();

    // Initialize GPIOs
    GPIO_Init();

    // Initialize DMA for audio transfer
    if (!DMA_Init()) {
        Error_Handler();
    }

    // Initialize Audio Buffer subsystem
    if (!AudioBuffer_Init()) {
        Error_Handler();
    }

    // Create the audio processing task
    TaskHandle_t audioTaskHandle;
    BaseType_t xReturned = xTaskCreate(
        AudioProcessingTask,
        "AudioProc",
        AUDIO_TASK_STACK_SIZE,
        NULL,
        AUDIO_TASK_PRIORITY,
        &audioTaskHandle
    );

    if (xReturned != pdPASS) {
        Error_Handler();
    }

    // Start the scheduler
    vTaskStartScheduler();

    // Should never get here unless there's insufficient RAM
    Error_Handler();
    
    return 0;
}

static void AudioProcessingTask(void *parameters) {
    (void)parameters; // Prevent unused parameter warning

    // Start the first DMA transfer
    if (!DMA_StartTransfer(AudioBuffer_GetBuffer(), AUDIO_BUFFER_SIZE)) {
        Error_Handler();
    }

    for (;;) {
        if (dma_transfer_complete) {
            dma_transfer_complete = false;
            
            // Process completed transfer and prepare next buffer
            if (!AudioBuffer_ProcessComplete()) {
                Error_Handler();
            }
            
            // Start next DMA transfer
            if (!DMA_StartTransfer(AudioBuffer_GetBuffer(), AUDIO_BUFFER_SIZE)) {
                Error_Handler();
            }
        }
        
        // Small delay to prevent task from hogging CPU
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void Error_Handler(void) {
    // Disable interrupts
    __disable_irq();
    
    // TODO: Add error indication (e.g., LED blinking)
    
    // Infinite loop
    while (1) {
        // Could add watchdog reset here if needed
    }
}

// FreeRTOS required callback
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}