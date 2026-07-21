#include "common.h"
#include "visualizer.h"

// ═══════════════════════════════════════════════════════════════
//  Milkdrop Visualizer для PS4
//  Использует projectM (libprojectM) + OpenGL ES (piglet)
// ═══════════════════════════════════════════════════════════════

#ifdef __ORBIS__
#include <orbis/Pigletv2VSH.h>   // OpenGL ES для PS4
#include <orbis/VideoOut.h>
#endif

// ─── projectM C API (из libprojectM) ──────────────────────────
// Если projectM не скомпилирован, используем заглушку

#ifdef USE_PROJECTM
#include <projectM-4/projectM.h>
#include <projectM-4/parameters.h>
#include <projectM-4/render_opengl.h>
#else
// ─── Заглушка (stub) когда projectM нет ──────────────────────
typedef void* projectm_handle;
typedef void* projectm_playlist_handle;

// Прототипы заглушек
static projectm_handle projectm_create(void) { return (void*)1; }
static void projectm_destroy(projectm_handle h) { (void)h; }
static void projectm_set_window_size(projectm_handle h, int w, int h) { (void)h; (void)w; (void)h; }
static void projectm_set_fps(projectm_handle h, int fps) { (void)h; (void)fps; }
static void projectm_set_mesh_size(projectm_handle h, int x, int y) { (void)h; (void)x; (void)y; }
static void projectm_set_aspect_correction(projectm_handle h, bool v) { (void)h; (void)v; }
static void projectm_set_preset_duration(projectm_handle h, double v) { (void)h; (void)v; }
static void projectm_set_soft_cut_duration(projectm_handle h, double v) { (void)h; (void)v; }
static void projectm_set_hard_cut_enabled(projectm_handle h, bool v) { (void)h; (void)v; }
static void projectm_set_hard_cut_sensitivity(projectm_handle h, float v) { (void)h; (void)v; }
static void projectm_set_beat_sensitivity(projectm_handle h, float v) { (void)h; (void)v; }
static void projectm_set_preset_locked(projectm_handle h, bool v) { (void)h; (void)v; }
static void projectm_load_preset_file(projectm_handle h, const char* f, bool s) { (void)h; (void)f; (void)s; }
static void projectm_opengl_render_frame(projectm_handle h) { (void)h; }
static void projectm_pcm_add_float(projectm_handle h, const float* s, int n, int c) { (void)h; (void)s; (void)n; (void)c; }
static void projectm_pcm_add_int16(projectm_handle h, const short* s, int n, int c) { (void)h; (void)s; (void)n; (void)c; }
#endif

// ─── Простой визуализатор (спектроанализатор без projectM) ────

#ifdef __ORBIS__
#include <orbis/VideoOut.h>
#endif

#define SIMPLE_BARS 64
#define SIMPLE_FFT_SIZE 128

// FFT через DFT (упрощённый, для примера)
static void simple_dft(const float *input, float *output, int n) {
    for (int k = 0; k < n / 2; k++) {
        float real = 0, imag = 0;
        for (int t = 0; t < n; t++) {
            float angle = 2.0f * 3.14159265f * k * t / n;
            real += input[t] * cosf(angle);
            imag -= input[t] * sinf(angle);
        }
        output[k] = sqrtf(real * real + imag * imag) / n;
    }
}

static float g_simple_bars[SIMPLE_BARS] = {0};
static float g_simple_fft[SIMPLE_FFT_SIZE / 2] = {0};
static float g_simple_input[SIMPLE_FFT_SIZE] = {0};
static int   g_simple_pos = 0;

static void simple_update_bars(const float *samples, int count) {
    // Добавляем сэмплы во входной буфер
    for (int i = 0; i < count && g_simple_pos < SIMPLE_FFT_SIZE; i++) {
        g_simple_input[g_simple_pos++] = samples[i];
    }

    // Когда буфер заполнен — считаем DFT
    if (g_simple_pos >= SIMPLE_FFT_SIZE) {
        simple_dft(g_simple_input, g_simple_fft, SIMPLE_FFT_SIZE);

        // Группируем в 64 полосы
        int bin_per_bar = (SIMPLE_FFT_SIZE / 2) / SIMPLE_BARS;
        for (int i = 0; i < SIMPLE_BARS; i++) {
            float sum = 0;
            for (int j = 0; j < bin_per_bar; j++) {
                sum += g_simple_fft[i * bin_per_bar + j];
            }
            float val = sum / bin_per_bar;

            // Сглаживание
            if (val > g_simple_bars[i])
                g_simple_bars[i] = val;
            else
                g_simple_bars[i] = g_simple_bars[i] * 0.85f + val * 0.15f;
        }

        g_simple_pos = 0;
    }
}

static void simple_render(int width, int height) {
    // PS4 framebuffer рендер (упрощённый)
    // В реальном проекте используем sceDisplay или piglet
#ifdef __ORBIS__
    // Здесь был бы рендер полос на framebuffer
    // Для демонстрации — просто printf
    printf("\033[2J\033[H"); // clear
    for (int y = 0; y < 20; y++) {
        printf("  ");
        for (int i = 0; i < SIMPLE_BARS; i++) {
            int bar_height = (int)(g_simple_bars[i] * 40.0f);
            if (bar_height > 19 - y)
                printf("█");
            else
                printf(" ");
        }
        printf("\n");
    }
#else
    // PC mode — ASCII art
    printf("\033[2J\033[H");
    printf("  ┌─ Simple Spectrum Analyzer ─┐\n");
    for (int y = 0; y < 16; y++) {
        printf("  │");
        for (int i = 0; i < SIMPLE_BARS; i++) {
            int bar_height = (int)(g_simple_bars[i] * 32.0f);
            if (bar_height > 15 - y)
                printf("▓");
            else
                printf(" ");
        }
        printf("│\n");
    }
    printf("  └────────────────────────────┘\n");
#endif
}

// ─── Static state ──────────────────────────────────────────────

static VizSettings  g_settings;
static VizMode      g_mode = VIZ_OFF;
static bool         g_initialized = false;

#ifdef USE_PROJECTM
static projectm_handle g_pm = NULL;
static projectm_playlist_handle g_playlist = NULL;
static float g_pcm_float_buf[2048 * 2] = {0}; // stereo
#endif

static float g_fps = 0;
static int   g_frame_count = 0;
static uint64_t g_last_fps_time = 0;

// ─── OpenGL ES Init (PS4 piglet) ──────────────────────────────

#ifdef __ORBIS__
static void init_opengl_es(void) {
    // Загружаем модуль piglet
    sceSysmoduleLoadModule(SCE_SYSMODULE_PIGLET);

    // Инициализация OpenGL ES 2.0
    // На PS4 piglet предоставляет EGL-подобный контекст

    // Настройка viewport
    glViewport(0, 0, g_settings.width, g_settings.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    printf("[Viz] OpenGL ES initialized (%dx%d)\n", g_settings.width, g_settings.height);
}
#endif

// ─── Public API ────────────────────────────────────────────────

int viz_init(const VizSettings *settings) {
    if (!settings) return -1;

    memcpy(&g_settings, settings, sizeof(VizSettings));
    g_mode = settings->mode;

    printf("[Viz] Initializing mode: %d\n", g_mode);

    if (g_mode == VIZ_SIMPLE) {
        memset(g_simple_bars, 0, sizeof(g_simple_bars));
        memset(g_simple_fft, 0, sizeof(g_simple_fft));
        memset(g_simple_input, 0, sizeof(g_simple_input));
        g_simple_pos = 0;

#ifdef __ORBIS__
        init_opengl_es();
#endif

        g_initialized = true;
        printf("[Viz] Simple visualizer ready\n");
        return 0;
    }

    if (g_mode == VIZ_MILKDROP) {
#ifdef USE_PROJECTM
#ifdef __ORBIS__
        init_opengl_es();
#endif

        // Создаём projectM instance
        g_pm = projectm_create();
        if (!g_pm) {
            printf("[Viz] Failed to create projectM\n");
            return -1;
        }

        // Настройки
        projectm_set_window_size(g_pm, settings->width, settings->height);
        projectm_set_fps(g_pm, settings->fps);
        projectm_set_mesh_size(g_pm, settings->mesh_x, settings->mesh_y);
        projectm_set_aspect_correction(g_pm, true);
        projectm_set_preset_duration(g_pm, settings->preset_duration);
        projectm_set_soft_cut_duration(g_pm, settings->soft_cut_duration);
        projectm_set_hard_cut_enabled(g_pm, settings->hard_cut_enabled);
        projectm_set_hard_cut_sensitivity(g_pm, settings->hard_cut_sensitivity);
        projectm_set_beat_sensitivity(g_pm, settings->beat_sensitivity);

        // Загружаем пресеты из папки
        if (settings->preset_path[0]) {
            // projectM автоматически сканирует папки
            printf("[Viz] Preset path: %s\n", settings->preset_path);
        }

        // Создаём плейлист пресетов
        g_playlist = projectm_playlist_create(g_pm);
        if (g_playlist) {
            projectm_playlist_set_shuffle(g_playlist, true);

            // Добавляем пресеты из папки
            if (settings->preset_path[0]) {
                projectm_playlist_add_path(g_playlist, settings->preset_path, true, false);
            }

            // Начинаем воспроизведение
            projectm_playlist_play_next(g_playlist, false);
        }

        g_initialized = true;
        printf("[Viz] Milkdrop visualizer ready (projectM)\n");
        return 0;
#else
        printf("[Viz] projectM not compiled! Falling back to simple mode\n");
        g_mode = VIZ_SIMPLE;
        return viz_init(settings); // Рекурсивно с простым режимом
#endif
    }

    if (g_mode == VIZ_OFF) {
        g_initialized = true;
        return 0;
    }

    return -1;
}

void viz_cleanup(void) {
    if (!g_initialized) return;

#ifdef USE_PROJECTM
    if (g_playlist) {
        // projectm_playlist_destroy(g_playlist); // Если доступно
        g_playlist = NULL;
    }
    if (g_pm) {
        projectm_destroy(g_pm);
        g_pm = NULL;
    }
#endif

    g_initialized = false;
    g_mode = VIZ_OFF;
    printf("[Viz] Cleanup done\n");
}

void viz_render_frame(const void *pcm_data, int sample_count, int channels) {
    if (!g_initialized || g_mode == VIZ_OFF) return;

    // Обновляем FPS
    g_frame_count++;
#ifdef __ORBIS__
    uint64_t now;
    sceRtcGetCurrentTick(&now);
    if (now - g_last_fps_time > 1000000) { // 1 секунда
        g_fps = g_frame_count * 1000000.0f / (now - g_last_fps_time);
        g_frame_count = 0;
        g_last_fps_time = now;
    }
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t now = tv.tv_sec * 1000000ULL + tv.tv_usec;
    if (now - g_last_fps_time > 1000000) {
        g_fps = g_frame_count * 1000000.0f / (now - g_last_fps_time);
        g_frame_count = 0;
        g_last_fps_time = now;
    }
#endif

    if (g_mode == VIZ_SIMPLE) {
        // Преобразуем int16 в float для анализа
        const int16_t *samples_i16 = (const int16_t *)pcm_data;
        float samples_float[2048];
        int mono_count = sample_count;
        if (channels == 2) mono_count = sample_count / 2;

        for (int i = 0; i < mono_count && i < 2048; i++) {
            if (channels == 2)
                samples_float[i] = (samples_i16[i * 2] + samples_i16[i * 2 + 1]) / 2.0f / 32768.0f;
            else
                samples_float[i] = samples_i16[i] / 32768.0f;
        }

        simple_update_bars(samples_float, mono_count);
        simple_render(g_settings.width, g_settings.height);
        return;
    }

    if (g_mode == VIZ_MILKDROP) {
#ifdef USE_PROJECTM
        // Конвертируем PCM в float если нужно
        if (channels == 2) {
            projectm_pcm_add_int16(g_pm, pcm_data, sample_count * 2, 2);
        } else {
            // Mono → stereo дублирование
            const int16_t *mono = (const int16_t *)pcm_data;
            for (int i = 0; i < sample_count && i < 1024; i++) {
                g_pcm_float_buf[i * 2] = mono[i] / 32768.0f;
                g_pcm_float_buf[i * 2 + 1] = mono[i] / 32768.0f;
            }
            projectm_pcm_add_float(g_pm, g_pcm_float_buf, sample_count, 2);
        }

        // Рендерим кадр
#ifdef __ORBIS__
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
        projectm_opengl_render_frame(g_pm);
#endif
    }
}

void viz_load_preset(const char *filename, bool smooth) {
    if (!g_initialized) return;

#ifdef USE_PROJECTM
    if (g_pm && filename) {
        projectm_load_preset_file(g_pm, filename, smooth);
        printf("[Viz] Loaded preset: %s\n", filename);
    }
#else
    printf("[Viz] projectM not available\n");
#endif
}

void viz_next_preset(bool smooth) {
    if (!g_initialized) return;

#ifdef USE_PROJECTM
    if (g_playlist) {
        projectm_playlist_play_next(g_playlist, smooth);
    }
#endif
}

void viz_prev_preset(bool smooth) {
    if (!g_initialized) return;

#ifdef USE_PROJECTM
    if (g_playlist) {
        projectm_playlist_play_prev(g_playlist, smooth);
    }
#endif
}

void viz_set_beat_sensitivity(float sensitivity) {
    g_settings.beat_sensitivity = sensitivity;
#ifdef USE_PROJECTM
    if (g_pm) {
        projectm_set_beat_sensitivity(g_pm, sensitivity);
    }
#endif
}

void viz_set_preset_locked(bool locked) {
#ifdef USE_PROJECTM
    if (g_pm) {
        projectm_set_preset_locked(g_pm, locked);
    }
#endif
}

void viz_set_mode(VizMode mode) {
    if (mode == g_mode) return;

    VizSettings saved = g_settings;
    viz_cleanup();
    saved.mode = mode;
    viz_init(&saved);
}

VizMode viz_get_mode(void) {
    return g_mode;
}

const char* viz_get_current_preset_name(void) {
#ifdef USE_PROJECTM
    if (g_playlist) {
        return projectm_playlist_get_filename(g_playlist,
            projectm_playlist_get_position(g_playlist));
    }
#endif
    return "N/A";
}

int viz_get_preset_count(void) {
#ifdef USE_PROJECTM
    if (g_playlist) {
        return projectm_playlist_size(g_playlist);
    }
#endif
    return 0;
}

float viz_get_fps(void) {
    return g_fps;
}
