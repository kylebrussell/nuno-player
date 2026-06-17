#ifndef NUNO_DISPLAY_H
#define NUNO_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "nuno/device_profile.h"

/*
 * The display layer is driven by the *active* DeviceProfile. Geometry, layout
 * metrics and colours are all read at runtime from that profile, so the same
 * UI core renders onto any iPod generation. Drawing colours are passed as a
 * ColorRole (see device_profile.h); the active profile's theme resolves the
 * role to concrete RGBA.
 */

/* --- Active profile + runtime geometry ----------------------------- */

/* Returns the currently active profile (never NULL after Display_Init; a
 * sensible default is returned beforehand so accessors are always safe). */
const DeviceProfile *Display_GetActiveProfile(void);
const UiMetrics     *Display_GetMetrics(void);
int                  Display_GetWidth(void);   /* active screen width  */
int                  Display_GetHeight(void);  /* active screen height */

/* Pixel width of `text` when drawn with the active profile's font scale.
 * Use this for centering instead of hand-estimating strlen * N. */
int Display_MeasureText(const char *text);

/*
 * The UI core was written against compile-time DISPLAY_WIDTH / DISPLAY_HEIGHT
 * constants. Those names now resolve to the active profile at runtime, so every
 * existing call site adapts to whichever iPod is being simulated with no edit.
 */
#define DISPLAY_WIDTH  (Display_GetWidth())
#define DISPLAY_HEIGHT (Display_GetHeight())

/* --- Lifecycle ----------------------------------------------------- */

bool Display_Init(const char *title, const DeviceProfile *profile);
void Display_Shutdown(void);

/* Tear down and recreate the window for a different device. Returns false on
 * failure (the previous window is left shut down). */
bool Display_SwitchProfile(const DeviceProfile *profile);

/* --- Screen drawing primitives (colour = ColorRole) ---------------- */

void Display_Clear(void);
void Display_Update(void);
void Display_DrawText(const char *text, int x, int y, uint8_t color);
void Display_DrawRect(int x, int y, int width, int height, uint8_t color);
void Display_FillRect(int x, int y, int width, int height, uint8_t color);

/*
 * Fill the selection highlight for a menu row. When the active profile's theme
 * has selectionGradient enabled (colour iPods) this paints a vertical gradient
 * from selectionGradTop down to selectionGradBottom for a glossy look; otherwise
 * it falls back to a flat COLOR_ROLE_SELECTED_BG fill, so monochrome profiles
 * keep their original inverted-bar appearance.
 */
void Display_FillSelection(int x, int y, int width, int height);

/*
 * Fill the title bar. On colour themes (selectionGradient enabled) this draws a
 * subtle vertical sheen above the COLOR_ROLE_TITLE_BG base so the bar reads as a
 * glossy header; on monochrome themes it is a flat COLOR_ROLE_TITLE_BG fill,
 * identical to the previous look.
 */
void Display_FillTitleBar(int x, int y, int width, int height);

/* --- Chassis / faceplate ------------------------------------------- */

void Display_RenderBackground(void);
void Display_RenderClickWheel(uint8_t activeButton);
void Display_Present(void);

/* Save the current rendered frame (full chassis canvas) to a BMP file.
 * Returns false if there is no renderer or the write fails. */
bool Display_SaveScreenshot(const char *path);

#endif // NUNO_DISPLAY_H
