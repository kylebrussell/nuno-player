#include "menu_renderer.h"
#include "ui_state.h"
#include "ui_tasks.h"
#include "nuno/display.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

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
        MenuRenderer_Render(&uiState, currentTime);
        SDL_Delay(16);
    }

    Display_Shutdown();
    SDL_Quit();
    return 0;
}
