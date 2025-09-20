#ifndef NUNO_MUSIC_LIBRARY_H
#define NUNO_MUSIC_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>

#include "nuno/music_catalog.h"

#ifndef NUNO_DEFAULT_LIBRARY_PATH
#define NUNO_DEFAULT_LIBRARY_PATH "assets/music"
#endif

bool MusicLibrary_Init(const char *library_root);
const char *MusicLibrary_GetRoot(void);
size_t MusicLibrary_GetTrackCount(void);
const MusicLibraryTrack *MusicLibrary_GetTrack(size_t index);
const MusicLibraryTrack *MusicLibrary_GetCurrentTrack(void);
size_t MusicLibrary_GetCurrentIndex(void);
bool MusicLibrary_OpenTrack(size_t index);
bool MusicLibrary_OpenNextTrack(void);
bool MusicLibrary_HasNextTrack(void);
bool MusicLibrary_OpenPreviousTrack(void);
bool MusicLibrary_HasPreviousTrack(void);
size_t MusicLibrary_GetRemainingTracks(void);

#endif /* NUNO_MUSIC_LIBRARY_H */
