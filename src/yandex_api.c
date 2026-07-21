#include "common.h"
#include "yandex_api.h"
#include "cjson/cJSON.h"

// ─── Static state ──────────────────────────────────────────────

static char  g_oauth_token[512] = {0};
static char  g_uid[32]          = {0};
static bool  g_initialized      = false;

#ifdef __ORBIS__
static int   g_http_user_agent  = -1;
static int   g_http_template    = -1;
#endif

// ─── Helpers ───────────────────────────────────────────────────

static YmpError ensure_init(void) {
    if (g_initialized) return YMP_OK;

#ifdef __ORBIS__
    // Load HTTP module
    int ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);
    if (ret < 0) return YMP_ERR_NETWORK;

    ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
    if (ret < 0) return YMP_ERR_NETWORK;

    // Create HTTP user agent
    ret = sceHttpCreateUserAgent("YandexMusicPS4/1.0 libhttp/3.50", &g_http_user_agent);
    if (ret < 0) return YMP_ERR_NETWORK;

    // Create template with TLS
    ret = sceHttpCreateTemplate(
        g_http_user_agent,
        "YandexMusicPS4/1.0",
        SCE_HTTP_HTTP_VERSION_1_1,
        SCE_HTTP_SSL_ALWAYS,
        &g_http_template
    );
    if (ret < 0) return YMP_ERR_NETWORK;

    // Set timeouts
    sceHttpSetResolveTimeOut(g_http_template, 5 * 1000000); // 5s in microseconds
    sceHttpSetConnectTimeOut(g_http_template, 5 * 1000000);
    sceHttpSetSendTimeOut(g_http_template, 30 * 1000000);
    sceHttpSetRecvTimeOut(g_http_template, 30 * 1000000);

    g_initialized = true;
#else
    // PC simulation mode (for testing)
    g_initialized = true;
#endif

    return YMP_OK;
}

// Make an HTTP GET request and return response body
static char* http_get(const char *url) {
#ifdef __ORBIS__
    int conn_id = -1;
    int ret = sceHttpCreateConnection(g_http_template, url, SCE_HTTP_DEFAULT_PORT, SCE_HTTP_SSL_ALWAYS, &conn_id);
    if (ret < 0) return NULL;

    int req_id = -1;
    ret = sceHttpCreateRequest(conn_id, SCE_HTTP_METHOD_GET, url, 0, &req_id);
    if (ret < 0) { sceHttpDeleteConnection(conn_id); return NULL; }

    ret = sceHttpSendRequest(req_id, NULL, 0);
    if (ret < 0) { sceHttpDeleteRequest(req_id); sceHttpDeleteConnection(conn_id); return NULL; }

    // Read response
    uint64_t content_len = 0;
    sceHttpGetContentLength(req_id, &content_len);

    // Allocate buffer (max 1MB)
    int buf_size = (content_len > 0 && content_len < 1048576) ? (int)content_len : 1048576;
    char *buf = (char *)malloc(buf_size);
    if (!buf) { sceHttpDeleteRequest(req_id); sceHttpDeleteConnection(conn_id); return NULL; }

    int total_read = 0;
    while (total_read < buf_size - 1) {
        int read = sceHttpReadData(req_id, buf + total_read, buf_size - 1 - total_read);
        if (read <= 0) break;
        total_read += read;
    }
    buf[total_read] = '\0';

    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);

    return buf;
#else
    // PC stub - returns empty
    (void)url;
    return strdup("{}");
#endif
}

// Make an HTTP GET with custom headers
static char* http_get_auth(const char *url, const char *extra_headers) {
#ifdef __ORBIS__
    int conn_id = -1;
    int ret = sceHttpCreateConnection(g_http_template, url, SCE_HTTP_DEFAULT_PORT, SCE_HTTP_SSL_ALWAYS, &conn_id);
    if (ret < 0) return NULL;

    int req_id = -1;
    ret = sceHttpCreateRequest(conn_id, SCE_HTTP_METHOD_GET, url, 0, &req_id);
    if (ret < 0) { sceHttpDeleteConnection(conn_id); return NULL; }

    // Add Authorization header
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: %s", extra_headers);
    sceHttpAddRequestHeader(req_id, auth_header, SCE_HTTP_HEADER_OVERWRITE);

    ret = sceHttpSendRequest(req_id, NULL, 0);
    if (ret < 0) { sceHttpDeleteRequest(req_id); sceHttpDeleteConnection(conn_id); return NULL; }

    uint64_t content_len = 0;
    sceHttpGetContentLength(req_id, &content_len);

    int buf_size = (content_len > 0 && content_len < 1048576) ? (int)content_len : 1048576;
    char *buf = (char *)malloc(buf_size);
    if (!buf) { sceHttpDeleteRequest(req_id); sceHttpDeleteConnection(conn_id); return NULL; }

    int total_read = 0;
    while (total_read < buf_size - 1) {
        int read = sceHttpReadData(req_id, buf + total_read, buf_size - 1 - total_read);
        if (read <= 0) break;
        total_read += read;
    }
    buf[total_read] = '\0';

    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);

    return buf;
#else
    (void)url; (void)extra_headers;
    return strdup("{}");
#endif
}

// ─── API Functions ─────────────────────────────────────────────

YmpError yandex_api_init(const char *oauth_token, const char *uid) {
    if (!oauth_token || !uid) return YMP_ERR_AUTH;

    SAFE_STRCPY(g_oauth_token, oauth_token);
    SAFE_STRCPY(g_uid, uid);

    return ensure_init();
}

void yandex_api_cleanup(void) {
#ifdef __ORBIS__
    if (g_http_template >= 0)  { sceHttpDeleteTemplate(g_http_template);  g_http_template = -1; }
    if (g_http_user_agent >= 0) { sceHttpDeleteUserAgent(g_http_user_agent); g_http_user_agent = -1; }
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
#endif
    g_initialized = false;
}

// ─── Get liked tracks ──────────────────────────────────────────

YmpError yandex_api_get_liked_tracks(Playlist *out) {
    if (!out) return YMP_ERR_MEMORY;

    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url),
        "%s/users/%s/likes/tracks",
        YANDEX_API_BASE, g_uid
    );

    char auth[1024];
    snprintf(auth, sizeof(auth), "OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    // Parse result.library.tracks
    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *library = result ? cJSON_GetObjectItem(result, "library") : NULL;
    cJSON *tracks = library ? cJSON_GetObjectItem(library, "tracks") : NULL;

    if (!tracks || !cJSON_IsArray(tracks)) {
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    int count = cJSON_GetArraySize(tracks);
    out->tracks = (TrackInfo *)calloc(count, sizeof(TrackInfo));
    if (!out->tracks) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    SAFE_STRCPY(out->id, "liked");
    SAFE_STRCPY(out->name, "Любимые треки");
    out->track_count = count;

    for (int i = 0; i < count && i < MAX_TRACKS; i++) {
        cJSON *item = cJSON_GetArrayItem(tracks, i);
        cJSON *id_obj = cJSON_GetObjectItem(item, "id");
        if (id_obj && cJSON_IsString(id_obj)) {
            SAFE_STRCPY(out->tracks[i].id, id_obj->valuestring);
        } else if (id_obj && cJSON_IsNumber(id_obj)) {
            snprintf(out->tracks[i].id, sizeof(out->tracks[i].id), "%d", id_obj->valueint);
        }
    }

    cJSON_Delete(json);

    // Fetch track details for each (batch request)
    // Yandex allows batch: /tracks?trackIds=1,2,3&with-positions=true
    // Build comma-separated IDs
    char ids_param[4096] = {0};
    int pos = 0;
    for (int i = 0; i < count && i < MAX_TRACKS; i++) {
        if (i > 0 && pos < sizeof(ids_param) - 1) {
            ids_param[pos++] = ',';
        }
        int written = snprintf(ids_param + pos, sizeof(ids_param) - pos, "%s", out->tracks[i].id);
        if (written > 0) pos += written;
    }

    char detail_url[MAX_URL_LEN];
    snprintf(detail_url, sizeof(detail_url),
        "%s/tracks?trackIds=%s&with-positions=true",
        YANDEX_API_BASE, ids_param
    );

    char *detail_resp = http_get_auth(detail_url, auth);
    if (detail_resp) {
        cJSON *djson = cJSON_Parse(detail_resp);
        free(detail_resp);
        if (djson) {
            cJSON *dresult = cJSON_GetObjectItem(djson, "result");
            cJSON *jtracks = dresult ? cJSON_GetObjectItem(dresult, "library") : NULL;
            jtracks = jtracks ? cJSON_GetObjectItem(jtracks, "tracks") : jtracks;

            // Fallback: try "result" as array
            if (!jtracks || !cJSON_IsArray(jtracks)) {
                jtracks = dresult;
            }

            if (jtracks && cJSON_IsArray(jtracks)) {
                int n = cJSON_GetArraySize(jtracks);
                for (int i = 0; i < n && i < count; i++) {
                    cJSON *t = cJSON_GetArrayItem(jtracks, i);
                    cJSON *title = cJSON_GetObjectItem(t, "title");
                    cJSON *artist_arr = cJSON_GetObjectItem(t, "artists");
                    cJSON *duration = cJSON_GetObjectItem(t, "durationMs");

                    if (title && cJSON_IsString(title)) {
                        SAFE_STRCPY(out->tracks[i].title, title->valuestring);
                    }
                    if (artist_arr && cJSON_IsArray(artist_arr) && cJSON_GetArraySize(artist_arr) > 0) {
                        cJSON *first_artist = cJSON_GetArrayItem(artist_arr, 0);
                        cJSON *aname = cJSON_GetObjectItem(first_artist, "name");
                        if (aname && cJSON_IsString(aname)) {
                            SAFE_STRCPY(out->tracks[i].artist, aname->valuestring);
                        }
                    }
                    if (duration && cJSON_IsNumber(duration)) {
                        out->tracks[i].duration_ms = duration->valueint;
                    }
                }
            }
            cJSON_Delete(djson);
        }
    }

    return YMP_OK;
}

// ─── Get track stream URL ──────────────────────────────────────

YmpError yandex_api_get_track_url(const char *track_id, char *url_out, int url_max) {
    if (!track_id || !url_out) return YMP_ERR_MEMORY;

    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url),
        "%s/track/%s/download-info",
        YANDEX_API_BASE, track_id
    );

    char auth[1024];
    snprintf(auth, sizeof(auth), "OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    if (!result) { cJSON_Delete(json); return YMP_ERR_API; }

    // Get the first download-info entry
    cJSON *first = NULL;
    if (cJSON_IsArray(result)) {
        first = cJSON_GetArrayItem(result, 0);
    } else {
        first = result;
    }

    if (!first) { cJSON_Delete(json); return YMP_ERR_NOT_FOUND; }

    cJSON *download_info_url = cJSON_GetObjectItem(first, "downloadInfoUrl");
    if (!download_info_url || !cJSON_IsString(download_info_url)) {
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    // Get the actual download URL (need to fetch downloadInfoUrl first)
    char *info_resp = http_get_auth(download_info_url->valuestring, auth);
    if (!info_resp) { cJSON_Delete(json); return YMP_ERR_NETWORK; }

    cJSON *info_json = cJSON_Parse(info_resp);
    free(info_resp);
    if (!info_json) { cJSON_Delete(json); return YMP_ERR_API; }

    cJSON *s3_url = cJSON_GetObjectItem(info_json, "s3Url");
    if (!s3_url || !cJSON_IsString(s3_url)) {
        cJSON_Delete(info_json);
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    SAFE_STRCPY(url_out, s3_url->valuestring);
    cJSON_Delete(info_json);
    cJSON_Delete(json);

    return YMP_OK;
}

// ─── Get user playlists ────────────────────────────────────────

YmpError yandex_api_get_playlists(Playlist **out, int *count) {
    if (!out || !count) return YMP_ERR_MEMORY;

    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url),
        "%s/users/%s/playlists/list",
        YANDEX_API_BASE, g_uid
    );

    char auth[1024];
    snprintf(auth, sizeof(auth), "OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    if (!result || !cJSON_IsArray(result)) {
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    int n = cJSON_GetArraySize(result);
    *out = (Playlist *)calloc(n, sizeof(Playlist));
    if (!*out) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    *count = n;

    for (int i = 0; i < n; i++) {
        cJSON *pl = cJSON_GetArrayItem(result, i);
        cJSON *kind = cJSON_GetObjectItem(pl, "kind");
        cJSON *title = cJSON_GetObjectItem(pl, "title");
        cJSON *track_count = cJSON_GetObjectItem(pl, "trackCount");

        if (kind && cJSON_IsNumber(kind)) {
            snprintf((*out)[i].id, sizeof((*out)[i].id), "%d", kind->valueint);
        }
        if (title && cJSON_IsString(title)) {
            SAFE_STRCPY((*out)[i].name, title->valuestring);
        }
        if (track_count && cJSON_IsNumber(track_count)) {
            (*out)[i].track_count = track_count->valueint;
        }
    }

    cJSON_Delete(json);
    return YMP_OK;
}

// ─── Search ────────────────────────────────────────────────────

YmpError yandex_api_search(const char *query, TrackInfo **out, int *count) {
    if (!query || !out || !count) return YMP_ERR_MEMORY;

    // URL encode query (simple implementation)
    char encoded[MAX_URL_LEN * 2] = {0};
    int epos = 0;
    for (const char *p = query; *p && epos < sizeof(encoded) - 3; p++) {
        if (*p == ' ') {
            encoded[epos++] = '%';
            encoded[epos++] = '2';
            encoded[epos++] = '0';
        } else if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                   (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.') {
            encoded[epos++] = *p;
        }
    }

    char url[MAX_URL_LEN * 2];
    snprintf(url, sizeof(url),
        "%s/search?type=track&text=%s&page=0&nococrrect=false",
        YANDEX_API_BASE, encoded
    );

    char auth[1024];
    snprintf(auth, sizeof(auth), "OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    // Navigate: result.tracks.results
    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *tracks_obj = result ? cJSON_GetObjectItem(result, "tracks") : NULL;
    cJSON *results = tracks_obj ? cJSON_GetObjectItem(tracks_obj, "results") : NULL;

    if (!results || !cJSON_IsArray(results)) {
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    int n = cJSON_GetArraySize(results);
    *out = (TrackInfo *)calloc(n, sizeof(TrackInfo));
    if (!*out) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    *count = n;

    for (int i = 0; i < n; i++) {
        cJSON *t = cJSON_GetArrayItem(results, i);

        cJSON *id = cJSON_GetObjectItem(t, "id");
        cJSON *title = cJSON_GetObjectItem(t, "title");
        cJSON *artist_arr = cJSON_GetObjectItem(t, "artists");
        cJSON *duration = cJSON_GetObjectItem(t, "durationMs");

        if (id && cJSON_IsString(id)) {
            SAFE_STRCPY((*out)[i].id, id->valuestring);
        }
        if (title && cJSON_IsString(title)) {
            SAFE_STRCPY((*out)[i].title, title->valuestring);
        }
        if (artist_arr && cJSON_IsArray(artist_arr) && cJSON_GetArraySize(artist_arr) > 0) {
            cJSON *first_artist = cJSON_GetArrayItem(artist_arr, 0);
            cJSON *aname = cJSON_GetObjectItem(first_artist, "name");
            if (aname && cJSON_IsString(aname)) {
                SAFE_STRCPY((*out)[i].artist, aname->valuestring);
            }
        }
        if (duration && cJSON_IsNumber(duration)) {
            (*out)[i].duration_ms = duration->valueint;
        }
    }

    cJSON_Delete(json);
    return YMP_OK;
}

// ─── Recommendations ───────────────────────────────────────────

YmpError yandex_api_get_recommendations(Playlist *out) {
    if (!out) return YMP_ERR_MEMORY;

    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url),
        "%s/users/%s/state/recommendations",
        YANDEX_API_BASE, g_uid
    );

    char auth[1024];
    snprintf(auth, sizeof(auth), "OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *tracks = result ? cJSON_GetObjectItem(result, "tracks") : NULL;

    if (!tracks || !cJSON_IsArray(tracks)) {
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    int n = cJSON_GetArraySize(tracks);
    out->tracks = (TrackInfo *)calloc(n, sizeof(TrackInfo));
    if (!out->tracks) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    SAFE_STRCPY(out->id, "recommendations");
    SAFE_STRCPY(out->name, "Рекомендации");
    out->track_count = n;

    for (int i = 0; i < n; i++) {
        cJSON *t = cJSON_GetArrayItem(tracks, i);
        cJSON *id = cJSON_GetObjectItem(t, "id");
        cJSON *title = cJSON_GetObjectItem(t, "title");

        if (id && cJSON_IsString(id)) {
            SAFE_STRCPY(out->tracks[i].id, id->valuestring);
        }
        if (title && cJSON_IsString(title)) {
            SAFE_STRCPY(out->tracks[i].title, title->valuestring);
        }
    }

    cJSON_Delete(json);
    return YMP_OK;
}

// ─── Get playlist tracks ──────────────────────────────────────

YmpError yandex_api_get_playlist_tracks(const char *playlist_id, Playlist *out) {
    if (!playlist_id || !out) return YMP_ERR_MEMORY;

    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url),
        "%s/users/%s/playlists/%s",
        YANDEX_API_BASE, g_uid, playlist_id
    );

    char auth[1024];
    snprintf(auth, sizeof(auth), "OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *revision = result ? cJSON_GetObjectItem(result, "revision") : NULL;
    cJSON *tracks = result ? cJSON_GetObjectItem(result, "tracks") : NULL;

    if (!tracks || !cJSON_IsArray(tracks)) {
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    int n = cJSON_GetArraySize(tracks);
    out->tracks = (TrackInfo *)calloc(n, sizeof(TrackInfo));
    if (!out->tracks) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    SAFE_STRCPY(out->id, playlist_id);
    out->track_count = n;

    // Extract track IDs for batch detail fetch
    char ids_param[4096] = {0};
    int pos = 0;

    for (int i = 0; i < n && i < MAX_TRACKS; i++) {
        cJSON *item = cJSON_GetArrayItem(tracks, i);
        cJSON *track = cJSON_GetObjectItem(item, "id");
        cJSON *album = cJSON_GetObjectItem(item, "album");

        if (track && cJSON_IsString(track)) {
            SAFE_STRCPY(out->tracks[i].id, track->valuestring);
            if (i > 0 && pos < sizeof(ids_param) - 1)
                ids_param[pos++] = ',';
            int w = snprintf(ids_param + pos, sizeof(ids_param) - pos, "%s", track->valuestring);
            if (w > 0) pos += w;
        }
        if (album && cJSON_IsString(album)) {
            SAFE_STRCPY(out->tracks[i].album, album->valuestring);
        }
    }

    // Batch fetch track details
    if (pos > 0) {
        char detail_url[MAX_URL_LEN];
        snprintf(detail_url, sizeof(detail_url),
            "%s/tracks?trackIds=%s&with-positions=true",
            YANDEX_API_BASE, ids_param
        );

        char *detail_resp = http_get_auth(detail_url, auth);
        if (detail_resp) {
            cJSON *djson = cJSON_Parse(detail_resp);
            free(detail_resp);
            if (djson) {
                cJSON *dresult = cJSON_GetObjectItem(djson, "result");
                cJSON *jtracks = dresult ? cJSON_GetObjectItem(dresult, "library") : NULL;
                jtracks = jtracks ? cJSON_GetObjectItem(jtracks, "tracks") : jtracks;
                if (!jtracks || !cJSON_IsArray(jtracks)) jtracks = dresult;

                if (jtracks && cJSON_IsArray(jtracks)) {
                    int m = cJSON_GetArraySize(jtracks);
                    for (int i = 0; i < m && i < n; i++) {
                        cJSON *t = cJSON_GetArrayItem(jtracks, i);
                        cJSON *title = cJSON_GetObjectItem(t, "title");
                        cJSON *artist_arr = cJSON_GetObjectItem(t, "artists");
                        cJSON *duration = cJSON_GetObjectItem(t, "durationMs");

                        if (title && cJSON_IsString(title))
                            SAFE_STRCPY(out->tracks[i].title, title->valuestring);
                        if (artist_arr && cJSON_IsArray(artist_arr) && cJSON_GetArraySize(artist_arr) > 0) {
                            cJSON *a = cJSON_GetArrayItem(artist_arr, 0);
                            cJSON *aname = cJSON_GetObjectItem(a, "name");
                            if (aname && cJSON_IsString(aname))
                                SAFE_STRCPY(out->tracks[i].artist, aname->valuestring);
                        }
                        if (duration && cJSON_IsNumber(duration))
                            out->tracks[i].duration_ms = duration->valueint;
                    }
                }
                cJSON_Delete(djson);
            }
        }
    }

    cJSON_Delete(json);
    return YMP_OK;
}

// ─── Landing (charts, new releases) ────────────────────────────

YmpError yandex_api_get_landing(Playlist **charts, int *chart_count) {
    if (!charts || !chart_count) return YMP_ERR_MEMORY;

    char auth[1024];
    snprintf(auth, sizeof(auth), "OAuth %s", g_oauth_token);

    char *resp = http_get_auth(YANDEX_API_BASE "/landing3", auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *blocks = result ? cJSON_GetObjectItem(result, "blocks") : NULL;

    if (!blocks || !cJSON_IsArray(blocks)) {
        cJSON_Delete(json);
        return YMP_ERR_API;
    }

    int n = cJSON_GetArraySize(blocks);
    *charts = (Playlist *)calloc(n, sizeof(Playlist));
    if (!*charts) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    *chart_count = n;

    for (int i = 0; i < n; i++) {
        cJSON *block = cJSON_GetArrayItem(blocks, i);
        cJSON *title = cJSON_GetObjectItem(block, "title");
        cJSON *entities = cJSON_GetObjectItem(block, "entities");

        if (title && cJSON_IsString(title)) {
            SAFE_STRCPY((*charts)[i].name, title->valuestring);
        }
        if (entities && cJSON_IsArray(entities)) {
            // Count tracks
            int tc = 0;
            int ne = cJSON_GetArraySize(entities);
            for (int j = 0; j < ne; j++) {
                cJSON *ent = cJSON_GetArrayItem(entities, j);
                cJSON *data = cJSON_GetObjectItem(ent, "data");
                if (data) tc++;
            }

            (*charts)[i].tracks = (TrackInfo *)calloc(tc, sizeof(TrackInfo));
            (*charts)[i].track_count = tc;

            int idx = 0;
            for (int j = 0; j < ne && idx < tc; j++) {
                cJSON *ent = cJSON_GetArrayItem(entities, j);
                cJSON *data = cJSON_GetObjectItem(ent, "data");
                if (!data) continue;

                cJSON *id = cJSON_GetObjectItem(data, "id");
                cJSON *ttitle = cJSON_GetObjectItem(data, "title");
                cJSON *artist_arr = cJSON_GetObjectItem(data, "artists");

                if (id && cJSON_IsString(id))
                    SAFE_STRCPY((*charts)[i].tracks[idx].id, id->valuestring);
                if (ttitle && cJSON_IsString(ttitle))
                    SAFE_STRCPY((*charts)[i].tracks[idx].title, ttitle->valuestring);
                if (artist_arr && cJSON_IsArray(artist_arr) && cJSON_GetArraySize(artist_arr) > 0) {
                    cJSON *a = cJSON_GetArrayItem(artist_arr, 0);
                    cJSON *aname = cJSON_GetObjectItem(a, "name");
                    if (aname && cJSON_IsString(aname))
                        SAFE_STRCPY((*charts)[i].tracks[idx].artist, aname->valuestring);
                }

                idx++;
            }
        }
    }

    cJSON_Delete(json);
    return YMP_OK;
}
