// ui_tasks.c
#include "ui_tasks.h"
#include "ui_state.h"
#include "menu_items.h"
#include <string.h>
#include <stdint.h>


// Initialize UI State
void initUIState(UIState* state) {
    state->currentMenuType = MENU_MAIN;
    state->isPlaying = false;
    state->volume = 50;
    state->currentTrackTime = 0;
    state->totalTrackTime = 300; // Example total track time (e.g., 5 minutes)
    strncpy(state->currentTrackTitle, "Track Title", MAX_TITLE_LENGTH);
    strncpy(state->currentArtist, "Artist Name", MAX_TITLE_LENGTH);

    state->currentMenu.itemCount = NUM_MAIN_MENU_ITEMS;
    state->currentMenu.selectedIndex = 0;
    state->currentMenu.scrollOffset = 0;
    
    // Initialize menu items
    for(int i = 0; i < state->currentMenu.itemCount; i++) {
        strncpy(state->currentMenu.items[i].text, MAIN_MENU_ITEMS[i], MAX_ITEM_LENGTH);
        state->currentMenu.items[i].selectable = true;
        state->currentMenu.items[i].submenu = MENU_MAIN; // Update as needed
    }
}

// Update UI State based on ClickWheelEvent
void updateUIState(UIState* state, ClickWheelEvent event) {
    switch(event) {
        case CLICK_UP:
            if(state->currentMenu.selectedIndex > 0) {
                state->currentMenu.selectedIndex--;
            }
            break;
        case CLICK_DOWN:
            if(state->currentMenu.selectedIndex < state->currentMenu.itemCount - 1) {
                state->currentMenu.selectedIndex++;
            }
            break;
        case CLICK_CENTER:
            // Handle selection
            selectMenuItem(state);
            break;
        // Handle other events as needed
        default:
            break;
    }
    
    // Update scrollOffset if necessary
    if(state->currentMenu.selectedIndex < state->currentMenu.scrollOffset) {
        state->currentMenu.scrollOffset = state->currentMenu.selectedIndex;
    } else if(state->currentMenu.selectedIndex >= state->currentMenu.scrollOffset + MAX_MENU_ITEMS) {
        state->currentMenu.scrollOffset = state->currentMenu.selectedIndex - MAX_MENU_ITEMS + 1;
    }
}