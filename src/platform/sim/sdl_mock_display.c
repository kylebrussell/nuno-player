#include "nuno/display.h"
#include "ui_tasks.h"

#include <SDL2/SDL.h>

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

static const SDL_Rect DISPLAY_RECT = {
    SIM_DISPLAY_MARGIN_X,
    SIM_DISPLAY_MARGIN_Y,
    DISPLAY_WIDTH,
    DISPLAY_HEIGHT
};

typedef struct {
    char ch;
    const char *rows[7];
} GlyphPattern;

static const GlyphPattern glyphs[] = {
    { ' ', { "     ", "     ", "     ", "     ", "     ", "     ", "     " } },
    { '!', { "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "     ", "  X  " } },
    { '\'', { "  X  ", "  X  ", " X   ", "     ", "     ", "     ", "     " } },
    { ',', { "     ", "     ", "     ", "     ", "     ", "  X  ", " X   " } },
    { '-', { "     ", "     ", "     ", " XXX ", "     ", "     ", "     " } },
    { '.', { "     ", "     ", "     ", "     ", "     ", "  X  ", "     " } },
    { ':', { "     ", "  X  ", "     ", "     ", "     ", "  X  ", "     " } },
    { '<', { "   X ", "  X  ", " X   ", "X    ", " X   ", "  X  ", "   X " } },
    { '>', { " X   ", "  X  ", "   X ", "    X", "   X ", "  X  ", " X   " } },
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

static inline Uint8 clamp_u8(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (Uint8)value;
}

static SDL_Color lerpColor(SDL_Color a, SDL_Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    SDL_Color result = {
        clamp_u8((int)((1.0f - t) * a.r + t * b.r)),
        clamp_u8((int)((1.0f - t) * a.g + t * b.g)),
        clamp_u8((int)((1.0f - t) * a.b + t * b.b)),
        clamp_u8((int)((1.0f - t) * a.a + t * b.a))
    };
    return result;
}

static void drawGlyphClipped(const GlyphPattern *glyph, int originX, int originY, SDL_Color color, const SDL_Rect *clip) {
    if (!renderer || !glyph) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (glyph->rows[row][col] == ' ') {
                continue;
            }
            int x = originX + col;
            int y = originY + row;
            if (clip) {
                if (x < clip->x || x >= clip->x + clip->w || y < clip->y || y >= clip->y + clip->h) {
                    continue;
                }
            }
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
}

static void beginDisplayDraw(void) {
    if (!renderer) {
        return;
    }
    SDL_RenderSetViewport(renderer, &DISPLAY_RECT);
    SDL_Rect clip = {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
    SDL_RenderSetClipRect(renderer, &clip);
}

static void endDisplayDraw(void) {
    if (!renderer) {
        return;
    }
    SDL_RenderSetClipRect(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
}

static void renderBrushedBackground(void) {
    if (!renderer) {
        return;
    }

    // SNES-era pixel-art take: 6-color silver palette + ordered dithering
    const SDL_Color palette[6] = {
        {235, 236, 242, 255}, // lightest
        {220, 222, 230, 255},
        {204, 206, 214, 255},
        {188, 190, 198, 255},
        {168, 170, 178, 255},
        {148, 150, 158, 255}  // darkest
    };

    static const uint8_t BAYER4[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };

    for (int y = 0; y < SIM_CANVAS_HEIGHT; ++y) {
        float gy = (SIM_CANVAS_HEIGHT <= 1) ? 0.0f : (float)y / (float)(SIM_CANVAS_HEIGHT - 1);
        float shade = gy; // vertical gradient

        // Edge vignette in palette space (darker near borders)
        float edge = 0.0f;
        int m = (y < 24) ? (24 - y) : (y > (SIM_CANVAS_HEIGHT - 25) ? (y - (SIM_CANVAS_HEIGHT - 25)) : 0);
        if (m > 0) edge = (float)m / 24.0f * 0.12f;

        for (int x = 0; x < SIM_CANVAS_WIDTH; ++x) {
            float gx = (SIM_CANVAS_WIDTH <= 1) ? 0.0f : (float)x / (float)(SIM_CANVAS_WIDTH - 1);
            float curve = 0.10f * (0.5f - gx) * (0.5f - gx); // slight horizontal curvature

            float v = shade + curve + edge;
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;

            float scaled = v * (float)(5); // 0..5 base index
            int idx = (int)scaled;
            float frac = scaled - (float)idx;

            // Ordered dithering between idx and idx+1
            float threshold = ((float)BAYER4[y & 3][x & 3] + 0.5f) / 16.0f;
            int finalIdx = idx + (frac > threshold ? 1 : 0);
            if (finalIdx < 0) finalIdx = 0; if (finalIdx > 5) finalIdx = 5;

            // Fake scanline vibe: darken odd rows slightly
            SDL_Color c = palette[finalIdx];
            if ((y & 1) == 1) {
                c.r = clamp_u8((int)c.r - 3);
                c.g = clamp_u8((int)c.g - 3);
                c.b = clamp_u8((int)c.b - 3);
            }

            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    // Pixel-art bevel around the display (chunkier than before)
    SDL_SetRenderDrawColor(renderer, 140, 140, 148, 255);
    SDL_Rect mid = {
        DISPLAY_RECT.x - 5,
        DISPLAY_RECT.y - 6,
        DISPLAY_RECT.w + 10,
        DISPLAY_RECT.h + 12
    };
    SDL_RenderDrawRect(renderer, &mid);
    SDL_SetRenderDrawColor(renderer, 244, 244, 248, 255);
    SDL_Rect inner = {
        DISPLAY_RECT.x - 1,
        DISPLAY_RECT.y - 1,
        DISPLAY_RECT.w + 2,
        DISPLAY_RECT.h + 2
    };
    SDL_RenderDrawRect(renderer, &inner);
}

static void renderDisplayBezel(void) {
    if (!renderer) {
        return;
    }

    SDL_Rect outer = {
        DISPLAY_RECT.x - 10,
        DISPLAY_RECT.y - 12,
        DISPLAY_RECT.w + 20,
        DISPLAY_RECT.h + 24
    };
    SDL_Rect mid = {
        DISPLAY_RECT.x - 5,
        DISPLAY_RECT.y - 6,
        DISPLAY_RECT.w + 10,
        DISPLAY_RECT.h + 12
    };
    SDL_Rect highlightInner = {
        DISPLAY_RECT.x - 1,
        DISPLAY_RECT.y - 1,
        DISPLAY_RECT.w + 2,
        DISPLAY_RECT.h + 2
    };

    SDL_Color outerColor = {180, 182, 188, 255};
    SDL_Color midColor = {214, 216, 222, 255};
    SDL_Color edgeDark = {140, 140, 148, 255};
    SDL_Color edgeLight = {244, 244, 248, 255};

    SDL_Color gradientTop = {228, 230, 235, 255};
    SDL_Color gradientBottom = {182, 186, 192, 255};

    for (int i = 0; i < outer.h; ++i) {
        float t = (float)i / (float)(outer.h - 1);
        SDL_Color rowColor = lerpColor(gradientTop, gradientBottom, t);
        SDL_SetRenderDrawColor(renderer, rowColor.r, rowColor.g, rowColor.b, rowColor.a);
        SDL_RenderDrawLine(renderer, outer.x, outer.y + i, outer.x + outer.w - 1, outer.y + i);
    }

    SDL_SetRenderDrawColor(renderer, midColor.r, midColor.g, midColor.b, midColor.a);
    SDL_RenderFillRect(renderer, &mid);

    SDL_SetRenderDrawColor(renderer, edgeDark.r, edgeDark.g, edgeDark.b, edgeDark.a);
    SDL_RenderDrawRect(renderer, &mid);

    SDL_SetRenderDrawColor(renderer, edgeLight.r, edgeLight.g, edgeLight.b, edgeLight.a);
    SDL_RenderDrawRect(renderer, &highlightInner);

}

static void drawCircleOutline(int cx, int cy, int radius, SDL_Color color) {
    if (!renderer) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    int x = radius;
    int y = 0;
    int decision = 1 - radius;

    while (x >= y) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);
        y++;
        if (decision <= 0) {
            decision += 2 * y + 1;
        } else {
            x--;
            decision += 2 * (y - x) + 1;
        }
    }
}

static void fillCircle(int cx, int cy, int radius, SDL_Color color) {
    if (!renderer) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    for (int dy = -radius; dy <= radius; ++dy) {
        int span = (int)sqrtf((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, cx - span, cy + dy, cx + span, cy + dy);
    }
}

static void fillRingSegment(int cx, int cy, int innerRadius, int outerRadius, float startDeg, float endDeg, SDL_Color color) {
    if (!renderer) {
        return;
    }

    float startRad = startDeg * (float)M_PI / 180.0f;
    float endRad = endDeg * (float)M_PI / 180.0f;
    if (endRad < startRad) {
        endRad += 2.0f * (float)M_PI;
    }

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = -outerRadius; y <= outerRadius; ++y) {
        for (int x = -outerRadius; x <= outerRadius; ++x) {
            float distSq = (float)(x * x + y * y);
            if (distSq > (float)(outerRadius * outerRadius) || distSq < (float)(innerRadius * innerRadius)) {
                continue;
            }
            float angle = atan2f(-(float)y, (float)x);
            if (angle < 0.0f) {
                angle += 2.0f * (float)M_PI;
            }
            float start = startRad;
            float end = endRad;
            if (start < 0.0f) {
                start += 2.0f * (float)M_PI;
                end += 2.0f * (float)M_PI;
            }
            if (angle < start) {
                angle += 2.0f * (float)M_PI;
            }
            if (angle >= start && angle <= end) {
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            }
        }
    }
}

static void drawWheelText(const char *text, int x, int y, SDL_Color color) {
    int penX = x;
    size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i) {
        const GlyphPattern *glyph = findGlyph(text[i]);
        if (!glyph) {
            penX += 4;
            continue;
        }
        drawGlyphClipped(glyph, penX, y, color, NULL);
        penX += 6;
    }
}

static int measureWheelText(const char *text) {
    int width = 0;
    size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i) {
        const GlyphPattern *glyph = findGlyph(text[i]);
        if (!glyph) {
            width += 4;
        } else {
            width += 6;
        }
    }
    return width;
}

static void renderWheelBase(void) {
    SDL_Color outerLight = {220, 220, 225, 255};
    SDL_Color outerDark = {180, 180, 185, 255};
    for (int r = SIM_WHEEL_OUTER_RADIUS; r >= SIM_WHEEL_INNER_RADIUS; --r) {
        float t = (float)(SIM_WHEEL_OUTER_RADIUS - r) / (float)(SIM_WHEEL_OUTER_RADIUS - SIM_WHEEL_INNER_RADIUS);
        SDL_Color color = {
            clamp_u8((int)(outerDark.r + (outerLight.r - outerDark.r) * t)),
            clamp_u8((int)(outerDark.g + (outerLight.g - outerDark.g) * t)),
            clamp_u8((int)(outerDark.b + (outerLight.b - outerDark.b) * t)),
            255
        };
        fillCircle(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y, r, color);
    }

    SDL_Color hubColor = {240, 240, 242, 255};
    fillCircle(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y, SIM_WHEEL_INNER_RADIUS, hubColor);

    SDL_Color outerOutline = {150, 150, 155, 255};
    SDL_Color innerOutline = {200, 200, 205, 255};
    drawCircleOutline(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y, SIM_WHEEL_OUTER_RADIUS, outerOutline);
    drawCircleOutline(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y, SIM_WHEEL_INNER_RADIUS, innerOutline);
}

static void renderWheelLabels(void) {
    SDL_Color textColor = {40, 40, 45, 255};
    const char *menuLabel = "MENU";
    const char *prevLabel = "<<";
    const char *nextLabel = ">>";
    const char *playLabel = "PLAY";

    int menuWidth = measureWheelText(menuLabel);
    int playWidth = measureWheelText(playLabel);
    int prevWidth = measureWheelText(prevLabel);
    int nextWidth = measureWheelText(nextLabel);

    drawWheelText(menuLabel,
                  SIM_WHEEL_CENTER_X - menuWidth / 2,
                  SIM_WHEEL_CENTER_Y - SIM_WHEEL_OUTER_RADIUS + 18,
                  textColor);

    drawWheelText(prevLabel,
                  SIM_WHEEL_CENTER_X - SIM_WHEEL_OUTER_RADIUS + 10,
                  SIM_WHEEL_CENTER_Y - 4,
                  textColor);

    drawWheelText(nextLabel,
                  SIM_WHEEL_CENTER_X + SIM_WHEEL_OUTER_RADIUS - nextWidth - 10,
                  SIM_WHEEL_CENTER_Y - 4,
                  textColor);

    drawWheelText(playLabel,
                  SIM_WHEEL_CENTER_X - playWidth / 2,
                  SIM_WHEEL_CENTER_Y + SIM_WHEEL_OUTER_RADIUS - 34,
                  textColor);
}

static void renderWheelHighlight(uint8_t activeButton) {
    if (activeButton == 0) {
        return;
    }

    SDL_Color highlight = {200, 200, 205, 220};
    switch (activeButton) {
        case BUTTON_MENU:
            fillRingSegment(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y,
                            SIM_WHEEL_INNER_RADIUS + 2, SIM_WHEEL_OUTER_RADIUS - 2,
                            45.0f, 135.0f, highlight);
            break;
        case BUTTON_NEXT:
            fillRingSegment(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y,
                            SIM_WHEEL_INNER_RADIUS + 2, SIM_WHEEL_OUTER_RADIUS - 2,
                            315.0f, 405.0f, highlight);
            break;
        case BUTTON_PLAY:
            fillRingSegment(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y,
                            SIM_WHEEL_INNER_RADIUS + 2, SIM_WHEEL_OUTER_RADIUS - 2,
                            225.0f, 315.0f, highlight);
            break;
        case BUTTON_PREV:
            fillRingSegment(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y,
                            SIM_WHEEL_INNER_RADIUS + 2, SIM_WHEEL_OUTER_RADIUS - 2,
                            135.0f, 225.0f, highlight);
            break;
        case BUTTON_CENTER: {
            SDL_Color centerHighlight = {210, 210, 220, 255};
            fillCircle(SIM_WHEEL_CENTER_X, SIM_WHEEL_CENTER_Y, SIM_WHEEL_INNER_RADIUS - 2, centerHighlight);
            break;
        }
        default:
            break;
    }
}

bool Display_Init(const char *title) {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
            fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return false;
        }
    }

    if (!title) {
        title = "NUNO Player";
    }

    const int scale = SIM_WINDOW_SCALE;
    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              SIM_CANVAS_WIDTH * scale,
                              SIM_CANVAS_HEIGHT * scale,
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

    SDL_RenderSetLogicalSize(renderer, SIM_CANVAS_WIDTH, SIM_CANVAS_HEIGHT);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

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
    beginDisplayDraw();
    SDL_SetRenderDrawColor(renderer, 242, 242, 245, 255);
    SDL_Rect fill = {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
    SDL_RenderFillRect(renderer, &fill);
    endDisplayDraw();
}

void Display_Update(void) {
    // Rendering pipeline now handled by simulator entry point.
}

void Display_DrawText(const char* text, int x, int y, uint8_t color) {
    if (!renderer || !text) {
        return;
    }
    beginDisplayDraw();
    SDL_Color textColor = (color == 0)
        ? (SDL_Color){240, 240, 245, 255}
        : (SDL_Color){25, 25, 25, 255};
    const GlyphPattern *glyph;
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
        glyph = findGlyph(c);
        if (!glyph) {
            penX += 4;
            continue;
        }
        SDL_Rect clip = {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
        drawGlyphClipped(glyph, penX, y, textColor, &clip);
        penX += 6;
    }
    endDisplayDraw();
}

void Display_DrawRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) {
        return;
    }
    beginDisplayDraw();
    SDL_Rect r = { x, y, width, height };
    SDL_Color c = (color == 0)
        ? (SDL_Color){240, 240, 245, 255}
        : (SDL_Color){30, 30, 35, 255};
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(renderer, &r);
    endDisplayDraw();
}

void Display_FillRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) {
        return;
    }
    beginDisplayDraw();
    SDL_Rect r = { x, y, width, height };
    SDL_Color c = (color == 0)
        ? (SDL_Color){240, 240, 245, 255}
        : (SDL_Color){35, 35, 40, 255};
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
    endDisplayDraw();
}

void Display_RenderBackground(void) {
    renderBrushedBackground();
    renderDisplayBezel();
}

void Display_RenderClickWheel(uint8_t activeButton) {
    renderWheelBase();
    renderWheelHighlight(activeButton);
    renderWheelLabels();
}

void Display_Present(void) {
    if (renderer) {
        SDL_RenderPresent(renderer);
    }
}
