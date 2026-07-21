#ifndef UI_H
#define UI_H

#include "common.h"

// Initialize UI (debug text overlay)
YmpError ui_init(AppContext *ctx);

// Cleanup UI
void ui_cleanup(void);

// Render one frame of the UI
void ui_render(void);

// Handle input (controller)
void ui_handle_input(void);

// Update display info
void ui_update_now_playing(const TrackInfo *track);

// Show a message on screen
void ui_show_message(const char *msg, int duration_ms);

#endif // UI_H
