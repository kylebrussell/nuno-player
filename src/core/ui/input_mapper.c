#include "nuno/input_mapper.h"

#include "nuno/input.h"
#include "ui_tasks.h"

#define INPUT_EVENT_QUEUE_CAPACITY 32U

static struct {
    InputEvent events[INPUT_EVENT_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
} inputQueue = {0};

bool Input_PushEvent(const InputEvent *event) {
    if (!event) {
        return false;
    }

    uint8_t next_head = (uint8_t)((inputQueue.head + 1U) % INPUT_EVENT_QUEUE_CAPACITY);
    if (next_head == inputQueue.tail) {
        inputQueue.tail = (uint8_t)((inputQueue.tail + 1U) % INPUT_EVENT_QUEUE_CAPACITY);
    }

    inputQueue.events[inputQueue.head] = *event;
    inputQueue.head = next_head;
    return true;
}

bool Input_PopEvent(InputEvent *event) {
    if (!event || inputQueue.head == inputQueue.tail) {
        return false;
    }

    *event = inputQueue.events[inputQueue.tail];
    inputQueue.tail = (uint8_t)((inputQueue.tail + 1U) % INPUT_EVENT_QUEUE_CAPACITY);
    return true;
}

size_t Input_GetPendingCount(void) {
    if (inputQueue.head >= inputQueue.tail) {
        return (size_t)(inputQueue.head - inputQueue.tail);
    }
    return (size_t)(INPUT_EVENT_QUEUE_CAPACITY - (inputQueue.tail - inputQueue.head));
}

static uint8_t map_zone_to_button(InputTapZone zone) {
    switch (zone) {
        case INPUT_TAP_ZONE_MENU:
            return BUTTON_MENU;
        case INPUT_TAP_ZONE_PREV:
            return BUTTON_PREV;
        case INPUT_TAP_ZONE_NEXT:
            return BUTTON_NEXT;
        case INPUT_TAP_ZONE_PLAY:
            return BUTTON_PLAY;
        default:
            return 0U;
    }
}

void InputMapper_ProcessEvents(UIState *state, uint32_t current_time_ms) {
    if (!state) {
        return;
    }

    InputEvent event;
    while (Input_PopEvent(&event)) {
        switch (event.type) {
            case INPUT_EVENT_SCROLL:
                if (event.data.scroll.delta != 0) {
                    handleRotation(state, event.data.scroll.delta, event.timestamp_ms);
                }
                break;
            case INPUT_EVENT_TAP_ZONE: {
                uint8_t button = map_zone_to_button(event.data.tap.zone);
                if (button != 0U) {
                    handleButtonPress(state, button, event.timestamp_ms);
                }
                break;
            }
            case INPUT_EVENT_CLICK:
                if (event.data.click.pressed) {
                    handleButtonPress(state, BUTTON_CENTER, event.timestamp_ms);
                }
                break;
            case INPUT_EVENT_NONE:
            default:
                break;
        }
    }

    (void)current_time_ms;
}
