<p align="center">
  <img src="docs/logo_2.png" width="220" alt="Pear-to-Pear logo">
</p>

<h1 align="center">Pear-to-Pear</h1>

<p align="center">
  Мультиплатформенное CLI-приложение и библиотечное ядро для p2p-хранения файлов
</p>

<p align="center">
  <a href="https://github.com/p2pSquad/PearToPear/actions/workflows/ci.yml">
    <img src="https://github.com/p2pSquad/PearToPear/actions/workflows/ci.yml/badge.svg" alt="CI">
  </a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20">
  <img src="https://img.shields.io/badge/CMake-enabled-blue" alt="CMake">
  <img src="https://img.shields.io/badge/gRPC-networking-blue" alt="gRPC">
  <img src="https://img.shields.io/badge/Protobuf-messages-orange" alt="Protobuf">
  <img src="https://img.shields.io/badge/SQLite-metadata-blue" alt="SQLite">
  <img src="https://img.shields.io/badge/gtest-tests-green" alt="gTest">
</p>

<p align="center">
  🇷🇺 Русский | 🇬🇧 <a href="./README.en.md">English</a>
</p>

---

## О проекте

**Pear-to-Pear** - это **мультиплатформенное CLI-приложение на C++20** для распределённого хранения файлов и **библиотечное ядро для p2p-проектов**.

Проект объединяет **общий файловый репозиторий**, **версионирование**, **локальное object-хранилище**, **SQLite-метаданные** и **сетевую синхронизацию между узлами**.

Идея Pear-to-Pear - дать группе устройств **общее пространство файлов без отдельного файлового сервера**. Каждый участник хранит данные у себя, а система поддерживает общее состояние репозитория: какие файлы существуют, какие версии актуальны и где можно получить содержимое.

По пользовательской логике Pear-to-Pear похож на **общий файловый диск с историей изменений**, но данные не переносятся в облако. Файлы остаются на устройствах участников, а между узлами синхронизируются метаданные и передаются только нужные версии файлов.

Проект можно использовать как **готовое CLI-приложение** или как **основу для собственных p2p-сценариев**. Внутренняя логика хранения, версий и синхронизации отделена от конкретной реализации сети, поэтому transport-слой можно расширять или заменять.

---

## Содержание

- [Быстрый старт](#быстрый-старт)
- [Возможности](#возможности)
- [Возможности CLI](#возможности-cli)
- [Структура workspace](#структура-workspace)
- [Как это работает](#как-это-работает)
  - [Архитектура локального узла](#архитектура-локального-узла)
  - [База данных](#база-данных)
  - [Синхронизация через change-log](#синхронизация-через-change-log)
- [Сборка и установка](#сборка-и-установка)
- [Тестирование и CI](#тестирование-и-ci)
- [Техническая документация](#техническая-документация)
- [Структура репозитория](#структура-репозитория)
- [Команда](#команда)
- [Лицензия](#лицензия)

---

## Быстрый старт

### 1. Собрать проект

```bash
cmake -S . -B build
cmake --build build -j
```

Бинарник после сборки:

```bash
./build/pear
```

Чтобы запускать программу просто как `pear`:

```bash
sudo install -m 755 build/pear /usr/local/bin/pear
```

Проверка:

```bash
pear --help
```

### 2. Создать два workspace

```bash
mkdir -p /tmp/pear-main /tmp/pear-peer

pear init /tmp/pear-main
pear init /tmp/pear-peer
```

### 3. Поднять главный узел

```bash
cd /tmp/pear-main
pear connect --main --listen 127.0.0.1:50051
```

### 4. Подключить второй узел

```bash
cd /tmp/pear-peer
pear connect --gu 127.0.0.1:50051 --listen 127.0.0.1:50052
```

### 5. Добавить и опубликовать файл

```bash
cd /tmp/pear-main
printf 'hello from pear\n' > note.txt

pear add note.txt
pear push
```

### 6. Обновить состояние и скачать файл на другом узле

```bash
cd /tmp/pear-peer
pear update
pear pull note.txt
cat note.txt
```

---

## Возможности

### Распределённое хранение файлов

Файлы хранятся на устройствах участников. Pear-to-Pear не требует отдельного сервера, на котором лежит всё содержимое репозитория.

### Общий файловый репозиторий

Узлы работают с единым списком файлов и версий. Пользователь может увидеть состояние репозитория через `pear ls` и локальные изменения через `pear status`.

### Версионирование

Изменения файлов сохраняются как версии. Содержимое идентифицируется object-хэшами, поэтому одинаковые данные не нужно хранить несколько раз.

### Синхронизация метаданных

Метаданные синхронизируются через change-log. Узлы получают новые операции, применяют их к локальной базе и приходят к общему состоянию репозитория.

### Readonly-режим

Файл можно добавить или перевести в readonly-режим. Это позволяет хранить содержимое как object внутри `.peer`, не держать лишнюю рабочую копию и при этом работать с файлом через workspace.

### Расширяемое ядро

Логика хранения, версий и синхронизации отделена от конкретной реализации сети. Транспортный слой можно расширять для других p2p-сценариев.

---

## Возможности CLI

### Создание репозитория

```bash
pear init <workspace_path>
pear deinit
```

### Подключение узлов

```bash
pear connect --main --listen <ip:port>
pear connect --gu <ip:port> --listen <ip:port>
pear disconnect
```

### Работа с файлами

```bash
pear add <path>...
pear add --all
pear add --readonly <path>...

pear unstage <path>...
pear unstage --all
```

### Readonly-режим

```bash
pear readonly <path>...
pear readonly --off <path>...
```

### Очистка старых данных

```bash
pear cleanup <keep_versions> <path>...
pear cleanup <keep_versions> --all
```

### Просмотр состояния

```bash
pear status
pear status --json

pear ls
pear ls --json

pear log
pear log --tail <n>
```

### Синхронизация и загрузка

```bash
pear update
pear push

pear pull <file-or-dir>...
pear pull --no-share <file-or-dir>...
```

---

## Структура workspace

После инициализации Pear-to-Pear создаёт рабочую директорию с пользовательскими файлами и служебной папкой `.peer`.

```text
workspace/
├── user files...
└── .peer/
    ├── meta
    ├── obj/
    └── config
```

- `workspace/` - обычные пользовательские файлы;
- `.peer/meta` - локальная SQLite-база с метаданными;
- `.peer/obj/` - object-хранилище версий файлов;
- `.peer/config` - локальная конфигурация узла.

Содержимое файлов хранится отдельно от метаданных. База знает, какие версии существуют и на каких устройствах они доступны, а сами версии лежат в object-хранилище.

---

## Как это работает

### Архитектура локального узла

Основной пользовательский вход - CLI. Команды передаются в фоновый процесс `demon`, который поднимает локальный сетевой узел и работает с сервисами синхронизации и хранения.

<p align="center">
  <img src="docs/assets/architecture.png" width="760" alt="Архитектура локального узла">
</p>

Внутри узла выделены два основных сервиса:

- `MasterService` - метаданные, устройства, версии и change-log;
- `StorageService` - передача содержимого файлов между peer-ами.

Ниже находятся два слоя хранения:

- `SQLiteDatabase` - состояние репозитория;
- `Workspace` - пользовательские файлы и object-хранилище.

### База данных

Каждый узел хранит локальную копию метаданных в SQLite.

<p align="center">
  <img src="docs/assets/database.png" width="760" alt="Схема базы данных">
</p>

Основные сущности:

- `DEVICES` - известные устройства;
- `FILES` - файлы, версии и актуальное состояние;
- `OBJECT_OWNERS` - устройства, на которых доступно содержимое object-а;
- `STAGING_FILES` - локальные изменения до публикации;
- `WAL` - журнал операций для синхронизации состояния.

### Синхронизация через change-log

Метаданные синхронизируются через change-log. Главный узел согласует порядок операций, но не является центральным файловым сервером.

<p align="center">
  <img src="docs/assets/change-log.png" width="760" alt="Синхронизация через change-log">
</p>

Узел сообщает последнюю известную запись, получает недостающие операции, сохраняет их локально и применяет к своей базе. Файлы при этом не пересылаются вместе с метаданными: содержимое скачивается отдельно через `pear pull`.

---

## Сборка и установка

### Зависимости для Ubuntu / Debian / WSL

```bash
sudo apt update
sudo apt install -y \
    cmake \
    g++ \
    pkg-config \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libprotobuf-dev \
    libgrpc++-dev \
    libsqlite3-dev \
    libssl-dev
```

### Сборка

```bash
cmake -S . -B build
cmake --build build -j
```

### Установка бинарника

```bash
sudo install -m 755 build/pear /usr/local/bin/pear
```

### Удаление бинарника

```bash
sudo rm -f /usr/local/bin/pear
```

---

## Тестирование и CI

Запуск тестов локально:

```bash
ctest --test-dir build --output-on-failure
```

Сборка и тесты также запускаются в GitHub Actions:

- [GitHub Actions](https://github.com/p2pSquad/PearToPear/actions)

---

## Техническая документация

Подробная документация вынесена в `docs/`:

- [Архитектура проекта](docs/architecture.md)
- [CLI reference](docs/cli.md)
- [Использование Pear-to-Pear как библиотеки](docs/library-usage.md)
- [Расширение transport-слоя](docs/transport.md)

Пример расширения transport-слоя:

- [PearToPearRelay](https://github.com/p2pSquad/PearToPearRelay)

---

## Структура репозитория

```text
PearToPear/
├── docs/          # документация, схемы, материалы проекта
├── proto/         # Protobuf и gRPC-описания
├── src/           # основной код проекта
│   └── pear/
│       ├── cli/   # CLI-команды
│       ├── db/    # SQLite и метаданные
│       ├── demon/ # фоновый процесс узла
│       ├── fs/    # workspace и object-хранилище
│       └── net/   # сетевой слой
├── tests/         # тесты
└── CMakeLists.txt
```

---

## Команда

<table align="center">
  <tr>
    <td align="center" width="180">
      <a href="https://github.com/dmkornef">
        <img src="https://github.com/dmkornef.png" width="96" height="96" style="border-radius: 50%;" alt="Корнев Дмитрий"/>
      </a>
      <br />
      <b>Корнев Дмитрий</b>
    </td>
    <td align="center" width="180">
      <a href="https://github.com/Borow22">
        <img src="https://github.com/Borow22.png" width="96" height="96" style="border-radius: 50%;" alt="Ламаш Станислав"/>
      </a>
      <br />
      <b>Ламаш Станислав</b>
    </td>
    <td align="center" width="180">
      <a href="https://github.com/iffka">
        <img src="https://github.com/iffka.png" width="96" height="96" style="border-radius: 50%;" alt="Тимофеев Дмитрий"/>
      </a>
      <br />
      <b>Тимофеев Дмитрий</b>
    </td>
  </tr>
</table>

---

## Лицензия

Проект распространяется по лицензии, указанной в файле [LICENSE](LICENSE).

---

<p align="center">
  Made with C++20 and love by HSE SPb AMI 2029 students
</p>
