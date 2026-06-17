#include "nuno/audio_task.h"
#include "nuno/audio_buffer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*
 * FreeRTOS audio producer task. See nuno/audio_task.h for the model. The work
 * (decode/fill) is portable and lives in AudioBuffer_Service(); this file only
 * provides the platform plumbing: a binary semaphore the DMA ISR gives, and a
 * task that drains it.
 */

#define AUDIO_PRODUCER_STACK_SIZE (configMINIMAL_STACK_SIZE * 4)
#define AUDIO_PRODUCER_PRIORITY   (tskIDLE_PRIORITY + 3)  /* above the audio kick + UI */

static TaskHandle_t s_task = NULL;
static SemaphoreHandle_t s_sem = NULL;

/*
 * Registered as the AudioBuffer producer wake. Runs in the DMA transfer-complete
 * ISR (reached via AudioBuffer_Done()). ISR-safe: give the semaphore and request
 * a context switch if a higher-priority task (this producer) was unblocked.
 */
static void audio_task_wake_from_isr(void) {
    if (!s_sem) {
        return;
    }
    BaseType_t higher_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_sem, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

static void audio_producer_task(void *parameters) {
    (void)parameters;
    for (;;) {
        /* Block until the ISR signals a freed buffer, then decode/refill it
         * off the interrupt. AudioBuffer_Service() is a no-op if nothing is
         * pending, so a spurious wake is harmless. */
        if (xSemaphoreTake(s_sem, portMAX_DELAY) == pdTRUE) {
            AudioBuffer_Service();
        }
    }
}

bool AudioTask_Start(void) {
    if (s_task != NULL) {
        return true;  /* already started */
    }

    s_sem = xSemaphoreCreateBinary();
    if (s_sem == NULL) {
        return false;
    }

    /* Register the ISR-safe wake before the task exists; the wake only gives the
     * semaphore, which is valid as soon as it is created. */
    AudioBuffer_SetProducerWake(audio_task_wake_from_isr);

    BaseType_t created = xTaskCreate(audio_producer_task,
                                     "AudioProd",
                                     AUDIO_PRODUCER_STACK_SIZE,
                                     NULL,
                                     AUDIO_PRODUCER_PRIORITY,
                                     &s_task);
    if (created != pdPASS) {
        AudioBuffer_SetProducerWake(NULL);
        vSemaphoreDelete(s_sem);
        s_sem = NULL;
        return false;
    }
    return true;
}
