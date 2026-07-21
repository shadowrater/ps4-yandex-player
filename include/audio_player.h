#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "common.h"

// Initialize audio output
YmpError audio_init(void);

// Cleanup audio
void audio_cleanup(void);

// Play a track from URL
YmpError audio_play(const char *url);

// Play a decoded file
YmpError audio_play_file(const char *filename);

// Pause current playback
YmpError audio_pause(void);

// Resume playback
YmpError audio_resume(void);

// Stop playback
YmpError audio_stop(void);

// Set volume (0-100)
YmpError audio_set_volume(int volume);

// Get current state
PlayerState audio_get_state(void);

// Get playback position (ms)
int audio_get_position_ms(void);

// Get duration (ms)
int audio_get_duration_ms(void);

// Check if track finished
bool audio_is_finished(void);

// Get PCM data for visualizer (float interleaved stereo)
// Returns number of samples per channel decoded
int audio_get_pcm(float *buffer, int max_samples);

#endif // AUDIO_PLAYER_H
