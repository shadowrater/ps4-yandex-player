#include "common.h"
#include "visualizer.h"

// ═══════════════════════════════════════════════════════════════
//  Milkdrop Visualizer для PS4
// ═══════════════════════════════════════════════════════════════

// ─── projectM stub (когда нет USE_PROJECTM) ───────────────────
#ifndef USE_PROJECTM
typedef void* projectm_handle;
typedef void* projectm_playlist_handle;
static projectm_handle projectm_create(void) { return (void*)1; }
static void projectm_destroy(projectm_handle h) { (void)h; }
static void projectm_set_window_size(projectm_handle h, int w, int ht) { (void)h; (void)w; (void)ht; }
static void projectm_set_fps(projectm_handle h, int fps) { (void)h; (void)fps; }
static void projectm_set_mesh_size(projectm_handle h, int x, int y) { (void)h; (void)x; (void)y; }
static void projectm_set_aspect_correction(projectm_handle h, int v) { (void)h; (void)v; }
static void projectm_set_preset_duration(projectm_handle h, double v) { (void)h; (void)v; }
static void projectm_set_soft_cut_duration(projectm_handle h, double v) { (void)h; (void)v; }
static void projectm_set_hard_cut_enabled(projectm_handle h, int v) { (void)h; (void)v; }
static void projectm_set_hard_cut_sensitivity(projectm_handle h, float v) { (void)h; (void)v; }
static void projectm_set_beat_sensitivity(projectm_handle h, float v) { (void)h; (void)v; }
static void projectm_set_preset_locked(projectm_handle h, int v) { (void)h; (void)v; }
static void projectm_load_preset_file(projectm_handle h, const char* f, int s) { (void)h; (void)f; (void)s; }
static void projectm_opengl_render_frame(projectm_handle h) { (void)h; }
static void projectm_pcm_add_float(projectm_handle h, const float* s, int n, int c) { (void)h; (void)s; (void)n; (void)c; }
static void projectm_pcm_add_int16(projectm_handle h, const short* s, int n, int c) { (void)h; (void)s; (void)n; (void)c; }
#endif

// ─── Simple spectrum analyzer ─────────────────────────────────

#define SIMPLE_BARS 64
#define SIMPLE_FFT_SIZE 128

static float g_simple_bars[SIMPLE_BARS] = {0};
static float g_simple_fft[SIMPLE_FFT_SIZE / 2] = {0};
static float g_simple_input[SIMPLE_FFT_SIZE] = {0};
static int   g_simple_pos = 0;

static void simple_update_bars(const float *samples, int count) {
    for (int i = 0; i < count && g_simple_pos < SIMPLE_FFT_SIZE; i++)
        g_simple_input[g_simple_pos++] = samples[i];

    if (g_simple_pos >= SIMPLE_FFT_SIZE) {
        for (int k = 0; k < SIMPLE_FFT_SIZE / 2; k++) {
            float real = 0, imag = 0;
            for (int t = 0; t < SIMPLE_FFT_SIZE; t++) {
                float angle = 6.2831853f * k * t / SIMPLE_FFT_SIZE;
                real += g_simple_input[t] * cosf(angle);
                imag -= g_simple_input[t] * sinf(angle);
            }
            g_simple_fft[k] = sqrtf(real * real + imag * imag) / SIMPLE_FFT_SIZE;
        }
        int bin_per_bar = (SIMPLE_FFT_SIZE / 2) / SIMPLE_BARS;
        for (int i = 0; i < SIMPLE_BARS; i++) {
            float sum = 0;
            for (int j = 0; j < bin_per_bar; j++)
                sum += g_simple_fft[i * bin_per_bar + j];
            float val = sum / bin_per_bar;
            if (val > g_simple_bars[i]) g_simple_bars[i] = val;
            else g_simple_bars[i] = g_simple_bars[i] * 0.85f + val * 0.15f;
        }
        g_simple_pos = 0;
    }
}

// ─── Static state ──────────────────────────────────────────────

static VizSettings  g_settings;
static VizMode      g_mode = VIZ_OFF;
static bool         g_initialized = false;
static float        g_fps = 0;
static int          g_frame_count = 0;
static uint64_t     g_last_fps_time = 0;

#ifdef USE_PROJECTM
static projectm_handle g_pm = NULL;
#endif

// ═══════════════════════════════════════════════════════════════

int viz_init(const VizSettings *settings) {
    if (!settings) return -1;
    memcpy(&g_settings, settings, sizeof(VizSettings));
    g_mode = settings->mode;

    if (g_mode == VIZ_SIMPLE) {
        memset(g_simple_bars, 0, sizeof(g_simple_bars));
        g_initialized = true;
        return 0;
    }

    if (g_mode == VIZ_MILKDROP) {
#ifdef USE_PROJECTM
        g_pm = projectm_create();
        if (!g_pm) return -1;
        projectm_set_window_size(g_pm, settings->width, settings->height);
        projectm_set_fps(g_pm, settings->fps);
        projectm_set_mesh_size(g_pm, settings->mesh_x, settings->mesh_y);
        projectm_set_aspect_correction(g_pm, 1);
        projectm_set_preset_duration(g_pm, settings->preset_duration);
        projectm_set_soft_cut_duration(g_pm, settings->soft_cut_duration);
        projectm_set_hard_cut_enabled(g_pm, settings->hard_cut_enabled ? 1 : 0);
        projectm_set_beat_sensitivity(g_pm, settings->beat_sensitivity);
        g_initialized = true;
        return 0;
#else
        g_mode = VIZ_SIMPLE;
        return viz_init(settings);
#endif
    }

    if (g_mode == VIZ_OFF) { g_initialized = true; return 0; }
    return -1;
}

void viz_cleanup(void) {
#ifdef USE_PROJECTM
    if (g_pm) { projectm_destroy(g_pm); g_pm = NULL; }
#endif
    g_initialized = false;
    g_mode = VIZ_OFF;
}

void viz_render_frame(const void *pcm_data, int sample_count, int channels) {
    if (!g_initialized || g_mode == VIZ_OFF) return;

    g_frame_count++;

    if (g_mode == VIZ_SIMPLE) {
        const short *s16 = (const short *)pcm_data;
        float fl[2048];
        int mono = (channels == 2) ? sample_count / 2 : sample_count;
        if (mono > 2048) mono = 2048;
        for (int i = 0; i < mono; i++)
            fl[i] = (channels == 2) ? (s16[i*2] + s16[i*2+1]) / 2.0f / 32768.0f : s16[i] / 32768.0f;
        simple_update_bars(fl, mono);
    }

    if (g_mode == VIZ_MILKDROP) {
#ifdef USE_PROJECTM
        if (channels == 2)
            projectm_pcm_add_int16(g_pm, pcm_data, sample_count * 2, 2);
#endif
    }
}

void viz_load_preset(const char *filename, int smooth) {
#ifdef USE_PROJECTM
    if (g_pm && filename) projectm_load_preset_file(g_pm, filename, smooth);
#endif
}

void viz_set_beat_sensitivity(float s) { g_settings.beat_sensitivity = s; }
void viz_set_preset_locked(int v) { (void)v; }
void viz_set_mode(VizMode m) { g_mode = m; }
VizMode viz_get_mode(void) { return g_mode; }
const char* viz_get_current_preset_name(void) { return "N/A"; }
int viz_get_preset_count(void) { return 0; }
float viz_get_fps(void) { return g_fps; }
