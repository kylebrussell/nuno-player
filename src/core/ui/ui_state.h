#ifndef UI_STATE_H
#define UI_STATE_H

#include <stdint.h>
#include <stdbool.h>

#include "nuno/display.h"

#define MAX_MENU_ITEMS 10
#define MAX_TITLE_LENGTH 32
#define MAX_ITEM_LENGTH 32

// Layout constants
#define ITEM_HEIGHT 16
#define TEXT_HEIGHT 12
#define TEXT_MARGIN 4

// Colors (for monochrome display these translate to different patterns)
#define NORMAL_TEXT_COLOR 1
#define SELECTED_TEXT_COLOR 0
#define HIGHLIGHT_COLOR 1
#define TITLE_TEXT_COLOR 1
#define PROGRESS_COLOR 1

typedef enum {
    MENU_MAIN,
    MENU_MUSIC,
    MENU_PHOTOS,     // Added
    MENU_GAMES,      // Added
    MENU_SETTINGS,
    MENU_NOW_PLAYING
} MenuType;

typedef struct MenuItem {
    char text[MAX_ITEM_LENGTH];
    bool selectable;
    MenuType submenu;  // Which menu to load if selected
} MenuItem;

typedef struct Menu {
    char title[MAX_TITLE_LENGTH];
    MenuItem items[MAX_MENU_ITEMS];
    uint8_t itemCount;
    uint8_t selectedIndex;
    uint8_t scrollOffset;  // For scrolling when more items than can fit on screen
} Menu;

typedef struct UIState {
    Menu currentMenu;
    MenuType currentMenuType;
    uint8_t batteryLevel;  // Battery percentage 0-100
    bool isPlaying;
    uint8_t volume;
    uint16_t currentTrackTime;  // in seconds
    uint16_t totalTrackTime;    // in seconds
    char currentTrackTitle[MAX_TITLE_LENGTH];
    char currentArtist[MAX_TITLE_LENGTH];
    MenuType navigationStack[8];
    uint8_t navigationDepth;
} UIState;

// Menu navigation functions
void initUIState(UIState* state);
void selectMenuItem(UIState* state);
void scrollUp(UIState* state);
void scrollDown(UIState* state);
void goBack(UIState* state);
void navigateToMenu(UIState* state, MenuType menuType);

#endif /* UI_STATE_H */
