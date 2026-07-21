#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "common.h"

// ─── Visualizer modes ──────────────────────────────────────────
typedef enum {
    VIZ_MILKDROP,       // projectM (Milkdrop) визуализация
    VIZ_SIMPLE,         // Простой спектроанализатор
    VIZ_OFF             // Без визуализации
} VizMode;

// ─── Settings ──────────────────────────────────────────────────
typedef struct {
    VizMode mode;
    int     width;          // Ширина рендера (1920 для PS4)
    int     height;         // Высота рендера (1080 для PS4)
    int     fps;            // Целевой FPS (60)
    int     mesh_x;         // Разрешение сетки X (48)
    int     mesh_y;         // Разрешение сетки Y (32)
    float   beat_sensitivity; // Чувствительность к биту (0.5-2.0)
    int     preset_duration;  // Смена пресета (сек)
    float   soft_cut_duration; // Плавная смена (сек)
    bool    hard_cut_enabled;  // Жёсткая смена
    float   hard_cut_sensitivity;
    char    preset_path[256];  // Путь к пресетам Milkdrop
    char    texture_path[256]; // Путь к текстурам
} VizSettings;

// ─── Public API ────────────────────────────────────────────────

// Инициализация визуализатора
// Возвращает 0 при успехе
int viz_init(const VizSettings *settings);

// Очистка ресурсов
void viz_cleanup(void);

// Рендер одного кадра
// pcm_data - сырые PCM данные (int16 или float)
// sample_count - количество сэмплов (на канал)
// channels - количество каналов (1=mono, 2=stereo)
void viz_render_frame(const void *pcm_data, int sample_count, int channels);

// Загрузка пресета по имени файла
void viz_load_preset(const char *filename, bool smooth);

// Следующий/предыдущий пресет
void viz_next_preset(bool smooth);
void viz_prev_preset(bool smooth);

// Управление
void viz_set_beat_sensitivity(float sensitivity);
void viz_set_preset_locked(bool locked);
void viz_set_mode(VizMode mode);

// Информация
VizMode viz_get_mode(void);
const char* viz_get_current_preset_name(void);
int viz_get_preset_count(void);

// Получить FPS рендера
float viz_get_fps(void);

#endif // VISUALIZER_H
