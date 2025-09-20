#include "ui_state.h"
#include "menu_items.h"

#include "nuno/music_catalog.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    const char *title;
    const char **items;
    uint8_t count;
} MenuDefinition;

static void populateMenu(UIState *state, MenuType type);
static void pushMenu(UIState *state, MenuType type);

void UIState_SetPlaybackHandler(UIState *state,
                                PlayTrackHandler handler,
                                void *context) {
    if (!state) {
        return;
    }

    state->playTrackHandler = handler;
    state->playTrackContext = context;
}

void initUIState(UIState *state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(UIState));
    state->batteryLevel = 85;
    state->volume = 50;
    state->totalTrackTime = 300;
    if (g_music_library_track_count > 0U) {
        const MusicLibraryTrack *initialTrack = &g_music_library_tracks[0];
        strncpy(state->currentTrackTitle, initialTrack->title, MAX_TITLE_LENGTH - 1);
        strncpy(state->currentArtist, initialTrack->artist, MAX_TITLE_LENGTH - 1);
        strncpy(state->currentAlbum, initialTrack->album, MAX_TITLE_LENGTH - 1);
    } else {
        strncpy(state->currentTrackTitle, "Now Playing", MAX_TITLE_LENGTH - 1);
        strncpy(state->currentArtist, "Artist", MAX_TITLE_LENGTH - 1);
        state->currentAlbum[0] = '\0';
    }

    state->navigationDepth = 0;
    state->playTrackHandler = NULL;
    state->playTrackContext = NULL;
    pushMenu(state, MENU_MAIN);
}

void selectMenuItem(UIState *state) {
    printf("selectMenuItem called, menuType=%d, selectedIndex=%d\n",
           state->currentMenuType, state->currentMenu.selectedIndex);

    if (!state || state->currentMenu.itemCount == 0) {
        printf("Early return: invalid state or empty menu\n");
        return;
    }

    MenuItem *item = &state->currentMenu.items[state->currentMenu.selectedIndex];
    if (!item->selectable) {
        printf("Item not selectable\n");
        return;
    }

    bool playback_started = false;

    if (state->currentMenuType == MENU_SONGS) {
        printf("In MENU_SONGS, selectedIndex=%d, track_count=%zu\n",
               state->currentMenu.selectedIndex, g_music_library_track_count);

        size_t selectedIndex = state->currentMenu.selectedIndex;
        if (selectedIndex < g_music_library_track_count) {
            const MusicLibraryTrack *track = &g_music_library_tracks[selectedIndex];
            printf("Selected track: %s by %s\n", track->title, track->artist);

            strncpy(state->currentTrackTitle, track->title, MAX_TITLE_LENGTH - 1);
            state->currentTrackTitle[MAX_TITLE_LENGTH - 1] = '\0';
            strncpy(state->currentArtist, track->artist, MAX_TITLE_LENGTH - 1);
            state->currentArtist[MAX_TITLE_LENGTH - 1] = '\0';
            strncpy(state->currentAlbum, track->album, MAX_TITLE_LENGTH - 1);
            state->currentAlbum[MAX_TITLE_LENGTH - 1] = '\0';
        }

        if (state->playTrackHandler) {
            printf("Calling playTrackHandler with index %zu\n", selectedIndex);
            playback_started = state->playTrackHandler(state->playTrackContext, selectedIndex);
            printf("Playback started: %s\n", playback_started ? "YES" : "NO");
        } else {
            printf("No playTrackHandler set\n");
        }
    } else {
        printf("Not in MENU_SONGS, menuType=%d\n", state->currentMenuType);
    }

    if (item->submenu != state->currentMenuType) {
        pushMenu(state, item->submenu);
    }

    if (playback_started) {
        state->isPlaying = true;
    }
}

void scrollUp(UIState *state) {
    if (!state || state->currentMenu.itemCount == 0) {
        return;
    }

    if (state->currentMenu.selectedIndex > 0) {
        state->currentMenu.selectedIndex--;
    }

    if (state->currentMenu.selectedIndex < state->currentMenu.scrollOffset) {
        state->currentMenu.scrollOffset = state->currentMenu.selectedIndex;
    }
}

void scrollDown(UIState *state) {
    if (!state || state->currentMenu.itemCount == 0) {
        return;
    }

    if (state->currentMenu.selectedIndex + 1 < state->currentMenu.itemCount) {
        state->currentMenu.selectedIndex++;
    }

    uint8_t visibleSlots = (DISPLAY_HEIGHT - TITLE_BAR_HEIGHT) / ITEM_HEIGHT;
    uint8_t bottomVisible = state->currentMenu.scrollOffset + (visibleSlots ? visibleSlots : 1) - 1;
    if (state->currentMenu.selectedIndex > bottomVisible) {
        state->currentMenu.scrollOffset = state->currentMenu.selectedIndex - (DISPLAY_HEIGHT / ITEM_HEIGHT) + 1;
    }
}

void goBack(UIState *state) {
    if (!state || state->navigationDepth == 0) {
        return;
    }

    if (state->navigationDepth > 1) {
        state->navigationDepth--;
        MenuType previous = state->navigationStack[state->navigationDepth - 1];
        populateMenu(state, previous);
    }
}

void navigateToMenu(UIState *state, MenuType menuType) {
    if (!state) {
        return;
    }

    if (state->currentMenuType == menuType) {
        return;
    }

    pushMenu(state, menuType);
}

static void pushMenu(UIState *state, MenuType type) {
    if (state->navigationDepth < sizeof(state->navigationStack) / sizeof(state->navigationStack[0])) {
        state->navigationStack[state->navigationDepth++] = type;
    } else {
        state->navigationStack[state->navigationDepth - 1] = type;
    }
    populateMenu(state, type);
}

static void populateMenu(UIState *state, MenuType type) {
    state->currentMenuType = type;
    state->currentMenu.selectedIndex = 0;
    state->currentMenu.scrollOffset = 0;
    state->currentMenu.itemCount = 0;

    const char *title = "Menu";

    switch (type) {
        case MENU_MAIN:
            title = "NUNO";
            state->currentMenu.itemCount = NUM_MAIN_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, MAIN_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = (i == 0 || i == 1 || i == 2 || i == 5);
                switch (i) {
                    case 0: state->currentMenu.items[i].submenu = MENU_MUSIC; break;
                    case 1: state->currentMenu.items[i].submenu = MENU_PHOTOS; break;
                    case 2: state->currentMenu.items[i].submenu = MENU_SETTINGS; break;
                    case 5: state->currentMenu.items[i].submenu = MENU_NOW_PLAYING; break;
                    default: state->currentMenu.items[i].submenu = MENU_MAIN; break;
                }
            }
            break;
        case MENU_MUSIC:
            title = "Music";
            state->currentMenu.itemCount = NUM_MUSIC_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, MUSIC_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                bool isSongs = (i == 3U);
                state->currentMenu.items[i].selectable = isSongs && (g_music_library_track_count > 0U);
                state->currentMenu.items[i].submenu = isSongs ? MENU_SONGS : MENU_MUSIC;
            }
            break;
        case MENU_SETTINGS:
            title = "Settings";
            state->currentMenu.itemCount = NUM_SETTINGS_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, SETTINGS_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = false;
                state->currentMenu.items[i].submenu = MENU_SETTINGS;
            }
            break;
        case MENU_PHOTOS:
            title = "Extras";
            state->currentMenu.itemCount = NUM_EXTRAS_MENU_ITEMS;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                strncpy(state->currentMenu.items[i].text, EXTRAS_MENU_ITEMS[i], MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = false;
                state->currentMenu.items[i].submenu = MENU_PHOTOS;
            }
            break;
        case MENU_GAMES:
            title = "Games";
            break;
        case MENU_NOW_PLAYING:
            title = "Now Playing";
            state->currentMenu.itemCount = 6;

            // Track title
            strncpy(state->currentMenu.items[0].text, state->currentTrackTitle, MAX_ITEM_LENGTH - 1);
            state->currentMenu.items[0].text[MAX_ITEM_LENGTH - 1] = '\0';
            state->currentMenu.items[0].selectable = false;
            state->currentMenu.items[0].submenu = MENU_NOW_PLAYING;

            // Artist name
            strncpy(state->currentMenu.items[1].text, state->currentArtist, MAX_ITEM_LENGTH - 1);
            state->currentMenu.items[1].text[MAX_ITEM_LENGTH - 1] = '\0';
            state->currentMenu.items[1].selectable = false;
            state->currentMenu.items[1].submenu = MENU_NOW_PLAYING;

            // Time display (current/total)
            uint16_t currentMin = state->currentTrackTime / 60;
            uint16_t currentSec = state->currentTrackTime % 60;
            uint16_t totalMin = state->totalTrackTime / 60;
            uint16_t totalSec = state->totalTrackTime % 60;
            snprintf(state->currentMenu.items[2].text, MAX_ITEM_LENGTH,
                    "%02u:%02u / %02u:%02u", currentMin, currentSec, totalMin, totalSec);
            state->currentMenu.items[2].selectable = false;
            state->currentMenu.items[2].submenu = MENU_NOW_PLAYING;

            // Play/Pause status
            strncpy(state->currentMenu.items[3].text,
                   state->isPlaying ? "Playing" : "Paused", MAX_ITEM_LENGTH - 1);
            state->currentMenu.items[3].text[MAX_ITEM_LENGTH - 1] = '\0';
            state->currentMenu.items[3].selectable = false;
            state->currentMenu.items[3].submenu = MENU_NOW_PLAYING;

            // Volume display
            snprintf(state->currentMenu.items[4].text, MAX_ITEM_LENGTH,
                    "Volume: %u%%", state->volume);
            state->currentMenu.items[4].selectable = false;
            state->currentMenu.items[4].submenu = MENU_NOW_PLAYING;

            // Instructions
            strncpy(state->currentMenu.items[5].text, "Menu=Back  Play=Pause", MAX_ITEM_LENGTH - 1);
            state->currentMenu.items[5].text[MAX_ITEM_LENGTH - 1] = '\0';
            state->currentMenu.items[5].selectable = false;
            state->currentMenu.items[5].submenu = MENU_NOW_PLAYING;
            break;
        case MENU_SONGS:
            title = "Songs";
            if (g_music_library_track_count == 0U) {
                state->currentMenu.itemCount = 1U;
                strncpy(state->currentMenu.items[0].text,
                        "No tracks found",
                        MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[0].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[0].selectable = false;
                state->currentMenu.items[0].submenu = MENU_SONGS;
                break;
            }

            size_t trackCount = g_music_library_track_count;
            if (trackCount > MAX_MENU_ITEMS) {
                trackCount = MAX_MENU_ITEMS;
            }

            state->currentMenu.itemCount = (uint8_t)trackCount;
            for (uint8_t i = 0; i < state->currentMenu.itemCount; ++i) {
                const MusicLibraryTrack *track = &g_music_library_tracks[i];
                const char *titleText = (track && track->title) ? track->title : "Unknown";
                strncpy(state->currentMenu.items[i].text, titleText, MAX_ITEM_LENGTH - 1);
                state->currentMenu.items[i].text[MAX_ITEM_LENGTH - 1] = '\0';
                state->currentMenu.items[i].selectable = true;
                state->currentMenu.items[i].submenu = MENU_NOW_PLAYING;
            }
            break;
    }

    strncpy(state->currentMenu.title, title, MAX_TITLE_LENGTH - 1);
    state->currentMenu.title[MAX_TITLE_LENGTH - 1] = '\0';
}

void refreshNowPlayingView(UIState* state) {
    if (!state || state->currentMenuType != MENU_NOW_PLAYING) {
        return;
    }

    // Refresh the Now Playing menu without changing navigation
    populateMenu(state, MENU_NOW_PLAYING);
}
