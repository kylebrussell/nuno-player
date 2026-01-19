#include "nuno/trackpad.h"

#include "nuno/board_config.h"
#include "nuno/input.h"
#include "nuno/platform.h"

#include <string.h>

#define TRACKPAD_REPORT_REG        0x0000u
#define TRACKPAD_REPORT_LEN        6u
#define TRACKPAD_STATUS_TOUCH_BIT  0x0001u

typedef struct {
    bool touch_active;
    uint16_t x;
    uint16_t y;
} TrackpadReport;

typedef struct {
    bool touch_active;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t last_x;
    uint16_t last_y;
    uint32_t touch_start_ms;
    int32_t scroll_accum;
    bool click_pressed;
} TrackpadState;

static TrackpadConfig g_config = {
    .max_x = 4095u,
    .max_y = 4095u,
    .tap_move_threshold = 64u,
    .tap_time_ms = 180u,
    .scroll_step = 48u,
    .zone_edge_ratio_percent = 25u
};

static TrackpadState g_state = {0};

static bool read_trackpad_report(TrackpadReport *report) {
    if (!report) {
        return false;
    }

    uint8_t address[2] = {
        (uint8_t)(TRACKPAD_REPORT_REG >> 8),
        (uint8_t)(TRACKPAD_REPORT_REG & 0xFFu)
    };
    uint8_t payload[TRACKPAD_REPORT_LEN] = {0};

    if (!platform_i2c_write(NUNO_TRACKPAD_I2C_ADDR, address, sizeof(address))) {
        return false;
    }
    if (!platform_i2c_read(NUNO_TRACKPAD_I2C_ADDR, payload, sizeof(payload))) {
        return false;
    }

    uint16_t status = (uint16_t)((payload[0] << 8) | payload[1]);
    report->touch_active = (status & TRACKPAD_STATUS_TOUCH_BIT) != 0u;
    report->x = (uint16_t)((payload[2] << 8) | payload[3]);
    report->y = (uint16_t)((payload[4] << 8) | payload[5]);
    return true;
}

static void emit_scroll_tick(int8_t delta, uint32_t timestamp_ms) {
    InputEvent event = {
        .type = INPUT_EVENT_SCROLL,
        .timestamp_ms = timestamp_ms
    };
    event.data.scroll.delta = delta;
    (void)Input_PushEvent(&event);
}

static void emit_tap_zone(InputTapZone zone, uint32_t timestamp_ms) {
    InputEvent event = {
        .type = INPUT_EVENT_TAP_ZONE,
        .timestamp_ms = timestamp_ms
    };
    event.data.tap.zone = zone;
    (void)Input_PushEvent(&event);
}

static void emit_click(bool pressed, uint32_t timestamp_ms) {
    InputEvent event = {
        .type = INPUT_EVENT_CLICK,
        .timestamp_ms = timestamp_ms
    };
    event.data.click.pressed = pressed;
    (void)Input_PushEvent(&event);
}

static InputTapZone classify_zone(uint16_t x, uint16_t y) {
    uint16_t top_edge = (uint16_t)((g_config.max_y * g_config.zone_edge_ratio_percent) / 100u);
    uint16_t bottom_edge = (uint16_t)(g_config.max_y - top_edge);
    uint16_t left_edge = (uint16_t)((g_config.max_x * g_config.zone_edge_ratio_percent) / 100u);
    uint16_t right_edge = (uint16_t)(g_config.max_x - left_edge);

    if (y <= top_edge) {
        return INPUT_TAP_ZONE_MENU;
    }
    if (y >= bottom_edge) {
        return INPUT_TAP_ZONE_PLAY;
    }
    if (x <= left_edge) {
        return INPUT_TAP_ZONE_PREV;
    }
    if (x >= right_edge) {
        return INPUT_TAP_ZONE_NEXT;
    }

    return INPUT_TAP_ZONE_MENU;
}

static void handle_touch_start(const TrackpadReport *report, uint32_t now_ms) {
    g_state.touch_active = true;
    g_state.start_x = report->x;
    g_state.start_y = report->y;
    g_state.last_x = report->x;
    g_state.last_y = report->y;
    g_state.touch_start_ms = now_ms;
    g_state.scroll_accum = 0;
}

static void handle_touch_move(const TrackpadReport *report, uint32_t now_ms) {
    int32_t dy = (int32_t)report->y - (int32_t)g_state.last_y;
    g_state.last_x = report->x;
    g_state.last_y = report->y;

    if (dy == 0) {
        return;
    }

    g_state.scroll_accum += dy;
    while (g_state.scroll_accum >= (int32_t)g_config.scroll_step) {
        emit_scroll_tick(1, now_ms);
        g_state.scroll_accum -= (int32_t)g_config.scroll_step;
    }
    while (g_state.scroll_accum <= -(int32_t)g_config.scroll_step) {
        emit_scroll_tick(-1, now_ms);
        g_state.scroll_accum += (int32_t)g_config.scroll_step;
    }
}

static void handle_touch_end(uint32_t now_ms) {
    uint32_t duration = now_ms - g_state.touch_start_ms;
    uint16_t dx = (g_state.last_x > g_state.start_x)
        ? (uint16_t)(g_state.last_x - g_state.start_x)
        : (uint16_t)(g_state.start_x - g_state.last_x);
    uint16_t dy = (g_state.last_y > g_state.start_y)
        ? (uint16_t)(g_state.last_y - g_state.start_y)
        : (uint16_t)(g_state.start_y - g_state.last_y);

    if (duration <= g_config.tap_time_ms &&
        dx <= g_config.tap_move_threshold &&
        dy <= g_config.tap_move_threshold) {
        InputTapZone zone = classify_zone(g_state.start_x, g_state.start_y);
        emit_tap_zone(zone, now_ms);
    }

    g_state.touch_active = false;
}

static void poll_click_switch(uint32_t now_ms) {
    bool pressed = (HAL_GPIO_ReadPin(NUNO_TRACKPAD_CLICK_PORT, NUNO_TRACKPAD_CLICK_PIN) == GPIO_PIN_RESET);
    if (pressed != g_state.click_pressed) {
        g_state.click_pressed = pressed;
        if (pressed) {
            emit_click(true, now_ms);
        }
    }
}

bool Trackpad_Init(void) {
    memset(&g_state, 0, sizeof(g_state));
    return true;
}

void Trackpad_SetConfig(const TrackpadConfig *config) {
    if (!config) {
        return;
    }
    g_config = *config;
}

void Trackpad_GetConfig(TrackpadConfig *config) {
    if (!config) {
        return;
    }
    *config = g_config;
}

void Trackpad_Poll(void) {
    TrackpadReport report = {0};
    uint32_t now_ms = platform_get_time_ms();

    poll_click_switch(now_ms);

    if (!read_trackpad_report(&report)) {
        if (g_state.touch_active) {
            handle_touch_end(now_ms);
        }
        return;
    }

    if (report.touch_active) {
        if (!g_state.touch_active) {
            handle_touch_start(&report, now_ms);
        } else {
            handle_touch_move(&report, now_ms);
        }
    } else if (g_state.touch_active) {
        handle_touch_end(now_ms);
    }
}
