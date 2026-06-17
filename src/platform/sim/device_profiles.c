#include "nuno/device_profile.h"

#include <string.h>

/*
 * Built-in iPod lineup. Each entry is described with a few high-level fields
 * (screen size, wheel type, body/theme palette); a shared layout pass computes
 * the chassis canvas size and wheel geometry so proportions stay consistent and
 * adding a new generation is a one-line append.
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
            c->bodyTop    = RGB(238, 239, 242);
            c->bodyBottom = RGB(206, 208, 214);
            c->bezelColor = RGB(150, 152, 160);
            w->ringLight  = RGB(238, 239, 242);
            w->ringDark   = RGB(198, 200, 207);
            w->hubColor   = RGB(246, 247, 250);
            w->labelColor = RGB(70, 72, 80);
            break;
        case BODY_SILVER:
            c->bodyTop    = RGB(222, 224, 229);
            c->bodyBottom = RGB(178, 181, 189);
            c->bezelColor = RGB(132, 134, 142);
            w->ringLight  = RGB(220, 222, 227);
            w->ringDark   = RGB(176, 179, 186);
            w->hubColor   = RGB(234, 236, 240);
            w->labelColor = RGB(54, 56, 64);
            break;
        case BODY_BLACK:
        default:
            c->bodyTop    = RGB(58, 60, 66);
            c->bodyBottom = RGB(28, 29, 34);
            c->bezelColor = RGB(12, 12, 16);
            w->ringLight  = RGB(86, 88, 96);
            w->ringDark   = RGB(44, 46, 52);
            w->hubColor   = RGB(96, 98, 106);
            w->labelColor = RGB(225, 226, 232);
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
    c->bodyTop    = shade(base, 26);
    c->bodyBottom = shade(base, -28);
    c->bezelColor = shade(base, -70);
    w->ringLight  = shade(base, 22);
    w->ringDark   = shade(base, -26);
    w->hubColor   = shade(base, 40);
    /* Keep wheel labels readable: dark on light tints, light on dark ones. */
    int luma = (3 * base.r + 6 * base.g + base.b) / 10;
    w->labelColor = (luma >= 140) ? (NunoColor){ 54, 56, 64, 255 }
                                  : (NunoColor){ 232, 233, 238, 255 };
}

/* ------------------------------------------------------------------ */
/* Layout pass: derive canvas + wheel geometry from the screen size    */
/* ------------------------------------------------------------------ */

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static DeviceProfile build(const char *id, const char *name, int year,
                           int screenW, int screenH, DisplayColorModel colorModel,
                           WheelType wheelType, ThemeKind themeKind, BodyKind body) {
    DeviceProfile p = {0};
    p.id = id;
    p.displayName = name;
    p.year = year;

    p.screen.width = screenW;
    p.screen.height = screenH;
    p.screen.colorModel = colorModel;

    /* Metrics: magnify the bitmap font on large panels so text stays legible. */
    int fontScale = (screenH >= 220) ? 2 : 1;
    p.metrics.fontScale = fontScale;
    p.metrics.titleBarHeight = 16 * fontScale;
    p.metrics.itemHeight     = 16 * fontScale;
    p.metrics.textHeight     = 12 * fontScale;
    p.metrics.textMargin     = 4 * fontScale;

    int marginX = clampi((screenW * 12) / 100, 16, 42);
    int topMargin = clampi((screenH * 14) / 100, 16, 42);
    p.screen.originX = marginX;
    p.screen.originY = topMargin;

    int canvasW = screenW + marginX * 2;

    /* A row of separate buttons (3G) sits between the screen and the wheel. */
    int buttonBand = (wheelType == WHEEL_TOUCH_BUTTONS) ? 26 : 0;
    int gap = clampi((screenH * 16) / 100, 14, 36) + buttonBand;

    int outerR = (canvasW * 42) / 100;
    int innerR = (outerR * 36) / 100;

    p.wheel.type = wheelType;
    p.wheel.centerX = canvasW / 2;
    p.wheel.centerY = topMargin + screenH + gap + outerR;
    p.wheel.outerRadius = outerR;
    p.wheel.innerRadius = innerR;
    p.wheel.buttonRowY = topMargin + screenH + buttonBand / 2 + 6;

    int bottomMargin = marginX;
    p.chassis.canvasWidth = canvasW;
    p.chassis.canvasHeight = p.wheel.centerY + outerR + bottomMargin;
    p.chassis.windowScale = (canvasW <= 240) ? 3 : 2;
    p.chassis.faceplateImage = NULL;

    p.theme = make_theme(themeKind);
    apply_body(&p.chassis, &p.wheel, body);

    /*
     * Main-menu features, grounded in the real lineup:
     *  - Photos: colour panels only (iPod photo, 5G, nano, classic).
     *  - Videos: the 5G introduced video; the classic kept it. The colour photo
     *    and nano 1G had RGB screens but no video, so gate on a tall RGB panel
     *    (>=240px high) which selects exactly the 5G and classic.
     *  - Extras: present across every generation.
     *  - Games: shipped as a sub-item of Extras on real iPods, so it is not a
     *    top-level entry by default (the flag exists for future profiles).
     */
    p.features.photos = (colorModel == DISPLAY_COLOR_RGB);
    p.features.videos = (colorModel == DISPLAY_COLOR_RGB) && (screenH >= 240);
    p.features.extras = true;
    p.features.games  = false;

    return p;
}

/* Build a colour-variant iPod mini: identical geometry/screen/theme to the
 * silver mini, only the anodized body + wheel are re-tinted. */
static DeviceProfile build_mini_variant(const char *id, const char *name,
                                        NunoColor tint) {
    DeviceProfile p = build(id, name, 2004, 138, 110, DISPLAY_COLOR_MONO_1BIT,
                            WHEEL_CLICK, THEME_MONO, BODY_SILVER);
    apply_tint(&p.chassis, &p.wheel, tint);
    return p;
}

/* ------------------------------------------------------------------ */
/* Registry                                                           */
/* ------------------------------------------------------------------ */

#define PROFILE_COUNT 11
static DeviceProfile g_profiles[PROFILE_COUNT];
static bool g_initialized = false;

static void ensure_init(void) {
    if (g_initialized) {
        return;
    }
    size_t i = 0;
    g_profiles[i++] = build("ipod-1g",      "iPod 1G (2001)",        2001, 160, 128, DISPLAY_COLOR_MONO_1BIT, WHEEL_SCROLL,        THEME_MONO,  BODY_WHITE);
    g_profiles[i++] = build("ipod-3g",      "iPod 3G (2003)",        2003, 160, 128, DISPLAY_COLOR_MONO_1BIT, WHEEL_TOUCH_BUTTONS, THEME_MONO,  BODY_WHITE);
    g_profiles[i++] = build("ipod-4g",      "iPod 4G (2004)",        2004, 160, 128, DISPLAY_COLOR_MONO_1BIT, WHEEL_CLICK,         THEME_MONO,  BODY_WHITE);
    g_profiles[i++] = build("ipod-mini",    "iPod mini (2004)",      2004, 138, 110, DISPLAY_COLOR_MONO_1BIT, WHEEL_CLICK,         THEME_MONO,  BODY_SILVER);
    g_profiles[i++] = build("ipod-photo",   "iPod photo (2004)",     2004, 220, 176, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_WHITE);
    g_profiles[i++] = build("ipod-5g",      "iPod 5G Video (2005)",  2005, 320, 240, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_WHITE);
    g_profiles[i++] = build("ipod-nano",    "iPod nano 1G (2005)",   2005, 176, 132, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_BLACK);
    g_profiles[i++] = build("ipod-classic", "iPod classic (2007)",   2007, 320, 240, DISPLAY_COLOR_RGB,       WHEEL_CLICK,         THEME_COLOR, BODY_SILVER);
    /* iPod mini anodized colour variants (same body, different dye). */
    g_profiles[i++] = build_mini_variant("ipod-mini-blue",  "iPod mini Blue (2004)",  RGB(150, 178, 196));
    g_profiles[i++] = build_mini_variant("ipod-mini-pink",  "iPod mini Pink (2004)",  RGB(214, 168, 178));
    g_profiles[i++] = build_mini_variant("ipod-mini-green", "iPod mini Green (2004)", RGB(176, 196, 150));
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
