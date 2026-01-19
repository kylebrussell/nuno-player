#include "nuno/music_catalog.h"

const MusicLibraryTrack g_music_library_tracks[] = {
    {
        .title = "Aria",
        .album = "Open Goldberg Variations",
        .artist = "Kimiko Ishizaka",
        .filename = "bach/open-goldberg-variations/Kimiko_Ishizaka_-_Open_Goldberg_Variations_-_01_Aria.mp3",
        .duration_seconds = 0U
    },
    {
        .title = "Variatio 1",
        .album = "Open Goldberg Variations",
        .artist = "Kimiko Ishizaka",
        .filename = "bach/open-goldberg-variations/Kimiko_Ishizaka_-_Open_Goldberg_Variations_-_02_Variatio_1.mp3",
        .duration_seconds = 0U
    },
    {
        .title = "Variatio 2",
        .album = "Open Goldberg Variations",
        .artist = "Kimiko Ishizaka",
        .filename = "bach/open-goldberg-variations/Kimiko_Ishizaka_-_Open_Goldberg_Variations_-_03_Variatio_2.mp3",
        .duration_seconds = 0U
    },
    {
        .title = "Aria (FLAC test)",
        .album = "Open Goldberg Variations",
        .artist = "Kimiko Ishizaka",
        // Drop the FLAC file under assets/music/ to enable this entry.
        .filename = "bach/open-goldberg-variations/Kimiko_Ishizaka_-_Open_Goldberg_Variations_-_01_Aria.flac",
        .duration_seconds = 0U
    }
};

const size_t g_music_library_track_count = sizeof(g_music_library_tracks) /
                                          sizeof(g_music_library_tracks[0]);
