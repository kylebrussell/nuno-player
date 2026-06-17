#ifndef NUNO_DEVICE_PROFILE_H
#define NUNO_DEVICE_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * DeviceProfile describes a single iPod generation: its screen, click wheel,
 * chassis and theme. The portable UI core (menu_renderer, ui_state) and the
 * simulator both read geometry/colors from the *active* profile at runtime
 * rather than from compile-time constants, so one playback/UI engine can drive
 * many different iPod skins.
 *
 * Everything here is plain data. Profiles are defined once in a registry
 * (see DeviceProfiles_* below) and selected at launch or swapped live.
 */

/* ------------------------------------------------------------------ */
/* Colour                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} NunoColor;

/*
 * The UI core never names raw colours. It draws using semantic roles which the
 * active profile's ThemePalette resolves to concrete RGBA. This is what lets a
 * monochrome iPod mini and a colour iPod 5G share the exact same renderer.
 *
 * The first two roles are pinned to 0/1 so legacy call sites that passed a bare
 * 0 (background) or 1 (foreground) keep working unchanged.
 */
typedef enum {
    COLOR_ROLE_BACKGROUND  = 0,  /* screen background fill            */
    COLOR_ROLE_FOREGROUND  = 1,  /* primary text / lines / icons      */
    COLOR_ROLE_SELECTED_BG = 2,  /* selection highlight fill          */
    COLOR_ROLE_SELECTED_FG = 3,  /* text drawn on top of a selection  */
    COLOR_ROLE_TITLE_BG    = 4,  /* title bar background              */
    COLOR_ROLE_TITLE_FG    = 5,  /* title bar text                    */
    COLOR_ROLE_ACCENT      = 6,  /* progress fill, battery, accents   */
    COLOR_ROLE_COUNT
} ColorRole;

/* How the panel reproduces the role palette. Drives nothing structural today
 * (the sim renders RGBA either way) but lets profiles declare intent and lets a
 * future hardware Display map roles onto a 1-bit / grayscale framebuffer. */
typedef enum {
    DISPLAY_COLOR_MONO_1BIT = 0, /* original 1G-4G / mini panels      */
    DISPLAY_COLOR_GRAY_2BIT,     /* 4-level grayscale                 */
    DISPLAY_COLOR_RGB            /* photo / 5G / nano / classic        */
} DisplayColorModel;

typedef struct {
    NunoColor colors[COLOR_ROLE_COUNT];
    /* Optional vertical gradient for the selection bar (colour iPods). When
     * selectionGradient is true the highlight fills from gradTop->gradBottom;
     * otherwise COLOR_ROLE_SELECTED_BG is used as a flat fill. */
    bool      selectionGradient;
    NunoColor selectionGradTop;
    NunoColor selectionGradBottom;
} ThemePalette;

/* ------------------------------------------------------------------ */
/* Screen + layout metrics                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    int               width;       /* logical pixels                   */
    int               height;
    DisplayColorModel colorModel;
    /* Top-left of the screen within the chassis canvas (sim only). */
    int               originX;
    int               originY;
} ScreenSpec;

/* Layout metrics consumed by the menu renderer. Scaled per screen so a 320x240
 * classic isn't drawn with tiny 7px text in one corner. */
typedef struct {
    int titleBarHeight;
    int itemHeight;
    int textHeight;   /* glyph cell height used for vertical centering */
    int textMargin;
    int fontScale;    /* integer glyph magnification (1, 2, ...)       */
} UiMetrics;

/* ------------------------------------------------------------------ */
/* Click wheel                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    WHEEL_SCROLL = 0,    /* 1G: mechanical rotating wheel, labels around ring */
    WHEEL_TOUCH,         /* 2G: touch wheel, labels around ring               */
    WHEEL_TOUCH_BUTTONS, /* 3G: touch wheel + separate row of 4 buttons       */
    WHEEL_CLICK          /* 4G+, mini, nano, classic: integrated Click Wheel  */
} WheelType;

typedef struct {
    WheelType type;
    int       centerX;      /* within chassis canvas */
    int       centerY;
    int       outerRadius;
    int       innerRadius;  /* center select button radius */
    /* Visual palette for the procedural wheel. */
    NunoColor ringLight;
    NunoColor ringDark;
    NunoColor hubColor;
    NunoColor labelColor;
    /* For WHEEL_TOUCH_BUTTONS: vertical center of the separate button row,
     * above the wheel. Ignored for other wheel types. */
    int       buttonRowY;
} WheelLayout;

/* ------------------------------------------------------------------ */
/* Chassis                                                            */
/* ------------------------------------------------------------------ */

/*
 * Surface finish of the chassis. The high-fidelity procedural renderer shades
 * each material differently: glossy white/black plastic gets a bright specular
 * sheen and crisp edge highlight; anodized aluminium gets a soft brushed sheen
 * and cooler edge falloff; matte black absorbs the sheen. Additive field — older
 * profiles that leave it zero get MATERIAL_PLASTIC_GLOSS, the previous look.
 */
typedef enum {
    MATERIAL_PLASTIC_GLOSS = 0, /* 1G-5G/photo white & nano black plastic   */
    MATERIAL_ALUMINIUM,         /* mini variants + classic anodized metal    */
    MATERIAL_PLASTIC_MATTE      /* softer, less specular plastic             */
} ChassisMaterial;

typedef struct {
    int       canvasWidth;   /* full faceplate size (sim window, pre-scale) */
    int       canvasHeight;
    int       windowScale;   /* integer upscale for the SDL window          */
    /* Procedural body fill: a vertical gradient between two silvers/colours. */
    NunoColor bodyTop;
    NunoColor bodyBottom;
    NunoColor bezelColor;    /* frame drawn around the screen               */
    /* --- High-fidelity procedural shading (additive; sensible defaults) --- */
    ChassisMaterial material;    /* surface finish; drives the sheen model   */
    int       cornerRadius;      /* body rounded-corner radius (canvas px).
                                  * <=0 => renderer derives ~10% of width.   */
    int       bodyInset;         /* gap from canvas edge to the body rect, so
                                  * the device floats over a drop shadow.
                                  * <=0 => renderer derives a small inset.    */
    NunoColor backdropTop;       /* window background behind the device      */
    NunoColor backdropBottom;
    /* Asset-ready override: when non-NULL the sim should blit this faceplate
     * PNG instead of drawing the procedural body (not yet implemented; the
     * field exists so profiles and the renderer are ready for it). */
    const char *faceplateImage;
} ChassisStyle;

/* ------------------------------------------------------------------ */
/* Main-menu features                                                 */
/* ------------------------------------------------------------------ */

/*
 * Which top-level main-menu entries a generation shipped with. Real iPods
 * differed: the mono 1G-4G/mini had no Photos and no video; iPod photo/nano/
 * classic added Photos; the 5G added Videos. The UI core consults these flags
 * (via Display_GetActiveProfile()) when building MENU_MAIN so one engine renders
 * each generation's authentic menu. Music, Settings and Now Playing are always
 * present and are not gated here.
 */
typedef struct {
    bool photos;   /* "Photos"  — colour iPods (photo, 5G, nano, classic) */
    bool videos;   /* "Videos"  — iPod 5G and later                       */
    bool extras;   /* "Extras"  — Clock/Contacts/Games etc.               */
    bool games;    /* "Games"   — standalone entry on later menus         */
} MenuFeatures;

/* ------------------------------------------------------------------ */
/* Profile                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    const char  *id;          /* stable slug, e.g. "ipod-5g"  */
    const char  *displayName; /* "iPod 5G (Video, 2005)"      */
    int          year;
    ScreenSpec   screen;
    UiMetrics    metrics;
    WheelLayout  wheel;
    ChassisStyle chassis;
    ThemePalette theme;
    MenuFeatures features;
} DeviceProfile;

/* ------------------------------------------------------------------ */
/* Registry                                                           */
/* ------------------------------------------------------------------ */

/* Number of profiles in the built-in lineup. */
size_t               DeviceProfiles_Count(void);
/* Profile by index [0, DeviceProfiles_Count()). NULL if out of range. */
const DeviceProfile *DeviceProfiles_Get(size_t index);
/* Profile by id slug, or NULL if not found. */
const DeviceProfile *DeviceProfiles_FindById(const char *id);
/* The profile used when none is requested (the iPod mini, NUNO's muse). */
const DeviceProfile *DeviceProfiles_Default(void);

#endif /* NUNO_DEVICE_PROFILE_H */
