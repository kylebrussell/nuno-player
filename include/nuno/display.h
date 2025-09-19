#ifndef NUNO_DISPLAY_H
#define NUNO_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

// Primary display geometry for the simulator and hardware abstraction.
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128

#define SIM_WINDOW_SCALE 3

#define SIM_WHEEL_GAP 20
#define SIM_WHEEL_OUTER_RADIUS 68
#define SIM_WHEEL_INNER_RADIUS 32

#define SIM_DISPLAY_MARGIN_X 18
#define SIM_DISPLAY_MARGIN_Y 18
#define SIM_BOTTOM_MARGIN 18

#define SIM_CANVAS_WIDTH (DISPLAY_WIDTH + SIM_DISPLAY_MARGIN_X * 2)
#define SIM_CANVAS_HEIGHT (SIM_DISPLAY_MARGIN_Y + DISPLAY_HEIGHT + SIM_WHEEL_GAP + SIM_WHEEL_OUTER_RADIUS * 2 + SIM_BOTTOM_MARGIN)

#define SIM_WHEEL_CENTER_X (SIM_CANVAS_WIDTH / 2)
#define SIM_WHEEL_CENTER_Y (SIM_DISPLAY_MARGIN_Y + DISPLAY_HEIGHT + SIM_WHEEL_GAP + SIM_WHEEL_OUTER_RADIUS)

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
