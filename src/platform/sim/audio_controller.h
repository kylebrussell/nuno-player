#ifndef NUNO_SIM_AUDIO_CONTROLLER_H
#define NUNO_SIM_AUDIO_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>

bool SimAudio_Init(void);
bool SimAudio_PlayTrack(void *context, size_t track_index);
void SimAudio_Shutdown(void);

#endif /* NUNO_SIM_AUDIO_CONTROLLER_H */
