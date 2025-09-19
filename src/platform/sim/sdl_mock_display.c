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

static const SDL_Rect DISPLAY_RECT = {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};

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

    for (int y = 0; y < SIM_CANVAS_HEIGHT; ++y) {
        float t = (float)y / (float)SIM_CANVAS_HEIGHT;
        int base = (int)(205 - 35 * t);
        int noise = (int)(12 * sinf((float)y * 0.35f));
        Uint8 shade = clamp_u8(base + noise);
        SDL_SetRenderDrawColor(renderer, shade, shade, shade + 6, 255);
        SDL_RenderDrawLine(renderer, 0, y, SIM_CANVAS_WIDTH, y);
    }
}

static void renderDisplayBezel(void) {
    if (!renderer) {
        return;
    }

    SDL_Rect outer = {
        DISPLAY_RECT.x - 8,
        DISPLAY_RECT.y - 8,
        DISPLAY_RECT.w + 16,
        DISPLAY_RECT.h + 16
    };
    SDL_Rect mid = {
        DISPLAY_RECT.x - 4,
        DISPLAY_RECT.y - 4,
        DISPLAY_RECT.w + 8,
        DISPLAY_RECT.h + 8
    };
    SDL_Rect highlight = {
        DISPLAY_RECT.x - 1,
        DISPLAY_RECT.y - 1,
        DISPLAY_RECT.w + 2,
        DISPLAY_RECT.h + 2
    };

    SDL_Color outerColor = {170, 170, 175, 255};
    SDL_Color midColor = {208, 208, 212, 255};
    SDL_Color edgeDark = {130, 130, 135, 255};
    SDL_Color edgeLight = {238, 238, 242, 255};

    SDL_SetRenderDrawColor(renderer, outerColor.r, outerColor.g, outerColor.b, outerColor.a);
    SDL_RenderFillRect(renderer, &outer);

    SDL_SetRenderDrawColor(renderer, midColor.r, midColor.g, midColor.b, midColor.a);
    SDL_RenderFillRect(renderer, &mid);

    SDL_SetRenderDrawColor(renderer, edgeDark.r, edgeDark.g, edgeDark.b, edgeDark.a);
    SDL_RenderDrawRect(renderer, &mid);

    SDL_SetRenderDrawColor(renderer, edgeLight.r, edgeLight.g, edgeLight.b, edgeLight.a);
    SDL_RenderDrawRect(renderer, &highlight);
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
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
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
