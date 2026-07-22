#include "common.h"
#include "decoder.h"

// ═══════════════════════════════════════════════════════════════
//  Audio Decoder - stb_vorbis (OGG) + minimp3 (MP3)
// ═══════════════════════════════════════════════════════════════

// ─── stb_vorbis (OGG Vorbis) ──────────────────────────────────
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

// ─── minimp3 (MP3) - fix get_bits conflict ────────────────────
#define get_bits minimp3_get_bits
#define SHOW_bits minimp3_SHOW_bits
#define SKIP_bits minimp3_SKIP_bits
#define get_bits1 minimp3_get_bits1
#define char2shortarray minimp3_char2shortarray
#define uint82shortarray minimp3_uint82shortarray
#define zeros shortarray_zeros
#define 2short_to_16bitarray minimp3_2short_to_16bitarray
#define 16bit_to_2shortarray minimp3_16bit_to_2shortarray
#include "minimp3/minimp3.h"
#undef get_bits
#undef SHOW_bits
#undef SKIP_bits
#undef get_bits1
#undef char2shortarray
#undef uint82shortarray
#undef zeros
#undef 2short_to_16bitarray
#undef 16bit_to_2shortarray

// ─── Форматы файлов ───────────────────────────────────────────

typedef enum {
    FMT_UNKNOWN = 0,
    FMT_OGG,
    FMT_MP3
} FileFormat;

static FileFormat detect_format(const char *filename) {
    if (!filename) return FMT_UNKNOWN;
    const char *ext = strrchr(filename, '.');
    if (!ext) return FMT_UNKNOWN;
    ext++;
    if (strcasecmp(ext, "ogg") == 0 || strcasecmp(ext, "oga") == 0) return FMT_OGG;
    if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "mpeg") == 0) return FMT_MP3;

    FILE *f = fopen(filename, "rb");
    if (!f) return FMT_UNKNOWN;
    unsigned char header[4] = {0};
    if (fread(header, 1, 4, f) == 4) {
        if (header[0] == 'O' && header[1] == 'g' && header[2] == 'g') { fclose(f); return FMT_OGG; }
        if (header[0] == 0xFF && (header[1] & 0xE0) == 0xE0) { fclose(f); return FMT_MP3; }
        if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') { fclose(f); return FMT_MP3; }
    }
    fclose(f);
    return FMT_UNKNOWN;
}

// ═══════════════════════════════════════════════════════════════
//  DecoderContext
// ═══════════════════════════════════════════════════════════════

struct DecoderContext {
    FileFormat format;
    AudioFormat info;
    char *file_data;
    int file_size;
    bool from_memory;

    // OGG
    stb_vorbis *vorbis;

    // MP3 (simple decoder - no mp3dec_ex)
    mp3dec_t mp3d;
    int current_pos; // byte offset in file_data
};

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
    ctx->current_pos = 0;

    const unsigned char *bytes = (const unsigned char *)data;

    if (bytes[0] == 'O' && bytes[1] == 'g' && bytes[2] == 'g' && bytes[3] == 'S') {
        // ─── OGG Vorbis ───
        ctx->format = FMT_OGG;
        int error = 0;
        ctx->vorbis = stb_vorbis_open_memory(bytes, size, &error, NULL);
        if (!ctx->vorbis) { free(ctx); return NULL; }

        stb_vorbis_info info = stb_vorbis_get_info(ctx->vorbis);
        ctx->info.sample_rate = info.sample_rate;
        ctx->info.channels = info.channels;
        ctx->info.total_samples = 0; // stb_vorbis не даёт total_samples из заголовка
        ctx->info.format = 0;
        ctx->info.bits_per_sample = 16;

    } else if ((bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0) ||
               (bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3')) {
        // ─── MP3 (simple decoder) ───
        ctx->format = FMT_MP3;
        mp3dec_init(&ctx->mp3d);

        // Декодируем первый фрейм чтобы узнать sample rate и channels
        mp3dec_frame_info_t frame_info;
        short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        int samples = mp3dec_decode_frame(&ctx->mp3d, bytes, size, pcm, &frame_info);
        if (samples > 0) {
            ctx->info.sample_rate = frame_info.hz;
            ctx->info.channels = frame_info.channels;
        } else {
            ctx->info.sample_rate = 44100;
            ctx->info.channels = 2;
        }
        ctx->info.total_samples = 0; // подсчитаем при необходимости
        ctx->info.format = 1;
        ctx->info.bits_per_sample = 16;
        ctx->current_pos = 0; // сброс для следующего декодирования
    } else {
        free(ctx);
        return NULL;
    }

    return ctx;
}

// ═══════════════════════════════════════════════════════════════
//  Open from file
// ═══════════════════════════════════════════════════════════════

DecoderContext* decoder_open(const char *filename) {
    if (!filename) return NULL;

    FileFormat fmt = detect_format(filename);
    if (fmt == FMT_UNKNOWN) return NULL;

    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return NULL; }

    char *data = (char *)malloc(size);
    if (!data) { fclose(f); return NULL; }
    if ((int)fread(data, 1, size, f) != size) { free(data); fclose(f); return NULL; }
    fclose(f);

    DecoderContext *ctx = decoder_open_memory(data, size);
    if (ctx) {
        ctx->from_memory = false;
        if (ctx->format == FMT_UNKNOWN) ctx->format = fmt;
    } else {
        free(data);
    }
    return ctx;
}

// ═══════════════════════════════════════════════════════════════
//  Close
// ═══════════════════════════════════════════════════════════════

void decoder_close(DecoderContext *ctx) {
    if (!ctx) return;
    if (ctx->vorbis) stb_vorbis_close(ctx->vorbis);
    if (ctx->file_data && !ctx->from_memory) free(ctx->file_data);
    free(ctx);
}

AudioFormat decoder_get_format(DecoderContext *ctx) {
    if (!ctx) { AudioFormat e = {0}; return e; }
    return ctx->info;
}

// ═══════════════════════════════════════════════════════════════
//  Decode
// ═══════════════════════════════════════════════════════════════

int decoder_decode(DecoderContext *ctx, float *output, int max_samples) {
    if (!ctx || !output || max_samples <= 0) return 0;

    // ─── OGG Vorbis ───
    if (ctx->format == FMT_OGG && ctx->vorbis) {
        return stb_vorbis_get_samples_float_interleaved(
            ctx->vorbis, ctx->info.channels, output,
            max_samples * ctx->info.channels
        );
    }

    // ─── MP3 (simple frame-by-frame decoder) ───
    if (ctx->format == FMT_MP3) {
        short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        mp3dec_frame_info_t frame_info;
        int total_decoded = 0;

        while (total_decoded < max_samples) {
            if (ctx->current_pos >= ctx->file_size) break;

            int samples = mp3dec_decode_frame(&ctx->mp3d,
                (const unsigned char *)ctx->file_data + ctx->current_pos,
                ctx->file_size - ctx->current_pos,
                pcm, &frame_info);

            if (samples <= 0 || frame_info.frame_bytes <= 0) break;

            ctx->current_pos += frame_info.frame_bytes;

            // Конвертируем int16 → float stereo
            for (int i = 0; i < samples; i++) {
                for (int ch = 0; ch < ctx->info.channels; ch++) {
                    int idx = i * ctx->info.channels + ch;
                    if (total_decoded * ctx->info.channels + idx >= max_samples * ctx->info.channels)
                        goto done;
                    output[total_decoded * ctx->info.channels + idx] = pcm[i * ctx->info.channels + ch] / 32768.0f;
                }
            }
            total_decoded += samples;
        }
        done:
        return total_decoded;
    }

    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  Seek / Tell
// ═══════════════════════════════════════════════════════════════

int decoder_seek(DecoderContext *ctx, int sample_pos) {
    if (!ctx) return -1;
    if (ctx->format == FMT_OGG && ctx->vorbis) {
        if (sample_pos < 0) sample_pos = 0;
        stb_vorbis_seek(ctx->vorbis, sample_pos);
        return sample_pos;
    }
    if (ctx->format == FMT_MP3) {
        // Простой seek: перезапуск декодера (нет index в простом режиме)
        mp3dec_init(&ctx->mp3d);
        ctx->current_pos = 0;
        return 0;
    }
    return -1;
}

int decoder_tell(DecoderContext *ctx) {
    if (!ctx) return 0;
    if (ctx->format == FMT_OGG && ctx->vorbis) {
        return stb_vorbis_get_sample_offset(ctx->vorbis);
    }
    if (ctx->format == FMT_MP3) {
        // Приблизительная позиция по байтам
        if (ctx->info.sample_rate > 0 && ctx->file_size > 0) {
            return (int)((long)ctx->current_pos * ctx->info.sample_rate * ctx->info.channels * 2 / ctx->file_size);
        }
        return 0;
    }
    return 0;
}
