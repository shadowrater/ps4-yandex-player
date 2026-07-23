#include "common.h"
#include "yandex_api.h"
#include "cjson/cJSON.h"

// ═══════════════════════════════════════════════════════════════
//  Yandex Music API -简化版 для PS4
//  Использует sceHttp для HTTP запросов
// ═══════════════════════════════════════════════════════════════

static char  g_oauth_token[512] = {0};
static char  g_uid[32]          = {0};
static bool  g_initialized      = false;

#ifdef __ORBIS__
static int32_t g_http_template = -1;
#endif

static YmpError ensure_init(void) {
    if (g_initialized) return YMP_OK;
#ifdef __ORBIS__
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL);

    int32_t libnetMemId = 0, libhttpCtxId = 0, libsslCtxId = 0;
    sceNetInit();
    libnetMemId = sceNetPoolCreate("netPool", 4096, 0);
    libsslCtxId = sceSslInit(128 * 1024);
    libhttpCtxId = sceHttpInit(libnetMemId, libsslCtxId, 128 * 1024);

    g_http_template = sceHttpCreateTemplate(libhttpCtxId, "YandexMusicPS4/1.0", ORBIS_HTTP_VERSION_1_1, 0);
    sceHttpSetResolveTimeOut(g_http_template, 5000000);
    sceHttpSetConnectTimeOut(g_http_template, 5000000);
    sceHttpSetSendTimeOut(g_http_template, 30000000);
    sceHttpSetRecvTimeOut(g_http_template, 30000000);
#endif
    g_initialized = true;
    return YMP_OK;
}

static char* http_get_auth(const char *url, const char *auth_header) {
#ifdef __ORBIS__
    int32_t conn_id = sceHttpCreateConnectionWithURL(g_http_template, url, true);
    if (conn_id < 0) return NULL;

    int32_t req_id = sceHttpCreateRequestWithURL(conn_id, ORBIS_METHOD_GET, url, 0);
    if (req_id < 0) { sceHttpDeleteConnection(conn_id); return NULL; }

    if (auth_header)
        sceHttpAddRequestHeader(req_id, auth_header, 0);

    int ret = sceHttpSendRequest(req_id, NULL, 0);
    if (ret < 0) { sceHttpDeleteRequest(req_id); sceHttpDeleteConnection(conn_id); return NULL; }

    uint64_t content_len = 0;
    sceHttpGetResponseContentLength(req_id, &content_len);

    int buf_size = (content_len > 0 && content_len < 1048576) ? (int)content_len : 65536;
    char *buf = (char *)malloc(buf_size);
    if (!buf) { sceHttpDeleteRequest(req_id); sceHttpDeleteConnection(conn_id); return NULL; }

    int total = 0;
    while (total < buf_size - 1) {
        int read = sceHttpReadData(req_id, buf + total, buf_size - 1 - total);
        if (read <= 0) break;
        total += read;
    }
    buf[total] = '\0';

    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);
    return buf;
#else
    (void)url; (void)auth_header;
    return strdup("{}");
#endif
}

// ─── API Functions ─────────────────────────────────────────────

YmpError yandex_api_init(const char *token, const char *uid) {
    if (!token || !uid) return YMP_ERR_AUTH;
    SAFE_STRCPY(g_oauth_token, token);
    SAFE_STRCPY(g_uid, uid);
    return ensure_init();
}

void yandex_api_cleanup(void) { g_initialized = false; }

YmpError yandex_api_get_liked_tracks(Playlist *out) {
    if (!out) return YMP_ERR_MEMORY;
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/users/%s/likes/tracks", YANDEX_API_BASE, g_uid);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *library = result ? cJSON_GetObjectItem(result, "library") : NULL;
    cJSON *tracks = library ? cJSON_GetObjectItem(library, "tracks") : NULL;

    if (!tracks || !cJSON_IsArray(tracks)) { cJSON_Delete(json); return YMP_ERR_API; }

    int count = cJSON_GetArraySize(tracks);
    out->tracks = (TrackInfo *)calloc(count, sizeof(TrackInfo));
    if (!out->tracks) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    SAFE_STRCPY(out->id, "liked");
    SAFE_STRCPY(out->name, "Любимые треки");
    out->track_count = count;

    for (int i = 0; i < count && i < MAX_TRACKS; i++) {
        cJSON *item = cJSON_GetArrayItem(tracks, i);
        cJSON *id_obj = cJSON_GetObjectItem(item, "id");
        if (id_obj && cJSON_IsString(id_obj))
            SAFE_STRCPY(out->tracks[i].id, id_obj->valuestring);
        else if (id_obj && cJSON_IsNumber(id_obj))
            snprintf(out->tracks[i].id, sizeof(out->tracks[i].id), "%d", id_obj->valueint);
    }
    cJSON_Delete(json);
    return YMP_OK;
}

YmpError yandex_api_get_track_url(const char *track_id, char *url_out, int url_max) {
    if (!track_id || !url_out) return YMP_ERR_MEMORY;
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/tracks/%s/download-info", YANDEX_API_BASE, track_id);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    if (!result) { cJSON_Delete(json); return YMP_ERR_API; }

    cJSON *first = cJSON_IsArray(result) ? cJSON_GetArrayItem(result, 0) : result;
    if (!first) { cJSON_Delete(json); return YMP_ERR_NOT_FOUND; }

    cJSON *info_url = cJSON_GetObjectItem(first, "downloadInfoUrl");
    if (!info_url || !cJSON_IsString(info_url)) { cJSON_Delete(json); return YMP_ERR_API; }

    // Fetch download info
    char *info_resp = http_get_auth(info_url->valuestring, auth);
    if (!info_resp) { cJSON_Delete(json); return YMP_ERR_NETWORK; }

    cJSON *info_json = cJSON_Parse(info_resp);
    free(info_resp);
    if (!info_json) { cJSON_Delete(json); return YMP_ERR_API; }

    cJSON *s3_url = cJSON_GetObjectItem(info_json, "s3Url");
    if (!s3_url || !cJSON_IsString(s3_url)) {
        cJSON_Delete(info_json); cJSON_Delete(json); return YMP_ERR_API;
    }

    SAFE_STRCPY(url_out, s3_url->valuestring);
    cJSON_Delete(info_json);
    cJSON_Delete(json);
    return YMP_OK;
}

YmpError yandex_api_get_playlists(Playlist **out, int *count) {
    if (!out || !count) return YMP_ERR_MEMORY;
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/users/%s/playlists/list", YANDEX_API_BASE, g_uid);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    if (!result || !cJSON_IsArray(result)) { cJSON_Delete(json); return YMP_ERR_API; }

    int n = cJSON_GetArraySize(result);
    *out = (Playlist *)calloc(n, sizeof(Playlist));
    if (!*out) { cJSON_Delete(json); return YMP_ERR_MEMORY; }
    *count = n;

    for (int i = 0; i < n; i++) {
        cJSON *pl = cJSON_GetArrayItem(result, i);
        cJSON *kind = cJSON_GetObjectItem(pl, "kind");
        cJSON *title = cJSON_GetObjectItem(pl, "title");
        if (kind && cJSON_IsNumber(kind))
            snprintf((*out)[i].id, sizeof((*out)[i].id), "%d", kind->valueint);
        if (title && cJSON_IsString(title))
            SAFE_STRCPY((*out)[i].name, title->valuestring);
    }
    cJSON_Delete(json);
    return YMP_OK;
}

YmpError yandex_api_search(const char *query, TrackInfo **out, int *count) {
    if (!query || !out || !count) return YMP_ERR_MEMORY;
    char encoded[MAX_URL_LEN * 2] = {0};
    int epos = 0;
    for (const char *p = query; *p && epos < sizeof(encoded) - 3; p++) {
        if (*p == ' ') { encoded[epos++] = '%'; encoded[epos++] = '2'; encoded[epos++] = '0'; }
        else if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
            encoded[epos++] = *p;
    }

    char url[MAX_URL_LEN * 2];
    snprintf(url, sizeof(url), "%s/search?type=track&text=%s&page=0", YANDEX_API_BASE, encoded);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *tracks_obj = result ? cJSON_GetObjectItem(result, "tracks") : NULL;
    cJSON *results = tracks_obj ? cJSON_GetObjectItem(tracks_obj, "results") : NULL;

    if (!results || !cJSON_IsArray(results)) { cJSON_Delete(json); return YMP_ERR_API; }

    int n = cJSON_GetArraySize(results);
    *out = (TrackInfo *)calloc(n, sizeof(TrackInfo));
    if (!*out) { cJSON_Delete(json); return YMP_ERR_MEMORY; }
    *count = n;

    for (int i = 0; i < n; i++) {
        cJSON *t = cJSON_GetArrayItem(results, i);
        cJSON *id = cJSON_GetObjectItem(t, "id");
        cJSON *title = cJSON_GetObjectItem(t, "title");
        cJSON *artist_arr = cJSON_GetObjectItem(t, "artists");
        if (id && cJSON_IsString(id)) SAFE_STRCPY((*out)[i].id, id->valuestring);
        if (title && cJSON_IsString(title)) SAFE_STRCPY((*out)[i].title, title->valuestring);
        if (artist_arr && cJSON_IsArray(artist_arr) && cJSON_GetArraySize(artist_arr) > 0) {
            cJSON *a = cJSON_GetArrayItem(artist_arr, 0);
            cJSON *aname = cJSON_GetObjectItem(a, "name");
            if (aname && cJSON_IsString(aname)) SAFE_STRCPY((*out)[i].artist, aname->valuestring);
        }
    }
    cJSON_Delete(json);
    return YMP_OK;
}

YmpError yandex_api_get_recommendations(Playlist *out) {
    if (!out) return YMP_ERR_MEMORY;
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/users/%s/state/recommendations", YANDEX_API_BASE, g_uid);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *tracks = result ? cJSON_GetObjectItem(result, "tracks") : NULL;

    if (!tracks || !cJSON_IsArray(tracks)) { cJSON_Delete(json); return YMP_ERR_API; }

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
        if (id && cJSON_IsString(id)) SAFE_STRCPY(out->tracks[i].id, id->valuestring);
        if (title && cJSON_IsString(title)) SAFE_STRCPY(out->tracks[i].title, title->valuestring);
    }
    cJSON_Delete(json);
    return YMP_OK;
}

YmpError yandex_api_get_playlist_tracks(const char *playlist_id, Playlist *out) {
    if (!playlist_id || !out) return YMP_ERR_MEMORY;
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/users/%s/playlists/%s", YANDEX_API_BASE, g_uid, playlist_id);

    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: OAuth %s", g_oauth_token);

    char *resp = http_get_auth(url, auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *tracks = result ? cJSON_GetObjectItem(result, "tracks") : NULL;

    if (!tracks || !cJSON_IsArray(tracks)) { cJSON_Delete(json); return YMP_ERR_API; }

    int n = cJSON_GetArraySize(tracks);
    out->tracks = (TrackInfo *)calloc(n, sizeof(TrackInfo));
    if (!out->tracks) { cJSON_Delete(json); return YMP_ERR_MEMORY; }

    SAFE_STRCPY(out->id, playlist_id);
    out->track_count = n;

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(tracks, i);
        cJSON *id = cJSON_GetObjectItem(item, "id");
        if (id && cJSON_IsString(id)) SAFE_STRCPY(out->tracks[i].id, id->valuestring);
    }
    cJSON_Delete(json);
    return YMP_OK;
}

YmpError yandex_api_get_landing(Playlist **charts, int *chart_count) {
    if (!charts || !chart_count) return YMP_ERR_MEMORY;
    char auth[1024];
    snprintf(auth, sizeof(auth), "Authorization: OAuth %s", g_oauth_token);

    char *resp = http_get_auth(YANDEX_API_BASE "/landing3", auth);
    if (!resp) return YMP_ERR_NETWORK;

    cJSON *json = cJSON_Parse(resp);
    free(resp);
    if (!json) return YMP_ERR_API;

    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *blocks = result ? cJSON_GetObjectItem(result, "blocks") : NULL;

    if (!blocks || !cJSON_IsArray(blocks)) { cJSON_Delete(json); return YMP_ERR_API; }

    int n = cJSON_GetArraySize(blocks);
    *charts = (Playlist *)calloc(n, sizeof(Playlist));
    if (!*charts) { cJSON_Delete(json); return YMP_ERR_MEMORY; }
    *chart_count = n;

    for (int i = 0; i < n; i++) {
        cJSON *block = cJSON_GetArrayItem(blocks, i);
        cJSON *title = cJSON_GetObjectItem(block, "title");
        if (title && cJSON_IsString(title))
            SAFE_STRCPY((*charts)[i].name, title->valuestring);
    }
    cJSON_Delete(json);
    return YMP_OK;
}
