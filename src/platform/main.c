#include "nuno/gpio.h"

int main(void) {
    // Initialize HAL Library
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();

    // Initialize GPIOs
    GPIO_Init();

    if (!DMA_Init()) {
             // Handle DMA initialization error
    }

    AudioBuffer_Init();
    
    // Start the first DMA transfer
    MA_Start(AudioBuffer_GetBuffer(), AUDIO_BUFFER_SIZE);

    // Main loop
    while (1) {
        // Application logic
         if (dma_transfer_complete) {
                 dma_transfer_complete = false;
                 // Refill buffer with new audio data
                 // Example: AudioBuffer_Done();
             }
    }
}