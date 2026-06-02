# RocksDB Viewer (Qt/QML Edition)

**Qt 6.9 Quick (QML)** と **C++20** で構築された、RocksDB データベースの閲覧・編集用アプリケーションです。  

Python/pywebview/HTMLで実装されていたRocksDBViewerを、ブラウザやWebViewに依存しないネイティブアプリケーションとして再実装したもので、  
同等の機能をネイティブパフォーマンスで提供します。  

![License](https://img.shields.io/badge/License-MIT-blue.svg)
![Qt Version](https://img.shields.io/badge/Qt-6.9+-green.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)

<p align="center">
  <img src="assets/RocksDBViewer@256.png" alt="RocksDB Viewer" width="256" valign="middle">
  <img src="assets/Qt.png" alt="Qt" width="139" valign="middle">
</p>

## スクリーンショット

| メイン画面 | データベース選択 |
|:---:|:---:|
| <img src="screenshot/main.png" alt="メイン画面" width="360"> | <img src="screenshot/select_db.png" alt="データベース選択" width="360"> |
| **データベース閲覧** | **エントリー編集** |
| <img src="screenshot/read_db.png" alt="データベース閲覧" width="360"> | <img src="screenshot/edit_db.png" alt="エントリー編集" width="360"> |

## 動作要件

- **Qt** 6.9 以降 (動作確認済み: 6.9.1)
- **RocksDB** C++ライブラリおよびヘッダ (動作確認済み: 11.1.1)
- **CMake** 3.16 以降
- **C++20** に対応したコンパイラ (GCC 11+, Clang 14+, MSVC 2019+)

> **RocksDB に関する注意:**  
> 現在の CMake 設定では、RocksDB をシステムパスから自動探索しません。  
> `ROCKSDB_ROOT` には、`include/rocksdb/db.h` と `lib/librocksdb.*` または `lib64/librocksdb.*` を含むRocksDBのインストール先、  
> またはソースビルド先を指定してください。  

## ビルド

### 1. RocksDB の準備

RocksDB をパッケージマネージャで導入できる場合は、そのインストールプレフィックスを `ROCKSDB_ROOT` に指定してください。  

依存関係パッケージのインストール例:

#### RocksDB をビルドする場合の依存関係

**RHEL**  

```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git \
    snappy-devel zlib-devel bzip2-devel lz4-devel libzstd-devel gflags-devel
```

**SUSE**  

```bash
sudo zypper install -t pattern devel_basis
sudo zypper install cmake git \
    snappy-devel zlib-devel libbz2-devel liblz4-devel libzstd-devel gflags-devel
```

#### 本アプリケーションをビルドする場合の依存関係

**RHEL**  

```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtquickcontrols2-devel qt6-linguist \
    rocksdb-devel
```

**SUSE**  

```bash
sudo zypper install -t pattern devel_basis
sudo zypper install cmake git \
    qt6-core-devel qt6-gui-devel qt6-quick-devel qt6-quickcontrols2-devel qt6-linguist-devel \
    rocksdb-devel
```

> **注意:**  
> ディストリビューションのリポジトリに適切なRocksDBパッケージがない場合は、以下のソースビルド手順を利用してください。  
> ディストリビューション付属のQtパッケージで **Qt 6.9 以降** を満たせない場合は、環境に合わせて別途 Qt 6.9 を導入してください。  

パッケージマネージャに RocksDB が存在しない場合は、公式リポジトリおよびインストールガイドを参照してソースからビルドしてください。  

- https://github.com/facebook/rocksdb
- https://github.com/facebook/rocksdb/blob/main/INSTALL.md

ソースビルド例:  

```bash
git clone https://github.com/facebook/rocksdb.git
cd rocksdb
make shared_lib
# または: make static_lib
```

### 2. 構成

```bash
mkdir build && cd build

cmake .. -DROCKSDB_ROOT=/path/to/rocksdb \
         -DCMAKE_BUILD_TYPE=Release

# またはQtのパスを指定する場合
cmake .. -DROCKSDB_ROOT=/path/to/rocksdb \
         -DCMAKE_PREFIX_PATH=/path/to/qt/6.9.1/gcc_64 \
         -DCMAKE_BUILD_TYPE=Release
```

### 3. ビルド

```bash
cmake --build . -j$(nproc)
```

## 実行

```bash
# データベースなしで起動
./RocksDBViewer

# 特定のデータベースパスを指定して起動
./RocksDBViewer /path/to/your/rocksdb
```

> **注意:**  
> RocksDB の共有ライブラリがシステムのライブラリパスに含まれていない場合は、`ROCKSDB_ROOT` 配下の `lib` または `lib64` を `LD_LIBRARY_PATH` に設定してください。  

```bash
export LD_LIBRARY_PATH=/path/to/rocksdb/lib:$LD_LIBRARY_PATH
# または: export LD_LIBRARY_PATH=/path/to/rocksdb/lib64:$LD_LIBRARY_PATH
./RocksDBViewer
```

## 機能一覧

- **ネイティブデスクトップGUI** — ブラウザやHTTPサーバを必要としません
- **RocksDBアクセス** — RocksDB C++ APIを使用
- **カラムファミリー対応** — 複数のカラムファミリーを閲覧・切り替え可能
- **CRUD 操作** — エントリーの追加、編集、削除、全削除
- **リアルタイム検索** — キーまたは値の内容でエントリーをフィルタリング
- **プレフィックス検索** — 特定の文字列で始まるキーを迅速に検索
- **ページネーション** — ページサイズを変更可能なページ分割で大規模データを閲覧
- **ソート** — 元の順序、昇順、降順の切り替え
- **データベース統計** — エントリー数、カラムファミリー数、ディスク使用量をリアルタイム表示
- **JSON 補助機能** — エディタ内での整形、圧縮、検証、スケルトンテンプレート生成
- **インポート / エクスポート** — JSONファイルの入出力
- **自動更新** — 10秒間隔の自動ポーリング (オプション)
- **最近使用したデータベース** — 過去に開いたパスへ素早くアクセス (最大10件まで保持)
- **カスタムファイルピッカー** — キーボードナビゲーション対応のフォルダー・ファイル選択ダイアログ
- **国際化** — 日本語 / 英語の言語切り替え（設定の永続化対応）
- **テーマ切り替え** — ライト / ダークモード（設定の永続化対応）
- **ウィンドウ状態の保持** — サイズ、位置、表示設定を記憶
- **設定の自動移行** — 古い保存形式からの自動移行機能
- **キーボードショートカット** — よく使う操作をキーボードで素早く実行

## キーボードショートカット

| ショートカット | 動作 |
|----------|--------|
| Ctrl+O | データベースを開く |
| Ctrl+R | データを更新 |
| Ctrl+N | 新規エントリーを追加 |
| Ctrl+F | 検索フィールドにフォーカス |
| Ctrl+T | テーマを切り替え |
| Ctrl+Q | アプリケーションを終了 |

## プロジェクト構成

```
RocksDBViewer/
├── CMakeLists.txt
├── README.md
├── README_JP.md
├── LICENSE
├── RocksDBViewer.desktop.in         # Linux デスクトップエントリーのテンプレート
├── assets/
│   ├── RocksDBViewer@128.png
│   ├── RocksDBViewer@256.png
│   ├── RocksDBViewer@512.png
│   └── Qt.png
├── i18n/
│   ├── rocksdbviewer_ja.ts          # 日本語翻訳
│   └── rocksdbviewer_en.ts          # 英語原文
├── licenses/                        # サードパーティライセンス文書
├── qml/
│   ├── main.qml                     # アプリケーションのメイン画面
│   ├── Theme.qml                    # テーマ定数
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
├── sample-db/                       # ローカル検証用のサンプル RocksDB データ
├── screenshot/
│   ├── main.png
│   ├── select_db.png
│   ├── read_db.png
│   └── edit_db.png
├── src/
│   ├── main.cpp                     # アプリ起動処理と QML 初期化
│   ├── backend/
│   │   ├── RocksDBBackend.h/.cpp    # RocksDB アクセス、CRUD、インポート/エクスポート
│   │   ├── EntryModel.h/.cpp        # エントリーデータ用 QAbstractListModel
│   │   └── FilterProxyModel.h/.cpp  # 検索・ソート・ページング用プロキシモデル
│   └── utils/
│       ├── JsonUtils.h/.cpp         # JSON 整形、圧縮、検証、スケルトン補助
│       ├── I18nManager.h/.cpp       # 実行時の言語切り替え
│       ├── FileSystemModel.h/.cpp   # カスタムピッカー用ファイルシステムモデル
│       ├── SettingsMigration.h/.cpp # 設定ストレージ移行ヘルパー
│       └── SettingsKeys.h           # 永続設定キー定数
└── tools/
    └── create_sample_db.cpp         # サンプルデータベース生成ツール
```

## 翻訳の更新

`.cpp` または `.qml` ファイルの原文を変更した後:  

```bash
# 文字列を抽出
lupdate src qml -ts i18n/rocksdbviewer_ja.ts i18n/rocksdbviewer_en.ts

# .ts ファイルを編集（Qt Linguist やテキストエディタ等）

# .qm ファイルを生成
lrelease i18n/rocksdbviewer_ja.ts i18n/rocksdbviewer_en.ts
```

## ライセンス

このプロジェクトは [MIT License](LICENSE) のもとで公開されています。  

サードパーティのライセンス:  
- [RocksDB](https://github.com/facebook/rocksdb) は [Apache License, Version 2.0](https://github.com/facebook/rocksdb/blob/main/LICENSE.Apache)
  および [BSD 3-Clause License](https://github.com/facebook/rocksdb/blob/main/LICENSE.leveldb) でライセンスされています。  
  詳細は `licenses/` ディレクトリをご覧ください。  
- [Qt](https://www.qt.io/) は [LGPL v3](https://www.gnu.org/licenses/lgpl-3.0.html) でライセンスされています。  
