#include "common.h"
#include "audio_player.h"
#include "decoder.h"

// ═══════════════════════════════════════════════════════════════
//  Audio Player - декодирование и воспроизведение
// ═══════════════════════════════════════════════════════════════

static PlayerState g_state       = PLAYER_STOPPED;
static int         g_volume      = 50;
static int         g_audio_port  = -1;
static char        g_current_url[MAX_URL_LEN] = {0};
static char        g_current_file[256] = {0};

static DecoderContext *g_decoder = NULL;
static float          *g_pcm_buffer = NULL;
static int             g_pcm_buf_size = 0;

#ifdef __ORBIS__
static OrbisPthread   g_play_thread = NULL;
static bool           g_thread_running = false;
#endif

#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_CHANNELS     2
#define AUDIO_BUFFER_SIZE  4096

#ifdef __ORBIS__
static void* audio_playback_thread(void *arg) {
    (void)arg;
    if (!g_decoder) { g_state = PLAYER_STOPPED; g_thread_running = false; return NULL; }

    AudioFormat fmt = decoder_get_format(g_decoder);
    float pcm_buf[AUDIO_BUFFER_SIZE * 2];
    int16_t *output_buf = (int16_t *)malloc(AUDIO_BUFFER_SIZE * 2 * sizeof(int16_t));
    if (!output_buf) { g_state = PLAYER_STOPPED; g_thread_running = false; return NULL; }

    int32_t vol = g_volume;
    sceAudioOutSetVolume(g_audio_port, 3, &vol);

    while (g_thread_running && g_state == PLAYER_PLAYING) {
        int samples = decoder_decode(g_decoder, pcm_buf, AUDIO_BUFFER_SIZE);
        if (samples <= 0) break;

        for (int i = 0; i < samples * AUDIO_CHANNELS; i++) {
            float s = pcm_buf[i];
            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;
            output_buf[i] = (int16_t)(s * 32767.0f);
        }

        if (g_pcm_buffer && g_pcm_buf_size > 0) {
            int n = samples * AUDIO_CHANNELS;
            if (n > g_pcm_buf_size) n = g_pcm_buf_size;
            for (int i = 0; i < n; i++) g_pcm_buffer[i] = pcm_buf[i];
        }

        sceAudioOutOutput(g_audio_port, output_buf);
        sceKernelSleep(1);
    }

    free(output_buf);
    g_state = PLAYER_STOPPED;
    g_thread_running = false;
    return NULL;
}
#endif

YmpError audio_init(void) {
#ifdef __ORBIS__
    int ret = sceAudioOutInit();
    if (ret != 0) return YMP_ERR_AUDIO;

    OrbisUserServiceUserId userId = 0;
    g_audio_port = sceAudioOutOpen(userId, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                                    256, AUDIO_SAMPLE_RATE,
                                    ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (g_audio_port <= 0) return YMP_ERR_AUDIO;
#endif

    g_state = PLAYER_STOPPED;
    g_pcm_buf_size = AUDIO_BUFFER_SIZE * 2;
    g_pcm_buffer = (float *)malloc(g_pcm_buf_size * sizeof(float));
    return YMP_OK;
}

void audio_cleanup(void) {
    audio_stop();
    if (g_decoder) { decoder_close(g_decoder); g_decoder = NULL; }
    if (g_pcm_buffer) { free(g_pcm_buffer); g_pcm_buffer = NULL; }
#ifdef __ORBIS__
    if (g_audio_port > 0) { sceAudioOutClose(g_audio_port); g_audio_port = -1; }
#endif
}

YmpError audio_play(const char *url) {
    if (!url) return YMP_ERR_MEMORY;
    SAFE_STRCPY(g_current_url, url);
    return YMP_OK;
}

YmpError audio_play_file(const char *filename) {
    if (!filename) return YMP_ERR_MEMORY;
    audio_stop();
    if (g_decoder) { decoder_close(g_decoder); g_decoder = NULL; }

    g_decoder = decoder_open(filename);
    if (!g_decoder) return YMP_ERR_AUDIO;

    SAFE_STRCPY(g_current_file, filename);
    g_state = PLAYER_PLAYING;

#ifdef __ORBIS__
    g_thread_running = true;
    OrbisPthreadAttr attr;
    scePthreadAttrInit(&attr);
    scePthreadAttrSetstacksize(&attr, 1024 * 1024);
    scePthreadCreate(&g_play_thread, &attr, audio_playback_thread, NULL, "YmpAudio");
    scePthreadAttrDestroy(&attr);
#endif
    return YMP_OK;
}

YmpError audio_pause(void) {
    if (g_state != PLAYER_PLAYING) return YMP_OK;
#ifdef __ORBIS__
    if (g_audio_port > 0) { int32_t vol = 0; sceAudioOutSetVolume(g_audio_port, 3, &vol); }
#endif
    g_state = PLAYER_PAUSED;
    return YMP_OK;
}

YmpError audio_resume(void) {
    if (g_state != PLAYER_PAUSED) return YMP_OK;
#ifdef __ORBIS__
    if (g_audio_port > 0) { int32_t vol = g_volume; sceAudioOutSetVolume(g_audio_port, 3, &vol); }
#endif
    g_state = PLAYER_PLAYING;
    return YMP_OK;
}

YmpError audio_stop(void) {
#ifdef __ORBIS__
    g_thread_running = false;
    if (g_play_thread) { scePthreadJoin(g_play_thread, NULL); g_play_thread = NULL; }
#endif
    g_state = PLAYER_STOPPED;
    memset(g_current_url, 0, sizeof(g_current_url));
    memset(g_current_file, 0, sizeof(g_current_file));
    return YMP_OK;
}

YmpError audio_set_volume(int volume) {
    g_volume = MAX(0, MIN(100, volume));
#ifdef __ORBIS__
    if (g_audio_port > 0 && g_state == PLAYER_PLAYING) {
        int32_t vol = g_volume; sceAudioOutSetVolume(g_audio_port, 3, &vol);
    }
#endif
    return YMP_OK;
}

PlayerState audio_get_state(void) { return g_state; }

int audio_get_position_ms(void) {
    if (!g_decoder) return 0;
    int pos = decoder_tell(g_decoder);
    AudioFormat fmt = decoder_get_format(g_decoder);
    return (fmt.sample_rate > 0) ? (int)((long)pos * 1000 / fmt.sample_rate) : 0;
}

int audio_get_duration_ms(void) {
    if (!g_decoder) return 0;
    AudioFormat fmt = decoder_get_format(g_decoder);
    return (fmt.sample_rate > 0) ? (int)((long)fmt.total_samples * 1000 / fmt.sample_rate) : 0;
}

bool audio_is_finished(void) { return (g_state == PLAYER_STOPPED && g_current_file[0] != '\0'); }

int audio_get_pcm(float *buffer, int max_samples) {
    if (!buffer || max_samples <= 0 || !g_pcm_buffer) return 0;
    int count = MIN(max_samples * 2, g_pcm_buf_size);
    for (int i = 0; i < count; i++) buffer[i] = g_pcm_buffer[i];
    return count / 2;
}
