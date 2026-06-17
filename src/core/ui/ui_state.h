#ifndef UI_STATE_H
#define UI_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "nuno/display.h"

#define MAX_MENU_ITEMS 10
#define MAX_TITLE_LENGTH 32
#define MAX_ITEM_LENGTH 32

// Layout metrics — resolved at runtime from the active device profile so the
// same renderer adapts to each iPod generation's screen size and font scale.
#define ITEM_HEIGHT      (Display_GetMetrics()->itemHeight)
#define TEXT_HEIGHT      (Display_GetMetrics()->textHeight)
#define TEXT_MARGIN      (Display_GetMetrics()->textMargin)
#define TITLE_BAR_HEIGHT (Display_GetMetrics()->titleBarHeight)

// Semantic colour roles — the active profile's theme resolves these to RGBA.
// Monochrome profiles keep the original inverted-selection look; colour
// profiles get blue selection/title bars from the same call sites.
#define NORMAL_TEXT_COLOR   COLOR_ROLE_FOREGROUND
#define SELECTED_TEXT_COLOR COLOR_ROLE_SELECTED_FG
#define HIGHLIGHT_COLOR     COLOR_ROLE_SELECTED_BG
#define TITLE_TEXT_COLOR    COLOR_ROLE_TITLE_FG
#define PROGRESS_COLOR      COLOR_ROLE_ACCENT

typedef enum {
    MENU_MAIN,
    MENU_MUSIC,
    MENU_PHOTOS,     // Added
    MENU_GAMES,      // Added
    MENU_SETTINGS,
    MENU_NOW_PLAYING,
    MENU_SONGS
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

typedef bool (*PlayTrackHandler)(void *context, size_t track_index);

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
    char currentAlbum[MAX_TITLE_LENGTH];
    MenuType navigationStack[8];
    uint8_t navigationDepth;
    PlayTrackHandler playTrackHandler;
    void *playTrackContext;
} UIState;

// Menu navigation functions
void initUIState(UIState* state);
void selectMenuItem(UIState* state);
void scrollUp(UIState* state);
void scrollDown(UIState* state);
void goBack(UIState* state);
void navigateToMenu(UIState* state, MenuType menuType);
void UIState_SetPlaybackHandler(UIState *state,
                                PlayTrackHandler handler,
                                void *context);
void refreshNowPlayingView(UIState* state);

#endif /* UI_STATE_H */
