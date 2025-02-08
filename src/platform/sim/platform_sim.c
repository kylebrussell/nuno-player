#include "nuno/platform.h"
#include "nuno/dma.h"
#include <SDL2/SDL.h>

bool DMA_Init(void) {
    return true;
}

bool DMA_StartTransfer(void* buffer, uint32_t size) {
    return true;
}

void DMA_Stop(void) {
}

bool platform_i2c_init(void) {
    return true;
}

bool platform_i2c_write(uint8_t addr, const uint8_t* data, size_t len) {
    return true;
}

bool platform_i2c_read(uint8_t addr, uint8_t* data, size_t len) {
    return true;
}

uint32_t platform_get_time_ms(void) {
    return SDL_GetTicks();
}

void platform_delay_ms(uint32_t ms) {
    SDL_Delay(ms);
}
