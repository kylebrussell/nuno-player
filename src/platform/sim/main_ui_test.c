#include "menu_renderer.h"
#include "ui_state.h"
#include "ui_tasks.h"
#include "nuno/display.h"
#include "nuno/device_profile.h"
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

/* All input geometry comes from the active device profile's wheel layout. */
static const WheelLayout *wheel(void) {
    return &Display_GetActiveProfile()->wheel;
}

static SDL_Rect getTrackpadRect(void) {
    const WheelLayout *w = wheel();
    SDL_Rect rect = {
        w->centerX - w->outerRadius,
        w->centerY - w->outerRadius,
        w->outerRadius * 2,
        w->outerRadius * 2
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

/* 3G-style separate button row hit test (mirrors sdl_mock_display layout). */
static uint8_t mapButtonRow(int x, int y) {
    const WheelLayout *w = wheel();
    if (w->type != WHEEL_TOUCH_BUTTONS) {
        return 0;
    }
    static const uint8_t buttons[4] = { BUTTON_PREV, BUTTON_MENU, BUTTON_PLAY, BUTTON_NEXT };
    int slotW = (w->outerRadius * 2) / 4;
    int left = w->centerX - w->outerRadius;
    if (y < w->buttonRowY - 12 || y > w->buttonRowY + 12) {
        return 0;
    }
    for (int i = 0; i < 4; ++i) {
        if (x >= left + i * slotW && x < left + (i + 1) * slotW) {
            return buttons[i];
        }
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
    const WheelLayout *w = wheel();
    float dx = (float)x - (float)w->centerX;
    float dy = (float)y - (float)w->centerY;
    return atan2f(-dy, dx);
}

static float pointDistance(int x, int y) {
    const WheelLayout *w = wheel();
    float dx = (float)x - (float)w->centerX;
    float dy = (float)y - (float)w->centerY;
    return sqrtf(dx * dx + dy * dy);
}

static void handleMouseButtonDown(const SDL_MouseButtonEvent *buttonEvent,
                                  UIState *state,
                                  WheelInteraction *wheelState,
                                  uint32_t currentTime) {
    if (buttonEvent->button != SDL_BUTTON_LEFT) {
        return;
    }

    wheelState->leftDown = true;
    wheelState->tracking = false;
    wheelState->segmentCandidate = false;
    wheelState->accumulated = 0.0f;
    wheelState->pendingButton = 0;

    /* Devices with a separate button row (3G): test the row first. */
    uint8_t rowButton = mapButtonRow(buttonEvent->x, buttonEvent->y);
    if (rowButton != 0) {
        wheelState->pendingButton = rowButton;
        wheelState->activeButton = rowButton;
        wheelState->segmentCandidate = true;
        return;
    }

    const WheelLayout *w = wheel();
    float distance = pointDistance(buttonEvent->x, buttonEvent->y);
    if (distance <= (float)w->innerRadius) {
        wheelState->pendingButton = BUTTON_CENTER;
        wheelState->activeButton = BUTTON_CENTER;
        return;
    }

    if (distance <= (float)w->outerRadius) {
        float angle = pointAngle(buttonEvent->x, buttonEvent->y);
        wheelState->tracking = true;
        wheelState->segmentCandidate = (w->type != WHEEL_TOUCH_BUTTONS);
        wheelState->lastAngle = angle;
        wheelState->pendingButton = (w->type != WHEEL_TOUCH_BUTTONS) ? angleToButton(angle) : 0;
        wheelState->activeButton = wheelState->pendingButton;
        return;
    }

    wheelState->activeButton = 0;
}

static void handleMouseMotion(const SDL_MouseMotionEvent *motionEvent,
                              UIState *state,
                              WheelInteraction *wheelState,
                              uint32_t currentTime) {
    if (!wheelState->leftDown || !wheelState->tracking) {
        return;
    }

    float angle = pointAngle(motionEvent->x, motionEvent->y);
    float delta = angle - wheelState->lastAngle;

    if (delta > (float)M_PI) {
        delta -= 2.0f * (float)M_PI;
    } else if (delta < -(float)M_PI) {
        delta += 2.0f * (float)M_PI;
    }

    wheelState->lastAngle = angle;
    wheelState->accumulated += delta;

    if (wheelState->segmentCandidate) {
        if (fabsf(wheelState->accumulated) > 0.12f) {
            wheelState->segmentCandidate = false;
            wheelState->pendingButton = 0;
            if (wheelState->activeButton != BUTTON_CENTER) {
                wheelState->activeButton = 0;
            }
        } else {
            uint8_t nextButton = angleToButton(angle);
            if (nextButton != wheelState->pendingButton) {
                wheelState->pendingButton = nextButton;
                wheelState->activeButton = nextButton;
            }
        }
    }

    const float rotationStep = 0.25f; // ~14 degrees per tick
    while (wheelState->accumulated <= -rotationStep) {
        handleRotation(state, 1, currentTime);
        wheelState->accumulated += rotationStep;
        wheelState->activeButton = 0;
    }
    while (wheelState->accumulated >= rotationStep) {
        handleRotation(state, -1, currentTime);
        wheelState->accumulated -= rotationStep;
        wheelState->activeButton = 0;
    }
}

static void handleMouseButtonUp(const SDL_MouseButtonEvent *buttonEvent,
                                UIState *state,
                                WheelInteraction *wheelState,
                                uint32_t currentTime) {
    if (buttonEvent->button != SDL_BUTTON_LEFT || !wheelState->leftDown) {
        return;
    }

    wheelState->leftDown = false;
    uint8_t buttonToFire = 0;
    const WheelLayout *w = wheel();

    float distance = pointDistance(buttonEvent->x, buttonEvent->y);
    if (wheelState->pendingButton == BUTTON_CENTER) {
        if (distance <= (float)w->innerRadius) {
            buttonToFire = BUTTON_CENTER;
        }
    } else if (wheelState->segmentCandidate && wheelState->pendingButton != 0) {
        if (w->type == WHEEL_TOUCH_BUTTONS) {
            /* Confirm the release is still over the same row button. */
            if (mapButtonRow(buttonEvent->x, buttonEvent->y) == wheelState->pendingButton) {
                buttonToFire = wheelState->pendingButton;
            }
        } else if (distance >= (float)(w->innerRadius - 6) &&
                   distance <= (float)(w->outerRadius + 6)) {
            buttonToFire = wheelState->pendingButton;
        }
    }

    if (buttonToFire != 0) {
        handleButtonPress(state, buttonToFire, currentTime);
    }

    wheelState->tracking = false;
    wheelState->segmentCandidate = false;
    wheelState->pendingButton = 0;
    wheelState->activeButton = 0;
    wheelState->accumulated = 0.0f;
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

/* ------------------------------------------------------------------ */
/* Device selection                                                   */
/* ------------------------------------------------------------------ */

static void printDeviceList(void) {
    printf("Available devices (--device <id>):\n");
    for (size_t i = 0; i < DeviceProfiles_Count(); ++i) {
        const DeviceProfile *p = DeviceProfiles_Get(i);
        printf("  %-14s %s\n", p->id, p->displayName);
    }
}

static size_t indexOfProfile(const DeviceProfile *profile) {
    for (size_t i = 0; i < DeviceProfiles_Count(); ++i) {
        if (DeviceProfiles_Get(i) == profile) {
            return i;
        }
    }
    return 0;
}

static void switchDevice(size_t index, WheelInteraction *wheelState, TrackpadInteraction *trackpad) {
    const DeviceProfile *p = DeviceProfiles_Get(index);
    if (!p) {
        return;
    }
    if (!Display_SwitchProfile(p)) {
        fprintf(stderr, "Failed to switch to device %s\n", p->id);
        return;
    }
    memset(wheelState, 0, sizeof(*wheelState));
    memset(trackpad, 0, sizeof(*trackpad));
    printf("Device: %s (%s, %dx%d)\n", p->displayName, p->id,
           p->screen.width, p->screen.height);
}

int main(int argc, char **argv) {
    const DeviceProfile *startProfile = DeviceProfiles_Default();
    const char *shotPath = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--list") == 0) {
            printDeviceList();
            return 0;
        }
        if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            shotPath = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            const DeviceProfile *p = DeviceProfiles_FindById(argv[++i]);
            if (!p) {
                fprintf(stderr, "Unknown device '%s'.\n", argv[i]);
                printDeviceList();
                return 1;
            }
            startProfile = p;
        } else if (strncmp(argv[i], "--device=", 9) == 0) {
            const DeviceProfile *p = DeviceProfiles_FindById(argv[i] + 9);
            if (!p) {
                fprintf(stderr, "Unknown device '%s'.\n", argv[i] + 9);
                printDeviceList();
                return 1;
            }
            startProfile = p;
        }
    }

    char windowTitle[128];
    snprintf(windowTitle, sizeof(windowTitle), "NUNO Simulator — %s", startProfile->displayName);
    if (!Display_Init(windowTitle, startProfile)) {
        return 1;
    }
    printf("Device: %s (%s, %dx%d). Use [ and ] to cycle generations.\n",
           startProfile->displayName, startProfile->id,
           startProfile->screen.width, startProfile->screen.height);

    if (!MenuRenderer_Init()) {
        Display_Shutdown();
        return 1;
    }

    // Headless capture mode: render one frame of the main menu and exit.
    if (shotPath) {
        UIState shotState;
        initUIState(&shotState);
        Display_RenderBackground();
        MenuRenderer_Render(&shotState, SDL_GetTicks());
        Display_RenderClickWheel(0);
        bool ok = Display_SaveScreenshot(shotPath);
        Display_Present();
        printf("Screenshot %s: %s\n", shotPath, ok ? "saved" : "FAILED");
        Display_Shutdown();
        SDL_Quit();
        return ok ? 0 : 1;
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
    WheelInteraction wheelState = {0};
    TrackpadInteraction trackpad = {0};
    bool trackpad_mode = false;
    size_t deviceIndex = indexOfProfile(startProfile);

    while (running) {
        uint32_t currentTime = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (!event.key.repeat) {
                        SDL_Keycode sym = event.key.keysym.sym;
                        if (sym == SDLK_t) {
                            trackpad_mode = !trackpad_mode;
                            memset(&trackpad, 0, sizeof(trackpad));
                            memset(&wheelState, 0, sizeof(wheelState));
                            printf("Trackpad mode: %s\n", trackpad_mode ? "ON" : "OFF");
                        } else if (sym == SDLK_RIGHTBRACKET) {
                            deviceIndex = (deviceIndex + 1) % DeviceProfiles_Count();
                            switchDevice(deviceIndex, &wheelState, &trackpad);
                        } else if (sym == SDLK_LEFTBRACKET) {
                            deviceIndex = (deviceIndex + DeviceProfiles_Count() - 1) % DeviceProfiles_Count();
                            switchDevice(deviceIndex, &wheelState, &trackpad);
                        } else {
                            handleKeyEvent(event.key.keysym, &uiState, currentTime);
                        }
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (trackpad_mode) {
                        handleTrackpadMouseButtonDown(&event.button, &uiState, &trackpad, currentTime);
                    } else {
                        handleMouseButtonDown(&event.button, &uiState, &wheelState, currentTime);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (trackpad_mode) {
                        handleTrackpadMouseButtonUp(&event.button, &uiState, &trackpad, currentTime);
                    } else {
                        handleMouseButtonUp(&event.button, &uiState, &wheelState, currentTime);
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (trackpad_mode) {
                        handleTrackpadMouseMotion(&event.motion, &uiState, &trackpad, currentTime);
                    } else if (event.motion.state & SDL_BUTTON_LMASK) {
                        handleMouseMotion(&event.motion, &uiState, &wheelState, currentTime);
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
        Display_RenderClickWheel(wheelState.leftDown ? wheelState.activeButton : 0);
        Display_Present();
        SDL_Delay(16);
    }

    Display_Shutdown();
    SimAudio_Shutdown();
    SDL_Quit();
    return 0;
}
