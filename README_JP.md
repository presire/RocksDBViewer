# RocksDB Viewer (Qt/QML Edition)

**Qt 6.9.1 Quick (QML)** と **C++20** で構築された、RocksDB データベースの閲覧・編集用アプリケーションです。  

Python/pywebview/HTMLで実装されていたRocksDBViewerを、ブラウザやWebViewに依存しないネイティブアプリケーションとして再実装したもので、  
同等の機能をネイティブパフォーマンスで提供します。  

## 動作要件

- **Qt** 6.5 以降 (動作確認済み: 6.9.1)
- **RocksDB** C++ライブラリ (動作確認済み: 11.1.1)
- **CMake** 3.16 以降
- **C++20** に対応したコンパイラ (GCC 11+, Clang 14+, MSVC 2019+)

## ビルド

### 1. 構成

```bash
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release

# またはQtのパスを指定する場合
# cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt/6.9.1/gcc_64 -DCMAKE_BUILD_TYPE=Release
```

### 2. ビルド

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
> RocksDBの共有ライブラリがシステムのライブラリパスに含まれていない場合は、`LD_LIBRARY_PATH` を設定してください。  

```bash
export LD_LIBRARY_PATH=/path/to/rocksdb/lib:$LD_LIBRARY_PATH
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
├── LICENSE
├── src/
│   ├── main.cpp
│   ├── backend/
│   │   ├── RocksDBBackend.h/.cpp    # DB 操作、CRUD、ファイル入出力
│   │   ├── EntryModel.h/.cpp        # テーブルデータ用 QAbstractListModel
│   │   └── FilterProxyModel.h/.cpp  # 検索・ソート用プロキシモデル
│   └── utils/
│       ├── JsonUtils.h/.cpp         # JSON 整形/圧縮/検証/スケルトン
│       ├── I18nManager.h/.cpp       # 言語切り替え
│       ├── FileSystemModel.h/.cpp   # カスタムピッカー用ファイルシステムモデル
│       ├── SettingsMigration.h/.cpp # 設定ストレージの移行
│       └── SettingsKeys.h           # 設定キー定数
├── qml/
│   ├── main.qml                     # メインウィンドウレイアウト
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
├── i18n/
│   ├── rocksdbviewer_ja.ts          # 日本語翻訳
│   └── rocksdbviewer_en.ts          # 英語翻訳（原文）
├── assets/
│   ├── RocksDBViewer@128.png
│   ├── RocksDBViewer@256.png
│   └── RocksDBViewer@512.png
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
