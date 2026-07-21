#ifndef DECODER_H
#define DECODER_H

// ═══════════════════════════════════════════════════════════════
//  Audio Decoder для PS4
//  Поддержка: OGG Vorbis (stb_vorbis) + MP3 (minimp3)
// ═══════════════════════════════════════════════════════════════

#include "common.h"

// Формат аудио
typedef struct {
    int sample_rate;
    int channels;
    int total_samples;
    int format;         // 0=OGG, 1=MP3, 2=WAV
    int bitrate;
    int bits_per_sample;
} AudioFormat;

// Дескриптор декодера
typedef struct DecoderContext DecoderContext;

// Открыть файл для декодирования
DecoderContext* decoder_open(const char *filename);

// Открыть из памяти
DecoderContext* decoder_open_memory(const void *data, int size);

// Закрыть декодер
void decoder_close(DecoderContext *ctx);

// Получить информацию о формате
AudioFormat decoder_get_format(DecoderContext *ctx);

// Декодировать сэмплы (возвращает количество декодированных сэмплов на канал)
// output - буфер float stereo interleaved [-1.0, 1.0]
// max_samples - максимальное количество сэмплов на канал
int decoder_decode(DecoderContext *ctx, float *output, int max_samples);

// Seek к позиции (в сэмплах)
int decoder_seek(DecoderContext *ctx, int sample_pos);

// Текущая позиция
int decoder_tell(DecoderContext *ctx);

#endif // DECODER_H
