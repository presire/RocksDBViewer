# RocksDB Viewer (Qt/QML Edition)

A desktop GUI application for browsing and editing RocksDB databases,  
built with **Qt 6.9.1 Quick (QML)** and **C++20**.  

This is a native reimplementation of the original Python/pywebview/HTML-based RocksDBViewer,  
offering the same feature set with native performance and no browser/WebView dependency.  

## Requirements

- **Qt** 6.5 or later (tested with 6.9.1)
- **RocksDB** C++ library (tested with 11.1.1)
- **CMake** 3.16 or later
- **C++ Compiler** with C++20 support (GCC 11+, Clang 14+, MSVC 2019+)

## Build

### 1. Configure

```bash
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release

# Or cmake with Qt path
# cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt/6.9.1/gcc_64 -DCMAKE_BUILD_TYPE=Release
```

### 2. Build

```bash
cmake --build . -j$(nproc)
```

## Run

```bash
# Launch without database
./RocksDBViewer

# Launch with a specific database path
./RocksDBViewer /path/to/your/rocksdb
```

> **Note:**  
> If RocksDB shared library is not in the system library path, set `LD_LIBRARY_PATH`:  

```bash
export LD_LIBRARY_PATH=/path/to/rocksdb/lib:$LD_LIBRARY_PATH
./RocksDBViewer
```

## Features

- **Native desktop GUI** — No browser or HTTP server required
- **Direct RocksDB access** — Uses the official RocksDB C++ API
- **Column Family support** — Browse and switch between multiple column families
- **CRUD operations** — Add, edit, delete, and clear all entries
- **Real-time search** — Filter entries by key or value content
- **Prefix search** — Quickly find keys starting with a specific string
- **Pagination** — Browse large datasets with configurable page sizes
- **Sort** — Toggle between original, ascending, and descending order
- **Database statistics** — Live display of entry counts, column families, and disk usage
- **JSON helpers** — Format, minify, validate, and generate skeleton templates in the editor
- **Import / Export** — JSON file I/O
- **Auto-refresh** — Optional 10-second interval polling
- **Recent databases** — Quick access to previously opened paths (up to 10)
- **Custom file picker** — Folder and file selection dialog with keyboard navigation
- **Internationalization** — Japanese / English language switch with persistent preference
- **Theme switch** — Light / Dark mode with persistent preference
- **Window state persistence** — Remembers size, position, and display preferences
- **Settings migration** — Automatic migration from legacy storage format
- **Keyboard shortcuts** — Quick access via keyboard for common actions

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open database |
| Ctrl+R | Refresh data |
| Ctrl+N | Add new entry |
| Ctrl+F | Focus search field |
| Ctrl+T | Toggle theme |
| Ctrl+Q | Quit application |

## Project Structure

```
RocksDBViewer/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── src/
│   ├── main.cpp
│   ├── backend/
│   │   ├── RocksDBBackend.h/.cpp    # DB operations, CRUD, file I/O
│   │   ├── EntryModel.h/.cpp        # QAbstractListModel for table data
│   │   └── FilterProxyModel.h/.cpp  # Search + sort proxy model
│   └── utils/
│       ├── JsonUtils.h/.cpp         # JSON format/minify/validate/skeleton
│       ├── I18nManager.h/.cpp       # Language switching
│       ├── FileSystemModel.h/.cpp   # File system model for custom picker
│       ├── SettingsMigration.h/.cpp # Settings storage migration
│       └── SettingsKeys.h           # Settings key constants
├── qml/
│   ├── main.qml                     # Main window layout
│   ├── Theme.qml                    # Theme constants
│   └── components/
│       ├── HeaderBar.qml
│       ├── DatabasePanel.qml
│       ├── ControlBar.qml
│       ├── DataTable.qml
│       ├── EditModal.qml
│       ├── ToastManager.qml
│       ├── WelcomePage.qml
│       ├── StatsPanel.qml
│       ├── SkeletonMenu.qml
│       └── FilePickerDialog.qml
├── i18n/
│   ├── rocksdbviewer_ja.ts          # Japanese translations
│   └── rocksdbviewer_en.ts          # English translations (source)
├── assets/
│   ├── RocksDBViewer@128.png
│   ├── RocksDBViewer@256.png
│   └── RocksDBViewer@512.png
└── tools/
    └── create_sample_db.cpp         # Sample database generator
```

## Updating Translations

After modifying source strings in `.cpp` or `.qml` files:

```bash
# Extract strings
lupdate src qml -ts i18n/rocksdbviewer_ja.ts i18n/rocksdbviewer_en.ts

# Edit .ts files (e.g., with Qt Linguist or text editor)

# Generate .qm files
lrelease i18n/rocksdbviewer_ja.ts i18n/rocksdbviewer_en.ts
```

## License

This project is licensed under the [MIT License](LICENSE).  

Third-party licenses:  
- [RocksDB](https://github.com/facebook/rocksdb) is licensed under the [Apache License, Version 2.0](https://github.com/facebook/rocksdb/blob/main/LICENSE.Apache)  
  and includes code under the [BSD 3-Clause License](https://github.com/facebook/rocksdb/blob/main/LICENSE.leveldb). See the `licenses/` directory for copies.  
- [Qt](https://www.qt.io/) is licensed under the [LGPL v3](https://www.gnu.org/licenses/lgpl-3.0.html).
