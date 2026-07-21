#include "common.h"
#include "yandex_api.h"
#include "audio_player.h"
#include "ui.h"
#include "visualizer.h"

// ─── Configuration ─────────────────────────────────────────────
// Set your Yandex OAuth token and user ID here
// Get token from: https://oauth.yandex.ru/client/new (or use passport)
#define YANDEX_OAUTH_TOKEN  "YOUR_OAUTH_TOKEN_HERE"
#define YANDEX_USER_ID      "YOUR_USER_ID_HERE"

// ─── App context ───────────────────────────────────────────────

static AppContext g_app;

// ─── Auto-play next track ──────────────────────────────────────

static void auto_play_next(void) {
    if (g_app.current_playlist.track_count == 0) return;

    if (g_app.repeat) {
        // Replay same track
        ui_show_message("Repeating...", 20);
    } else if (g_app.shuffle) {
        // Random next
        int next = rand() % g_app.current_playlist.track_count;
        g_app.current_track = next;
    } else if (g_app.current_track < g_app.current_playlist.track_count - 1) {
        g_app.current_track++;
    } else {
        // End of playlist
        audio_stop();
        ui_show_message("End of playlist", 30);
        return;
    }

    // Get stream URL and play
    char url[MAX_URL_LEN];
    YmpError err = yandex_api_get_track_url(
        g_app.current_playlist.tracks[g_app.current_track].id,
        url, sizeof(url)
    );

    if (err == YMP_OK) {
        audio_play(url);
    } else {
        printf("[Main] Failed to get track URL: %d\n", err);
    }
}

// ─── Load liked tracks ─────────────────────────────────────────

static void load_liked_tracks(void) {
    // Free old playlist
    if (g_app.current_playlist.tracks) {
        free(g_app.current_playlist.tracks);
        memset(&g_app.current_playlist, 0, sizeof(Playlist));
    }

    YmpError err = yandex_api_get_liked_tracks(&g_app.current_playlist);
    if (err != YMP_OK) {
        printf("[Main] Failed to load liked tracks: %d\n", err);
        ui_show_message("Failed to load tracks!", 60);
        return;
    }

    printf("[Main] Loaded %d liked tracks\n", g_app.current_playlist.track_count);
    g_app.current_track = 0;
    g_app.selected_item = 0;
    g_app.scroll_offset = 0;
}

// ─── Load recommendations ──────────────────────────────────────

static void load_recommendations(void) {
    if (g_app.current_playlist.tracks) {
        free(g_app.current_playlist.tracks);
        memset(&g_app.current_playlist, 0, sizeof(Playlist));
    }

    YmpError err = yandex_api_get_recommendations(&g_app.current_playlist);
    if (err != YMP_OK) {
        printf("[Main] Failed to load recommendations: %d\n", err);
        ui_show_message("Failed to load recommendations!", 60);
        return;
    }

    printf("[Main] Loaded %d recommended tracks\n", g_app.current_playlist.track_count);
    g_app.current_track = 0;
    g_app.selected_item = 0;
    g_app.scroll_offset = 0;
}

// ─── Main ──────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("==========================================\n");
    printf("  Yandex Music Player v%s\n", APP_VERSION);
    printf("  For PS4 (OpenOrbis)\n");
    printf("==========================================\n\n");

    // Initialize context
    memset(&g_app, 0, sizeof(AppContext));
    g_app.volume = 50;
    g_app.state = PLAYER_STOPPED;
    g_app.current_track = -1;

    // Initialize subsystems
    printf("[Init] Initializing audio...\n");
    YmpError err = audio_init();
    if (err != YMP_OK) {
        printf("[Init] Audio init failed: %d\n", err);
        // Continue anyway (may work on PC)
    }

    printf("[Init] Initializing Yandex API...\n");
    err = yandex_api_init(YANDEX_OAUTH_TOKEN, YANDEX_USER_ID);
    if (err != YMP_OK) {
        printf("[Init] Yandex API init failed: %d\n", err);
        ui_show_message("API init failed! Check token.", 120);
    }

    printf("[Init] Initializing UI...\n");
    err = ui_init(&g_app);
    if (err != YMP_OK) {
        printf("[Init] UI init failed: %d\n", err);
    }

    // Initialize visualizer (Milkdrop / Simple)
    printf("[Init] Initializing visualizer...\n");
    VizSettings viz_settings = {
        .mode = VIZ_MILKDROP,      // Milkdrop визуализация
        .width = 1920,
        .height = 1080,
        .fps = 60,
        .mesh_x = 48,
        .mesh_y = 32,
        .beat_sensitivity = 1.0f,
        .preset_duration = 15,      // Смена пресета каждые 15 сек
        .soft_cut_duration = 3.0f,
        .hard_cut_enabled = false,
        .hard_cut_sensitivity = 1.0f,
        .preset_path = "/app0/assets/presets/",
        .texture_path = "/app0/assets/textures/",
    };

    if (viz_init(&viz_settings) != 0) {
        printf("[Init] Visualizer init failed, trying simple mode\n");
        viz_settings.mode = VIZ_SIMPLE;
        viz_init(&viz_settings);
    }

    printf("[Init] Initialization complete!\n\n");

    // Auto-load liked tracks
    load_liked_tracks();

    // ─── Main loop ─────────────────────────────────────────────

    uint64_t last_render = 0;

    while (1) {
        // Handle input
        ui_handle_input();

        // Check if track finished
        if (audio_is_finished()) {
            auto_play_next();
        }

        // Render visualizer (gets PCM data from audio)
        if (viz_get_mode() != VIZ_OFF) {
            // Получаем PCM данные из аудио буфера
            // (В реальном проекте audio_player должен предоставлять доступ к PCM)
            // short pcm_buf[2048];
            // int samples = audio_get_pcm(pcm_buf, 2048);
            // if (samples > 0) {
            //     viz_render_frame(pcm_buf, samples, 2);
            // }
        }

        // Render UI (~60fps)
        ui_render();

        // Frame timing
#ifdef __ORBIS__
        uint64_t now;
        sceRtcGetCurrentTick(&now);
        if (now - last_render < (1000000 / 60)) {
            sceKernelSleep(16); // ~16ms per frame
            continue;
        }
        last_render = now;
#else
        // PC mode - simple sleep
        usleep(16000);
#endif
    }

    // Cleanup (never reached in this loop, but good practice)
    viz_cleanup();
    ui_cleanup();
    audio_cleanup();
    yandex_api_cleanup();

    return 0;
}
