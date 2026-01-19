#include "nuno/gpio.h"
#include "nuno/platform.h"
#include "nuno/audio_buffer.h"
#include "nuno/dma.h"
#include "nuno/trackpad.h"
#include "FreeRTOS.h"
#include "task.h"

#include "ui_tasks.h"       // For UI functions and definitions
#include "ui_state.h"       // UI state and menu definitions
#include "menu_renderer.h"  // UI renderer interface
#include "nuno/input_mapper.h"

// Error handler function prototype
static void Error_Handler(void);

// Audio processing task prototype
static void AudioProcessingTask(void *parameters);
static void UITask(void *parameters);
static void InputTask(void *parameters);

// Constants for task configuration
#define AUDIO_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 2)
#define AUDIO_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)
#define UI_TASK_STACK_SIZE        (configMINIMAL_STACK_SIZE * 3)
#define UI_TASK_PRIORITY          (tskIDLE_PRIORITY + 1)
#define INPUT_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 2)
#define INPUT_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)

int main(void) {
    // Initialize HAL Library
    if (HAL_Init() != HAL_OK) {
        Error_Handler();
    }

    // Configure the system clock
    SystemClock_Config();

    // Initialize GPIOs
    GPIO_Init();

    // Initialize I2C (shared by trackpad + codec control)
    if (!platform_i2c_init()) {
        Error_Handler();
    }

    // Initialize DMA for audio transfer
    if (!DMA_Init()) {
        Error_Handler();
    }

    // Initialize Audio Buffer subsystem
    if (!AudioBuffer_Init()) {
        Error_Handler();
    }

    // (Optional) Initialize display hardware here so that
    // Display_Clear, Display_Update, etc. are ready to use.
    // For example: Display_Init();

    // If your menu renderer has its own initialization routine:
    if (!MenuRenderer_Init()) {
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

    // Create the UI task that handles UI events and rendering
    TaskHandle_t uiTaskHandle;
    xReturned = xTaskCreate(
        UITask,
        "UI_Task",
        UI_TASK_STACK_SIZE,
        NULL,
        UI_TASK_PRIORITY,
        &uiTaskHandle
    );
    if (xReturned != pdPASS) {
        Error_Handler();
    }

    // Create the input task that polls the trackpad
    TaskHandle_t inputTaskHandle;
    xReturned = xTaskCreate(
        InputTask,
        "InputTask",
        INPUT_TASK_STACK_SIZE,
        NULL,
        INPUT_TASK_PRIORITY,
        &inputTaskHandle
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

// The UI task integrates UI event processing and renders the UI at a fixed interval.
static void UITask(void *parameters) {
    (void)parameters; // Prevent unused parameter warning

    UIState uiState;
    // Initialize UI state using our UI system setup function
    initUIState(&uiState);

    // Optional: Initialize additional UI components (e.g., track info, volume) here.

    for (;;) {
        // Get the current time (in milliseconds) from the FreeRTOS tick count
        // Adjust with your tick frequency (if configTICK_RATE_HZ is not 1000)
        uint32_t currentTime = xTaskGetTickCount() * (1000 / configTICK_RATE_HZ);

        InputMapper_ProcessEvents(&uiState, currentTime);

        // Process any UI events (from buttons, rotation, etc.)
        // In a real application, events might come from an ISR or queue.
        processUIEvents(&uiState, currentTime);

        // Render the UI with the current state and animations
        MenuRenderer_Render(&uiState, currentTime);

        // Delay to limit the refresh rate. Adjust delay as needed for smooth animations.
        vTaskDelay(pdMS_TO_TICKS(16)); // ~60 FPS refresh rate
    }
}

static void InputTask(void *parameters) {
    (void)parameters;

    if (!Trackpad_Init()) {
        Error_Handler();
    }

    for (;;) {
        Trackpad_Poll();
        vTaskDelay(pdMS_TO_TICKS(10)); // 100 Hz polling
    }
}

static void Error_Handler(void) {
    // Disable interrupts
    __disable_irq();
    
    // Optional: Indicate the error (e.g., blink an LED)
    
    // Infinite loop
    while (1) {
        // Could add watchdog reset here if needed
    
    }
}

// FreeRTOS required callback for stack overflow handling
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}