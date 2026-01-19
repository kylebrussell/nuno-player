#ifndef NUNO_INPUT_H
#define NUNO_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_SCROLL,
    INPUT_EVENT_TAP_ZONE,
    INPUT_EVENT_CLICK
} InputEventType;

typedef enum {
    INPUT_TAP_ZONE_MENU = 0,
    INPUT_TAP_ZONE_PREV,
    INPUT_TAP_ZONE_NEXT,
    INPUT_TAP_ZONE_PLAY
} InputTapZone;

typedef struct {
    InputEventType type;
    uint32_t timestamp_ms;
    union {
        struct {
            int8_t delta;
        } scroll;
        struct {
            InputTapZone zone;
        } tap;
        struct {
            bool pressed;
        } click;
    } data;
} InputEvent;

bool Input_PushEvent(const InputEvent *event);
bool Input_PopEvent(InputEvent *event);
size_t Input_GetPendingCount(void);

#endif /* NUNO_INPUT_H */
