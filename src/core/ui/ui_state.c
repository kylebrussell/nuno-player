#include "ui_state.h"
#include "menu_items.h"

#include <string.h>

typedef struct {
    const char *title;
    const char **items;
    uint8_t count;
} MenuDefinition;

static void populateMenu(UIState *state, MenuType type);
static void pushMenu(UIState *state, MenuType type);

void initUIState(UIState *state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(UIState));
    state->batteryLevel = 85;
    state->volume = 50;
    state->totalTrackTime = 300;
    strncpy(state->currentTrackTitle, "Now Playing", MAX_TITLE_LENGTH - 1);
    strncpy(state->currentArtist, "Artist", MAX_TITLE_LENGTH - 1);

    state->navigationDepth = 0;
    pushMenu(state, MENU_MAIN);
}

void selectMenuItem(UIState *state) {
    if (!state || state->currentMenu.itemCount == 0) {
        return;
    }

    MenuItem *item = &state->currentMenu.items[state->currentMenu.selectedIndex];
    if (!item->selectable) {
        return;
    }

    if (item->submenu != state->currentMenuType) {
        pushMenu(state, item->submenu);
    }
}

void scrollUp(UIState *state) {
    if (!state || state->currentMenu.itemCount == 0) {
        return;
    }

    if (state->currentMenu.selectedIndex > 0) {
        state->currentMenu.selectedIndex--;
    }

    if (state->currentMenu.selectedIndex < state->currentMenu.scrollOffset) {
        state->currentMenu.scrollOffset = state->currentMenu.selectedIndex;
    }
}

void scrollDown(UIState *state) {
    if (!state || state->currentMenu.itemCount == 0) {
        return;
    }

    if (state->currentMenu.selectedIndex + 1 < state->currentMenu.itemCount) {
        state->currentMenu.selectedIndex++;
    }

    uint8_t visibleSlots = (DISPLAY_HEIGHT - TITLE_BAR_HEIGHT) / ITEM_HEIGHT;
    uint8_t bottomVisible = state->currentMenu.scrollOffset + (visibleSlots ? visibleSlots : 1) - 1;
    if (state->currentMenu.selectedIndex > bottomVisible) {
        state->currentMenu.scrollOffset = state->currentMenu.selectedIndex - (DISPLAY_HEIGHT / ITEM_HEIGHT) + 1;
    }
}

void goBack(UIState *state) {
    if (!state || state->navigationDepth == 0) {
        return;
    }

    if (state->navigationDepth > 1) {
        state->navigationDepth--;
        MenuType previous = state->navigationStack[state->navigationDepth - 1];
        populateMenu(state, previous);
    }
}

void navigateToMenu(UIState *state, MenuType menuType) {
    if (!state) {
        return;
    }

    if (state->currentMenuType == menuType) {
        return;
    }

    pushMenu(state, menuType);
}

static void pushMenu(UIState *state, MenuType type) {
    if (state->navigationDepth < sizeof(state->navigationStack) / sizeof(state->navigationStack[0])) {
        state->navigationStack[state->navigationDepth++] = type;
    } else {
        state->navigationStack[state->navigationDepth - 1] = type;
    }
    populateMenu(state, type);
}

static void populateMenu(UIState *state, MenuType type) {
    state->currentMenuType = type;
    state->currentMenu.selectedIndex = 0;
    state->currentMenu.scrollOffset = 0;
    state->currentMenu.itemCount = 0;

    const char *title = "Menu";

    switch (type) {
        case MENU_MAIN:
            title = "iPod";
            state->currentMenu.itemCount = NUM_MAIN_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, MAIN_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = (i == 0 || i == 1 || i == 2 || i == 5);
                switch (i) {
                    case 0: state->currentMenu.items[i].submenu = MENU_MUSIC; break;
                    case 1: state->currentMenu.items[i].submenu = MENU_PHOTOS; break;
                    case 2: state->currentMenu.items[i].submenu = MENU_SETTINGS; break;
                    case 5: state->currentMenu.items[i].submenu = MENU_NOW_PLAYING; break;
                    default: state->currentMenu.items[i].submenu = MENU_MAIN; break;
                }
            }
            break;
        case MENU_MUSIC:
            title = "Music";
            state->currentMenu.itemCount = NUM_MUSIC_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, MUSIC_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = true;
                state->currentMenu.items[i].submenu = MENU_NOW_PLAYING;
            }
            break;
        case MENU_SETTINGS:
            title = "Settings";
            state->currentMenu.itemCount = NUM_SETTINGS_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, SETTINGS_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = false;
                state->currentMenu.items[i].submenu = MENU_SETTINGS;
            }
            break;
        case MENU_PHOTOS:
            title = "Extras";
            state->currentMenu.itemCount = NUM_EXTRAS_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, EXTRAS_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = false;
                state->currentMenu.items[i].submenu = MENU_PHOTOS;
            }
            break;
        case MENU_GAMES:
            title = "Games";
            break;
        case MENU_NOW_PLAYING:
            title = "Now Playing";
            state->currentMenu.itemCount = 3;
            strncpy(state->currentMenu.items[0].text, state->currentTrackTitle, MAX_ITEM_LENGTH - 1);
            strncpy(state->currentMenu.items[1].text, state->currentArtist, MAX_ITEM_LENGTH - 1);
            strncpy(state->currentMenu.items[2].text, "Press Menu to go back", MAX_ITEM_LENGTH - 1);
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = false;
                state->currentMenu.items[i].submenu = MENU_NOW_PLAYING;
            }
            break;
    }

    strncpy(state->currentMenu.title, title, MAX_TITLE_LENGTH - 1);
    state->currentMenu.title[MAX_TITLE_LENGTH - 1] = '\0';
}
