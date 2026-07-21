#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// PS4-specific
#ifdef __ORBIS__
#include <orbis/libkernel.h>
#include <orbis/libSceNet.h>
#include <orbis/libSceHttp.h>
#include <orbis/libSceAudioOut.h>
#include <orbis/libSceSysmodule.h>
#include <orbis/libSceRtc.h>
#include <orbis/libSceSystemService.h>
#include <orbis/libSceAppMgr.h>
#endif

// App constants
#define APP_TITLE      "Yandex Music Player"
#define APP_VERSION    "1.0.0"
#define MAX_TRACKS     100
#define MAX_URL_LEN    512
#define MAX_TITLE_LEN  256
#define MAX_ARTIST_LEN 128
#define AUDIO_BUF_SIZE 65536  // 64KB audio buffer
#define HTTP_TIMEOUT   30     // seconds

// Yandex Music API
#define YANDEX_API_BASE    "https://api.music.yandex.net"
#define YANDEX_MUSIC_HOST  "music.yandex.ru"

// Error codes
typedef enum {
    YMP_OK = 0,
    YMP_ERR_NETWORK,
    YMP_ERR_AUTH,
    YMP_ERR_API,
    YMP_ERR_AUDIO,
    YMP_ERR_MEMORY,
    YMP_ERR_NOT_FOUND
} YmpError;

// Track info structure
typedef struct {
    char id[32];
    char title[MAX_TITLE_LEN];
    char artist[MAX_ARTIST_LEN];
    char album[MAX_TITLE_LEN];
    char uri[MAX_URL_LEN];       // stream URL
    char cover_url[MAX_URL_LEN]; // album art URL
    int  duration_ms;
    int  bitrate;
} TrackInfo;

// Playlist
typedef struct {
    char        id[32];
    char        name[MAX_TITLE_LEN];
    int         track_count;
    TrackInfo  *tracks;
} Playlist;

// Player state
typedef enum {
    PLAYER_STOPPED,
    PLAYER_PLAYING,
    PLAYER_PAUSED
} PlayerState;

// App context
typedef struct {
    // Auth
    char oauth_token[512];
    char uid[32];

    // Audio
    PlayerState state;
    int         current_track;
    int         volume;          // 0-100
    int         audio_port;
    bool        shuffle;
    bool        repeat;

    // Library
    Playlist   *playlists;
    int         playlist_count;
    Playlist    current_playlist;

    // UI
    bool        ui_dirty;
    int         selected_item;
    int         scroll_offset;
} AppContext;

// Utility macros
#define SAFE_FREE(p) do { if (p) { free(p); (p) = NULL; } } while(0)
#define SAFE_STRCPY(dst, src) do { strncpy(dst, src, sizeof(dst)-1); dst[sizeof(dst)-1]='\0'; } while(0)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif // COMMON_H
