#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "common.h"

typedef enum {
    VIZ_MILKDROP,
    VIZ_SIMPLE,
    VIZ_OFF
} VizMode;

typedef struct {
    VizMode mode;
    int width, height, fps;
    int mesh_x, mesh_y;
    float beat_sensitivity;
    int preset_duration;
    float soft_cut_duration;
    int hard_cut_enabled;
    float hard_cut_sensitivity;
    char preset_path[256];
    char texture_path[256];
} VizSettings;

int viz_init(const VizSettings *settings);
void viz_cleanup(void);
void viz_render_frame(const void *pcm_data, int sample_count, int channels);
void viz_load_preset(const char *filename, int smooth);
void viz_set_beat_sensitivity(float s);
void viz_set_preset_locked(int v);
void viz_set_mode(VizMode m);
VizMode viz_get_mode(void);
const char* viz_get_current_preset_name(void);
int viz_get_preset_count(void);
float viz_get_fps(void);

#endif
