# 🚀 Инструкция по сборке через GitHub

## Шаг 1: Установить GitHub CLI

Скачай и установи [GitHub CLI (gh)](https://cli.github.com/).

## Шаг 2: Авторизоваться

```bash
gh auth login
```

Следуй инструкциям (выбери GitHub.com → HTTPS → Login with browser).

## Шаг 3: Создать репозиторий и запушить

```bash
cd "D:\hermes work\ps4-yandex-player"

# Инициализация git
git init
git add .
git commit -m "Initial commit: Yandex Music Player for PS4"

# Создать репозиторий на GitHub
gh repo create ps4-yandex-player --public --source=. --remote=origin --push
```

## Шаг 4: GitHub Actions автоматически соберёт проект!

После пуша GitHub Actions автоматически:
1. Запустит Docker с OpenOrbis SDK
2. Скомпилирует проект
3. Создаст .pkg файл
4. Сохранит как артефакт

## Шаг 5: Скачать собранный .pkg

1. Перейди в репозиторий на GitHub
2. Вкладка **Actions** → выбери последний workflow run
3. Внизу секция **Artifacts** → скачай `YandexMusicPlayer-PS4.zip`
4. Внутри будет `DEAD0001.pkg`

## Шаг 6: Установить на PS4

1. Включи **GoldHEN** на PS4
2. Запусти **Remote Package Installer**
3. Введи URL или загрузи .pkg через USB
4. Установи из **Settings → Debug Settings → Package Installer**

---

## Альтернатива: Сборка локально (Linux/Docker)

```bash
# Скачать Docker образ
docker pull openorbisofficial/toolchain:latest

# Запустить контейнер
docker run -it -v "D:/hermes work/ps4-yandex-player:/build" openorbisofficial/toolchain:latest bash

# В контейнере:
cd /build
make all
```
