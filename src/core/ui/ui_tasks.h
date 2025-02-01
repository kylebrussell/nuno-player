#ifndef UI_TASKS_H
#define UI_TASKS_H

#include "ui_state.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the UI task system and state
 * @param state Pointer to UIState structure to initialize
 * @return true if initialization successful, false otherwise
 */
void initUIState(UIState* state);

/**
 * @brief Process UI input events and update state
 * @param state Pointer to current UI state
 * @param currentTime Current system time in milliseconds
 * @return true if UI state was modified, false otherwise
 */
bool processUIEvents(UIState* state, uint32_t currentTime);

/**
 * @brief Update playback information in UI state
 * @param state Pointer to UI state
 * @param currentTime Current track time in seconds
 * @param totalTime Total track time in seconds
 * @param isPlaying Current playback status
 */
void updatePlaybackInfo(UIState* state, 
                       uint16_t currentTime, 
                       uint16_t totalTime, 
                       bool isPlaying);

/**
 * @brief Update track information in UI state
 * @param state Pointer to UI state
 * @param trackTitle New track title
 * @param artistName New artist name
 */
void updateTrackInfo(UIState* state, 
                    const char* trackTitle, 
                    const char* artistName);

/**
 * @brief Update volume level in UI state
 * @param state Pointer to UI state
 * @param volume New volume level (0-100)
 */
void updateVolume(UIState* state, uint8_t volume);

/**
 * @brief Navigate to a specific menu
 * @param state Pointer to UI state
 * @param menuType Target menu to display
 */
void navigateToMenu(UIState* state, MenuType menuType);

/**
 * @brief Handle click wheel rotation event
 * @param state Pointer to UI state
 * @param direction Rotation direction (positive for clockwise, negative for counter-clockwise)
 * @param currentTime Current system time in milliseconds
 */
void handleRotation(UIState* state, int8_t direction, uint32_t currentTime);

/**
 * @brief Handle click wheel button press event
 * @param state Pointer to UI state
 * @param button Button identifier (center, menu, play/pause, etc.)
 * @param currentTime Current system time in milliseconds
 */
void handleButtonPress(UIState* state, uint8_t button, uint32_t currentTime);

// Button definitions for click wheel
#define BUTTON_CENTER    0x01
#define BUTTON_MENU      0x02
#define BUTTON_PLAY      0x04
#define BUTTON_PREV      0x08
#define BUTTON_NEXT      0x10

// Menu navigation constants
#define ITEMS_PER_SCREEN 6
#define SCROLL_THRESHOLD 3  // Items from edge to trigger scroll

// Timing constants
#define BUTTON_REPEAT_DELAY_MS    500   // Delay before button repeat starts
#define BUTTON_REPEAT_INTERVAL_MS 100   // Interval between repeated actions
#define SCROLL_ANIMATION_TIME_MS  150   // Duration of scroll animation

#endif // UI_TASKS_H