#include "nuno/gpio.h"

int main(void) {
    // Initialize HAL Library
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();

    // Initialize GPIOs
    GPIO_Init();

    // Main loop
    while (1) {
        // Application logic
    }
}