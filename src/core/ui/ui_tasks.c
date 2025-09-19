#include "ui_tasks.h"
#include "ui_state.h"

#include <string.h>

bool processUIEvents(UIState* state, uint32_t currentTime) {
    (void)state;
    (void)currentTime;
    return false;
}

void updatePlaybackInfo(UIState* state,
                        uint16_t currentTime,
                        uint16_t totalTime,
                        bool isPlaying) {
    if (!state) {
        return;
    }

    state->currentTrackTime = currentTime;
    state->totalTrackTime = totalTime ? totalTime : state->totalTrackTime;
    state->isPlaying = isPlaying;
}

void updateTrackInfo(UIState* state,
                     const char* trackTitle,
                     const char* artistName) {
    if (!state) {
        return;
    }

    if (trackTitle) {
        strncpy(state->currentTrackTitle, trackTitle, MAX_TITLE_LENGTH - 1);
        state->currentTrackTitle[MAX_TITLE_LENGTH - 1] = '\0';
    }

    if (artistName) {
        strncpy(state->currentArtist, artistName, MAX_TITLE_LENGTH - 1);
        state->currentArtist[MAX_TITLE_LENGTH - 1] = '\0';
    }
}

void updateVolume(UIState* state, uint8_t volume) {
    if (!state) {
        return;
    }
    state->volume = volume;
}

void handleRotation(UIState* state, int8_t direction, uint32_t currentTime) {
    (void)currentTime;
    if (!state) {
        return;
    }

    if (direction > 0) {
        scrollDown(state);
    } else if (direction < 0) {
        scrollUp(state);
    }
}

void handleButtonPress(UIState* state, uint8_t button, uint32_t currentTime) {
    (void)currentTime;
    if (!state) {
        return;
    }

    switch (button) {
        case BUTTON_CENTER:
            selectMenuItem(state);
            break;
        case BUTTON_MENU:
            goBack(state);
            break;
        case BUTTON_PLAY:
            state->isPlaying = !state->isPlaying;
            break;
        case BUTTON_NEXT:
            navigateToMenu(state, MENU_NOW_PLAYING);
            break;
        case BUTTON_PREV:
            navigateToMenu(state, MENU_MAIN);
            break;
        default:
            break;
    }
}
