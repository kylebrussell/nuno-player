#ifndef NUNO_INPUT_MAPPER_H
#define NUNO_INPUT_MAPPER_H

#include <stdint.h>

typedef struct UIState UIState;

void InputMapper_ProcessEvents(UIState *state, uint32_t current_time_ms);

#endif /* NUNO_INPUT_MAPPER_H */
