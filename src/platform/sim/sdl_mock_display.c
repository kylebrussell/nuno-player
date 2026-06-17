#include "nuno/display.h"
#include "nuno/device_profile.h"
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
static const DeviceProfile *g_profile = NULL;

/* ------------------------------------------------------------------ */
/* Active profile + geometry accessors                                */
/* ------------------------------------------------------------------ */

const DeviceProfile *Display_GetActiveProfile(void) {
    if (!g_profile) {
        g_profile = DeviceProfiles_Default();
    }
    return g_profile;
}

const UiMetrics *Display_GetMetrics(void) {
    return &Display_GetActiveProfile()->metrics;
}

int Display_GetWidth(void) {
    return Display_GetActiveProfile()->screen.width;
}

int Display_GetHeight(void) {
    return Display_GetActiveProfile()->screen.height;
}

static SDL_Rect screenRect(void) {
    const DeviceProfile *p = Display_GetActiveProfile();
    SDL_Rect r = { p->screen.originX, p->screen.originY,
                   p->screen.width, p->screen.height };
    return r;
}

/* ------------------------------------------------------------------ */
/* Colour role resolution                                             */
/* ------------------------------------------------------------------ */

static SDL_Color roleColor(uint8_t role) {
    const DeviceProfile *p = Display_GetActiveProfile();
    ColorRole r = (role < COLOR_ROLE_COUNT) ? (ColorRole)role : COLOR_ROLE_FOREGROUND;
    NunoColor c = p->theme.colors[r];
    SDL_Color out = { c.r, c.g, c.b, (Uint8)(c.a ? c.a : 255) };
    return out;
}

static inline Uint8 clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
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

static SDL_Color nunoToSDL(NunoColor c) {
    SDL_Color out = { c.r, c.g, c.b, (Uint8)(c.a ? c.a : 255) };
    return out;
}

/* ------------------------------------------------------------------ */
/* Bitmap font                                                        */
/* ------------------------------------------------------------------ */

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
    { '/', { "    X", "    X", "   X ", "  X  ", " X   ", "X    ", "X    " } },
    { '%', { "XX  X", "XX  X", "   X ", "  X  ", " X   ", "X  XX", "X  XX" } },
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

/* Advance per glyph in screen pixels at the given font scale. */
static int glyphAdvance(char c, int scale) {
    if (c == ' ' || findGlyph(c) == NULL) {
        return 4 * scale;
    }
    return 6 * scale;
}

int Display_MeasureText(const char *text) {
    if (!text) {
        return 0;
    }
    int scale = Display_GetMetrics()->fontScale;
    int width = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        width += glyphAdvance(text[i], scale);
    }
    return width;
}

/* ------------------------------------------------------------------ */
/* Low-level drawing helpers                                          */
/* ------------------------------------------------------------------ */

/* Draw a glyph in screen-viewport coordinates, magnified by `scale`. */
static void drawGlyphScaled(const GlyphPattern *glyph, int originX, int originY,
                            SDL_Color color, int scale, const SDL_Rect *clip) {
    if (!renderer || !glyph) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (glyph->rows[row][col] == ' ') {
                continue;
            }
            int x = originX + col * scale;
            int y = originY + row * scale;
            if (clip) {
                if (x < clip->x || x + scale > clip->x + clip->w ||
                    y < clip->y || y + scale > clip->y + clip->h) {
                    continue;
                }
            }
            SDL_Rect cell = { x, y, scale, scale };
            SDL_RenderFillRect(renderer, &cell);
        }
    }
}

static void drawCircleOutline(int cx, int cy, int radius, SDL_Color color) {
    if (!renderer) return;
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
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    for (int dy = -radius; dy <= radius; ++dy) {
        int span = (int)sqrtf((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, cx - span, cy + dy, cx + span, cy + dy);
    }
}

static void fillRingSegment(int cx, int cy, int innerRadius, int outerRadius,
                            float startDeg, float endDeg, SDL_Color color) {
    if (!renderer) return;
    float startRad = startDeg * (float)M_PI / 180.0f;
    float endRad = endDeg * (float)M_PI / 180.0f;
    if (endRad < startRad) {
        endRad += 2.0f * (float)M_PI;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = -outerRadius; y <= outerRadius; ++y) {
        for (int x = -outerRadius; x <= outerRadius; ++x) {
            float distSq = (float)(x * x + y * y);
            if (distSq > (float)(outerRadius * outerRadius) ||
                distSq < (float)(innerRadius * innerRadius)) {
                continue;
            }
            float angle = atan2f(-(float)y, (float)x);
            if (angle < 0.0f) angle += 2.0f * (float)M_PI;
            float start = startRad;
            float end = endRad;
            if (start < 0.0f) {
                start += 2.0f * (float)M_PI;
                end += 2.0f * (float)M_PI;
            }
            if (angle < start) angle += 2.0f * (float)M_PI;
            if (angle >= start && angle <= end) {
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            }
        }
    }
}

/* Chassis-space text (wheel labels): always 1x scale, drawn on the faceplate. */
static void drawChassisText(const char *text, int x, int y, SDL_Color color) {
    int penX = x;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        const GlyphPattern *glyph = findGlyph(text[i]);
        if (!glyph) {
            penX += 4;
            continue;
        }
        drawGlyphScaled(glyph, penX, y, color, 1, NULL);
        penX += 6;
    }
}

static int measureChassisText(const char *text) {
    int width = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        width += (findGlyph(text[i]) == NULL) ? 4 : 6;
    }
    return width;
}

/* ------------------------------------------------------------------ */
/* Screen drawing primitives (UI core target)                        */
/* ------------------------------------------------------------------ */

static void beginDisplayDraw(void) {
    if (!renderer) return;
    SDL_Rect rect = screenRect();
    SDL_RenderSetViewport(renderer, &rect);
    SDL_Rect clip = { 0, 0, rect.w, rect.h };
    SDL_RenderSetClipRect(renderer, &clip);
}

static void endDisplayDraw(void) {
    if (!renderer) return;
    SDL_RenderSetClipRect(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
}

void Display_Clear(void) {
    if (!renderer) return;
    beginDisplayDraw();
    SDL_Color bg = roleColor(COLOR_ROLE_BACKGROUND);
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_Rect fill = { 0, 0, Display_GetWidth(), Display_GetHeight() };
    SDL_RenderFillRect(renderer, &fill);
    endDisplayDraw();
}

void Display_Update(void) {
    // Compositing handled by the simulator entry point.
}

void Display_DrawText(const char *text, int x, int y, uint8_t color) {
    if (!renderer || !text) return;
    beginDisplayDraw();
    SDL_Color c = roleColor(color);
    int scale = Display_GetMetrics()->fontScale;
    SDL_Rect clip = { 0, 0, Display_GetWidth(), Display_GetHeight() };
    int penX = x;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        char ch = text[i];
        if (ch == ' ') {
            penX += 4 * scale;
            continue;
        }
        const GlyphPattern *glyph = findGlyph(ch);
        if (!glyph) {
            penX += 4 * scale;
            continue;
        }
        drawGlyphScaled(glyph, penX, y, c, scale, &clip);
        penX += 6 * scale;
    }
    endDisplayDraw();
}

void Display_DrawRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) return;
    beginDisplayDraw();
    SDL_Rect r = { x, y, width, height };
    SDL_Color c = roleColor(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(renderer, &r);
    endDisplayDraw();
}

void Display_FillRect(int x, int y, int width, int height, uint8_t color) {
    if (!renderer) return;
    beginDisplayDraw();
    SDL_Rect r = { x, y, width, height };
    SDL_Color c = roleColor(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
    endDisplayDraw();
}

/* Fill a rect with a top->bottom vertical gradient. Assumes the screen viewport
 * is already active (call inside begin/endDisplayDraw). The gradient is keyed to
 * the full [y, y+height) span so adjacent rows blend continuously. */
static void fillVerticalGradient(int x, int y, int width, int height,
                                 SDL_Color top, SDL_Color bottom) {
    if (!renderer || height <= 0 || width <= 0) return;
    for (int row = 0; row < height; ++row) {
        float t = (height <= 1) ? 0.0f : (float)row / (float)(height - 1);
        SDL_Color c = lerpColor(top, bottom, t);
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_Rect line = { x, y + row, width, 1 };
        SDL_RenderFillRect(renderer, &line);
    }
}

void Display_FillSelection(int x, int y, int width, int height) {
    if (!renderer) return;
    const DeviceProfile *p = Display_GetActiveProfile();
    beginDisplayDraw();
    if (p->theme.selectionGradient) {
        fillVerticalGradient(x, y, width, height,
                             nunoToSDL(p->theme.selectionGradTop),
                             nunoToSDL(p->theme.selectionGradBottom));
    } else {
        SDL_Rect r = { x, y, width, height };
        SDL_Color c = roleColor(COLOR_ROLE_SELECTED_BG);
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        SDL_RenderFillRect(renderer, &r);
    }
    endDisplayDraw();
}

void Display_FillTitleBar(int x, int y, int width, int height) {
    if (!renderer) return;
    const DeviceProfile *p = Display_GetActiveProfile();
    beginDisplayDraw();
    SDL_Color base = roleColor(COLOR_ROLE_TITLE_BG);
    if (p->theme.selectionGradient) {
        /* Lift the top edge into a soft highlight, settle to the base below. */
        SDL_Color top = {
            clamp_u8(base.r + 34), clamp_u8(base.g + 34), clamp_u8(base.b + 34), 255
        };
        fillVerticalGradient(x, y, width, height, top, base);
    } else {
        SDL_Rect r = { x, y, width, height };
        SDL_SetRenderDrawColor(renderer, base.r, base.g, base.b, base.a);
        SDL_RenderFillRect(renderer, &r);
    }
    endDisplayDraw();
}

/* ------------------------------------------------------------------ */
/* Chassis / faceplate (procedural, profile-driven)                   */
/* ------------------------------------------------------------------ */

/*
 * Optional bitmap faceplate. When the active profile sets
 * chassis.faceplateImage, the sim blits that image scaled to the canvas instead
 * of drawing the procedural body. The texture is loaded once and cached; the
 * cache is keyed on the path string so switching profiles (or pointing a profile
 * at a different file) reloads it. SDL_LoadBMP keeps this dependency-free; to
 * accept PNG/JPEG, vendor a single-header decoder here and produce the same
 * SDL_Surface before SDL_CreateTextureFromSurface.
 */
static SDL_Texture *g_faceplateTex = NULL;
static const char  *g_faceplateKey = NULL; /* path the cached texture was built from */

static void releaseFaceplate(void) {
    if (g_faceplateTex) {
        SDL_DestroyTexture(g_faceplateTex);
        g_faceplateTex = NULL;
    }
    g_faceplateKey = NULL;
}

/* Return a cached texture for `path`, loading + caching it on first use.
 * Returns NULL (and logs once) if the file cannot be loaded, so the caller can
 * fall back to the procedural body. */
static SDL_Texture *faceplateTexture(const char *path) {
    if (!renderer || !path) {
        return NULL;
    }
    if (g_faceplateTex && g_faceplateKey == path) {
        return g_faceplateTex;
    }
    releaseFaceplate();

    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        fprintf(stderr, "faceplate load failed (%s): %s\n", path, SDL_GetError());
        /* Cache the failure against this key so we don't re-attempt every frame. */
        g_faceplateKey = path;
        return NULL;
    }
    g_faceplateTex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    g_faceplateKey = path;
    if (!g_faceplateTex) {
        fprintf(stderr, "faceplate texture failed (%s): %s\n", path, SDL_GetError());
    }
    return g_faceplateTex;
}

static void renderBody(void) {
    if (!renderer) return;
    const DeviceProfile *p = Display_GetActiveProfile();
    int W = p->chassis.canvasWidth;
    int H = p->chassis.canvasHeight;

    /* Asset path: blit the faceplate scaled to fill the canvas, if one loads. */
    if (p->chassis.faceplateImage) {
        SDL_Texture *tex = faceplateTexture(p->chassis.faceplateImage);
        if (tex) {
            SDL_Rect dst = { 0, 0, W, H };
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            return;
        }
        /* Load failed — fall through to the procedural body. */
    }

    SDL_Color top = nunoToSDL(p->chassis.bodyTop);
    SDL_Color bottom = nunoToSDL(p->chassis.bodyBottom);

    /* Vertical body gradient with a faint ordered dither to avoid banding. */
    static const uint8_t BAYER4[4][4] = {
        {  0,  8,  2, 10 },
        { 12,  4, 14,  6 },
        {  3, 11,  1,  9 },
        { 15,  7, 13,  5 }
    };
    for (int y = 0; y < H; ++y) {
        float t = (H <= 1) ? 0.0f : (float)y / (float)(H - 1);
        SDL_Color base = lerpColor(top, bottom, t);
        for (int x = 0; x < W; ++x) {
            int dither = (int)BAYER4[y & 3][x & 3] - 8; /* -8..+7 */
            SDL_Color c = {
                clamp_u8(base.r + dither / 4),
                clamp_u8(base.g + dither / 4),
                clamp_u8(base.b + dither / 4),
                255
            };
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
}

static void renderBezel(void) {
    if (!renderer) return;
    const DeviceProfile *p = Display_GetActiveProfile();
    SDL_Rect s = screenRect();
    SDL_Color bezel = nunoToSDL(p->chassis.bezelColor);

    SDL_Rect outer = { s.x - 6, s.y - 6, s.w + 12, s.h + 12 };
    SDL_SetRenderDrawColor(renderer, bezel.r, bezel.g, bezel.b, 255);
    SDL_RenderFillRect(renderer, &outer);

    /* Dark inner frame + light highlight for a recessed-glass look. */
    SDL_Rect frame = { s.x - 2, s.y - 2, s.w + 4, s.h + 4 };
    SDL_SetRenderDrawColor(renderer, clamp_u8(bezel.r - 40), clamp_u8(bezel.g - 40), clamp_u8(bezel.b - 40), 255);
    SDL_RenderDrawRect(renderer, &frame);
    SDL_Rect hl = { s.x - 1, s.y - 1, s.w + 2, s.h + 2 };
    SDL_SetRenderDrawColor(renderer, clamp_u8(bezel.r + 60), clamp_u8(bezel.g + 60), clamp_u8(bezel.b + 60), 200);
    SDL_RenderDrawRect(renderer, &hl);
}

void Display_RenderBackground(void) {
    renderBody();
    renderBezel();
}

/* --- Click wheel --------------------------------------------------- */

static void renderWheelRing(void) {
    const WheelLayout *w = &Display_GetActiveProfile()->wheel;
    SDL_Color light = nunoToSDL(w->ringLight);
    SDL_Color dark = nunoToSDL(w->ringDark);
    for (int r = w->outerRadius; r >= w->innerRadius; --r) {
        float t = (w->outerRadius == w->innerRadius) ? 0.0f
                : (float)(w->outerRadius - r) / (float)(w->outerRadius - w->innerRadius);
        SDL_Color color = lerpColor(dark, light, t);
        fillCircle(w->centerX, w->centerY, r, color);
    }
    fillCircle(w->centerX, w->centerY, w->innerRadius, nunoToSDL(w->hubColor));

    SDL_Color outerOutline = { clamp_u8(dark.r - 24), clamp_u8(dark.g - 24), clamp_u8(dark.b - 24), 255 };
    SDL_Color hubOutline = { clamp_u8(dark.r - 14), clamp_u8(dark.g - 14), clamp_u8(dark.b - 14), 255 };
    drawCircleOutline(w->centerX, w->centerY, w->outerRadius, outerOutline);
    /* Delineate the center select button (visible even on white bodies). */
    drawCircleOutline(w->centerX, w->centerY, w->innerRadius, hubOutline);
}

static void renderRingLabels(void) {
    const WheelLayout *w = &Display_GetActiveProfile()->wheel;
    SDL_Color color = nunoToSDL(w->labelColor);
    int midR = (w->outerRadius + w->innerRadius) / 2;

    const char *menu = "MENU";
    const char *prev = "<<";
    const char *next = ">>";
    const char *play = "PLAY";

    drawChassisText(menu, w->centerX - measureChassisText(menu) / 2,
                    w->centerY - midR - 3, color);
    drawChassisText(play, w->centerX - measureChassisText(play) / 2,
                    w->centerY + midR - 4, color);
    drawChassisText(prev, w->centerX - midR - measureChassisText(prev) / 2,
                    w->centerY - 3, color);
    drawChassisText(next, w->centerX + midR - measureChassisText(next) / 2,
                    w->centerY - 3, color);
}

/* Separate horizontal button row for 3G-style touch wheels. */
static const char *kRowLabels[4] = { "<<", "MENU", "PLAY", ">>" };
static const uint8_t kRowButtons[4] = { BUTTON_PREV, BUTTON_MENU, BUTTON_PLAY, BUTTON_NEXT };

static SDL_Rect rowButtonRect(int index) {
    const DeviceProfile *p = Display_GetActiveProfile();
    int slotW = (p->wheel.outerRadius * 2) / 4;
    int left = p->wheel.centerX - p->wheel.outerRadius;
    SDL_Rect r = { left + index * slotW, p->wheel.buttonRowY - 9, slotW, 18 };
    return r;
}

static void renderButtonRow(uint8_t activeButton) {
    const WheelLayout *w = &Display_GetActiveProfile()->wheel;
    SDL_Color label = nunoToSDL(w->labelColor);
    SDL_Color highlight = { 200, 200, 205, 220 };
    for (int i = 0; i < 4; ++i) {
        SDL_Rect r = rowButtonRect(i);
        if (activeButton == kRowButtons[i]) {
            SDL_SetRenderDrawColor(renderer, highlight.r, highlight.g, highlight.b, highlight.a);
            SDL_RenderFillRect(renderer, &r);
        }
        int tw = measureChassisText(kRowLabels[i]);
        drawChassisText(kRowLabels[i], r.x + (r.w - tw) / 2, r.y + 6, label);
    }
}

static void renderWheelHighlight(uint8_t activeButton) {
    if (activeButton == 0) return;
    const WheelLayout *w = &Display_GetActiveProfile()->wheel;
    SDL_Color highlight = { 200, 200, 205, 220 };
    int inner = w->innerRadius + 2;
    int outer = w->outerRadius - 2;
    switch (activeButton) {
        case BUTTON_MENU:
            fillRingSegment(w->centerX, w->centerY, inner, outer, 45.0f, 135.0f, highlight);
            break;
        case BUTTON_NEXT:
            fillRingSegment(w->centerX, w->centerY, inner, outer, 315.0f, 405.0f, highlight);
            break;
        case BUTTON_PLAY:
            fillRingSegment(w->centerX, w->centerY, inner, outer, 225.0f, 315.0f, highlight);
            break;
        case BUTTON_PREV:
            fillRingSegment(w->centerX, w->centerY, inner, outer, 135.0f, 225.0f, highlight);
            break;
        case BUTTON_CENTER: {
            SDL_Color centerHighlight = { 210, 210, 220, 255 };
            fillCircle(w->centerX, w->centerY, w->innerRadius - 2, centerHighlight);
            break;
        }
        default:
            break;
    }
}

void Display_RenderClickWheel(uint8_t activeButton) {
    const WheelLayout *w = &Display_GetActiveProfile()->wheel;
    renderWheelRing();
    if (w->type == WHEEL_TOUCH_BUTTONS) {
        /* The ring is unlabeled; navigation labels live in the separate row. */
        if (activeButton == BUTTON_CENTER) {
            renderWheelHighlight(BUTTON_CENTER);
        }
        renderButtonRow(activeButton);
    } else {
        renderWheelHighlight(activeButton);
        renderRingLabels();
    }
}

void Display_Present(void) {
    if (renderer) {
        SDL_RenderPresent(renderer);
    }
}

bool Display_SaveScreenshot(const char *path) {
    if (!renderer || !path) {
        return false;
    }
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return false;
    }
    bool ok = (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                    surface->pixels, surface->pitch) == 0) &&
              (SDL_SaveBMP(surface, path) == 0);
    SDL_FreeSurface(surface);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static bool createWindow(const char *title) {
    const DeviceProfile *p = Display_GetActiveProfile();
    int scale = p->chassis.windowScale > 0 ? p->chassis.windowScale : 2;

    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              p->chassis.canvasWidth * scale,
                              p->chassis.canvasHeight * scale,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        window = NULL;
        return false;
    }

    SDL_RenderSetLogicalSize(renderer, p->chassis.canvasWidth, p->chassis.canvasHeight);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    return true;
}

bool Display_Init(const char *title, const DeviceProfile *profile) {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
            fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return false;
        }
    }
    g_profile = profile ? profile : DeviceProfiles_Default();
    if (!title) {
        title = "NUNO Player";
    }
    return createWindow(title);
}

bool Display_SwitchProfile(const DeviceProfile *profile) {
    if (!profile) {
        return false;
    }
    char title[128];
    snprintf(title, sizeof(title), "NUNO Simulator — %s", profile->displayName);

    /* The faceplate texture belongs to the renderer being torn down. */
    releaseFaceplate();
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    g_profile = profile;
    return createWindow(title);
}

void Display_Shutdown(void) {
    releaseFaceplate();
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
