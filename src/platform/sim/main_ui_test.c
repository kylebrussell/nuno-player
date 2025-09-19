#include "menu_renderer.h"
#include "ui_state.h"
#include "ui_tasks.h"
#include "nuno/display.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdbool.h>

typedef struct {
    bool leftDown;
    bool tracking;
    bool segmentCandidate;
    float lastAngle;
    float accumulated;
    uint8_t pendingButton;
    uint8_t activeButton;
} WheelInteraction;

static float normalizeAngle(float radians) {
    if (radians < 0.0f) {
        radians += 2.0f * (float)M_PI;
    }
    if (radians >= 2.0f * (float)M_PI) {
        radians -= 2.0f * (float)M_PI;
    }
    return radians;
}

static uint8_t angleToButton(float radians) {
    float degrees = normalizeAngle(radians) * 180.0f / (float)M_PI;

    if (degrees >= 45.0f && degrees < 135.0f) {
        return BUTTON_MENU;
    }
    if (degrees >= 135.0f && degrees < 225.0f) {
        return BUTTON_PREV;
    }
    if (degrees >= 225.0f && degrees < 315.0f) {
        return BUTTON_PLAY;
    }
    return BUTTON_NEXT;
}

static void handleKeyEvent(SDL_Keysym keysym, UIState *state, uint32_t currentTime) {
    switch (keysym.sym) {
        case SDLK_UP:
        case SDLK_k:
            handleRotation(state, -1, currentTime);
            break;
        case SDLK_DOWN:
        case SDLK_j:
            handleRotation(state, 1, currentTime);
            break;
        case SDLK_LEFT:
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            handleButtonPress(state, BUTTON_MENU, currentTime);
            break;
        case SDLK_RIGHT:
            handleButtonPress(state, BUTTON_NEXT, currentTime);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            handleButtonPress(state, BUTTON_CENTER, currentTime);
            break;
        case SDLK_SPACE:
            handleButtonPress(state, BUTTON_PLAY, currentTime);
            break;
        default:
            break;
    }
}

static float pointAngle(int x, int y) {
    float dx = (float)x - (float)SIM_WHEEL_CENTER_X;
    float dy = (float)y - (float)SIM_WHEEL_CENTER_Y;
    return atan2f(-dy, dx);
}

static float pointDistance(int x, int y) {
    float dx = (float)x - (float)SIM_WHEEL_CENTER_X;
    float dy = (float)y - (float)SIM_WHEEL_CENTER_Y;
    return sqrtf(dx * dx + dy * dy);
}

static void handleMouseButtonDown(const SDL_MouseButtonEvent *buttonEvent,
                                  UIState *state,
                                  WheelInteraction *wheel,
                                  uint32_t currentTime) {
    (void)state;
    (void)currentTime;
    if (buttonEvent->button != SDL_BUTTON_LEFT) {
        return;
    }

    wheel->leftDown = true;
    wheel->tracking = false;
    wheel->segmentCandidate = false;
    wheel->accumulated = 0.0f;
    wheel->pendingButton = 0;

    float distance = pointDistance(buttonEvent->x, buttonEvent->y);
    if (distance <= (float)SIM_WHEEL_INNER_RADIUS) {
        wheel->pendingButton = BUTTON_CENTER;
        wheel->activeButton = BUTTON_CENTER;
        return;
    }

    if (distance <= (float)SIM_WHEEL_OUTER_RADIUS) {
        float angle = pointAngle(buttonEvent->x, buttonEvent->y);
        wheel->tracking = true;
        wheel->segmentCandidate = true;
        wheel->lastAngle = angle;
        wheel->pendingButton = angleToButton(angle);
        wheel->activeButton = wheel->pendingButton;
        return;
    }

    wheel->activeButton = 0;
}

static void handleMouseMotion(const SDL_MouseMotionEvent *motionEvent,
                              UIState *state,
                              WheelInteraction *wheel,
                              uint32_t currentTime) {
    if (!wheel->leftDown || !wheel->tracking) {
        return;
    }

    float angle = pointAngle(motionEvent->x, motionEvent->y);
    float delta = angle - wheel->lastAngle;

    if (delta > (float)M_PI) {
        delta -= 2.0f * (float)M_PI;
    } else if (delta < -(float)M_PI) {
        delta += 2.0f * (float)M_PI;
    }

    wheel->lastAngle = angle;
    wheel->accumulated += delta;

    if (wheel->segmentCandidate) {
        if (fabsf(wheel->accumulated) > 0.12f) {
            wheel->segmentCandidate = false;
            wheel->pendingButton = 0;
            if (wheel->activeButton != BUTTON_CENTER) {
                wheel->activeButton = 0;
            }
        } else {
            uint8_t nextButton = angleToButton(angle);
            if (nextButton != wheel->pendingButton) {
                wheel->pendingButton = nextButton;
                wheel->activeButton = nextButton;
            }
        }
    }

    const float rotationStep = 0.25f; // ~14 degrees per tick
    while (wheel->accumulated <= -rotationStep) {
        handleRotation(state, 1, currentTime);
        wheel->accumulated += rotationStep;
        wheel->activeButton = 0;
    }
    while (wheel->accumulated >= rotationStep) {
        handleRotation(state, -1, currentTime);
        wheel->accumulated -= rotationStep;
        wheel->activeButton = 0;
    }
}

static void handleMouseButtonUp(const SDL_MouseButtonEvent *buttonEvent,
                                UIState *state,
                                WheelInteraction *wheel,
                                uint32_t currentTime) {
    if (buttonEvent->button != SDL_BUTTON_LEFT || !wheel->leftDown) {
        return;
    }

    wheel->leftDown = false;
    uint8_t buttonToFire = 0;

    float distance = pointDistance(buttonEvent->x, buttonEvent->y);
    if (wheel->pendingButton == BUTTON_CENTER) {
        if (distance <= (float)SIM_WHEEL_INNER_RADIUS) {
            buttonToFire = BUTTON_CENTER;
        }
    } else if (wheel->segmentCandidate && wheel->pendingButton != 0) {
        if (distance >= (float)(SIM_WHEEL_INNER_RADIUS - 6) &&
            distance <= (float)(SIM_WHEEL_OUTER_RADIUS + 6)) {
            buttonToFire = wheel->pendingButton;
        }
    }

    if (buttonToFire != 0) {
        handleButtonPress(state, buttonToFire, currentTime);
    }

    wheel->tracking = false;
    wheel->segmentCandidate = false;
    wheel->pendingButton = 0;
    wheel->activeButton = 0;
    wheel->accumulated = 0.0f;
}

int main(void) {
    if (!Display_Init("NUNO Simulator")) {
        return 1;
    }

    if (!MenuRenderer_Init()) {
        Display_Shutdown();
        return 1;
    }

    UIState uiState;
    initUIState(&uiState);

    bool running = true;
    SDL_Event event;
    WheelInteraction wheel = {0};

    while (running) {
        uint32_t currentTime = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (!event.key.repeat) {
                        handleKeyEvent(event.key.keysym, &uiState, currentTime);
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    handleMouseButtonDown(&event.button, &uiState, &wheel, currentTime);
                    break;
                case SDL_MOUSEBUTTONUP:
                    handleMouseButtonUp(&event.button, &uiState, &wheel, currentTime);
                    break;
                case SDL_MOUSEMOTION:
                    if (event.motion.state & SDL_BUTTON_LMASK) {
                        handleMouseMotion(&event.motion, &uiState, &wheel, currentTime);
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    if (event.wheel.y > 0) {
                        handleRotation(&uiState, -1, currentTime);
                    } else if (event.wheel.y < 0) {
                        handleRotation(&uiState, 1, currentTime);
                    }
                    break;
                default:
                    break;
            }
        }

        processUIEvents(&uiState, currentTime);
        Display_RenderBackground();
        MenuRenderer_Render(&uiState, currentTime);
        Display_RenderClickWheel(wheel.leftDown ? wheel.activeButton : 0);
        Display_Present();
        SDL_Delay(16);
    }

    Display_Shutdown();
    SDL_Quit();
    return 0;
}
