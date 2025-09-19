#ifndef NUNO_DISPLAY_H
#define NUNO_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

// Primary display geometry for the simulator and hardware abstraction.
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128

bool Display_Init(const char *title);
void Display_Shutdown(void);

void Display_Clear(void);
void Display_Update(void);
void Display_DrawText(const char *text, int x, int y, uint8_t color);
void Display_DrawRect(int x, int y, int width, int height, uint8_t color);
void Display_FillRect(int x, int y, int width, int height, uint8_t color);

#endif // NUNO_DISPLAY_H
