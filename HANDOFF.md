# HANDOFF: PS4 Yandex Music Player

## Статус: 90% код готов, сборка требует фиксов в decoder.c

## Репозиторий
https://github.com/shadowrater/ps4-yandex-player

## Что сделано

### Код проекта (все файлы написаны):
- `src/main.c` — точка входа, игровой цикл, интеграция всех модулей
- `src/yandex_api.c` — полный API клиент Яндекс Музыки (OAuth, плейлисты, поиск, лайки, рекомендации)
- `src/audio_player.c` — аудио движок с OrbisPthread, sceAudioOut, декодером
- `src/decoder.c` — **ТРЕБУЕТ ИСПРАВЛЕНИЙ** (см. ниже)
- `src/visualizer.c` — Milkdrop (projectM) + простой FFT визуализатор
- `src/ui.c` — текстовый UI, DualShock 4 навигация
- `src/cjson/cJSON.c` — JSON парсер (встроен)
- `include/common.h` — общие типы, константы, макросы
- `include/minimp3/minimp3.h` + `minimp3_ex.h` — MP3 декодер (скачан)
- `.github/workflows/build.yml` — GitHub Actions (Ubuntu + clang-18 + OpenOrbis SDK)

### Что изучено (использованные источники):
- **Yaamp** (github.com/umnik1/yaamp) — Electron Winamp + Яндекс Музыка. Взяли: CLIENT_ID/SECRET для OAuth, API эндпоинты, MD5 алгоритм генерации URL треков
- **YaMuTools** (github.com/Chimildic/YaMuTools) — Chrome extension. Взяли: web handler URLs (`handlers/playlist.jsx`, `handlers/library.jsx`), CSRF авторизация
- **projectM** (github.com/projectM-visualizer/projectm) — Milkdrop визуализатор (4329⭐). API: `projectm_create()`, `projectm_opengl_render_frame()`, `projectm_pcm_add_int16()`
- **minimp3** (github.com/lieff/minimp3) — header-only MP3 декодер (1935⭐)
- **OpenOrbis oosdk_libraries** — ogg, vorbis, libpng для PS4
- **OpenOrbis SDK** — Piglet (OpenGL ES), AudioOut, Pad, структура заголовков

### Docker/Build окружение:
- OpenOrbis SDK скачан в `D:\hermes work\openorbis-sdk\`
- GitHub repo: `shadowrater/ps4-yandex-player`
- GitHub Actions workflow: Ubuntu runner + clang-18 из apt.llvm.org + SDK v0.5.4

## Что НУЖНО сделать для успешной сборки

### 1. Исправить `src/decoder.c` (КРИТИЧНО)

**Проблема A:** `mp3dec_ex_t` API неправильный
```c
// НЕПРАВИЛЬНО (текущий код):
ctx->mp3d_ex.mem = bytes;    // нет такого поля
ctx->mp3d_ex.size = size;    // нет такого поля
mp3dec_ex_open(&ctx->mp3d_ex, NULL);  // wrong signature

// ПРАВИЛЬНО:
mp3dec_ex_open_buf(&ctx->mp3d_ex, bytes, size, 0);  // из minimp3_ex.h
```

**Проблема B:** `stb_vorbis_get_sample_length()` не существует
```c
// ЗАМЕНИТЬ на:
ctx->info.total_samples = stb_vorbis_get_sample_offset(ctx->vorbis);
// Или подсчитать вручную через stb_vorbis_get_info()
```

**Проблема C:** конфликт `get_bits` между minimp3.h и другими заголовками
```c
// РЕШЕНИЕ: добавить перед #include minimp3.h:
#define get_bits minimp3_get_bits
#include "minimp3/minimp3.h"
#undef get_bits
```

**Проблема D:** `mp3dec_ex_t` структура не имеет полей `hz`, `channels`, `cur`
```c
// Правильные имена полей (проверить в minimp3_ex.h):
// info.sample_rate = dec->info.hz;     // или mp3dec_frame_info_t
// info.channels = dec->info.channels;
// position = dec->consumed;            // не cur
```

### 2. Убрать ненужные LDFLAGS в Makefile

```makefile
# Закомментировать пока нет бинарников:
# LDFLAGS += -lScePiglet -lSceDisplay -lGLESv2 -lEGL
```

### 3. Проверить что workflow собирается

После фикса decoder.c — push и проверить GitHub Actions.

## Управление ( DualShock 4 )

| Кнопка | Действие |
|--------|----------|
| D-Pad ↑↓ | Навигация / Громкость |
| X | Play / Pause |
| O | Назад |
| L1/R1 | Prev / Next трек |
| L2/R2 | Пресеты визуализатора |
| △ | Shuffle |
| □ | Repeat |
| Select | Режим визуализатора |

## OAuth для Яндекс

В `src/main.c` вписать:
```c
#define YANDEX_OAUTH_TOKEN  "YOUR_TOKEN"
#define YANDEX_USER_ID      "YOUR_UID"
```

Или добавить авторизацию по логину/паролю (CLIENT_ID/SECRET из Yaamp):
```
CLIENT_ID: 23cabbbdc6cd418abb4b39c32c41195d
CLIENT_SECRET: 53bc75238f0c4d08a118e51fe9203300
POST https://oauth.yandex.ru/token
```
