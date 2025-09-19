#include "nuno/display.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

bool Display_Init(const char *title) {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return false;
        }
    }

    if (!title) {
        title = "NUNO Player";
    }

    const int scale = 4;
    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              DISPLAY_WIDTH * scale,
                              DISPLAY_HEIGHT * scale,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        Display_Shutdown();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        Display_Shutdown();
        return false;
    }

    SDL_RenderSetLogicalSize(renderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    return true;
}

void Display_Shutdown(void) {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

void Display_Clear(void) {
    if (!renderer) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    SDL_RenderClear(renderer);
}

void Display_Update(void) {
    if (!renderer) {
        return;
    }
    SDL_RenderPresent(renderer);
}

void Display_DrawText(const char* text, int x, int y, uint8_t color) {
    (void)text;
    if (!renderer) {
        return;
    }
    SDL_Rect r = { x, y, 64, 12 };
    uint8_t shade = (color == 0) ? 0 : 20;
    SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);
    SDL_RenderFillRect(renderer, &r);
}

void Display_DrawRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) {
        return;
    }
    uint8_t shade = (color == 0) ? 0 : 20;
    SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);
    SDL_Rect r = { x, y, width, height };
    SDL_RenderDrawRect(renderer, &r);
}

void Display_FillRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) {
        return;
    }
    uint8_t shade = (color == 0) ? 0 : 20;
    SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);
    SDL_Rect r = { x, y, width, height };
    SDL_RenderFillRect(renderer, &r);
}
