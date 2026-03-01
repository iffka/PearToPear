# p2p::storage — FS слой (Workspace)

Этот модуль отвечает **только за работу с локальной файловой структурой** репозитория.
Он не знает про сеть и не работает с БД напрямую (это уровень выше)

## Структура репозитория на диске

Workspace определяется корневой директорией `root`

Внутри `root` создаётся скрытая служебная папка

- `root/.peer/` — служебные данные
- `root/.peer/obj/` — хранилище объектов (копии файлов для передачи)
- `root/.peer/meta/` — метаданные (позже: БД sqlite и т.п.)

Также в рабочей директории `root` могут появляться

- `*.empty` — пустые read-only файлы-плейсхолдеры, показывающие наличие файла в системе

## Workspace

`Workspace` — это объект-контекст, который хранит пути к корню репозитория и служебным директориям

### Создание и поиск workspace

#### `static Workspace init(const fs::path& root = fs::current_path())`

Инициализирует новый workspace в директории `root`

Поведение
- если в `root` или выше по дереву уже найден `.peer` → кидает исключение `std::runtime_error("Workspace already initialized")`
- иначе создаёт директории
  - `root/.peer`
  - `root/.peer/obj`
  - `root/.peer/meta`
- возвращает `Workspace(root)`

Зачем
- используется для команды `ptop init`

#### `static Workspace discover(const fs::path& start_dir = fs::current_path())`

Находит существующий workspace, поднимаясь вверх от `start_dir`

Поведение
- ищет `.peer` в `start_dir`, затем в родителях, пока не дойдёт до корня ФС
- если `.peer` не найден → кидает `std::runtime_error("No workspace found")`
- если найден → возвращает `Workspace(root)` где `root` — директория, содержащая `.peer`

Зачем
- используется для всех команд, кроме `init` (как у git)

#### `static std::optional<fs::path> find_peer_root(const fs::path& start_dir)`

Низкоуровневый поиск корня workspace

Возвращает
- `fs::path` на директорию, содержащую `.peer`, если найдено
- `std::nullopt`, если `.peer` не найден до корня ФС

Используется внутри `init/discover`

### Пути (геттеры)

#### `const fs::path& get_root() const`

Возвращает `root` — корень рабочей директории

#### `const fs::path& get_peer_dir() const`

Возвращает путь `root/.peer`

#### `const fs::path& get_obj_dir() const`

Возвращает путь `root/.peer/obj`

#### `const fs::path& get_meta_dir() const`

Возвращает путь `root/.peer/meta`

### Объекты (копии файлов для передачи)

#### `fs::path create_objectfile(const fs::path& path_to_local_file)`

Создаёт объект в `obj` директории как копию локального файла

Поведение
- проверяет, что `path_to_local_file` существует и является regular file
  - иначе кидает `std::runtime_error("Invalid file")`
- вычисляет `object_id = generate_object_id(path_to_local_file)`
- копирует файл в `root/.peer/obj/<object_id>`
- возвращает путь к созданному объекту

Примечания
- сейчас `object_id` временно равен `filename`
- коллизии имён возможны (будет исправлено после решения по БД/хэшу)

#### `void delete_objectfile(const std::string& id)`

Удаляет объект по идентификатору

Поведение
- строит путь `root/.peer/obj/<id>`
- проверяет, что это существующий regular file
  - иначе кидает `std::runtime_error("Invalid object file")`
- удаляет файл

### Empty-файлы (плейсхолдеры)

#### `void create_all_empty_files(const std::vector<std::string>& names_to_meta_files)`

Создаёт набор `.empty` файлов в рабочей директории

Поведение
- для каждого имени `name` вызывает `create_empty_file(name)`

Использование
- при подключении/синхронизации метаданных можно создать видимые плейсхолдеры для файлов, которых ещё нет локально

#### `fs::path create_empty_file(const std::string& filename)` (private)

Создаёт один плейсхолдер

Поведение
- формирует путь `root/<filename>.empty`
- если файл уже существует → возвращает путь без изменений
- иначе создаёт пустой файл и ставит права read-only (owner/group/others read)
- возвращает путь

## object id

#### `static std::string generate_object_id(const fs::path& path_to_local_file)`

Пока временная реализация

- возвращает `path_to_local_file.filename().string()`

Будет заменено
- на хэш содержимого (например SHA-256) или другое согласованное правило, когда появится БД и стратегия дедупликации

## Типичные сценарии

### Инициализация (команда `ptop init`)

1) `Workspace ws = Workspace::init()`
2) структура `.peer` создана

### Обычная команда (git-style)

1) `Workspace ws = Workspace::discover()`
2) `ws.get_obj_dir()` и другие пути доступны

### Создание объекта для передачи

1) `Workspace ws = Workspace::discover()`
2) `fs::path obj = ws.create_objectfile("file.txt")`

### Создание плейсхолдеров из метаданных

1) `Workspace ws = Workspace::discover()`
2) `ws.create_all_empty_files({"a.txt","b.txt"})`