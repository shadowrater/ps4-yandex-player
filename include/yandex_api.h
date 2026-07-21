#ifndef YANDEX_API_H
#define YANDEX_API_H

#include "common.h"

// Initialize Yandex API (sets up HTTP client)
YmpError yandex_api_init(const char *oauth_token, const char *uid);

// Cleanup
void yandex_api_cleanup(void);

// Get user's playlists
YmpError yandex_api_get_playlists(Playlist **out, int *count);

// Get tracks in a playlist
YmpError yandex_api_get_playlist_tracks(const char *playlist_id, Playlist *out);

// Get track stream URL (for playback)
YmpError yandex_api_get_track_url(const char *track_id, char *url_out, int url_max);

// Search for tracks
YmpError yandex_api_search(const char *query, TrackInfo **out, int *count);

// Get liked tracks
YmpError yandex_api_get_liked_tracks(Playlist *out);

// Get daily mix / recommendations
YmpError yandex_api_get_recommendations(Playlist *out);

// Get landing page (charts, new releases, etc.)
YmpError yandex_api_get_landing(Playlist **charts, int *chart_count);

#endif // YANDEX_API_H
