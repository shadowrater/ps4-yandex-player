#include "common.h"
#include "decoder.h"

// ═══════════════════════════════════════════════════════════════
//  Audio Decoder - stb_vorbis (OGG) + minimp3 (MP3)
//  Header-only библиотеки, определение через IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

// ─── stb_vorbis (OGG Vorbis) ──────────────────────────────────
#define STB_VORBIS_IMPLEMENTATION
#include "stb/stb_vorbis.c"

// ─── minimp3 (MP3) ────────────────────────────────────────────
#define MINIMP3_IMPLEMENTATION
#include "minimp3/minimp3.h"
#include "minimp3/minimp3_ex.h"

// ─── Форматы файлов ───────────────────────────────────────────

typedef enum {
    FMT_UNKNOWN = 0,
    FMT_OGG,
    FMT_MP3
} FileFormat;

// Определяем формат по расширению файла
static FileFormat detect_format(const char *filename) {
    if (!filename) return FMT_UNKNOWN;

    // Ищем последнюю точку
    const char *ext = strrchr(filename, '.');
    if (!ext) return FMT_UNKNOWN;

    ext++; // пропускаем точку

    // Сравниваем (case-insensitive)
    if (strcasecmp(ext, "ogg") == 0 || strcasecmp(ext, "oga") == 0)
        return FMT_OGG;
    if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "mpeg") == 0)
        return FMT_MP3;

    // Пробуем определить по магическим байтам
    FILE *f = fopen(filename, "rb");
    if (!f) return FMT_UNKNOWN;

    unsigned char header[4] = {0};
    if (fread(header, 1, 4, f) == 4) {
        if (header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S')
            { fclose(f); return FMT_OGG; }
        if (header[0] == 0xFF && (header[1] & 0xE0) == 0xE0)
            { fclose(f); return FMT_MP3; }
        if (header[0] == 'I' && header[1] == 'D' && header[2] == '3')
            { fclose(f); return FMT_MP3; } // ID3 tag = MP3
    }

    fclose(f);
    return FMT_UNKNOWN;
}

// ═══════════════════════════════════════════════════════════════
//  DecoderContext - объединённый контекст для всех форматов
// ═══════════════════════════════════════════════════════════════

struct DecoderContext {
    FileFormat format;
    AudioFormat info;

    // Общее
    char *file_data;
    int file_size;
    bool from_memory;

    // OGG
    stb_vorbis *vorbis;

    // MP3
    mp3dec_t mp3d;
    mp3dec_ex_t mp3d_ex;
    bool ex_decoder_open;
    int current_frame;
    int total_frames;
};

// ─── Определяем количество MP3 фреймов ────────────────────────

static int mp3_count_frames(const unsigned char *data, int size) {
    mp3dec_t dec;
    mp3dec_init(&dec);

    int frames = 0;
    int pos = 0;

    while (pos < size) {
        mp3dec_frame_info_t info;
        short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        int samples = mp3dec_decode_frame(&dec, data + pos, size - pos, pcm, &info);

        if (samples <= 0) break;
        if (info.frame_bytes <= 0) break;

        frames++;
        pos += info.frame_bytes;
    }

    return frames;
}

// ═══════════════════════════════════════════════════════════════
//  Open from file
// ═══════════════════════════════════════════════════════════════

DecoderContext* decoder_open(const char *filename) {
    if (!filename) return NULL;

    FileFormat fmt = detect_format(filename);
    if (fmt == FMT_UNKNOWN) {
        printf("[Decoder] Unknown format: %s\n", filename);
        return NULL;
    }

    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("[Decoder] Failed to open: %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return NULL; }

    char *data = (char *)malloc(size);
    if (!data) { fclose(f); return NULL; }

    if ((int)fread(data, 1, size, f) != size) {
        free(data); fclose(f); return NULL;
    }
    fclose(f);

    DecoderContext *ctx = decoder_open_memory(data, size);
    if (ctx) {
        ctx->from_memory = false;
        // Перекрываем формат если определён по расширению
        if (ctx->format == FMT_UNKNOWN)
            ctx->format = fmt;
    } else {
        free(data);
    }

    return ctx;
}

// ═══════════════════════════════════════════════════════════════
//  Open from memory
// ═══════════════════════════════════════════════════════════════

DecoderContext* decoder_open_memory(const void *data, int size) {
    if (!data || size <= 0) return NULL;

    DecoderContext *ctx = (DecoderContext *)calloc(1, sizeof(DecoderContext));
    if (!ctx) return NULL;

    ctx->file_data = (char *)data;
    ctx->file_size = size;
    ctx->from_memory = true;

    // Определяем формат по магическим байтам
    const unsigned char *bytes = (const unsigned char *)data;

    if (bytes[0] == 'O' && bytes[1] == 'g' && bytes[2] == 'g' && bytes[3] == 'S') {
        // ─── OGG Vorbis ───
        ctx->format = FMT_OGG;

        int error = 0;
        ctx->vorbis = stb_vorbis_open_memory(bytes, size, &error, NULL);
        if (!ctx->vorbis) {
            printf("[Decoder] Failed to decode OGG header (error %d)\n", error);
            free(ctx);
            return NULL;
        }

        stb_vorbis_info info = stb_vorbis_get_info(ctx->vorbis);
        ctx->info.sample_rate = info.sample_rate;
        ctx->info.channels = info.channels;
        ctx->info.total_samples = stb_vorbis_get_sample_length(ctx->vorbis);
        ctx->info.format = 0; // OGG
        ctx->info.bitrate = 0;
        ctx->info.bits_per_sample = 16;

        printf("[Decoder] OGG: %d Hz, %d ch, %d samples\n",
               info.sample_rate, info.channels, ctx->info.total_samples);

    } else if ((bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0) ||
               (bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3')) {
        // ─── MP3 ───
        ctx->format = FMT_MP3;

        // Пробуем mp3dec_ex (полный декодер с seeking)
        memset(&ctx->mp3d_ex, 0, sizeof(ctx->mp3d_ex));
        ctx->mp3d_ex.mem = bytes;
        ctx->mp3d_ex.size = size;

        if (mp3dec_ex_open(&ctx->mp3d_ex, NULL) == 0) {
            ctx->ex_decoder_open = true;
            ctx->info.sample_rate = ctx->mp3d_ex.hz;
            ctx->info.channels = ctx->mp3d_ex.channels;
            ctx->info.total_samples = ctx->mp3d_ex.samples;
            ctx->info.format = 1; // MP3
            ctx->info.bitrate = 0;
            ctx->info.bits_per_sample = 16;

            printf("[Decoder] MP3 (ex): %d Hz, %d ch, %d samples\n",
                   ctx->mp3d_ex.hz, ctx->mp3d_ex.channels, ctx->mp3d_ex.samples);
        } else {
            // Fallback: простой декодер
            mp3dec_init(&ctx->mp3d);
            ctx->current_frame = 0;

            // Подсчитываем фреймы (прибл.)
            ctx->total_frames = mp3_count_frames(bytes, size);

            ctx->info.sample_rate = 44100; // предполагаем
            ctx->info.channels = 2;
            ctx->info.total_samples = ctx->total_frames * 1152;
            ctx->info.format = 1; // MP3
            ctx->info.bitrate = 0;
            ctx->info.bits_per_sample = 16;

            printf("[Decoder] MP3 (simple): ~%d frames\n", ctx->total_frames);
        }
    } else {
        printf("[Decoder] Unsupported format\n");
        free(ctx);
        return NULL;
    }

    return ctx;
}

// ═══════════════════════════════════════════════════════════════
//  Close
// ═══════════════════════════════════════════════════════════════

void decoder_close(DecoderContext *ctx) {
    if (!ctx) return;

    if (ctx->vorbis) {
        stb_vorbis_close(ctx->vorbis);
    }

    if (ctx->ex_decoder_open) {
        mp3dec_ex_close(&ctx->mp3d_ex);
    }

    if (ctx->file_data && !ctx->from_memory) {
        free(ctx->file_data);
    }

    free(ctx);
}

// ═══════════════════════════════════════════════════════════════
//  Get format
// ═══════════════════════════════════════════════════════════════

AudioFormat decoder_get_format(DecoderContext *ctx) {
    if (!ctx) {
        AudioFormat empty = {0};
        return empty;
    }
    return ctx->info;
}

// ═══════════════════════════════════════════════════════════════
//  Decode samples
// ═══════════════════════════════════════════════════════════════

int decoder_decode(DecoderContext *ctx, float *output, int max_samples) {
    if (!ctx || !output || max_samples <= 0) return 0;

    // ─── OGG Vorbis ───
    if (ctx->format == FMT_OGG && ctx->vorbis) {
        return stb_vorbis_get_samples_float_interleaved(
            ctx->vorbis,
            ctx->info.channels,
            output,
            max_samples * ctx->info.channels
        );
    }

    // ─── MP3 (ex decoder) ───
    if (ctx->format == FMT_MP3 && ctx->ex_decoder_open) {
        int samples_read = mp3dec_ex_read(&ctx->mp3d_ex, (mp3d_sample_t *)output,
                                          max_samples * ctx->info.channels);
        return samples_read / ctx->info.channels;
    }

    // ─── MP3 (simple decoder) ───
    if (ctx->format == FMT_MP3) {
        short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        mp3dec_frame_info_t info;

        int total_decoded = 0;

        while (total_decoded < max_samples) {
            int samples = mp3dec_decode_frame(&ctx->mp3d,
                (const unsigned char *)ctx->file_data + ctx->current_frame,
                ctx->file_size - ctx->current_frame,
                pcm, &info);

            if (samples <= 0 || info.frame_bytes <= 0) break;

            ctx->current_frame += info.frame_bytes;

            // Конвертируем int16 → float
            for (int i = 0; i < samples * ctx->info.channels && total_decoded < max_samples; i++) {
                output[total_decoded * ctx->info.channels + i % ctx->info.channels] =
                    pcm[i] / 32768.0f;
            }

            total_decoded += samples;
        }

        return total_decoded;
    }

    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  Seek
// ═══════════════════════════════════════════════════════════════

int decoder_seek(DecoderContext *ctx, int sample_pos) {
    if (!ctx) return -1;

    if (ctx->format == FMT_OGG && ctx->vorbis) {
        if (sample_pos < 0) sample_pos = 0;
        stb_vorbis_seek(ctx->vorbis, sample_pos);
        return sample_pos;
    }

    if (ctx->format == FMT_MP3 && ctx->ex_decoder_open) {
        if (sample_pos < 0) sample_pos = 0;
        if (sample_pos >= ctx->info.total_samples)
            sample_pos = ctx->info.total_samples - 1;

        mp3dec_ex_seek(&ctx->mp3d_ex, sample_pos);
        return sample_pos;
    }

    return -1;
}

// ═══════════════════════════════════════════════════════════════
//  Tell
// ═══════════════════════════════════════════════════════════════

int decoder_tell(DecoderContext *ctx) {
    if (!ctx) return 0;

    if (ctx->format == FMT_OGG && ctx->vorbis) {
        return stb_vorbis_get_sample_offset(ctx->vorbis);
    }

    if (ctx->format == FMT_MP3 && ctx->ex_decoder_open) {
        return ctx->mp3d_ex.cur;
    }

    return 0;
}
