#include "common.h"
#include "ui.h"
#include "audio_player.h"

// ─── PS4 Debug Text Overlay ────────────────────────────────────
// On a jailbroken PS4, we use the debug text overlay (sceSysDiag)
// or direct framebuffer access for the UI.

#ifdef __ORBIS__



#endif

// ─── Static state ──────────────────────────────────────────────

static AppContext *g_ctx = NULL;

// UI screen modes
typedef enum {
    UI_SCREEN_MENU,
    UI_SCREEN_PLAYLIST,
    UI_SCREEN_NOW_PLAYING,
    UI_SCREEN_SEARCH,
    UI_SCREEN_SETTINGS
} UIScreen;

static UIScreen  g_current_screen = UI_SCREEN_MENU;
static char      g_message[256]  = {0};
static int       g_message_ttl   = 0;

// Menu items
static const char *g_menu_items[] = {
    "Liked Tracks",
    "Playlists",
    "Search",
    "Recommendations",
    "Settings",
    "Exit"
};
#define MENU_COUNT 6

// ─── Display helpers ───────────────────────────────────────────

// Print a line at position (using console output as fallback)
static void print_line(int y, const char *text) {
    // PS4 debug console output
    printf("\033[%d;1H%s", y, text);
}

static void clear_screen(void) {
    printf("\033[2J\033[H");
}

static void set_color(int fg) {
    printf("\033[3%dm", fg);
}

static void reset_color(void) {
    printf("\033[0m");
}

static void draw_header(void) {
    set_color(6); // cyan
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║           YANDEX MUSIC PLAYER  v1.0.0                  ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    reset_color();
}

static void draw_volume_bar(void) {
    int vol = g_ctx ? g_ctx->volume : 50;
    int bars = vol / 5;

    set_color(2); // green
    printf("  VOL: [");
    for (int i = 0; i < 20; i++) {
        if (i < bars)
            printf("█");
        else
            printf("░");
    }
    printf("] %d%%", vol);
    reset_color();
}

static void draw_state(void) {
    PlayerState state = audio_get_state();
    set_color(5); // magenta
    printf("  STATE: ");
    switch (state) {
        case PLAYER_PLAYING:  set_color(2); printf("▶ PLAYING"); break;
        case PLAYER_PAUSED:   set_color(3); printf("❚❚ PAUSED"); break;
        case PLAYER_STOPPED:  set_color(1); printf("■ STOPPED"); break;
    }
    reset_color();
    printf("\n");
}

// ─── Screen renderers ──────────────────────────────────────────

static void render_menu(void) {
    clear_screen();
    draw_header();

    printf("\n");
    set_color(7);
    printf("  Use D-Pad ↑↓ to navigate, X to select, ○ to back\n\n");
    reset_color();

    for (int i = 0; i < MENU_COUNT; i++) {
        if (i == g_ctx->selected_item) {
            set_color(6); // cyan
            printf("  ▸ %s ◂\n", g_menu_items[i]);
        } else {
            set_color(7);
            printf("    %s\n", g_menu_items[i]);
        }
    }

    printf("\n");
    draw_volume_bar();
    printf("\n");
    draw_state();
}

static void render_playlist(void) {
    clear_screen();
    draw_header();

    Playlist *pl = &g_ctx->current_playlist;
    set_color(6);
    printf("  📁 %s (%d tracks)\n\n", pl->name, pl->track_count);
    reset_color();

    if (!pl->tracks || pl->track_count == 0) {
        set_color(3);
        printf("  No tracks loaded.\n");
        reset_color();
        return;
    }

    int start = g_ctx->scroll_offset;
    int visible = 15;

    for (int i = start; i < start + visible && i < pl->track_count; i++) {
        if (i == g_ctx->current_track && g_ctx->state == PLAYER_PLAYING) {
            set_color(2); // green - playing
            printf("  ▶ ");
        } else if (i == g_ctx->selected_item) {
            set_color(6); // cyan - selected
            printf("  ▸ ");
        } else {
            set_color(7);
            printf("    ");
        }

        // Track number
        printf("%2d. ", i + 1);

        // Track info
        if (pl->tracks[i].title[0]) {
            printf("%s", pl->tracks[i].title);
        } else {
            printf("Track %s", pl->tracks[i].id);
        }

        if (pl->tracks[i].artist[0]) {
            printf(" — %s", pl->tracks[i].artist);
        }

        // Duration
        if (pl->tracks[i].duration_ms > 0) {
            int sec = pl->tracks[i].duration_ms / 1000;
            int min = sec / 60;
            sec %= 60;
            printf("  [%d:%02d]", min, sec);
        }

        reset_color();
        printf("\n");
    }

    if (pl->track_count > visible) {
        printf("\n  ─── %d more tracks (scroll) ───\n",
               pl->track_count - start - visible);
    }

    printf("\n");
    draw_volume_bar();
    printf("\n");
    draw_state();
}

static void render_now_playing(void) {
    clear_screen();
    draw_header();

    Playlist *pl = &g_ctx->current_playlist;
    if (g_ctx->current_track >= 0 && g_ctx->current_track < pl->track_count) {
        TrackInfo *t = &pl->tracks[g_ctx->current_track];

        printf("\n");
        set_color(2);
        printf("  ▶ NOW PLAYING\n\n");
        reset_color();

        set_color(7);
        printf("  Title:    %s\n", t->title[0] ? t->title : t->id);
        printf("  Artist:   %s\n", t->artist[0] ? t->artist : "Unknown");
        printf("  Album:    %s\n", t->album[0] ? t->album : "—");

        if (t->duration_ms > 0) {
            int sec = t->duration_ms / 1000;
            int min = sec / 60;
            sec %= 60;
            printf("  Duration: %d:%02d\n", min, sec);
        }

        // Progress bar (simulated)
        printf("\n  ");
        set_color(6);
        int progress = 0; // Would track real position
        printf("00:00 [");
        for (int i = 0; i < 30; i++) {
            printf(i < progress ? "━" : "─");
        }
        printf("] 00:00\n");
        reset_color();
    }

    printf("\n");
    draw_volume_bar();
    printf("\n");
    draw_state();

    printf("\n");
    set_color(8); // gray
    printf("  Controls: X=Pause/Play  ◄/►=Prev/Next  ↑↓=Vol\n");
    printf("            L1=Shuffle    R1=Repeat     ○=Back\n");
    reset_color();
}

static void render_search(void) {
    clear_screen();
    draw_header();

    set_color(6);
    printf("\n  🔍 SEARCH\n\n");
    reset_color();

    set_color(7);
    printf("  Enter search term:\n\n");
    printf("  > ");
    reset_color();

    // Would show input field in real app
    printf("____________________\n\n");

    set_color(8);
    printf("  (On-screen keyboard would appear here)\n");
    printf("  Use controller to type\n");
    reset_color();
}

// ─── Public API ────────────────────────────────────────────────

YmpError ui_init(AppContext *ctx) {
    if (!ctx) return YMP_ERR_MEMORY;
    g_ctx = ctx;
    g_current_screen = UI_SCREEN_MENU;
    g_ctx->selected_item = 0;
    g_ctx->scroll_offset = 0;
    return YMP_OK;
}

void ui_cleanup(void) {
    g_ctx = NULL;
}

void ui_render(void) {
    switch (g_current_screen) {
        case UI_SCREEN_MENU:          render_menu();          break;
        case UI_SCREEN_PLAYLIST:      render_playlist();      break;
        case UI_SCREEN_NOW_PLAYING:   render_now_playing();   break;
        case UI_SCREEN_SEARCH:        render_search();        break;
        case UI_SCREEN_SETTINGS:      render_menu();          break; // TODO
    }

    // Show temporary message
    if (g_message_ttl > 0) {
        printf("\n  %s\n", g_message);
        g_message_ttl--;
    }
}

void ui_handle_input(void) {
#ifdef __ORBIS__
    // Read gamepad input
    OrbisPadData pad;
    memset(&pad, 0, sizeof(pad));

    if (scePadReadState(0, &pad) < 0) return;

    static uint32_t prev_buttons = 0;
    uint32_t pressed = pad.buttons & ~prev_buttons;
    prev_buttons = pad.buttons;

    if (pressed == 0) return;

    // D-Pad navigation
    if (pressed & ORBIS_PAD_BUTTON_UP) {
        if (g_ctx->selected_item > 0) g_ctx->selected_item--;
        if (g_current_screen == UI_SCREEN_PLAYLIST && g_ctx->selected_item < g_ctx->scroll_offset)
            g_ctx->scroll_offset = g_ctx->selected_item;
    }
    if (pressed & ORBIS_PAD_BUTTON_DOWN) {
        Playlist *pl = &g_ctx->current_playlist;
        int max = (g_current_screen == UI_SCREEN_PLAYLIST) ? pl->track_count - 1 : MENU_COUNT - 1;
        if (g_ctx->selected_item < max) g_ctx->selected_item++;
        if (g_current_screen == UI_SCREEN_PLAYLIST && g_ctx->selected_item >= g_ctx->scroll_offset + 15)
            g_ctx->scroll_offset = g_ctx->selected_item - 14;
    }

    // D-Pad Left/Right - volume
    if (pressed & ORBIS_PAD_BUTTON_LEFT) {
        audio_set_volume(g_ctx->volume - 5);
        g_ctx->volume = MAX(0, g_ctx->volume - 5);
    }
    if (pressed & ORBIS_PAD_BUTTON_RIGHT) {
        audio_set_volume(g_ctx->volume + 5);
        g_ctx->volume = MIN(100, g_ctx->volume + 5);
    }

    // X button - select / play
    if (pressed & ORBIS_PAD_BUTTON_CROSS) {
        switch (g_current_screen) {
            case UI_SCREEN_MENU:
                switch (g_ctx->selected_item) {
                    case 0: // Liked Tracks
                        ui_show_message("Loading liked tracks...", 30);
                        g_current_screen = UI_SCREEN_PLAYLIST;
                        break;
                    case 1: // Playlists
                        ui_show_message("Loading playlists...", 30);
                        g_current_screen = UI_SCREEN_PLAYLIST;
                        break;
                    case 2: // Search
                        g_current_screen = UI_SCREEN_SEARCH;
                        break;
                    case 3: // Recommendations
                        ui_show_message("Loading recommendations...", 30);
                        g_current_screen = UI_SCREEN_PLAYLIST;
                        break;
                    case 5: // Exit
                        audio_stop();
                        // Would call sceKernelExit(0) or similar
                        printf("Exiting...\n");
                        break;
                }
                break;

            case UI_SCREEN_PLAYLIST:
                if (g_ctx->current_track >= 0 && g_ctx->current_track < g_ctx->current_playlist.track_count) {
                    if (audio_get_state() == PLAYER_PLAYING) {
                        audio_pause();
                    } else if (audio_get_state() == PLAYER_PAUSED) {
                        audio_resume();
                    } else {
                        // Start playing selected track
                        g_ctx->current_track = g_ctx->selected_item;
                        g_current_screen = UI_SCREEN_NOW_PLAYING;
                        ui_show_message("Playing...", 20);
                    }
                }
                break;

            case UI_SCREEN_NOW_PLAYING:
                if (audio_get_state() == PLAYER_PLAYING) {
                    audio_pause();
                } else if (audio_get_state() == PLAYER_PAUSED) {
                    audio_resume();
                }
                break;

            case UI_SCREEN_SEARCH:
                // Would open on-screen keyboard
                break;

            default:
                break;
        }
    }

    // O button - back
    if (pressed & ORBIS_PAD_BUTTON_CIRCLE) {
        switch (g_current_screen) {
            case UI_SCREEN_NOW_PLAYING:
            case UI_SCREEN_PLAYLIST:
            case UI_SCREEN_SEARCH:
                audio_stop();
                g_current_screen = UI_SCREEN_MENU;
                g_ctx->selected_item = 0;
                break;
            default:
                break;
        }
    }

    // L1 - previous track
    if (pressed & ORBIS_PAD_BUTTON_L1) {
        Playlist *pl = &g_ctx->current_playlist;
        if (g_ctx->current_track > 0) {
            g_ctx->current_track--;
            ui_show_message("Previous track", 15);
        }
    }

    // R1 - next track
    if (pressed & ORBIS_PAD_BUTTON_R1) {
        Playlist *pl = &g_ctx->current_playlist;
        if (g_ctx->current_track < pl->track_count - 1) {
            g_ctx->current_track++;
            ui_show_message("Next track", 15);
        }
    }

    // Triangle - toggle shuffle
    if (pressed & ORBIS_PAD_BUTTON_TRIANGLE) {
        g_ctx->shuffle = !g_ctx->shuffle;
        ui_show_message(g_ctx->shuffle ? "Shuffle ON" : "Shuffle OFF", 20);
    }

    // Square - toggle repeat
    if (pressed & ORBIS_PAD_BUTTON_SQUARE) {
        g_ctx->repeat = !g_ctx->repeat;
        ui_show_message(g_ctx->repeat ? "Repeat ON" : "Repeat OFF", 20);
    }

    // D-Pad Up/Down with L1 held - volume
    if ((pressed & ORBIS_PAD_BUTTON_UP) && (pad.buttons & ORBIS_PAD_BUTTON_L1)) {
        audio_set_volume(g_ctx->volume + 5);
        g_ctx->volume = MIN(100, g_ctx->volume + 5);
    }
    if ((pressed & ORBIS_PAD_BUTTON_DOWN) && (pad.buttons & ORBIS_PAD_BUTTON_L1)) {
        audio_set_volume(g_ctx->volume - 5);
        g_ctx->volume = MAX(0, g_ctx->volume - 5);
    }

#endif
}

void ui_update_now_playing(const TrackInfo *track) {
    (void)track;
    // Updates are handled by render_now_playing()
}

void ui_show_message(const char *msg, int duration_ms) {
    SAFE_STRCPY(g_message, msg);
    g_message_ttl = duration_ms / 16; // ~60fps
}
