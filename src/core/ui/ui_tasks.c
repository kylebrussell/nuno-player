#include "ui_tasks.h"
#include "ui_state.h"
#include "menu_renderer.h"

#include <string.h>

typedef enum {
    UI_EVENT_BUTTON,
    UI_EVENT_ROTATION
} UIEventType;

typedef struct {
    UIEventType type;
    uint32_t timestamp;
    union {
        struct {
            uint8_t button;
        } button;
        struct {
            int8_t delta;
        } rotation;
    } data;
} UIEvent;

#define UI_EVENT_QUEUE_CAPACITY 32

static struct {
    UIEvent events[UI_EVENT_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
} eventQueue = {0};

static void enqueueEvent(UIEvent event) {
    uint8_t nextHead = (uint8_t)((eventQueue.head + 1) % UI_EVENT_QUEUE_CAPACITY);
    if (nextHead == eventQueue.tail) {
        eventQueue.tail = (uint8_t)((eventQueue.tail + 1) % UI_EVENT_QUEUE_CAPACITY);
    }
    eventQueue.events[eventQueue.head] = event;
    eventQueue.head = nextHead;
}

static bool dequeueEvent(UIEvent *event) {
    if (eventQueue.head == eventQueue.tail) {
        return false;
    }
    *event = eventQueue.events[eventQueue.tail];
    eventQueue.tail = (uint8_t)((eventQueue.tail + 1) % UI_EVENT_QUEUE_CAPACITY);
    return true;
}

static bool applyButtonEvent(UIState *state, uint8_t button) {
    if (!state) {
        return false;
    }

    bool changed = false;
    switch (button) {
        case BUTTON_CENTER: {
            MenuType previousMenu = state->currentMenuType;
            uint8_t previousIndex = state->currentMenu.selectedIndex;
            selectMenuItem(state);
            changed = (state->currentMenuType != previousMenu) ||
                      (state->currentMenu.selectedIndex != previousIndex);
            break;
        }
        case BUTTON_MENU: {
            MenuType previousMenu = state->currentMenuType;
            uint8_t previousDepth = state->navigationDepth;
            goBack(state);
            changed = (state->currentMenuType != previousMenu) ||
                      (state->navigationDepth != previousDepth);
            break;
        }
        case BUTTON_PLAY:
            state->isPlaying = !state->isPlaying;
            changed = true;
            break;
        case BUTTON_NEXT: {
            MenuType previousMenu = state->currentMenuType;
            navigateToMenu(state, MENU_NOW_PLAYING);
            changed = (state->currentMenuType != previousMenu);
            break;
        }
        case BUTTON_PREV: {
            MenuType previousMenu = state->currentMenuType;
            navigateToMenu(state, MENU_MAIN);
            changed = (state->currentMenuType != previousMenu);
            break;
        }
        default:
            break;
    }
    return changed;
}

static bool applyRotationEvent(UIState *state, int8_t direction, uint32_t timestamp) {
    if (!state || direction == 0) {
        return false;
    }

    uint8_t previousIndex = state->currentMenu.selectedIndex;
    uint8_t previousOffset = state->currentMenu.scrollOffset;

    if (direction > 0) {
        scrollDown(state);
    } else {
        scrollUp(state);
    }

    bool changed = (state->currentMenu.selectedIndex != previousIndex) ||
                   (state->currentMenu.scrollOffset != previousOffset);

    if (changed) {
        float targetOffset = (float)(state->currentMenu.scrollOffset * ITEM_HEIGHT);
        MenuRenderer_StartScroll(targetOffset, timestamp);
    }

    return changed;
}

bool processUIEvents(UIState* state, uint32_t currentTime) {
    if (!state) {
        return false;
    }

    bool changed = false;
    UIEvent event;
    while (dequeueEvent(&event)) {
        switch (event.type) {
            case UI_EVENT_BUTTON:
                changed |= applyButtonEvent(state, event.data.button.button);
                break;
            case UI_EVENT_ROTATION:
                changed |= applyRotationEvent(state, event.data.rotation.delta, event.timestamp);
                break;
            default:
                break;
        }
    }

    (void)currentTime;
    return changed;
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
    (void)state;
    if (direction == 0) {
        return;
    }

    UIEvent event = {
        .type = UI_EVENT_ROTATION,
        .timestamp = currentTime
    };
    event.data.rotation.delta = direction;
    enqueueEvent(event);
}

void handleButtonPress(UIState* state, uint8_t button, uint32_t currentTime) {
    (void)state;
    UIEvent event = {
        .type = UI_EVENT_BUTTON,
        .timestamp = currentTime
    };
    event.data.button.button = button;
    enqueueEvent(event);
}
