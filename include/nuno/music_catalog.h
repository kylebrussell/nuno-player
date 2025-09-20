#ifndef NUNO_MUSIC_CATALOG_H
#define NUNO_MUSIC_CATALOG_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *title;
    const char *album;
    const char *artist;
    const char *filename;
    uint32_t duration_seconds;
} MusicLibraryTrack;

extern const MusicLibraryTrack g_music_library_tracks[];
extern const size_t g_music_library_track_count;

#endif /* NUNO_MUSIC_CATALOG_H */
