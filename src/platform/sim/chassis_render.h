#ifndef NUNO_SIM_CHASSIS_RENDER_H
#define NUNO_SIM_CHASSIS_RENDER_H

/*
 * High-fidelity procedural chassis renderer (header-only).
 *
 * The default sim drew the iPod body, screen bezel and click wheel directly with
 * hard-pixel SDL primitives, which left jaggy circles and flat gradients. This
 * module instead rasterises the whole *chassis* layer (body, bezel frame, wheel)
 * into a CPU-side ARGB buffer using analytic, distance-field anti-aliasing, then
 * uploads it once as an SDL texture for the renderer to blit. Every silhouette
 * edge — rounded body corners, the wheel ring, the centre button — is alpha
 * blended against its background, so curves read smooth at any window scale.
 *
 * The pixel-font screen UI is deliberately NOT drawn here: it stays on the
 * native nearest-neighbour path so the 5x7 bitmap glyphs remain crisp.
 *
 * Why a software buffer instead of an SDL render-target texture: it sidesteps
 * the active SDL_RenderSetLogicalSize() mapping entirely (no target/viewport
 * juggling), gives exact control over coverage and shading, and is plenty fast
 * for a once-per-frame faceplate. Everything is static/inline so this compiles
 * into sdl_mock_display.c with no new translation unit.
 */

#include "nuno/device_profile.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Float colour helpers                                               */
/* ------------------------------------------------------------------ */

typedef struct { float r, g, b, a; } CRColor; /* 0..1 premultiplied-friendly */

static inline CRColor cr_rgb(float r, float g, float b) {
    CRColor c = { r, g, b, 1.0f };
    return c;
}
static inline CRColor cr_from_nuno(NunoColor c) {
    CRColor o = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
                  (c.a ? c.a : 255) / 255.0f };
    return o;
}
static inline float cr_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
static inline CRColor cr_lerp(CRColor a, CRColor b, float t) {
    t = cr_clamp01(t);
    CRColor o = { a.r + (b.r - a.r) * t,
                  a.g + (b.g - a.g) * t,
                  a.b + (b.b - a.b) * t,
                  a.a + (b.a - a.a) * t };
    return o;
}
static inline CRColor cr_scale(CRColor c, float s) {
    CRColor o = { cr_clamp01(c.r * s), cr_clamp01(c.g * s),
                  cr_clamp01(c.b * s), c.a };
    return o;
}
static inline CRColor cr_add(CRColor c, float d) {
    CRColor o = { cr_clamp01(c.r + d), cr_clamp01(c.g + d),
                  cr_clamp01(c.b + d), c.a };
    return o;
}

/* ------------------------------------------------------------------ */
/* Software ARGB canvas                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t *px;   /* ARGB8888, row-major */
    int       w;
    int       h;
} CRCanvas;

static inline uint32_t cr_pack(CRColor c) {
    uint8_t a = (uint8_t)(cr_clamp01(c.a) * 255.0f + 0.5f);
    uint8_t r = (uint8_t)(cr_clamp01(c.r) * 255.0f + 0.5f);
    uint8_t g = (uint8_t)(cr_clamp01(c.g) * 255.0f + 0.5f);
    uint8_t b = (uint8_t)(cr_clamp01(c.b) * 255.0f + 0.5f);
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) |
           ((uint32_t)g << 8)  | (uint32_t)b;
}
static inline CRColor cr_unpack(uint32_t v) {
    CRColor c = { ((v >> 16) & 0xFF) / 255.0f,
                  ((v >> 8)  & 0xFF) / 255.0f,
                  (v & 0xFF) / 255.0f,
                  ((v >> 24) & 0xFF) / 255.0f };
    return c;
}

/* Source-over blend of `src` (with coverage) over the existing pixel. */
static inline void cr_blend(CRCanvas *cv, int x, int y, CRColor src, float cov) {
    if (x < 0 || y < 0 || x >= cv->w || y >= cv->h) return;
    float a = cr_clamp01(src.a * cov);
    if (a <= 0.0f) return;
    uint32_t *p = &cv->px[y * cv->w + x];
    CRColor dst = cr_unpack(*p);
    float outA = a + dst.a * (1.0f - a);
    if (outA <= 0.0001f) { *p = 0; return; }
    CRColor o;
    o.r = (src.r * a + dst.r * dst.a * (1.0f - a)) / outA;
    o.g = (src.g * a + dst.g * dst.a * (1.0f - a)) / outA;
    o.b = (src.b * a + dst.b * dst.a * (1.0f - a)) / outA;
    o.a = outA;
    *p = cr_pack(o);
}

/* Opaque write (no read-back); used for full backdrop fills. */
static inline void cr_set(CRCanvas *cv, int x, int y, CRColor c) {
    if (x < 0 || y < 0 || x >= cv->w || y >= cv->h) return;
    cv->px[y * cv->w + x] = cr_pack(c);
}

/* ------------------------------------------------------------------ */
/* Signed-distance primitives (positive = inside)                     */
/* ------------------------------------------------------------------ */

/* Smoothstep-style coverage for a 1px-soft edge given signed distance d
 * (in pixels, positive inside the shape). */
static inline float cr_cov(float d) {
    /* Linear ramp across ~1px straddling the edge: clean and cheap. */
    float c = d + 0.5f;
    return cr_clamp01(c);
}

/* Distance from point to the boundary of an axis-aligned rounded rect,
 * positive inside. (cx,cy) centre, (hx,hy) half-extents, r corner radius. */
static inline float cr_sd_round_rect(float px, float py,
                                     float cx, float cy,
                                     float hx, float hy, float r) {
    float qx = fabsf(px - cx) - (hx - r);
    float qy = fabsf(py - cy) - (hy - r);
    float ax = qx > 0.0f ? qx : 0.0f;
    float ay = qy > 0.0f ? qy : 0.0f;
    float outside = sqrtf(ax * ax + ay * ay);
    float inside = fminf(fmaxf(qx, qy), 0.0f);
    /* signed distance to edge, negative outside -> flip so positive=inside */
    return -((outside + inside) - r);
}

/* ------------------------------------------------------------------ */
/* Filled rounded rect with a vertical gradient + optional shading fns */
/* ------------------------------------------------------------------ */

/* Per-pixel shader callback: given normalised body coords (0..1 across the
 * body rect) returns a multiplicative/additive tweak already folded into a
 * colour. Pass NULL to use the plain gradient. */
typedef CRColor (*CRShadeFn)(float nx, float ny, CRColor base, void *user);

typedef struct {
    float cx, cy, hx, hy, r;
    CRColor top, bottom;
    CRShadeFn shade;
    void *user;
} CRRoundRect;

static inline void cr_fill_round_rect(CRCanvas *cv, const CRRoundRect *rr) {
    int x0 = (int)floorf(rr->cx - rr->hx) - 1;
    int y0 = (int)floorf(rr->cy - rr->hy) - 1;
    int x1 = (int)ceilf(rr->cx + rr->hx) + 1;
    int y1 = (int)ceilf(rr->cy + rr->hy) + 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
    float topY = rr->cy - rr->hy;
    float spanY = (rr->hy * 2.0f) > 1.0f ? (rr->hy * 2.0f) : 1.0f;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float fx = (float)x + 0.5f;
            float fy = (float)y + 0.5f;
            float d = cr_sd_round_rect(fx, fy, rr->cx, rr->cy, rr->hx, rr->hy, rr->r);
            float cov = cr_cov(d);
            if (cov <= 0.0f) continue;
            float t = (fy - topY) / spanY;
            CRColor base = cr_lerp(rr->top, rr->bottom, t);
            if (rr->shade) {
                float nx = (fx - (rr->cx - rr->hx)) / (rr->hx * 2.0f);
                float ny = (fy - topY) / spanY;
                base = rr->shade(nx, ny, base, rr->user);
            }
            cr_blend(cv, x, y, base, cov);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Filled / outlined circle and annulus (AA)                          */
/* ------------------------------------------------------------------ */

static inline void cr_fill_circle(CRCanvas *cv, float cx, float cy, float radius,
                                  CRColor color) {
    int x0 = (int)floorf(cx - radius) - 1;
    int y0 = (int)floorf(cy - radius) - 1;
    int x1 = (int)ceilf(cx + radius) + 1;
    int y1 = (int)ceilf(cy + radius) + 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float dx = (float)x + 0.5f - cx;
            float dy = (float)y + 0.5f - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float cov = cr_cov(radius - dist);
            if (cov > 0.0f) cr_blend(cv, x, y, color, cov);
        }
    }
}

/* Soft-edged annulus band [inner,outer] filled with `color`. */
static inline void cr_fill_annulus(CRCanvas *cv, float cx, float cy,
                                   float inner, float outer, CRColor color) {
    int x0 = (int)floorf(cx - outer) - 1;
    int y0 = (int)floorf(cy - outer) - 1;
    int x1 = (int)ceilf(cx + outer) + 1;
    int y1 = (int)ceilf(cy + outer) + 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float dx = (float)x + 0.5f - cx;
            float dy = (float)y + 0.5f - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float covOut = cr_cov(outer - dist);
            float covIn  = cr_cov(dist - inner);
            float cov = covOut < covIn ? covOut : covIn;
            if (cov > 0.0f) cr_blend(cv, x, y, color, cov);
        }
    }
}

/* Soft circular outline of given line width, centred on `radius`. */
static inline void cr_stroke_circle(CRCanvas *cv, float cx, float cy,
                                    float radius, float width, CRColor color) {
    cr_fill_annulus(cv, cx, cy, radius - width * 0.5f, radius + width * 0.5f, color);
}

/* ------------------------------------------------------------------ */
/* Angular highlight wedge (soft glow over an arc of the wheel ring)  */
/* ------------------------------------------------------------------ */

/* Fill the ring band [inner,outer] but only where the pixel angle falls within
 * [startDeg,endDeg] (CCW from +x, y-up), with the alpha feathered toward the
 * arc ends and radial centre for a soft glow rather than a flat wedge. */
static inline void cr_fill_arc_glow(CRCanvas *cv, float cx, float cy,
                                    float inner, float outer,
                                    float startDeg, float endDeg, CRColor color) {
    float startRad = startDeg * (float)M_PI / 180.0f;
    float endRad = endDeg * (float)M_PI / 180.0f;
    if (endRad < startRad) endRad += 2.0f * (float)M_PI;
    float midRad = (startRad + endRad) * 0.5f;
    float halfSpan = (endRad - startRad) * 0.5f;
    float midR = (inner + outer) * 0.5f;
    float bandHalf = (outer - inner) * 0.5f;

    int x0 = (int)floorf(cx - outer) - 1;
    int y0 = (int)floorf(cy - outer) - 1;
    int x1 = (int)ceilf(cx + outer) + 1;
    int y1 = (int)ceilf(cy + outer) + 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float dx = (float)x + 0.5f - cx;
            float dy = (float)y + 0.5f - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > outer + 1.0f || dist < inner - 1.0f) continue;
            float ang = atan2f(-dy, dx);
            /* wrap angle into [start, start+2pi) */
            while (ang < startRad) ang += 2.0f * (float)M_PI;
            while (ang > startRad + 2.0f * (float)M_PI) ang -= 2.0f * (float)M_PI;
            float da = fabsf(ang - midRad);
            if (da > halfSpan) continue;
            /* radial coverage (soft band) */
            float covOut = cr_cov(outer - dist);
            float covIn  = cr_cov(dist - inner);
            float covBand = covOut < covIn ? covOut : covIn;
            /* feather toward arc ends and band edges -> soft glow */
            float angFall = 1.0f - (da / halfSpan);          /* 1 centre -> 0 ends */
            angFall = angFall * angFall;
            float radFall = 1.0f - fabsf(dist - midR) / (bandHalf + 0.001f);
            radFall = cr_clamp01(radFall);
            float glow = covBand * angFall * (0.45f + 0.55f * radFall);
            if (glow > 0.0f) cr_blend(cv, x, y, color, glow);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Drop shadow: soft offset shadow of a rounded rect onto the canvas  */
/* ------------------------------------------------------------------ */

static inline void cr_drop_shadow(CRCanvas *cv, float cx, float cy,
                                  float hx, float hy, float r,
                                  float blur, float offY, float strength) {
    int x0 = (int)floorf(cx - hx - blur) - 1;
    int y0 = (int)floorf(cy - hy - blur) - 1;
    int x1 = (int)ceilf(cx + hx + blur) + 1;
    int y1 = (int)ceilf(cy + hy + blur + offY) + 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
    CRColor shadow = { 0.0f, 0.0f, 0.0f, 1.0f };
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float fx = (float)x + 0.5f;
            float fy = (float)y + 0.5f - offY;
            float d = cr_sd_round_rect(fx, fy, cx, cy, hx, hy, r); /* +inside */
            /* d>0 inside body; we want a halo just outside (d in [-blur,0]) */
            float a;
            if (d >= 0.0f) {
                a = strength; /* under the body (will be covered, harmless) */
            } else {
                float t = 1.0f + d / blur; /* 1 at edge -> 0 at blur away */
                a = strength * cr_clamp01(t) * cr_clamp01(t);
            }
            if (a > 0.0f) cr_blend(cv, x, y, shadow, a);
        }
    }
}

#endif /* NUNO_SIM_CHASSIS_RENDER_H */
