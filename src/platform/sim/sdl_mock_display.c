// sdl_mock_display.c
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#define DISPLAY_WIDTH  160
#define DISPLAY_HEIGHT 128

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

// Basic display API implementations

void Display_Clear(void) {
    // White BG
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
}

void Display_Update(void) {
    SDL_RenderPresent(renderer);
}

void Display_DrawText(const char* text, int x, int y, uint8_t color) {
    // Minimal stub: Instead of real text, draw a filled rect as placeholder.
    SDL_Rect r = { x, y, 50, 12 };
    // Use black for text (ignoring 'color' for now)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &r);
    // In a real setup, you'd use SDL_ttf here.
}

void Display_DrawRect(int x, int y, int width, int height, uint8_t color) {
    SDL_Rect r = { x, y, width, height };
    if (color == 0)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &r);
}

void Display_FillRect(int x, int y, int width, int height, uint8_t color) {
    SDL_Rect r = { x, y, width, height };
    if (color == 0)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &r);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    // Scale window up for easier viewing (4x scale)
    window = SDL_CreateWindow("SDL Mock Display", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, DISPLAY_WIDTH * 4, DISPLAY_HEIGHT * 4,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    bool running = true;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }
        // Simulate UI rendering cycle:
        Display_Clear();
        // For demo, draw a black filled rectangle and a "text" placeholder
        Display_FillRect(10, 10, 50, 20, 0);
        Display_DrawText("Hello", 15, 15, 0);
        Display_DrawRect(5, 5, DISPLAY_WIDTH - 10, DISPLAY_HEIGHT - 10, 0);
        Display_Update();
        SDL_Delay(16); // ~60 FPS
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}