#include "common.h"
#include "audio_player.h"
#include "decoder.h"

// ═══════════════════════════════════════════════════════════════
//  Audio Player - декодирование и воспроизведение
//  Использует stb_vorbis для OGG, minimp3 для MP3
// ═══════════════════════════════════════════════════════════════

// ─── Static state ──────────────────────────────────────────────

static PlayerState g_state       = PLAYER_STOPPED;
static int         g_volume      = 50;      // 0-100
static int         g_audio_port  = -1;
static char        g_current_url[MAX_URL_LEN] = {0};
static char        g_current_file[256] = {0};

static DecoderContext *g_decoder = NULL;
static float          *g_pcm_buffer = NULL;
static int             g_pcm_buf_size = 0;

#ifdef __ORBIS__
static ScePthread   g_play_thread = NULL;
static bool         g_thread_running = false;
#endif

// Audio format constants
#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_CHANNELS     2
#define AUDIO_BUFFER_SIZE  4096   // Сэмплов на канал за вызов

// ─── Thread: decode and output ─────────────────────────────────

#ifdef __ORBIS__
static void* audio_playback_thread(void *arg) {
    (void)arg;

    if (!g_decoder) {
        printf("[Audio] No decoder\n");
        g_state = PLAYER_STOPPED;
        g_thread_running = false;
        return NULL;
    }

    AudioFormat fmt = decoder_get_format(g_decoder);
    printf("[Audio] Decoding: %d Hz, %d ch\n", fmt.sample_rate, fmt.channels);

    // Выделяем буфер для PCM
    float pcm_buf[AUDIO_BUFFER_SIZE * 2]; // stereo interleaved

    // Выделяем int16 буфер для вывода
    int16_t *output_buf = (int16_t *)malloc(AUDIO_BUFFER_SIZE * 2 * sizeof(int16_t));
    if (!output_buf) {
        printf("[Audio] Failed to allocate output buffer\n");
        g_state = PLAYER_STOPPED;
        g_thread_running = false;
        return NULL;
    }

    // Устанавливаем громкость (просто значение, без флагов)
    int32_t vol = g_volume;
    sceAudioOutSetVolume(g_audio_port, 3, &vol); // 3 = both channels

    while (g_thread_running && g_state == PLAYER_PLAYING) {
        // Декодируем сэмплы
        int samples_decoded = decoder_decode(g_decoder, pcm_buf, AUDIO_BUFFER_SIZE);

        if (samples_decoded <= 0) {
            // Конец файла
            printf("[Audio] End of track\n");
            break;
        }

        // Конвертируем float [-1.0, 1.0] в int16
        for (int i = 0; i < samples_decoded * AUDIO_CHANNELS; i++) {
            float sample = pcm_buf[i];
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            output_buf[i] = (int16_t)(sample * 32767.0f);
        }

        // Копируем в общий буфер для визуализатора
        if (g_pcm_buffer && g_pcm_buf_size > 0) {
            int copy_count = samples_decoded * AUDIO_CHANNELS;
            if (copy_count > g_pcm_buf_size) copy_count = g_pcm_buf_size;
            for (int i = 0; i < copy_count; i++) {
                g_pcm_buffer[i] = pcm_buf[i];
            }
        }

        // Выводим аудио
        sceAudioOutOutput(g_audio_port, output_buf);

        // Небольшая задержка чтобы не нагружать CPU
        sceKernelSleep(1);
    }

    free(output_buf);

    g_state = PLAYER_STOPPED;
    g_thread_running = false;

    printf("[Audio] Playback finished\n");
    return NULL;
}
#endif

// ─── Public API ────────────────────────────────────────────────

YmpError audio_init(void) {
#ifdef __ORBIS__
    // Инициализируем аудио
    int ret = sceAudioOutInit();
    if (ret != 0) {
        printf("[Audio] sceAudioOutInit failed: 0x%08X\n", (unsigned int)ret);
        return YMP_ERR_AUDIO;
    }

    // Получаем ID пользователя
    OrbisUserServiceUserId userId = 0; // ORBIS_USER_SERVICE_USER_ID_SYSTEM

    // Открываем аудио порт
    g_audio_port = sceAudioOutOpen(
        userId,
        ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,
        0,                      // диффузия
        256,                    // 256 сэмплов на буфер
        AUDIO_SAMPLE_RATE,
        ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO
    );

    if (g_audio_port <= 0) {
        printf("[Audio] Failed to open port: 0x%08X\n", (unsigned int)g_audio_port);
        return YMP_ERR_AUDIO;
    }

    printf("[Audio] Port opened: %d\n", g_audio_port);
#else
    printf("[Audio] PC mode - audio stub\n");
#endif

    g_state = PLAYER_STOPPED;

    // Выделяем PCM буфер для визуализатора
    g_pcm_buf_size = AUDIO_BUFFER_SIZE * 2;
    g_pcm_buffer = (float *)malloc(g_pcm_buf_size * sizeof(float));

    return YMP_OK;
}

void audio_cleanup(void) {
    audio_stop();

    if (g_decoder) {
        decoder_close(g_decoder);
        g_decoder = NULL;
    }

    if (g_pcm_buffer) {
        free(g_pcm_buffer);
        g_pcm_buffer = NULL;
    }

#ifdef __ORBIS__
    if (g_audio_port > 0) {
        sceAudioOutClose(g_audio_port);
        g_audio_port = -1;
    }
#endif
}

YmpError audio_play(const char *url) {
    if (!url) return YMP_ERR_MEMORY;

    // Для URL нужно сначала скачать файл
    printf("[Audio] URL playback not yet implemented: %s\n", url);
    SAFE_STRCPY(g_current_url, url);

    return YMP_OK;
}

YmpError audio_play_file(const char *filename) {
    if (!filename) return YMP_ERR_MEMORY;

    // Останавливаем предыдущее воспроизведение
    audio_stop();

    // Закрываем старый декодер
    if (g_decoder) {
        decoder_close(g_decoder);
        g_decoder = NULL;
    }

    // Открываем новый файл
    g_decoder = decoder_open(filename);
    if (!g_decoder) {
        printf("[Audio] Failed to open: %s\n", filename);
        return YMP_ERR_AUDIO;
    }

    SAFE_STRCPY(g_current_file, filename);
    g_state = PLAYER_PLAYING;

#ifdef __ORBIS__
    // Создаём поток воспроизведения
    g_thread_running = true;

    int ret = sceKernelCreateThread(&g_play_thread, "YmpAudio",
                                    audio_playback_thread, 1024 * 1024, 0, 0);
    if (ret < 0) {
        printf("[Audio] Failed to create thread: 0x%08X\n", (unsigned int)ret);
        g_state = PLAYER_STOPPED;
        return YMP_ERR_AUDIO;
    }

    sceKernelStartThread(g_play_thread, 0, NULL);
#else
    printf("[Audio] Playing: %s\n", filename);
#endif

    return YMP_OK;
}

YmpError audio_pause(void) {
    if (g_state != PLAYER_PLAYING) return YMP_OK;

#ifdef __ORBIS__
    if (g_audio_port > 0) {
        int32_t vol = 0;
        sceAudioOutSetVolume(g_audio_port, 3, &vol); // 0 volume
    }
#endif

    g_state = PLAYER_PAUSED;
    return YMP_OK;
}

YmpError audio_resume(void) {
    if (g_state != PLAYER_PAUSED) return YMP_OK;

#ifdef __ORBIS__
    if (g_audio_port > 0) {
        int32_t vol = g_volume;
        sceAudioOutSetVolume(g_audio_port, 3, &vol);
    }
#endif

    g_state = PLAYER_PLAYING;
    return YMP_OK;
}

YmpError audio_stop(void) {
#ifdef __ORBIS__
    g_thread_running = false;

    if (g_play_thread >= 0) {
        sceKernelDeleteThread(g_play_thread);
        g_play_thread = -1;
    }
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
        int32_t vol = g_volume;
        sceAudioOutSetVolume(g_audio_port, 3, &vol);
    }
#endif

    return YMP_OK;
}

PlayerState audio_get_state(void) {
    return g_state;
}

int audio_get_position_ms(void) {
    if (!g_decoder) return 0;

    int sample_pos = decoder_tell(g_decoder);
    AudioFormat fmt = decoder_get_format(g_decoder);

    if (fmt.sample_rate > 0)
        return (int)((long)sample_pos * 1000 / fmt.sample_rate);

    return 0;
}

int audio_get_duration_ms(void) {
    if (!g_decoder) return 0;

    AudioFormat fmt = decoder_get_format(g_decoder);
    if (fmt.sample_rate > 0)
        return (int)((long)fmt.total_samples * 1000 / fmt.sample_rate);

    return 0;
}

bool audio_is_finished(void) {
    return (g_state == PLAYER_STOPPED && g_current_file[0] != '\0');
}

int audio_get_pcm(float *buffer, int max_samples) {
    if (!buffer || max_samples <= 0) return 0;

    if (!g_pcm_buffer || g_pcm_buf_size <= 0) return 0;

    int count = MIN(max_samples * 2, g_pcm_buf_size);
    for (int i = 0; i < count; i++) {
        buffer[i] = g_pcm_buffer[i];
    }

    return count / 2; // Сэмплов на канал
}
