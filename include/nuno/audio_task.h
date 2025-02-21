#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/**
 * @brief Create and start the audio processing task
 * 
 * @return true if task was created successfully, false otherwise
 */
bool AudioTask_Start(void);

/**
 * @brief Stop and cleanup the audio processing task
 */
void AudioTask_Stop(void);

/**
 * @brief Get task statistics
 * 
 * @param stats Pointer to TaskStatus_t structure to fill with stats
 */
void AudioTask_GetStats(TaskStatus_t *stats);

#endif // AUDIO_TASK_H 