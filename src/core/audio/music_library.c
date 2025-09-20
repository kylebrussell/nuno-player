#include "nuno/music_library.h"

#include "nuno/filesystem.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(PATH_MAX)
#define PATH_MAX 512
#endif

typedef struct {
    char root[PATH_MAX];
    size_t current_index;
    bool initialised;
} MusicLibraryState;

static MusicLibraryState g_library = {
    .root = {0},
    .current_index = (size_t)-1,
    .initialised = false
};

static bool resolve_track_path(size_t index, char *buffer, size_t size) {
    if (!buffer || size == 0U || index >= g_music_library_track_count) {
        return false;
    }

    if (!g_library.initialised) {
        return false;
    }

    int written = snprintf(buffer, size, "%s/%s", g_library.root, g_music_library_tracks[index].filename);
    if (written <= 0) {
        return false;
    }

    return (size_t)written < size;
}

bool MusicLibrary_Init(const char *library_root) {
    const char *root = library_root ? library_root : NUNO_DEFAULT_LIBRARY_PATH;
    size_t length = strlen(root);

    if (length == 0U || length >= sizeof(g_library.root)) {
        return false;
    }

    memset(&g_library, 0, sizeof(g_library));
    strncpy(g_library.root, root, sizeof(g_library.root) - 1U);
    g_library.initialised = true;
    g_library.current_index = (size_t)-1;

    return true;
}

const char *MusicLibrary_GetRoot(void) {
    return g_library.initialised ? g_library.root : NULL;
}

size_t MusicLibrary_GetTrackCount(void) {
    return g_music_library_track_count;
}

const MusicLibraryTrack *MusicLibrary_GetTrack(size_t index) {
    if (!g_library.initialised || index >= g_music_library_track_count) {
        return NULL;
    }
    return &g_music_library_tracks[index];
}

const MusicLibraryTrack *MusicLibrary_GetCurrentTrack(void) {
    if (!g_library.initialised || g_library.current_index >= g_music_library_track_count) {
        return NULL;
    }
    return &g_music_library_tracks[g_library.current_index];
}

size_t MusicLibrary_GetCurrentIndex(void) {
    return g_library.current_index;
}

bool MusicLibrary_OpenTrack(size_t index) {
    printf("MusicLibrary_OpenTrack called with index %zu\n", index);
    if (!g_library.initialised || index >= g_music_library_track_count) {
        printf("Library not initialized or invalid index\n");
        return false;
    }

    printf("Library root: %s\n", g_library.root);
    printf("Track count: %zu\n", g_music_library_track_count);

    char path[PATH_MAX];
    if (!resolve_track_path(index, path, sizeof(path))) {
        printf("Failed to resolve track path for index %zu\n", index);
        return false;
    }

    printf("Resolved path: %s\n", path);

    printf("Opening file with filesystem...\n");
    if (!FileSystem_OpenFile(path)) {
        printf("FileSystem_OpenFile failed for path: %s\n", path);
        return false;
    }

    printf("File opened successfully\n");
    g_library.current_index = index;

    return true;
}

bool MusicLibrary_OpenNextTrack(void) {
    if (!g_library.initialised) {
        return false;
    }

    size_t next_index = (g_library.current_index == (size_t)-1) ? 0U : g_library.current_index + 1U;
    if (next_index >= g_music_library_track_count) {
        return false;
    }

    return MusicLibrary_OpenTrack(next_index);
}

bool MusicLibrary_OpenPreviousTrack(void) {
    if (!g_library.initialised) {
        return false;
    }

    if (g_library.current_index == (size_t)-1) {
        return false;
    }

    size_t prev_index = (g_library.current_index == 0U)
        ? (size_t)-1
        : (g_library.current_index - 1U);

    if (prev_index == (size_t)-1) {
        return false; // no previous track before 0
    }

    return MusicLibrary_OpenTrack(prev_index);
}

bool MusicLibrary_HasPreviousTrack(void) {
    if (!g_library.initialised || g_library.current_index == (size_t)-1) {
        return false;
    }
    return g_library.current_index > 0U;
}

bool MusicLibrary_HasNextTrack(void) {
    if (!g_library.initialised || g_library.current_index == (size_t)-1) {
        return g_music_library_track_count > 0U;
    }
    return (g_library.current_index + 1U) < g_music_library_track_count;
}

size_t MusicLibrary_GetRemainingTracks(void) {
    if (!g_library.initialised || g_library.current_index >= g_music_library_track_count) {
        return g_music_library_track_count;
    }
    return g_music_library_track_count - g_library.current_index - 1U;
}
