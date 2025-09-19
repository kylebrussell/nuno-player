#ifndef NUNO_DISPLAY_H
#define NUNO_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

// Primary display geometry for the simulator and hardware abstraction.
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128

#define SIM_WHEEL_GAP 20
#define SIM_WHEEL_OUTER_RADIUS 68
#define SIM_WHEEL_INNER_RADIUS 32
#define SIM_TOTAL_HEIGHT (DISPLAY_HEIGHT + SIM_WHEEL_GAP + SIM_WHEEL_OUTER_RADIUS * 2)
#define SIM_WHEEL_CENTER_X (DISPLAY_WIDTH / 2)
#define SIM_WHEEL_CENTER_Y (DISPLAY_HEIGHT + SIM_WHEEL_GAP + SIM_WHEEL_OUTER_RADIUS)

bool Display_Init(const char *title);
void Display_Shutdown(void);

void Display_Clear(void);
void Display_Update(void);
void Display_DrawText(const char *text, int x, int y, uint8_t color);
void Display_DrawRect(int x, int y, int width, int height, uint8_t color);
void Display_FillRect(int x, int y, int width, int height, uint8_t color);

void Display_RenderBackground(void);
void Display_RenderClickWheel(uint8_t activeButton);
void Display_Present(void);

#endif // NUNO_DISPLAY_H
