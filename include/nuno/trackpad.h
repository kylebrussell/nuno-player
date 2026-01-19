#ifndef NUNO_TRACKPAD_H
#define NUNO_TRACKPAD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t max_x;
    uint16_t max_y;
    uint16_t tap_move_threshold;
    uint16_t tap_time_ms;
    uint16_t scroll_step;
    uint16_t zone_edge_ratio_percent;
} TrackpadConfig;

bool Trackpad_Init(void);
void Trackpad_Poll(void);

void Trackpad_SetConfig(const TrackpadConfig *config);
void Trackpad_GetConfig(TrackpadConfig *config);

#endif /* NUNO_TRACKPAD_H */
