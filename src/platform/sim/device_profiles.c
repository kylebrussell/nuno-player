#include "nuno/device_profile.h"

#include <string.h>

/*
 * Built-in iPod lineup. Each entry is described by its REAL physical
 * dimensions (body W x H, screen W x H, click-wheel diameter — all in
 * millimetres) plus a few high-level fields (colour model, wheel type,
 * body/theme palette). A single layout pass scales every device through one
 * global PIXELS-PER-MM constant, so the chassis canvas, screen footprint and
 * wheel geometry all live in a consistent physical space: a nano comes out
 * genuinely narrower and smaller than a classic, the nano 3G is short & wide,
 * and the nano 4G/5G are tall & narrow with portrait screens — without any
 * renderer change, because the renderer reads all geometry from the profile.
 *
 * Because the screen footprint is derived from the panel's real size, the
 * pixel dimensions also serve as the UI's logical resolution; the menu
 * renderer is fully runtime-adaptive (Display_GetWidth/Height + metrics), so
 * each generation's screen occupies its authentic fraction of the face.
 */

#define RGB(r, g, b)  ((NunoColor){ (r), (g), (b), 255 })

/* ------------------------------------------------------------------ */
/* Theme + body palette presets                                       */
/* ------------------------------------------------------------------ */

typedef enum { THEME_MONO, THEME_COLOR } ThemeKind;
typedef enum { BODY_WHITE, BODY_SILVER, BODY_BLACK } BodyKind;

static ThemePalette make_theme(ThemeKind kind) {
    ThemePalette t = {0};
    if (kind == THEME_MONO) {
        /* Cool grey LCD: black text on a pale panel, inverted selection bar. */
        NunoColor bg = RGB(208, 214, 214);
        NunoColor fg = RGB(28, 30, 33);
        t.colors[COLOR_ROLE_BACKGROUND]  = bg;
        t.colors[COLOR_ROLE_FOREGROUND]  = fg;
        t.colors[COLOR_ROLE_SELECTED_BG] = fg;
        t.colors[COLOR_ROLE_SELECTED_FG] = bg;
        t.colors[COLOR_ROLE_TITLE_BG]    = bg;
        t.colors[COLOR_ROLE_TITLE_FG]    = fg;
        t.colors[COLOR_ROLE_ACCENT]      = fg;
        t.selectionGradient = false;
    } else {
        /* White background, black text, glossy blue selection + title bar. */
        t.colors[COLOR_ROLE_BACKGROUND]  = RGB(246, 247, 249);
        t.colors[COLOR_ROLE_FOREGROUND]  = RGB(22, 24, 28);
        t.colors[COLOR_ROLE_SELECTED_BG] = RGB(74, 124, 214);
        t.colors[COLOR_ROLE_SELECTED_FG] = RGB(255, 255, 255);
        t.colors[COLOR_ROLE_TITLE_BG]    = RGB(86, 112, 170);
        t.colors[COLOR_ROLE_TITLE_FG]    = RGB(255, 255, 255);
        t.colors[COLOR_ROLE_ACCENT]      = RGB(60, 110, 205);
        t.selectionGradient = true;
        t.selectionGradTop    = RGB(126, 170, 236);
        t.selectionGradBottom = RGB(52, 104, 198);
    }
    return t;
}

static void apply_body(ChassisStyle *c, WheelLayout *w, BodyKind body) {
    switch (body) {
        case BODY_WHITE:
            /* Glossy white polycarbonate (1G-5G, photo). */
            c->bodyTop    = RGB(248, 249, 251);
            c->bodyBottom = RGB(214, 217, 223);
            c->bezelColor = RGB(150, 152, 160);
            c->material   = MATERIAL_PLASTIC_GLOSS;
            w->ringLight  = RGB(245, 246, 249);
            w->ringDark   = RGB(206, 209, 216);
            w->hubColor   = RGB(250, 251, 253);
            w->labelColor = RGB(96, 99, 108);
            break;
        case BODY_SILVER:
            /* Anodized aluminium (mini base, classic). */
            c->bodyTop    = RGB(228, 230, 234);
            c->bodyBottom = RGB(182, 186, 193);
            c->bezelColor = RGB(120, 123, 131);
            c->material   = MATERIAL_ALUMINIUM;
            w->ringLight  = RGB(226, 228, 233);
            w->ringDark   = RGB(180, 184, 191);
            w->hubColor   = RGB(236, 238, 242);
            w->labelColor = RGB(70, 73, 82);
            break;
        case BODY_BLACK:
        default:
            /* Glossy black plastic (nano 1G). */
            c->bodyTop    = RGB(48, 50, 56);
            c->bodyBottom = RGB(20, 21, 26);
            c->bezelColor = RGB(10, 10, 14);
            c->material   = MATERIAL_PLASTIC_GLOSS;
            w->ringLight  = RGB(78, 80, 90);
            w->ringDark   = RGB(34, 36, 42);
            w->hubColor   = RGB(90, 92, 102);
            w->labelColor = RGB(214, 216, 224);
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Anodized-aluminium body tints (iPod mini colour variants)           */
/* ------------------------------------------------------------------ */

static uint8_t u8_clamp(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static NunoColor shade(NunoColor c, int delta) {
    return (NunoColor){ u8_clamp((int)c.r + delta),
                        u8_clamp((int)c.g + delta),
                        u8_clamp((int)c.b + delta), 255 };
}

/*
 * Recolour a chassis + wheel to an anodized-aluminium tint. The mini's five
 * finishes (silver/gold/pink/blue/green) shared one body; only the dye changed,
 * so we derive the top/bottom gradient, bezel and wheel ring from a single
 * mid-tone `base` colour. This makes adding a finish a one-line append.
 */
static void apply_tint(ChassisStyle *c, WheelLayout *w, NunoColor base) {
    c->bodyTop    = shade(base, 30);
    c->bodyBottom = shade(base, -30);
    c->bezelColor = shade(base, -74);
    c->material   = MATERIAL_ALUMINIUM;
    w->ringLight  = shade(base, 26);
    w->ringDark   = shade(base, -28);
    w->hubColor   = shade(base, 44);
    /* Keep wheel labels readable: dark on light tints, light on dark ones. */
    int luma = (3 * base.r + 6 * base.g + base.b) / 10;
    w->labelColor = (luma >= 140) ? (NunoColor){ 52, 54, 62, 255 }
                                  : (NunoColor){ 234, 235, 240, 255 };
}

/* ------------------------------------------------------------------ */
/* Layout pass: derive canvas + screen + wheel geometry from real mm    */
/* ------------------------------------------------------------------ */

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/*
 * Global PIXELS-PER-MM. Every device is laid out in this one physical space so
 * relative sizes come out true: a 40 mm nano body is genuinely narrower than a
 * 61.8 mm classic body, the same way they are in real life. Tuned so the
 * smallest panels (nano: ~1.5") still render legible bitmap text and the
 * largest body (classic) fits a comfortable window after windowScale.
 */
#define PPMM 4.0f

static int mm_to_px(float mm) {
    return (int)(mm * PPMM + 0.5f);
}

/*
 * Per-generation physical description. All distances are real-world
 * millimetres; the layout pass below converts them to pixels through PPMM.
 */
typedef struct {
    const char       *id;
    const char       *name;
    int               year;
    float             bodyW_mm;     /* faceplate width                       */
    float             bodyH_mm;     /* faceplate height                      */
    float             screenW_mm;   /* active panel width  (footprint)       */
    float             screenH_mm;   /* active panel height (footprint)       */
    float             wheelDiam_mm; /* click-wheel / scroll-wheel diameter   */
    DisplayColorModel colorModel;
    WheelType         wheelType;
    ThemeKind         themeKind;
    BodyKind          body;
    /* Authentic main-menu feature set for this generation. */
    MenuFeatures      features;
} DeviceSpec;

/*
 * Real click-wheel diameters are ~0.66-0.72x the body width on the full-size
 * iPods and the mini, but the nano family used a proportionally smaller wheel
 * relative to its narrow/short bodies. Diameters below are taken per family so
 * the wheel sits believably on each face; the layout pass turns the diameter
 * into outer/inner radii directly.
 */
static DeviceProfile build_spec(const DeviceSpec *d) {
    DeviceProfile p = {0};
    p.id = d->id;
    p.displayName = d->name;
    p.year = d->year;

    int canvasW = mm_to_px(d->bodyW_mm);
    int canvasH = mm_to_px(d->bodyH_mm);
    int screenW = mm_to_px(d->screenW_mm);
    int screenH = mm_to_px(d->screenH_mm);

    p.screen.width  = screenW;
    p.screen.height = screenH;
    p.screen.colorModel = d->colorModel;

    /* The screen sits centred horizontally, below a realistic top bezel. The
     * early iPods (and the mini) wore tall top bezels with the wheel filling
     * the lower half; the 5G/classic and nano panels grew to nearly fill the
     * upper face. The remaining vertical space below the screen houses the
     * wheel. */
    p.screen.originX = (canvasW - screenW) / 2;
    p.screen.originY = clampi((canvasH - screenH) / 4, 10, mm_to_px(13.0f));

    /* Metrics: the screen footprint is now the panel's real physical size (mm x
     * PPMM), which is small in pixels (~100-180px tall). The 5x7 bitmap font at
     * 1x already gives the authentic iPod look — small text, ~6-11 menu items.
     * Magnifying to 2x here made text comically large and clipped rows, so 1x is
     * the right call for the whole current lineup; reserve 2x for hypothetical
     * much larger panels. */
    int fontScale = (screenH >= 280) ? 2 : 1;
    p.metrics.fontScale = fontScale;
    p.metrics.titleBarHeight = 16 * fontScale;
    p.metrics.itemHeight     = 16 * fontScale;
    p.metrics.textHeight     = 12 * fontScale;
    p.metrics.textMargin     = 4 * fontScale;

    /* A row of separate buttons (3G) sits between the screen and the wheel. */
    int buttonBand = (d->wheelType == WHEEL_TOUCH_BUTTONS) ? mm_to_px(7.0f) : 0;

    int outerR = mm_to_px(d->wheelDiam_mm * 0.5f);
    int innerR = (int)(outerR * 0.34f + 0.5f);

    /* Centre the wheel within the lower face: the band between the bottom of
     * the screen (plus any 3G button row) and the bottom of the body. */
    int screenBottom = p.screen.originY + screenH + buttonBand;
    int lowerCenter = (screenBottom + canvasH) / 2;
    /* Keep the whole wheel inside the body with a small bottom margin. */
    int minCenter = screenBottom + outerR + mm_to_px(2.0f);
    int maxCenter = canvasH - outerR - mm_to_px(3.0f);
    int centerY = clampi(lowerCenter, minCenter, maxCenter);

    p.wheel.type = d->wheelType;
    p.wheel.centerX = canvasW / 2;
    p.wheel.centerY = centerY;
    p.wheel.outerRadius = outerR;
    p.wheel.innerRadius = innerR;
    p.wheel.buttonRowY = p.screen.originY + screenH + buttonBand / 2 + 4;

    p.chassis.canvasWidth  = canvasW;
    p.chassis.canvasHeight = canvasH;
    /* Larger bodies are upscaled less so the live window stays a comfortable
     * size; small nanos/minis get a bigger integer scale so they remain
     * usable. The relative-scale montage renders at a common PPMM instead. */
    p.chassis.windowScale = (canvasW <= 180) ? 4 : (canvasW <= 230 ? 3 : 2);
    p.chassis.faceplateImage = NULL;

    /*
     * High-fidelity body framing: the device floats inside the canvas over a
     * drop shadow, with rounded corners. Inset and radius scale with the body so
     * small minis/nanos and large 5G/classic bodies keep believable proportions.
     */
    p.chassis.bodyInset = clampi(canvasW / 30, 5, 12);
    p.chassis.cornerRadius = clampi((canvasW * 11) / 100, 12, 34);

    /* Neutral studio backdrop the device sits on. */
    p.chassis.backdropTop    = RGB(214, 216, 220);
    p.chassis.backdropBottom = RGB(176, 178, 184);

    p.theme = make_theme(d->themeKind);
    apply_body(&p.chassis, &p.wheel, d->body);

    p.features = d->features;
    return p;
}

/* Build a colour-variant iPod mini: identical geometry/screen/theme to the
 * silver mini, only the anodized body + wheel are re-tinted. */
static DeviceProfile build_mini_variant(const DeviceSpec *base, const char *id,
                                        const char *name, NunoColor tint) {
    DeviceSpec d = *base;
    d.id = id;
    d.name = name;
    DeviceProfile p = build_spec(&d);
    apply_tint(&p.chassis, &p.wheel, tint);
    return p;
}

/* ------------------------------------------------------------------ */
/* Registry                                                           */
/* ------------------------------------------------------------------ */

/*
 * Authentic main-menu feature sets, grounded in the real lineup:
 *  - Photos: colour panels (iPod photo and every colour iPod after it).
 *  - Videos: the 5G introduced video; the classic and the nano 3G/4G/5G kept
 *    it. The early colour panels (photo, nano 1G/2G) had RGB screens but no
 *    video.
 *  - Extras: present across every generation (Clock/Contacts/Games etc.).
 *  - Games: shipped as a sub-item of Extras on real iPods, so it is not a
 *    top-level entry here.
 */
#define FEAT_MONO   ((MenuFeatures){ false, false, true, false })
#define FEAT_PHOTOS ((MenuFeatures){ true,  false, true, false })
#define FEAT_VIDEO  ((MenuFeatures){ true,  true,  true, false })

/*
 * The full click/scroll-wheel iPod lineage, described in real millimetres.
 * Body W x H, screen W x H and wheel diameter are physical; the layout pass
 * scales them all through one global PPMM so proportions stay true to life.
 */
static const DeviceSpec kSpecs[] = {
    /* id              name                       year  bodyW  bodyH  scrW   scrH  wheel  colour                   wheel type           theme        body         features */
    /* --- Full-size white-plastic lineage (~same body) --- */
    { "ipod-1g",      "iPod 1G (2001)",          2001, 61.8f, 101.6f, 31.2f, 25.0f, 41.0f, DISPLAY_COLOR_MONO_1BIT, WHEEL_SCROLL,        THEME_MONO,  BODY_WHITE,  FEAT_MONO   },
    { "ipod-2g",      "iPod 2G (2002)",          2002, 61.8f, 101.6f, 31.2f, 25.0f, 41.0f, DISPLAY_COLOR_MONO_1BIT, WHEEL_TOUCH,         THEME_MONO,  BODY_WHITE,  FEAT_MONO   },
    { "ipod-3g",      "iPod 3G (2003)",          2003, 61.0f, 104.1f, 31.2f, 25.0f, 40.0f, DISPLAY_COLOR_MONO_1BIT, WHEEL_TOUCH_BUTTONS, THEME_MONO,  BODY_WHITE,  FEAT_MONO   },
    { "ipod-4g",      "iPod 4G (2004)",          2004, 61.0f, 104.1f, 31.2f, 25.0f, 41.0f, DISPLAY_COLOR_MONO_1BIT, WHEEL_CLICK,         THEME_MONO,  BODY_WHITE,  FEAT_MONO   },
    { "ipod-photo",   "iPod photo (2004)",       2004, 61.0f, 104.1f, 31.2f, 25.0f, 41.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_WHITE,  FEAT_PHOTOS },
    { "ipod-5g",      "iPod 5G Video (2005)",    2005, 61.8f, 103.5f, 50.8f, 38.1f, 41.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_WHITE,  FEAT_VIDEO  },
    { "ipod-classic", "iPod classic (2007)",     2007, 61.8f, 103.5f, 50.8f, 38.1f, 41.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_SILVER, FEAT_VIDEO  },
    /* --- iPod mini (anodized aluminium; smaller body) --- */
    { "ipod-mini",    "iPod mini (2004)",        2004, 51.0f, 91.4f,  26.1f, 20.9f, 33.0f, DISPLAY_COLOR_MONO_1BIT, WHEEL_CLICK,         THEME_MONO,  BODY_SILVER, FEAT_MONO   },
    /* --- iPod nano family --- */
    { "ipod-nano",    "iPod nano 1G (2005)",     2005, 40.0f, 90.0f,  30.5f, 22.9f, 26.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_BLACK,  FEAT_PHOTOS },
    { "ipod-nano2g",  "iPod nano 2G (2006)",     2006, 40.0f, 90.0f,  30.5f, 22.9f, 26.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_SILVER, FEAT_PHOTOS },
    { "ipod-nano3g",  "iPod nano 3G (2007)",     2007, 52.3f, 69.8f,  40.6f, 30.5f, 31.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_SILVER, FEAT_VIDEO  },
    { "ipod-nano4g",  "iPod nano 4G (2008)",     2008, 38.7f, 90.7f,  30.5f, 40.6f, 25.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_BLACK,  FEAT_VIDEO  },
    { "ipod-nano5g",  "iPod nano 5G (2009)",     2009, 38.5f, 90.7f,  28.0f, 44.0f, 25.0f, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_BLACK,  FEAT_VIDEO  },
};

#define SPEC_COUNT (sizeof(kSpecs) / sizeof(kSpecs[0]))

/* iPod mini anodized colour variants (same body, different dye). */
typedef struct { const char *id; const char *name; NunoColor tint; } MiniVariant;
static const MiniVariant kMiniVariants[] = {
    { "ipod-mini-blue",  "iPod mini Blue (2004)",  RGB(150, 178, 196) },
    { "ipod-mini-pink",  "iPod mini Pink (2004)",  RGB(214, 168, 178) },
    { "ipod-mini-green", "iPod mini Green (2004)", RGB(176, 196, 150) },
};

#define MINI_VARIANT_COUNT (sizeof(kMiniVariants) / sizeof(kMiniVariants[0]))

#define PROFILE_COUNT (SPEC_COUNT + MINI_VARIANT_COUNT)
static DeviceProfile g_profiles[PROFILE_COUNT];
static bool g_initialized = false;

/* Locate the silver iPod mini spec, used as the base for the colour variants. */
static const DeviceSpec *find_mini_spec(void) {
    for (size_t i = 0; i < SPEC_COUNT; ++i) {
        if (strcmp(kSpecs[i].id, "ipod-mini") == 0) {
            return &kSpecs[i];
        }
    }
    return &kSpecs[0];
}

static void ensure_init(void) {
    if (g_initialized) {
        return;
    }
    size_t i = 0;
    for (size_t s = 0; s < SPEC_COUNT; ++s) {
        g_profiles[i++] = build_spec(&kSpecs[s]);
    }
    const DeviceSpec *mini = find_mini_spec();
    for (size_t v = 0; v < MINI_VARIANT_COUNT; ++v) {
        g_profiles[i++] = build_mini_variant(mini, kMiniVariants[v].id,
                                             kMiniVariants[v].name,
                                             kMiniVariants[v].tint);
    }
    g_initialized = true;
}

size_t DeviceProfiles_Count(void) {
    return PROFILE_COUNT;
}

const DeviceProfile *DeviceProfiles_Get(size_t index) {
    if (index >= PROFILE_COUNT) {
        return NULL;
    }
    ensure_init();
    return &g_profiles[index];
}

const DeviceProfile *DeviceProfiles_FindById(const char *id) {
    if (!id) {
        return NULL;
    }
    ensure_init();
    for (size_t i = 0; i < PROFILE_COUNT; ++i) {
        if (strcmp(g_profiles[i].id, id) == 0) {
            return &g_profiles[i];
        }
    }
    return NULL;
}

const DeviceProfile *DeviceProfiles_Default(void) {
    return DeviceProfiles_FindById("ipod-mini");
}
