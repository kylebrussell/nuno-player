// main_ui_test.c
#include "ui_state.h"
#include "menu_renderer.h"
#include "ui_tasks.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

extern void Display_Clear(void);
extern void Display_Update(void);
extern void Display_DrawText(const char* text, int x, int y, uint8_t color);
extern void Display_DrawRect(int x, int y, int width, int height, uint8_t color);
extern void Display_FillRect(int x, int y, int width, int height, uint8_t color);

int main(int argc, char* argv[]) {
    // Initialize SDL (using our SDL mock display code)
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("UI Test",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          DISPLAY_WIDTH * 4,
                                          DISPLAY_HEIGHT * 4,
                                          SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Hook our renderer into the SDL mock display functions if needed,
    // or simply compile/link them together.
    
    // Set up your UI state
    UIState uiState;
    initUIState(&uiState);
    
    uint32_t startTime = SDL_GetTicks();
    bool running = true;
    SDL_Event e;
    
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            // Map SDL events to your UI events if desired (simulate click wheel, etc.)
        }
        
        uint32_t currentTime = SDL_GetTicks() - startTime;
        // Call your UI render routine which internally calls Display_Clear, DrawText, etc.
        MenuRenderer_Render(&uiState, currentTime);
        
        SDL_Delay(16); // ~60 FPS
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}