#ifndef NUNO_PLATFORM_H
#define NUNO_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "nuno/dma.h"

// I2C Interface
bool platform_i2c_init(void);
bool platform_i2c_write(uint8_t addr, const uint8_t* data, size_t len);
bool platform_i2c_read(uint8_t addr, uint8_t* data, size_t len);

// GPIO Interface
bool platform_gpio_init(void);
void platform_gpio_write(uint8_t pin, bool state);
bool platform_gpio_read(uint8_t pin);

// Time Interface
void platform_delay_ms(uint32_t ms);
uint32_t platform_get_time_ms(void);

#endif // NUNO_PLATFORM_H
