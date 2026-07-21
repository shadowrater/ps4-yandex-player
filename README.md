# 🎵 Yandex Music Player for PS4

Музыкальный плеер для взломанной PlayStation 4 с поддержкой Яндекс Музыки.

## ✨ Возможности

- 🔐 Авторизация через OAuth токен Яндекс
- 🎵 Прослушивание любимых треков
- 📋 Просмотр плейлистов
- 🔍 Поиск по каталогу Яндекс Музыки
- 💡 Рекомендации на основе вкусов
- 🎮 Управление геймпадом DualShock 4
- 🔀 Режимы Shuffle и Repeat
- 🔊 Регулировка громкости

## 🎮 Управление

| Кнопка | Действие |
|--------|----------|
| D-Pad ↑↓ | Навигация по списку |
| D-Pad ←→ | Громкость ±5% |
| **X** | Выбор / Play/Pause |
| **O** | Назад |
| **L1** | Предыдущий трек |
| **R1** | Следующий трек |
| **△** | Shuffle ON/OFF |
| **□** | Repeat ON/OFF |

## 📋 Требования

- **PS4** с прошивкой ≤ 9.00 (или с кастомной прошивкой)
- **GoldHEN** или аналогичный Exploit
- **OpenOrbis SDK** установленный в `$ORBIS_SDK`
- **Python 3** для скриптов сборки

## 🔧 Установка OpenOrbis SDK

```bash
# 1. Клонируйте SDK
git clone https://github.com/OpenOrbis/openorbis-ps4-toolchain.git
cd openorbis-ps4-toolchain

# 2. Установите
./setup.sh

# 3. Установите переменную окружения
export ORBIS_SDK=/path/to/openorbis-ps4-toolchain
```

## 🔑 Получение OAuth токена Яндекс

1. Перейдите на https://oauth.yandex.ru/client/new
2. Создайте приложение с доступом к **Яндекс Музыка**
3. Получите OAuth токен через OAuth 2.0
4. Узнайте свой **UID** через API Яндекс

## 🛠 Сборка

```bash
# Установите переменную окружения
export ORBIS_SDK=/path/to/openorbis-ps4-toolchain

# Соберите проект
cd ps4-yandex-player
make

# Результат: build/DEAD0001.pkg
```

## 📦 Установка на PS4

1. Подключитесь к PS4 по FTP (например, через FileZilla)
2. Скопируйте `DEAD0001.pkg` в `/data/pkg/`
3. Или используйте **Remote Package Installer**:
   - Запустите Remote Package Installer на PS4
   - Введите URL: `http://<YOUR_PC_IP>:8080/DEAD0001.pkg`
4. Перейдите в **Settings → Debug Settings → Game → Package Installer**
5. Установите пакет

## 🔨 Структура проекта

```
ps4-yandex-player/
├── Makefile                    # Скрипт сборки
├── README.md                   # Этот файл
├── src/
│   ├── main.c                 # Точка входа
│   ├── yandex_api.c           # API Яндекс Музыки
│   ├── audio_player.c         # Аудио движок
│   ├── ui.c                   # Интерфейс (текстовый)
│   └── cjson/
│       ├── cJSON.c            # JSON парсер
│       └── cJSON.h
├── include/
│   ├── common.h               # Общие определения
│   ├── yandex_api.h           # API интерфейс
│   ├── audio_player.h         # Аудио интерфейс
│   └── ui.h                   # UI интерфейс
├── tools/
│   ├── create_sfo.py          # Генератор param.sfo
│   └── create_pkg.py          # Упаковщик в PKG
└── sce_sys/
    ├── param.sfo              # Метаданные приложения
    ├── icon0.png              # Иконка (128x128)
    └── package/               # Для PKG
```

## 🎨 Кастомизация

### Изменить Title ID
Отредактируйте `Makefile`:
```makefile
TITLEID := DEAD0001  # Ваш уникальный ID
```

### Изменить иконку
Замените `sce_sys/icon0.png` (рекомендуется 512x512 PNG).

### Добавить свои плейлисты
Отредактируйте `src/yandex_api.c`, добавив новые эндпоинты API.

## ⚠️ Известные ограничения

1. **Аудио кодеки**: PS4 не имеет встроенного MP3/OGG декодера. Нужно добавить:
   - [minimp3](https://github.com/lieff/minimp3) для MP3
   - [stb_vorbis](https://github.com/nothings/stb) для OGG Vorbis
   
2. **UI**: Текстовый интерфейс через debug console. Для полноценного UI нужен:
   - Доступ к framebuffer (sceVideoOut)
   - Или встроенный WebView

3. **Онлайн-клавиатура**: Ввод текста требует подключения к скрипту ввода PS4.

## 📚 Полезные ссылки

- [OpenOrbis Wiki](https://github.com/OpenOrbis/openorbis-ps4-toolchain/wiki)
- [PS4 Homebrew](https://ps4.dev/)
- [Yandex Music API](https://yandex.ru/dev/music/)
- [Документация PS4 SDK](https://ps4.dev/api/)

## 📝 Лицензия

MIT License - свободное использование и модификация.

---

**Сделано с ❤️ для PS4 Homebrew комьюнити**
