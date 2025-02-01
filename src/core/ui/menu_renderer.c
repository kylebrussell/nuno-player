#include "menu_renderer.h"
#include "ui_state.h"
#include "menu_items.h"
#include <string.h>
#include <math.h>

// Animation constants
#define SCROLL_ANIMATION_DURATION_MS 150
#define TRANSITION_ANIMATION_DURATION_MS 200
#define PROGRESS_BAR_HEIGHT 2
#define BATTERY_ICON_WIDTH 15
#define BATTERY_ICON_HEIGHT 8

typedef struct {
    float currentScrollOffset;
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
    return t * (2 - t);
}

static void updateScrollAnimation(uint32_t currentTime) {
    if (!scrollState.isAnimating) return;
    
    float elapsed = (float)(currentTime - scrollState.animationStartTime) / SCROLL_ANIMATION_DURATION_MS;
    if (elapsed >= 1.0f) {
        scrollState.currentScrollOffset = scrollState.targetScrollOffset;
        scrollState.isAnimating = false;
        return;
    }
    
    float progress = easeOutQuad(elapsed);
    float distance = scrollState.targetScrollOffset - scrollState.currentScrollOffset;
    scrollState.currentScrollOffset += distance * progress;
}

void MenuRenderer_StartScroll(float targetOffset, uint32_t currentTime) {
    scrollState.animationStartTime = currentTime;
    scrollState.targetScrollOffset = targetOffset;
    scrollState.isAnimating = true;
}

void MenuRenderer_StartTransition(MenuType from, MenuType to, uint32_t currentTime) {
    transitionState.startTime = currentTime;
    transitionState.fromMenu = from;
    transitionState.toMenu = to;
    transitionState.progress = 0.0f;
    transitionState.isActive = true;
}

static void renderMenuItem(const MenuItem* item, uint8_t index, float yOffset, bool selected) {
    // Calculate y position with smooth scrolling offset
    int y = (index * ITEM_HEIGHT) - (int)scrollState.currentScrollOffset;
    
    // Only render if item is visible
    if (y < -ITEM_HEIGHT || y > DISPLAY_HEIGHT) return;
    
    // Apply selection highlight
    if (selected) {
        Display_FillRect(0, y, DISPLAY_WIDTH, ITEM_HEIGHT, HIGHLIGHT_COLOR);
    }
    
    // Render text with appropriate styling
    Display_DrawText(item->text, TEXT_MARGIN, y + (ITEM_HEIGHT - TEXT_HEIGHT) / 2,
                    selected ? SELECTED_TEXT_COLOR : NORMAL_TEXT_COLOR);
}

static void renderProgressBar(uint16_t current, uint16_t total) {
    int width = (int)((float)current / total * DISPLAY_WIDTH);
    Display_FillRect(0, DISPLAY_HEIGHT - PROGRESS_BAR_HEIGHT,
                    width, PROGRESS_BAR_HEIGHT, PROGRESS_COLOR);
}

static void renderBatteryIndicator(uint8_t percentage) {
    // Draw battery outline
    Display_DrawRect(DISPLAY_WIDTH - BATTERY_ICON_WIDTH - 5, 2,
                    BATTERY_ICON_WIDTH, BATTERY_ICON_HEIGHT, NORMAL_TEXT_COLOR);
    
    // Draw battery level
    int fillWidth = (int)((float)percentage / 100 * (BATTERY_ICON_WIDTH - 4));
    Display_FillRect(DISPLAY_WIDTH - BATTERY_ICON_WIDTH - 3, 4,
                    fillWidth, BATTERY_ICON_HEIGHT - 4, NORMAL_TEXT_COLOR);
}

void MenuRenderer_Render(const UIState* state, uint32_t currentTime) {
    Display_Clear();
    
    // Update animations
    updateScrollAnimation(currentTime);
    
    // Render menu title
    Display_DrawText(state->currentMenu.title, TEXT_MARGIN, 0, TITLE_TEXT_COLOR);
    
    // Render menu items with smooth scrolling
    for (int i = 0; i < state->currentMenu.itemCount; i++) {
        renderMenuItem(&state->currentMenu.items[i], i,
                      scrollState.currentScrollOffset,
                      i == state->currentMenu.selectedIndex);
    }
    
    // Render progress bar for playback
    if (state->currentMenuType == MENU_NOW_PLAYING && state->isPlaying) {
        renderProgressBar(state->currentTrackTime, state->totalTrackTime);
    }
    
    // Render battery indicator
    renderBatteryIndicator(state->batteryLevel);
    
    Display_Update();
}