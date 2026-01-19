#include "menu_renderer.h"
#include "ui_state.h"
#include "ui_tasks.h"
#include "nuno/display.h"
#include "platform/sim/audio_controller.h"

#include "nuno/audio_pipeline.h"
#include "nuno/audio_buffer.h"
#include "nuno/format_decoder.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool leftDown;
    bool tracking;
    bool segmentCandidate;
    float lastAngle;
    float accumulated;
    uint8_t pendingButton;
    uint8_t activeButton;
} WheelInteraction;

typedef struct {
    bool down;
    int startX;
    int startY;
    int lastX;
    int lastY;
    float scrollAccum;
    uint32_t downTime;
} TrackpadInteraction;

#define TRACKPAD_TAP_MAX_MS 180U
#define TRACKPAD_TAP_MAX_MOVE 10
#define TRACKPAD_SCROLL_STEP 12.0f
#define TRACKPAD_ZONE_RATIO 0.25f

static SDL_Rect getTrackpadRect(void) {
    SDL_Rect rect = {
        SIM_WHEEL_CENTER_X - SIM_WHEEL_OUTER_RADIUS,
        SIM_WHEEL_CENTER_Y - SIM_WHEEL_OUTER_RADIUS,
        SIM_WHEEL_OUTER_RADIUS * 2,
        SIM_WHEEL_OUTER_RADIUS * 2
    };
    return rect;
}

static bool pointInRect(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

static uint8_t mapTrackpadZone(int x, int y, SDL_Rect rect) {
    float relX = (float)(x - rect.x) / (float)rect.w;
    float relY = (float)(y - rect.y) / (float)rect.h;

    if (relY <= TRACKPAD_ZONE_RATIO) {
        return BUTTON_MENU;
    }
    if (relY >= (1.0f - TRACKPAD_ZONE_RATIO)) {
        return BUTTON_PLAY;
    }
    if (relX <= TRACKPAD_ZONE_RATIO) {
        return BUTTON_PREV;
    }
    if (relX >= (1.0f - TRACKPAD_ZONE_RATIO)) {
        return BUTTON_NEXT;
    }
    return 0;
}

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

static void handleTrackpadMouseButtonDown(const SDL_MouseButtonEvent *buttonEvent,
                                          UIState *state,
                                          TrackpadInteraction *trackpad,
                                          uint32_t currentTime) {
    SDL_Rect rect = getTrackpadRect();
    if (buttonEvent->button == SDL_BUTTON_RIGHT) {
        handleButtonPress(state, BUTTON_CENTER, currentTime);
        return;
    }
    if (buttonEvent->button != SDL_BUTTON_LEFT) {
        return;
    }
    if (!pointInRect(buttonEvent->x, buttonEvent->y, rect)) {
        return;
    }

    trackpad->down = true;
    trackpad->startX = buttonEvent->x;
    trackpad->startY = buttonEvent->y;
    trackpad->lastX = buttonEvent->x;
    trackpad->lastY = buttonEvent->y;
    trackpad->scrollAccum = 0.0f;
    trackpad->downTime = currentTime;
}

static void handleTrackpadMouseMotion(const SDL_MouseMotionEvent *motionEvent,
                                      UIState *state,
                                      TrackpadInteraction *trackpad,
                                      uint32_t currentTime) {
    if (!trackpad->down) {
        return;
    }

    int dy = motionEvent->y - trackpad->lastY;
    trackpad->lastX = motionEvent->x;
    trackpad->lastY = motionEvent->y;

    if (dy == 0) {
        return;
    }

    trackpad->scrollAccum += (float)dy;
    while (trackpad->scrollAccum >= TRACKPAD_SCROLL_STEP) {
        handleRotation(state, 1, currentTime);
        trackpad->scrollAccum -= TRACKPAD_SCROLL_STEP;
    }
    while (trackpad->scrollAccum <= -TRACKPAD_SCROLL_STEP) {
        handleRotation(state, -1, currentTime);
        trackpad->scrollAccum += TRACKPAD_SCROLL_STEP;
    }
}

static void handleTrackpadMouseButtonUp(const SDL_MouseButtonEvent *buttonEvent,
                                        UIState *state,
                                        TrackpadInteraction *trackpad,
                                        uint32_t currentTime) {
    if (buttonEvent->button != SDL_BUTTON_LEFT || !trackpad->down) {
        return;
    }

    trackpad->down = false;
    uint32_t duration = currentTime - trackpad->downTime;
    int dx = trackpad->lastX - trackpad->startX;
    int dy = trackpad->lastY - trackpad->startY;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    if (duration <= TRACKPAD_TAP_MAX_MS && dx <= TRACKPAD_TAP_MAX_MOVE && dy <= TRACKPAD_TAP_MAX_MOVE) {
        SDL_Rect rect = getTrackpadRect();
        uint8_t button = mapTrackpadZone(trackpad->startX, trackpad->startY, rect);
        if (button != 0U) {
            handleButtonPress(state, button, currentTime);
        }
    }
}

int main(void) {
    if (!Display_Init("NUNO Simulator")) {
        return 1;
    }

    if (!MenuRenderer_Init()) {
        Display_Shutdown();
        return 1;
    }

    bool audio_ready = SimAudio_Init();
    printf("Audio initialization: %s\n", audio_ready ? "SUCCESS" : "FAILED");

    UIState uiState;
    initUIState(&uiState);

    if (audio_ready) {
        UIState_SetPlaybackHandler(&uiState, SimAudio_PlayTrack, NULL);
        printf("Playback handler set\n");
    } else {
        printf("No playback handler set due to audio init failure\n");
    }

    bool running = true;
    SDL_Event event;
    WheelInteraction wheel = {0};
    TrackpadInteraction trackpad = {0};
    bool trackpad_mode = false;

    while (running) {
        uint32_t currentTime = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (!event.key.repeat) {
                        if (event.key.keysym.sym == SDLK_t) {
                            trackpad_mode = !trackpad_mode;
                            memset(&trackpad, 0, sizeof(trackpad));
                            memset(&wheel, 0, sizeof(wheel));
                            printf("Trackpad mode: %s\n", trackpad_mode ? "ON" : "OFF");
                        } else {
                            handleKeyEvent(event.key.keysym, &uiState, currentTime);
                        }
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (trackpad_mode) {
                        handleTrackpadMouseButtonDown(&event.button, &uiState, &trackpad, currentTime);
                    } else {
                        handleMouseButtonDown(&event.button, &uiState, &wheel, currentTime);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (trackpad_mode) {
                        handleTrackpadMouseButtonUp(&event.button, &uiState, &trackpad, currentTime);
                    } else {
                        handleMouseButtonUp(&event.button, &uiState, &wheel, currentTime);
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (trackpad_mode) {
                        handleTrackpadMouseMotion(&event.motion, &uiState, &trackpad, currentTime);
                    } else if (event.motion.state & SDL_BUTTON_LMASK) {
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

        // Update playback time/progress once per frame
        {
            bool isPlaying = (AudioPipeline_GetState() == PIPELINE_STATE_PLAYING);
            AudioBufferStats stats = {0};
            AudioBuffer_GetBufferStats(&stats);
            FormatDecoder *dec = AudioBuffer_GetDecoder();
            uint32_t sr = dec ? format_decoder_get_sample_rate(dec) : 0U;
            uint16_t seconds = 0;
            if (sr > 0U) {
                seconds = (uint16_t)(stats.total_samples / sr);
            }
            // Pass totalTime as 0 to keep existing value in UI
            updatePlaybackInfo(&uiState, seconds, 0, isPlaying);
        }
        Display_RenderBackground();
        MenuRenderer_Render(&uiState, currentTime);
        Display_RenderClickWheel(wheel.leftDown ? wheel.activeButton : 0);
        Display_Present();
        SDL_Delay(16);
    }

    Display_Shutdown();
    SimAudio_Shutdown();
    SDL_Quit();
    return 0;
}
