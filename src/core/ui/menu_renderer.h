#ifndef MENU_RENDERER_H
#define MENU_RENDERER_H

#include "ui_state.h"
#include <stdint.h>

// Display interface functions - implement these for your hardware
void Display_Clear(void);
void Display_Update(void);
void Display_DrawText(const char* text, int x, int y, uint8_t color);
void Display_DrawRect(int x, int y, int width, int height, uint8_t color);
void Display_FillRect(int x, int y, int width, int height, uint8_t color);

/**
 * @brief Initialize the menu renderer
 * @return true if initialization successful, false otherwise
 */
bool MenuRenderer_Init(void);

/**
 * @brief Start a scroll animation to a target offset
 * @param targetOffset The final scroll position to animate to
 * @param currentTime Current system time in milliseconds
 */
void MenuRenderer_StartScroll(float targetOffset, uint32_t currentTime);

/**
 * @brief Start a transition animation between menus
 * @param from Source menu type
 * @param to Destination menu type
 * @param currentTime Current system time in milliseconds
 */
void MenuRenderer_StartTransition(MenuType from, MenuType to, uint32_t currentTime);

/**
 * @brief Render the current UI state
 * @param state Pointer to the current UI state
 * @param currentTime Current system time in milliseconds
 */
void MenuRenderer_Render(const UIState* state, uint32_t currentTime);

/**
 * @brief Check if any animations are currently active
 * @return true if animations are in progress, false otherwise
 */
bool MenuRenderer_IsAnimating(void);

/**
 * @brief Set the display brightness
 * @param brightness Brightness level (0-100)
 */
void MenuRenderer_SetBrightness(uint8_t brightness);

/**
 * @brief Force immediate completion of any active animations
 */
void MenuRenderer_FinishAnimations(void);

// Animation configuration
#define MENU_RENDERER_SCROLL_SPEED 0.5f        // Pixels per millisecond
#define MENU_RENDERER_TRANSITION_SPEED 0.3f    // Progress per millisecond
#define MENU_RENDERER_MAX_FPS 60              // Maximum refresh rate

#endif // MENU_RENDERER_H