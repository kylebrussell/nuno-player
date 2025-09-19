#include "menu_renderer.h"
#include "ui_state.h"
#include "menu_items.h"

#include <math.h>
#include <string.h>

// Animation constants
#define SCROLL_ANIMATION_DURATION_MS 150
#define TRANSITION_ANIMATION_DURATION_MS 200
#define PROGRESS_BAR_HEIGHT 2
#define BATTERY_ICON_WIDTH 15
#define BATTERY_ICON_HEIGHT 8

typedef struct {
    float currentScrollOffset;
    float startScrollOffset;
    float targetScrollOffset;
    uint32_t animationStartTime;
    bool isAnimating;
} ScrollState;

typedef struct {
    float progress;
    uint32_t startTime;
    MenuType fromMenu;
    MenuType toMenu;
    bool isActive;
} TransitionState;

static ScrollState scrollState = {0};
static TransitionState transitionState = {0};

// Helper function for smooth easing
static float easeOutQuad(float t) {
    return t * (2.0f - t);
}

static void updateScrollAnimation(uint32_t currentTime) {
    if (!scrollState.isAnimating) {
        return;
    }

    float elapsed = (float)(currentTime - scrollState.animationStartTime) / SCROLL_ANIMATION_DURATION_MS;
    if (elapsed >= 1.0f) {
        scrollState.currentScrollOffset = scrollState.targetScrollOffset;
        scrollState.isAnimating = false;
        return;
    }

    float progress = easeOutQuad(elapsed);
    scrollState.currentScrollOffset = scrollState.startScrollOffset +
        (scrollState.targetScrollOffset - scrollState.startScrollOffset) * progress;
}

bool MenuRenderer_Init(void) {
    memset(&scrollState, 0, sizeof(scrollState));
    memset(&transitionState, 0, sizeof(transitionState));
    return true;
}

void MenuRenderer_StartScroll(float targetOffset, uint32_t currentTime) {
    if (fabsf(scrollState.currentScrollOffset - targetOffset) < 0.5f) {
        scrollState.currentScrollOffset = targetOffset;
        scrollState.isAnimating = false;
        return;
    }

    scrollState.animationStartTime = currentTime;
    scrollState.startScrollOffset = scrollState.currentScrollOffset;
    scrollState.targetScrollOffset = targetOffset;
    scrollState.isAnimating = true;
}

void MenuRenderer_StartTransition(MenuType from, MenuType to, uint32_t currentTime) {
    transitionState.startTime = currentTime;
    transitionState.fromMenu = from;
    transitionState.toMenu = to;
    transitionState.progress = 0.0f;
    transitionState.isActive = (from != to);
}

static void renderMenuItem(const MenuItem* item, uint8_t index, bool selected) {
    int y = TITLE_BAR_HEIGHT + (index * ITEM_HEIGHT) - (int)scrollState.currentScrollOffset;

    if ((y + ITEM_HEIGHT) <= TITLE_BAR_HEIGHT || y >= DISPLAY_HEIGHT) {
        return;
    }

    if (selected) {
        Display_FillRect(0, y, DISPLAY_WIDTH, ITEM_HEIGHT, HIGHLIGHT_COLOR);
    }

    Display_DrawText(item->text,
                     TEXT_MARGIN,
                     y + (ITEM_HEIGHT - TEXT_HEIGHT) / 2,
                     selected ? SELECTED_TEXT_COLOR : NORMAL_TEXT_COLOR);
}

static void renderProgressBar(uint16_t current, uint16_t total) {
    if (total == 0) {
        return;
    }

    int width = (int)((float)current / total * DISPLAY_WIDTH);
    if (width < 0) {
        width = 0;
    } else if (width > DISPLAY_WIDTH) {
        width = DISPLAY_WIDTH;
    }

    Display_FillRect(0,
                     DISPLAY_HEIGHT - PROGRESS_BAR_HEIGHT,
                     width,
                     PROGRESS_BAR_HEIGHT,
                     PROGRESS_COLOR);
}

static void renderBatteryIndicator(uint8_t percentage) {
    Display_DrawRect(DISPLAY_WIDTH - BATTERY_ICON_WIDTH - 5,
                     2,
                     BATTERY_ICON_WIDTH,
                     BATTERY_ICON_HEIGHT,
                     NORMAL_TEXT_COLOR);

    int fillWidth = (int)((float)percentage / 100.0f * (BATTERY_ICON_WIDTH - 4));
    if (fillWidth < 0) {
        fillWidth = 0;
    }
    Display_FillRect(DISPLAY_WIDTH - BATTERY_ICON_WIDTH - 3,
                     4,
                     fillWidth,
                     BATTERY_ICON_HEIGHT - 4,
                     NORMAL_TEXT_COLOR);
}

bool MenuRenderer_IsAnimating(void) {
    return scrollState.isAnimating || transitionState.isActive;
}

void MenuRenderer_SetBrightness(uint8_t brightness) {
    (void)brightness;
}

void MenuRenderer_FinishAnimations(void) {
    scrollState.currentScrollOffset = scrollState.targetScrollOffset;
    scrollState.isAnimating = false;
    transitionState.isActive = false;
}

void MenuRenderer_Render(const UIState* state, uint32_t currentTime) {
    if (!state) {
        return;
    }

    float targetOffset = (float)(state->currentMenu.scrollOffset * ITEM_HEIGHT);
    if (!scrollState.isAnimating) {
        scrollState.currentScrollOffset = targetOffset;
    } else if (fabsf(scrollState.targetScrollOffset - targetOffset) > 0.1f) {
        scrollState.startScrollOffset = scrollState.currentScrollOffset;
        scrollState.targetScrollOffset = targetOffset;
        scrollState.animationStartTime = currentTime;
    }

    updateScrollAnimation(currentTime);

    Display_Clear();
    for (int i = 0; i < state->currentMenu.itemCount; ++i) {
        renderMenuItem(&state->currentMenu.items[i], i,
                       i == state->currentMenu.selectedIndex);
    }

    Display_FillRect(0, 0, DISPLAY_WIDTH, TITLE_BAR_HEIGHT, 0);
    Display_DrawText(state->currentMenu.title, TEXT_MARGIN, 2, TITLE_TEXT_COLOR);

    if (state->currentMenuType == MENU_NOW_PLAYING && state->isPlaying) {
        renderProgressBar(state->currentTrackTime, state->totalTrackTime);
    }

    renderBatteryIndicator(state->batteryLevel);

    Display_Update();
}
