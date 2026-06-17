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

/* High-fidelity procedural chassis (header-only software AA renderer). */
#include "chassis_render.h"
#include "chassis_scene.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static const DeviceProfile *g_profile = NULL;

/* ------------------------------------------------------------------ */
/* High-fidelity chassis layer                                        */
/* ------------------------------------------------------------------ */

/*
 * The chassis (body, bezel, wheel) is rasterised with anti-aliasing into a CPU
 * ARGB buffer at the *window* output resolution, then uploaded to a streaming
 * texture and blitted 1:1 over the whole window with the logical-size mapping
 * temporarily disabled. Building at output resolution (rather than the logical
 * canvas) means our soft AA edges land on real device pixels instead of being
 * nearest-upscaled into blocks. The pixel-font screen UI keeps drawing on the
 * normal logical/nearest path afterwards, so glyphs stay crisp.
 *
 * Two buffers/textures: the body+bezel layer (rebuilt only when the profile or
 * size changes) and the wheel layer (rebuilt when the pressed button changes).
 */
typedef struct {
    SDL_Texture *tex;
    uint32_t    *px;
    int          w, h;
} ChassisLayer;

static ChassisLayer g_bodyLayer  = {0};
static ChassisLayer g_wheelLayer = {0};
static const DeviceProfile *g_bodyBuiltFor  = NULL;
static const DeviceProfile *g_wheelBuiltFor = NULL;
static int g_wheelBuiltButton = -1;

static int outputScale(void) {
    const DeviceProfile *p = Display_GetActiveProfile();
    return p->chassis.windowScale > 0 ? p->chassis.windowScale : 2;
}

static void chassisLayerFree(ChassisLayer *layer) {
    if (layer->tex) { SDL_DestroyTexture(layer->tex); layer->tex = NULL; }
    if (layer->px)  { free(layer->px); layer->px = NULL; }
    layer->w = layer->h = 0;
}

/* Ensure `layer` has a CPU buffer + linear-filtered texture of size w*h. */
static bool chassisLayerEnsure(ChassisLayer *layer, int w, int h) {
    if (layer->w == w && layer->h == h && layer->tex && layer->px) {
        return true;
    }
    chassisLayerFree(layer);
    layer->px = (uint32_t *)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    if (!layer->px) return false;
    layer->tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!layer->tex) { free(layer->px); layer->px = NULL; return false; }
    SDL_SetTextureBlendMode(layer->tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(layer->tex, SDL_ScaleModeLinear);
    layer->w = w; layer->h = h;
    return true;
}

/* Upload the layer buffer and blit it 1:1 over the full window, bypassing the
 * logical-size mapping so the AA pixels map to real output pixels. */
static void chassisLayerBlit(ChassisLayer *layer) {
    if (!renderer || !layer->tex || !layer->px) return;
    SDL_UpdateTexture(layer->tex, NULL, layer->px,
                      layer->w * (int)sizeof(uint32_t));
    int lw = 0, lh = 0;
    SDL_RenderGetLogicalSize(renderer, &lw, &lh);
    SDL_RenderSetLogicalSize(renderer, 0, 0); /* draw in real output pixels */
    SDL_Rect dst = { 0, 0, layer->w, layer->h };
    SDL_RenderCopy(renderer, layer->tex, NULL, &dst);
    SDL_RenderSetLogicalSize(renderer, lw, lh); /* restore for screen UI */
}

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

static int measureChassisText(const char *text) {
    int width = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        width += (findGlyph(text[i]) == NULL) ? 4 : 6;
    }
    return width;
}

/* --- Glyph rendering into the supersampled chassis buffer (AA labels) --- */

/* Draw one glyph into a CRCanvas at (originX,originY) in buffer pixels, each
 * font cell magnified by `scale`, in colour `c` with alpha `alpha`. */
static void crDrawGlyph(CRCanvas *cv, const GlyphPattern *glyph,
                        int originX, int originY, int scale,
                        CRColor c, float alpha) {
    if (!glyph) return;
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (glyph->rows[row][col] == ' ') continue;
            for (int sy = 0; sy < scale; ++sy) {
                for (int sx = 0; sx < scale; ++sx) {
                    cr_blend(cv, originX + col * scale + sx,
                                 originY + row * scale + sy, c, alpha);
                }
            }
        }
    }
}

/* Draw a string into the buffer with an embossed look: a light (or dark) shadow
 * offset by 1*scale down-right, then the label colour on top. Returns advance. */
static void crDrawChassisTextEmboss(CRCanvas *cv, const char *text,
                                    int x, int y, int scale,
                                    CRColor label, CRColor emboss) {
    int penX = x;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        const GlyphPattern *glyph = findGlyph(text[i]);
        if (!glyph) { penX += 4 * scale; continue; }
        crDrawGlyph(cv, glyph, penX + scale, y + scale, scale, emboss, 0.55f);
        crDrawGlyph(cv, glyph, penX, y, scale, label, 1.0f);
        penX += 6 * scale;
    }
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

/* Build the body + recessed-bezel layer into the CPU buffer at output
 * resolution. The screen interior is left transparent-painted as the body fill;
 * the UI then clears and draws into the screen rect on the native path. */
static void buildBodyLayer(void) {
    const DeviceProfile *p = Display_GetActiveProfile();
    int ss = outputScale();
    int W = p->chassis.canvasWidth * ss;
    int H = p->chassis.canvasHeight * ss;
    if (!chassisLayerEnsure(&g_bodyLayer, W, H)) return;

    CRCanvas cv = { g_bodyLayer.px, W, H };
    /* Scale the profile geometry into the supersampled scene by temporarily
     * working in output pixels: cr_paint_* read p->chassis dimensions, so build
     * a scaled copy of the profile for the scene. */
    DeviceProfile sp = *p;
    sp.chassis.canvasWidth  = W;
    sp.chassis.canvasHeight = H;
    sp.chassis.cornerRadius = (p->chassis.cornerRadius > 0
                               ? p->chassis.cornerRadius
                               : (int)(p->chassis.canvasWidth * 0.085f)) * ss;
    sp.chassis.bodyInset = (p->chassis.bodyInset > 0
                            ? p->chassis.bodyInset
                            : (p->chassis.canvasWidth < 220 ? 8 : 12)) * ss;

    cr_paint_body(&cv, &sp);

    SDL_Rect s = screenRect();
    cr_paint_bezel(&cv, &sp, s.x * ss, s.y * ss, s.w * ss, s.h * ss);
}

void Display_RenderBackground(void) {
    if (!renderer) return;
    const DeviceProfile *p = Display_GetActiveProfile();

    /* Asset path: blit the faceplate scaled to fill the canvas, if one loads. */
    if (p->chassis.faceplateImage) {
        SDL_Texture *tex = faceplateTexture(p->chassis.faceplateImage);
        if (tex) {
            SDL_Rect dst = { 0, 0, p->chassis.canvasWidth, p->chassis.canvasHeight };
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            return;
        }
        /* Load failed — fall through to the procedural body. */
    }

    if (g_bodyBuiltFor != p || !g_bodyLayer.tex) {
        buildBodyLayer();
        g_bodyBuiltFor = p;
    }
    chassisLayerBlit(&g_bodyLayer);
}

/* --- Click wheel --------------------------------------------------- */

static const char *kRowLabels[4] = { "<<", "MENU", "PLAY", ">>" };
static const uint8_t kRowButtons[4] = { BUTTON_PREV, BUTTON_MENU, BUTTON_PLAY, BUTTON_NEXT };

static SDL_Rect rowButtonRect(int index) {
    const DeviceProfile *p = Display_GetActiveProfile();
    int slotW = (p->wheel.outerRadius * 2) / 4;
    int left = p->wheel.centerX - p->wheel.outerRadius;
    SDL_Rect r = { left + index * slotW, p->wheel.buttonRowY - 9, slotW, 18 };
    return r;
}

/* Soft press-glow over a quadrant of the touch ring (output-buffer space). */
static void crWheelPressGlow(CRCanvas *cv, const WheelLayout *w, int ss,
                             uint8_t activeButton) {
    if (activeButton == 0) return;
    float cx = (float)w->centerX * ss;
    float cy = (float)w->centerY * ss;
    float inner = (float)(w->innerRadius + 3) * ss;
    float outer = (float)(w->outerRadius - 2) * ss;
    CRColor glow = cr_rgb(1.0f, 1.0f, 1.0f);
    glow.a = 0.45f;
    switch (activeButton) {
        case BUTTON_MENU: cr_fill_arc_glow(cv, cx, cy, inner, outer, 45.0f, 135.0f, glow); break;
        case BUTTON_PREV: cr_fill_arc_glow(cv, cx, cy, inner, outer, 135.0f, 225.0f, glow); break;
        case BUTTON_PLAY: cr_fill_arc_glow(cv, cx, cy, inner, outer, 225.0f, 315.0f, glow); break;
        case BUTTON_NEXT: cr_fill_arc_glow(cv, cx, cy, inner, outer, 315.0f, 405.0f, glow); break;
        case BUTTON_CENTER: {
            CRColor c = cr_rgb(1.0f, 1.0f, 1.0f); c.a = 0.40f;
            cr_fill_circle(cv, cx, cy, (float)(w->innerRadius - 3) * ss, c);
            break;
        }
        default: break;
    }
}

/* Build the entire wheel layer (ring, hub, glow, labels) into the buffer. */
static void buildWheelLayer(uint8_t activeButton) {
    const DeviceProfile *p = Display_GetActiveProfile();
    const WheelLayout *w = &p->wheel;
    int ss = outputScale();
    int W = p->chassis.canvasWidth * ss;
    int H = p->chassis.canvasHeight * ss;
    if (!chassisLayerEnsure(&g_wheelLayer, W, H)) return;
    memset(g_wheelLayer.px, 0, (size_t)W * (size_t)H * sizeof(uint32_t));

    CRCanvas cv = { g_wheelLayer.px, W, H };

    /* Subtle glass glare over the screen (this layer blits over the UI). */
    SDL_Rect s = screenRect();
    cr_paint_screen_glare(&cv, s.x * ss, s.y * ss, s.w * ss, s.h * ss);

    DeviceProfile sp = *p;
    sp.wheel.centerX = w->centerX * ss;
    sp.wheel.centerY = w->centerY * ss;
    sp.wheel.outerRadius = w->outerRadius * ss;
    sp.wheel.innerRadius = w->innerRadius * ss;

    cr_paint_wheel(&cv, &sp);

    CRColor label = cr_from_nuno(w->labelColor);
    /* Emboss colour: lighten on dark labels, darken on light labels. */
    float lum = 0.3f * label.r + 0.6f * label.g + 0.1f * label.b;
    CRColor emboss = (lum < 0.5f) ? cr_rgb(1.0f, 1.0f, 1.0f) : cr_rgb(0.0f, 0.0f, 0.0f);

    if (w->type == WHEEL_TOUCH_BUTTONS) {
        if (activeButton == BUTTON_CENTER) {
            crWheelPressGlow(&cv, w, ss, BUTTON_CENTER);
        }
        /* Separate button row — the iPod 3G's signature: four touch buttons
         * with backlit-RED labels under the screen, pressed ones glowing. */
        CRColor redLabel  = cr_rgb(0.85f, 0.17f, 0.15f);
        CRColor redShadow = cr_rgb(0.42f, 0.05f, 0.05f);
        CRColor pill = cr_from_nuno(p->chassis.bodyBottom);
        for (int i = 0; i < 4; ++i) {
            SDL_Rect r = rowButtonRect(i);
            int bx = r.x * ss, by = r.y * ss, bw = r.w * ss, bh = r.h * ss;
            bool pressed = (activeButton == kRowButtons[i]);
            CRRoundRect rr;
            rr.cx = bx + bw * 0.5f; rr.cy = by + bh * 0.5f;
            rr.hx = bw * 0.5f - 2 * ss; rr.hy = bh * 0.5f;
            rr.r = bh * 0.5f;
            if (pressed) {
                /* soft red backlight wash behind a pressed button */
                rr.top = cr_rgb(0.97f, 0.85f, 0.83f);
                rr.bottom = cr_rgb(0.92f, 0.70f, 0.68f);
            } else {
                rr.top = cr_add(pill, 0.05f);
                rr.bottom = cr_add(pill, -0.05f);
            }
            rr.shade = NULL; rr.user = NULL;
            cr_fill_round_rect(&cv, &rr);
            int tw = measureChassisText(kRowLabels[i]) * ss;
            CRColor lc = pressed ? cr_rgb(0.74f, 0.10f, 0.09f) : redLabel;
            crDrawChassisTextEmboss(&cv, kRowLabels[i],
                                    bx + (bw - tw) / 2,
                                    by + (bh - 7 * ss) / 2, ss, lc, redShadow);
        }
    } else {
        crWheelPressGlow(&cv, w, ss, activeButton);
        int midR = (w->outerRadius + w->innerRadius) / 2;
        /* Scale labels up on large wheels so they don't read as tiny on the 5G/
         * classic; combine with the supersample factor for the buffer pen. */
        int ls = (w->outerRadius >= 150) ? 2 : 1;
        int gs = ss * ls;                 /* glyph scale in buffer pixels */
        int gh = 7 * gs;                  /* glyph cell height in buffer px */
        const char *menu = "MENU", *prev = "<<", *next = ">>", *play = "PLAY";
        crDrawChassisTextEmboss(&cv, menu,
            w->centerX * ss - measureChassisText(menu) * gs / 2,
            (w->centerY - midR) * ss - gh / 2, gs, label, emboss);
        crDrawChassisTextEmboss(&cv, play,
            w->centerX * ss - measureChassisText(play) * gs / 2,
            (w->centerY + midR) * ss - gh / 2, gs, label, emboss);
        crDrawChassisTextEmboss(&cv, prev,
            (w->centerX - midR) * ss - measureChassisText(prev) * gs / 2,
            w->centerY * ss - gh / 2, gs, label, emboss);
        crDrawChassisTextEmboss(&cv, next,
            (w->centerX + midR) * ss - measureChassisText(next) * gs / 2,
            w->centerY * ss - gh / 2, gs, label, emboss);
    }
}

void Display_RenderClickWheel(uint8_t activeButton) {
    if (!renderer) return;
    const DeviceProfile *p = Display_GetActiveProfile();
    if (g_wheelBuiltFor != p || g_wheelBuiltButton != (int)activeButton ||
        !g_wheelLayer.tex) {
        buildWheelLayer(activeButton);
        g_wheelBuiltFor = p;
        g_wheelBuiltButton = (int)activeButton;
    }
    chassisLayerBlit(&g_wheelLayer);
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

/* Free both chassis layers and reset their build trackers. Call whenever the
 * renderer (which owns the textures) is about to be destroyed. */
static void releaseChassisLayers(void) {
    chassisLayerFree(&g_bodyLayer);
    chassisLayerFree(&g_wheelLayer);
    g_bodyBuiltFor = NULL;
    g_wheelBuiltFor = NULL;
    g_wheelBuiltButton = -1;
}

bool Display_SwitchProfile(const DeviceProfile *profile) {
    if (!profile) {
        return false;
    }
    char title[128];
    snprintf(title, sizeof(title), "NUNO Simulator — %s", profile->displayName);

    /* The faceplate + chassis textures belong to the renderer being torn down. */
    releaseFaceplate();
    releaseChassisLayers();
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
    releaseChassisLayers();
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
