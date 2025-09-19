#include "nuno/display.h"

#include <SDL2/SDL.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

typedef struct {
    char ch;
    const char *rows[7];
} GlyphPattern;

static const GlyphPattern glyphs[] = {
    { ' ', { "     ", "     ", "     ", "     ", "     ", "     ", "     " } },
    { '!', { "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "     ", "  X  " } },
    { '\'', { "  X  ", "  X  ", " X   ", "     ", "     ", "     ", "     " } },
    { ',', { "     ", "     ", "     ", "     ", "     ", "  X  ", " X   " } },
    { '&', { " XX  ", "X  X ", "X  X ", " XX  ", "X X X", "X  X ", " XX X" } },
    { '-', { "     ", "     ", "     ", " XXX ", "     ", "     ", "     " } },
    { '.', { "     ", "     ", "     ", "     ", "     ", "  X  ", "     " } },
    { ':', { "     ", "  X  ", "     ", "     ", "     ", "  X  ", "     " } },
    { '?', { " XXX ", "X   X", "    X", "   X ", "  X  ", "     ", "  X  " } },
    { '0', { " XXX ", "X   X", "X  XX", "X X X", "XX  X", "X   X", " XXX " } },
    { '1', { "  X  ", " XX  ", "  X  ", "  X  ", "  X  ", "  X  ", "XXXXX" } },
    { '2', { " XXX ", "X   X", "    X", "   X ", "  X  ", " X   ", "XXXXX" } },
    { '3', { " XXX ", "X   X", "    X", "  XX ", "    X", "X   X", " XXX " } },
    { '4', { "   X ", "  XX ", " X X ", "X  X ", "XXXXX", "   X ", "   X " } },
    { '5', { "XXXXX", "X    ", "X    ", "XXXX ", "    X", "    X", "XXXX " } },
    { '6', { " XXX ", "X   X", "X    ", "XXXX ", "X   X", "X   X", " XXX " } },
    { '7', { "XXXXX", "    X", "   X ", "  X  ", "  X  ", "  X  ", "  X  " } },
    { '8', { " XXX ", "X   X", "X   X", " XXX ", "X   X", "X   X", " XXX " } },
    { '9', { " XXX ", "X   X", "X   X", " XXXX", "    X", "X   X", " XXX " } },
    { 'A', { " XXX ", "X   X", "X   X", "XXXXX", "X   X", "X   X", "X   X" } },
    { 'B', { "XXXX ", "X   X", "X   X", "XXXX ", "X   X", "X   X", "XXXX " } },
    { 'C', { " XXX ", "X   X", "X    ", "X    ", "X    ", "X   X", " XXX " } },
    { 'D', { "XXXX ", "X   X", "X   X", "X   X", "X   X", "X   X", "XXXX " } },
    { 'E', { "XXXXX", "X    ", "X    ", "XXXX ", "X    ", "X    ", "XXXXX" } },
    { 'F', { "XXXXX", "X    ", "X    ", "XXXX ", "X    ", "X    ", "X    " } },
    { 'G', { " XXX ", "X   X", "X    ", "X XXX", "X   X", "X   X", " XXXX" } },
    { 'H', { "X   X", "X   X", "X   X", "XXXXX", "X   X", "X   X", "X   X" } },
    { 'I', { "XXXXX", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "XXXXX" } },
    { 'J', { "  XXX", "   X ", "   X ", "   X ", "   X ", "X  X ", " XX  " } },
    { 'K', { "X   X", "X  X ", "X X  ", "XX   ", "X X  ", "X  X ", "X   X" } },
    { 'L', { "X    ", "X    ", "X    ", "X    ", "X    ", "X    ", "XXXXX" } },
    { 'M', { "X   X", "XX XX", "X X X", "X   X", "X   X", "X   X", "X   X" } },
    { 'N', { "X   X", "XX  X", "X X X", "X  XX", "X   X", "X   X", "X   X" } },
    { 'O', { " XXX ", "X   X", "X   X", "X   X", "X   X", "X   X", " XXX " } },
    { 'P', { "XXXX ", "X   X", "X   X", "XXXX ", "X    ", "X    ", "X    " } },
    { 'Q', { " XXX ", "X   X", "X   X", "X   X", "X X X", "X  X ", " XX X" } },
    { 'R', { "XXXX ", "X   X", "X   X", "XXXX ", "X X  ", "X  X ", "X   X" } },
    { 'S', { " XXXX", "X    ", "X    ", " XXX ", "    X", "    X", "XXXX " } },
    { 'T', { "XXXXX", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "  X  " } },
    { 'U', { "X   X", "X   X", "X   X", "X   X", "X   X", "X   X", " XXX " } },
    { 'V', { "X   X", "X   X", "X   X", "X   X", " X X ", " X X ", "  X  " } },
    { 'W', { "X   X", "X   X", "X   X", "X X X", "X X X", "XX XX", "X   X" } },
    { 'X', { "X   X", "X   X", " X X ", "  X  ", " X X ", "X   X", "X   X" } },
    { 'Y', { "X   X", "X   X", " X X ", "  X  ", "  X  ", "  X  ", "  X  " } },
    { 'Z', { "XXXXX", "    X", "   X ", "  X  ", " X   ", "X    ", "XXXXX" } }
};

static const GlyphPattern* findGlyph(char c) {
    char upper = (char)toupper((unsigned char)c);
    size_t count = sizeof(glyphs) / sizeof(glyphs[0]);
    for (size_t i = 0; i < count; ++i) {
        if (glyphs[i].ch == upper) {
            return &glyphs[i];
        }
    }
    return NULL;
}

static void setPaletteColor(uint8_t color) {
    if (!renderer) {
        return;
    }
    if (color == 0) {
        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
    }
}

static void drawGlyph(const GlyphPattern *glyph, int originX, int originY, uint8_t color) {
    if (!renderer || !glyph) {
        return;
    }

    setPaletteColor(color);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (glyph->rows[row][col] != ' ') {
                int x = originX + col;
                int y = originY + row;
                if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
                    SDL_RenderDrawPoint(renderer, x, y);
                }
            }
        }
    }
}

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
    if (!renderer || !text) {
        return;
    }

    int penX = x;
    size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i) {
        char c = text[i];
        if (c == '\0') {
            break;
        }
        if (c == ' ') {
            penX += 4;
            continue;
        }

        const GlyphPattern *glyph = findGlyph(c);
        if (!glyph) {
            penX += 4;
            continue;
        }

        drawGlyph(glyph, penX, y, color);
        penX += 6;
    }
}

void Display_DrawRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) {
        return;
    }
    SDL_Rect r = { x, y, width, height };
    setPaletteColor(color);
    SDL_RenderDrawRect(renderer, &r);
}

void Display_FillRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) {
        return;
    }
    SDL_Rect r = { x, y, width, height };
    setPaletteColor(color);
    SDL_RenderFillRect(renderer, &r);
}
