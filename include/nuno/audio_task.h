#ifndef NUNO_AUDIO_TASK_H
#define NUNO_AUDIO_TASK_H

#include <stdbool.h>

/*
 * Hardware audio producer task (FreeRTOS).
 *
 * This is the embedded counterpart to the simulator's producer thread
 * (src/platform/sim/platform_sim.c). It implements the same single playback
 * model: the DMA transfer-complete ISR is the CONSUMER signal - it calls
 * AudioBuffer_Done(), which (because a producer wake is registered here) only
 * flips the active buffer and gives this task's semaphore from the ISR; it does
 * NOT decode. This task then wakes and calls AudioBuffer_Service() to decode and
 * refill the freed buffer off the ISR - no filesystem reads or decode in
 * interrupt context.
 *
 * Call AudioTask_Start() once during audio bring-up (DMA_Init does this). It
 * creates the semaphore + task and registers the ISR-safe producer wake.
 */
bool AudioTask_Start(void);

#endif /* NUNO_AUDIO_TASK_H */
