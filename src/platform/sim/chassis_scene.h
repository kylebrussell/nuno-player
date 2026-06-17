#ifndef NUNO_SIM_CHASSIS_SCENE_H
#define NUNO_SIM_CHASSIS_SCENE_H

/*
 * Scene composition for the high-fidelity chassis. Builds the procedural iPod
 * faceplate (backdrop, drop shadow, material-shaded rounded body, recessed
 * screen bezel) and the physical click wheel into a CRCanvas using the AA
 * primitives in chassis_render.h. Pure software, header-only, profile-driven.
 *
 * The renderer (sdl_mock_display.c) owns the SDL texture lifecycle; this file
 * only paints pixels into a buffer.
 */

#include "chassis_render.h"
#include "nuno/device_profile.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/* Material shading                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    ChassisMaterial material;
    float sheenStrength;   /* peak specular lift of the diagonal sheen */
    float edgeDarken;      /* vignette amount toward the body edges    */
} CRBodyShade;

/*
 * Body surface shader: a diagonal sheen sweeping from the top-left, a gentle
 * radial vignette darkening the edges, and a soft top highlight. Aluminium gets
 * a tighter, cooler brushed sheen; glossy plastic a broad bright one; matte
 * plastic almost none. nx,ny are 0..1 across the body rect.
 */
static inline CRColor cr_body_shader(float nx, float ny, CRColor base, void *user) {
    const CRBodyShade *s = (const CRBodyShade *)user;

    /* Diagonal sheen: bright band running top-left -> bottom-right. */
    float diag = (nx + ny) * 0.5f;            /* 0 at TL, 1 at BR  */
    float sheen = 0.0f;
    if (s->material == MATERIAL_ALUMINIUM) {
        /* Two soft brushed bands for an anodized look. */
        float b1 = expf(-((diag - 0.30f) * (diag - 0.30f)) / 0.010f);
        float b2 = 0.4f * expf(-((diag - 0.62f) * (diag - 0.62f)) / 0.030f);
        sheen = (b1 + b2) * s->sheenStrength;
    } else if (s->material == MATERIAL_PLASTIC_MATTE) {
        sheen = 0.35f * s->sheenStrength *
                expf(-((diag - 0.32f) * (diag - 0.32f)) / 0.060f);
    } else { /* glossy plastic */
        sheen = s->sheenStrength *
                expf(-((diag - 0.30f) * (diag - 0.30f)) / 0.045f);
    }

    /* Top highlight + bottom settle (already partly in the gradient). */
    float topHi = 0.10f * cr_clamp01(1.0f - ny * 2.2f);

    /* Radial vignette: darken toward the rim. */
    float vx = (nx - 0.5f) * 2.0f;
    float vy = (ny - 0.5f) * 2.0f;
    float rr = sqrtf(vx * vx + vy * vy);
    float vignette = s->edgeDarken * cr_clamp01((rr - 0.55f) / 0.45f);

    CRColor c = base;
    c = cr_add(c, sheen + topHi - vignette);
    return c;
}

/* ------------------------------------------------------------------ */
/* Body + screen bezel                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    int cornerRadius;
    int bodyInset;
    /* body rect (canvas px) */
    float bx, by, bw, bh;
} CRBodyRect;

/* Resolve the body rectangle + radius from the profile, applying sensible
 * defaults when the profile leaves the new fields zero. */
static inline CRBodyRect cr_body_rect(const DeviceProfile *p, int W, int H) {
    CRBodyRect r;
    int inset = p->chassis.bodyInset > 0 ? p->chassis.bodyInset
                                         : (W < 220 ? 8 : 12);
    int radius = p->chassis.cornerRadius > 0 ? p->chassis.cornerRadius
                                             : (int)(W * 0.085f);
    r.bodyInset = inset;
    r.cornerRadius = radius;
    r.bx = (float)inset;
    r.by = (float)inset;
    r.bw = (float)(W - inset * 2);
    r.bh = (float)(H - inset * 2);
    return r;
}

/* Paint the backdrop, drop shadow and the material-shaded body. */
static inline void cr_paint_body(CRCanvas *cv, const DeviceProfile *p) {
    int W = cv->w, H = cv->h;
    const ChassisStyle *ch = &p->chassis;

    /* --- Backdrop: soft vertical gradient (default neutral studio grey). --- */
    CRColor bgTop = (ch->backdropTop.a || ch->backdropTop.r ||
                     ch->backdropTop.g || ch->backdropTop.b)
                        ? cr_from_nuno(ch->backdropTop)
                        : cr_rgb(0.85f, 0.86f, 0.88f);
    CRColor bgBot = (ch->backdropBottom.a || ch->backdropBottom.r ||
                     ch->backdropBottom.g || ch->backdropBottom.b)
                        ? cr_from_nuno(ch->backdropBottom)
                        : cr_rgb(0.70f, 0.71f, 0.74f);
    for (int y = 0; y < H; ++y) {
        float t = (H <= 1) ? 0.0f : (float)y / (float)(H - 1);
        /* gentle ease so the gradient reads as soft studio lighting */
        float te = t * t * (3.0f - 2.0f * t);
        CRColor c = cr_lerp(bgTop, bgBot, te);
        for (int x = 0; x < W; ++x) cr_set(cv, x, y, c);
    }

    CRBodyRect br = cr_body_rect(p, W, H);
    float cx = br.bx + br.bw * 0.5f;
    float cy = br.by + br.bh * 0.5f;
    float hx = br.bw * 0.5f;
    float hy = br.bh * 0.5f;
    float rad = (float)br.cornerRadius;

    /* --- Drop shadow beneath the device. --- */
    float blur = (float)(br.bodyInset) * 1.6f + 6.0f;
    cr_drop_shadow(cv, cx, cy, hx, hy, rad, blur, blur * 0.45f, 0.30f);

    /* --- Body: material-shaded rounded rect. --- */
    CRBodyShade shade;
    shade.material = ch->material;
    switch (ch->material) {
        case MATERIAL_ALUMINIUM:     shade.sheenStrength = 0.16f; shade.edgeDarken = 0.16f; break;
        case MATERIAL_PLASTIC_MATTE: shade.sheenStrength = 0.10f; shade.edgeDarken = 0.10f; break;
        case MATERIAL_PLASTIC_GLOSS:
        default:                     shade.sheenStrength = 0.22f; shade.edgeDarken = 0.12f; break;
    }

    CRRoundRect rr;
    rr.cx = cx; rr.cy = cy; rr.hx = hx; rr.hy = hy; rr.r = rad;
    rr.top = cr_from_nuno(ch->bodyTop);
    rr.bottom = cr_from_nuno(ch->bodyBottom);
    rr.shade = cr_body_shader;
    rr.user = &shade;
    cr_fill_round_rect(cv, &rr);

    /* --- Crisp 1px rim: bright top-left highlight, dark bottom-right. --- */
    CRColor rimHi = cr_add(rr.top, 0.20f);
    CRColor rimLo = cr_scale(rr.bottom, 0.72f);
    /* top + left highlight arc, bottom + right shadow arc via two strokes */
    {
        /* approximate by stroking the full rounded edge then overlaying a
         * lighter top-left and darker bottom-right gradient strip */
        int x0 = (int)(cx - hx) - 1, x1 = (int)(cx + hx) + 1;
        int y0 = (int)(cy - hy) - 1, y1 = (int)(cy + hy) + 1;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
        if (x1 > W) x1 = W; if (y1 > H) y1 = H;
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
                float d = cr_sd_round_rect(fx, fy, cx, cy, hx, hy, rad);
                /* edge band: |d| within ~1.2px of the boundary */
                float edge = 1.2f - fabsf(d);
                if (edge <= 0.0f) continue;
                float cov = cr_clamp01(edge);
                /* light if the outward normal points up-left */
                float nlx = (fx - cx), nly = (fy - cy);
                float nl = -(nlx + nly); /* >0 => top-left */
                float w = cr_clamp01((nl / (hx + hy)) * 1.5f + 0.5f);
                CRColor rim = cr_lerp(rimLo, rimHi, w);
                cr_blend(cv, x, y, rim, cov * 0.55f);
            }
        }
    }
}

/*
 * Paint the recessed screen bezel + inset glass frame around the screen rect
 * (sx,sy,sw,sh in canvas px). The actual screen pixels are drawn later by the UI
 * on the native path; here we only paint the surrounding recess so the display
 * reads as a piece of glass sunk into the body:
 *   - an outer rounded bezel ring shaded darker than the body,
 *   - a hard inner well that is darker still (the recess wall),
 *   - a bright highlight along the top/left inner edge + shadow bottom/right.
 */
static inline void cr_paint_bezel(CRCanvas *cv, const DeviceProfile *p,
                                  int sx, int sy, int sw, int sh) {
    CRColor bezel = cr_from_nuno(p->chassis.bezelColor);
    float bw = 6.0f; /* bezel band width */

    /* Outer bezel: rounded frame slightly larger than the screen. */
    {
        float cx = sx + sw * 0.5f, cy = sy + sh * 0.5f;
        float hx = sw * 0.5f + bw, hy = sh * 0.5f + bw;
        CRRoundRect rr;
        rr.cx = cx; rr.cy = cy; rr.hx = hx; rr.hy = hy;
        rr.r = bw + 2.0f;
        rr.top = cr_add(bezel, 0.10f);
        rr.bottom = cr_scale(bezel, 0.82f);
        rr.shade = NULL; rr.user = NULL;
        cr_fill_round_rect(cv, &rr);
    }

    /* Recess wall: a darker square just inside the bezel, the "well". */
    {
        float cx = sx + sw * 0.5f, cy = sy + sh * 0.5f;
        float hx = sw * 0.5f + 2.0f, hy = sh * 0.5f + 2.0f;
        CRRoundRect rr;
        rr.cx = cx; rr.cy = cy; rr.hx = hx; rr.hy = hy; rr.r = 2.0f;
        CRColor wall = cr_scale(bezel, 0.55f);
        rr.top = wall; rr.bottom = wall;
        rr.shade = NULL; rr.user = NULL;
        cr_fill_round_rect(cv, &rr);
    }

    /* Inner edge lighting: bright top/left, dark bottom/right (inset glass). */
    {
        CRColor hi = cr_rgb(1.0f, 1.0f, 1.0f);
        CRColor lo = cr_rgb(0.0f, 0.0f, 0.0f);
        int x0 = sx - 3, y0 = sy - 3, x1 = sx + sw + 3, y1 = sy + sh + 3;
        for (int x = x0; x < x1; ++x) {
            cr_blend(cv, x, sy - 1, hi, 0.35f);   /* top highlight */
            cr_blend(cv, x, sy - 2, hi, 0.18f);
            cr_blend(cv, x, sy + sh, lo, 0.30f);  /* bottom shadow */
            cr_blend(cv, x, sy + sh + 1, lo, 0.15f);
        }
        for (int y = y0; y < y1; ++y) {
            cr_blend(cv, sx - 1, y, hi, 0.35f);    /* left highlight */
            cr_blend(cv, sx - 2, y, hi, 0.18f);
            cr_blend(cv, sx + sw, y, lo, 0.30f);   /* right shadow */
            cr_blend(cv, sx + sw + 1, y, lo, 0.15f);
        }
    }
}

/*
 * A very subtle diagonal glass glare over the screen rect (sx,sy,sw,sh in buffer
 * px). Drawn into the wheel/overlay layer so it composites *over* the UI without
 * being cleared by it. Kept low-alpha and confined to a top-left wedge so it
 * never washes out the pixel-font text underneath.
 */
static inline void cr_paint_screen_glare(CRCanvas *cv, int sx, int sy,
                                         int sw, int sh) {
    float x0 = (float)sx, y0 = (float)sy;
    float fw = (float)sw, fh = (float)sh;
    CRColor glare = cr_rgb(1.0f, 1.0f, 1.0f);
    for (int y = sy; y < sy + sh; ++y) {
        for (int x = sx; x < sx + sw; ++x) {
            float u = ((float)x - x0) / fw;  /* 0..1 left->right */
            float v = ((float)y - y0) / fh;  /* 0..1 top->bottom */
            /* diagonal band emanating from the top-left corner */
            float diag = 1.0f - (u + v);     /* 1 at TL -> -1 at BR */
            float band = cr_clamp01(diag);
            band = band * band;              /* concentrate toward corner */
            float a = 0.05f * band;          /* peak ~5% — barely there  */
            if (a > 0.003f) cr_blend(cv, x, y, glare, a);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Click wheel                                                        */
/* ------------------------------------------------------------------ */

/*
 * Physical-looking click wheel:
 *   - a faint recessed groove where the wheel meets the body,
 *   - the touch ring shaded with a soft concentric sheen (light top-left),
 *   - a slightly inset centre button with its own dome highlight,
 *   - subtle inner/outer edge lines so the ring reads as a raised surface.
 * Labels and the press highlight are drawn by the caller on top.
 */
static inline void cr_paint_wheel(CRCanvas *cv, const DeviceProfile *p) {
    const WheelLayout *w = &p->wheel;
    float cx = (float)w->centerX;
    float cy = (float)w->centerY;
    float outer = (float)w->outerRadius;
    float inner = (float)w->innerRadius;

    CRColor light = cr_from_nuno(w->ringLight);
    CRColor dark  = cr_from_nuno(w->ringDark);
    CRColor hub   = cr_from_nuno(w->hubColor);

    /* Recessed groove: a soft dark halo just outside the wheel, then a thin
     * bright lip, selling the wheel as set into the body. */
    cr_fill_annulus(cv, cx, cy, outer + 1.0f, outer + 4.5f,
                    cr_scale(dark, 0.70f));
    cr_stroke_circle(cv, cx, cy, outer + 2.2f, 1.4f, cr_scale(dark, 0.55f));
    cr_stroke_circle(cv, cx, cy, outer + 4.2f, 1.0f, cr_add(light, 0.10f));

    /* Touch ring base: radial gradient inner(light)->outer(dark), then a
     * diagonal sheen for a brushed/glossy surface. Painted per-pixel. */
    {
        int x0 = (int)(cx - outer) - 2, x1 = (int)(cx + outer) + 2;
        int y0 = (int)(cy - outer) - 2, y1 = (int)(cy + outer) + 2;
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
                if (cov <= 0.0f) continue;
                /* radial position 0(inner)..1(outer) */
                float rt = (dist - inner) / (outer - inner + 0.001f);
                rt = cr_clamp01(rt);
                CRColor base = cr_lerp(light, dark, rt * 0.85f);
                /* diagonal sheen: brighten where normal faces top-left */
                float diag = (-(dx + dy)) / (outer * 1.41f); /* -1..1 */
                float sheen = 0.10f * cr_clamp01(diag);
                /* very faint concentric ring texture (touch-surface feel) */
                float ring = 0.006f * sinf(dist * 0.6f);
                base = cr_add(base, sheen + ring);
                cr_blend(cv, x, y, base, cov);
            }
        }
    }

    /* Ring edge lines: dark outer rim, soft inner shadow at the hub boundary. */
    cr_stroke_circle(cv, cx, cy, outer - 0.6f, 1.2f, cr_scale(dark, 0.78f));
    cr_stroke_circle(cv, cx, cy, inner + 1.0f, 1.6f, cr_scale(dark, 0.85f));

    /* Centre button: inset disc with a dome highlight. A thin dark groove rings
     * it (the gap), then the hub fill, then a top-left specular. */
    cr_fill_circle(cv, cx, cy, inner + 0.5f, cr_scale(dark, 0.8f)); /* gap */
    cr_fill_circle(cv, cx, cy, inner - 1.5f, hub);
    /* dome: brighten upper-left, darken lower-right across the button */
    {
        int x0 = (int)(cx - inner) - 1, x1 = (int)(cx + inner) + 1;
        int y0 = (int)(cy - inner) - 1, y1 = (int)(cy + inner) + 1;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
        if (x1 > cv->w) x1 = cv->w; if (y1 > cv->h) y1 = cv->h;
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                float dx = (float)x + 0.5f - cx;
                float dy = (float)y + 0.5f - cy;
                float dist = sqrtf(dx * dx + dy * dy);
                float cov = cr_cov((inner - 1.5f) - dist);
                if (cov <= 0.0f) continue;
                float diag = (-(dx + dy)) / (inner * 1.41f + 0.001f);
                float shade = 0.10f * diag; /* + top-left, - bottom-right */
                /* soft rim shadow near the button edge */
                float rimT = cr_clamp01((dist - (inner - 5.0f)) / 5.0f);
                shade -= 0.06f * rimT;
                cr_blend(cv, x, y, cr_add(hub, shade), cov);
            }
        }
    }
    /* crisp specular dot toward the top-left of the button */
    cr_fill_circle(cv, cx - inner * 0.30f, cy - inner * 0.34f,
                   inner * 0.22f, cr_add(hub, 0.16f));
    /* fine outline around the button */
    cr_stroke_circle(cv, cx, cy, inner - 1.0f, 1.0f, cr_scale(dark, 0.88f));
}

#endif /* NUNO_SIM_CHASSIS_SCENE_H */
