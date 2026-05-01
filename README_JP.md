# RocksDB GUI Viewer

RocksDB データベースを閲覧・編集するためのデスクトップ GUI ツールです。[pywebview](https://pywebview.flowrl.com/) と [rocksdict](https://congyuwang.github.io/RocksDict/) を使用し、ブラウザ・HTTP サーバ・ポート開放なしで単一のネイティブウィンドウとして動作します。

## 概要

このツールは以下のコンポーネントで構成されています:

1. **RocksDBViewer.py** — デスクトップエントリポイント (pywebview + rocksdict)
2. **Create_RocksDB.py** — サンプルデータベース作成スクリプト (初回セットアップ用)
3. **RocksDBViewer.html** — ネイティブウィンドウ内に表示される Web UI
4. **js/rocksdb-manager.js** — `window.pywebview.api` 経由で Python を呼び出す UI ロジック

## セットアップ

### 必要な環境

- Python 3.7 以上
- プラットフォームネイティブの WebView バックエンド:
  - **Linux**: `WebKit2GTK` (例: `python3-webkit2gtk` パッケージ、または `webkit2gtk` ランタイム)
  - **Windows**: WebView2 (Windows 11 はプリインストール、Windows 10 は [Evergreen ランタイム](https://developer.microsoft.com/ja-jp/microsoft-edge/webview2/) を導入)
  - **macOS**: WebKit (組み込み)

### Python パッケージのインストール

```bash
pip3 install pywebview rocksdict
```

**venv (仮想環境) を使う場合**: pywebview は GTK もしくは Qt のいずれかのバックエンドを必要とします。venv からはシステム側の `PyGObject` (`gi`) を参照できないため、pip だけで完結する Qt バックエンドを追加してください:

```bash
pip3 install qtpy pyside6
```

### ファイル構成

```
project/
├── RocksDBViewer.py           # デスクトップエントリポイント (pywebview)
├── Create_RocksDB.py          # サンプルデータベース作成スクリプト
├── RocksDBViewer.html         # Web UI (pywebview 内で描画)
├── js/
│   └── rocksdb-manager.js    # UI ロジック
├── css/
│   └── tailwind.css          # Tailwind CSS
├── assets/
│   └── RocksDBViewer.png     # アプリケーションアイコン
└── sample_rocksdb/            # RocksDB データベース (自動生成)
```

## 使い方

### 0. サンプルデータベースの作成 (初回のみ)

```bash
# デフォルトパス (./sample_rocksdb)
python3 Create_RocksDB.py

# カスタムパス
python3 Create_RocksDB.py /path/to/your/rocksdb
```

**作成されるサンプルデータ:**
- `default` (7件)、`users` (5件)、`products` (7件)、`orders` (5件)、`config` (17件)、`logs` (20件)

### 1. デスクトップアプリの起動 (推奨)

#### コマンドラインから起動

```bash
# 引数なしで起動 (ウェルカム画面 → ウィンドウ内の「データベースを開く」ボタンで選択)
python3 RocksDBViewer.py

# パス指定で起動 (起動と同時に DB を開く)
python3 RocksDBViewer.py /path/to/your/rocksdb
```

> ウィンドウ右上の「**データベースを開く**」ボタンから、いつでもネイティブのフォルダ選択ダイアログで別の RocksDB ディレクトリへ切り替えられます。

#### ダブルクリックで起動 (Linux)

`RocksDBViewer.sh` を実行するか、`.desktop` ファイルをアプリケーションメニューに登録するとダブルクリックで起動できます。

```bash
# 直接ランチャースクリプトを実行
./RocksDBViewer.sh                          # デフォルト DB
./RocksDBViewer.sh /path/to/your/rocksdb    # カスタム DB

# アプリケーションメニュー / デスクトップに登録 (KDE / GNOME / XFCE)
./install_desktop.sh
```

`install_desktop.sh` を実行すると `~/.local/share/applications/RocksDBViewer.desktop` が配置され、アプリケーションメニュー・ランチャー・KRunner などから「RocksDB GUI Viewer」で検索・起動できます。デスクトップ上のショートカット配置も対話式で選べます。

> **メモ**: プロジェクトをほかのディレクトリに移動した場合は `install_desktop.sh` を再実行すると `.desktop` ファイル内の絶対パスが現在のディレクトリ基準で書き換わります。

ネイティブウィンドウが自動的に開き、データベースに接続します。ブラウザ・ポート・URL 入力は不要です。

### 2. データの操作

#### カラムファミリーの選択

ウィンドウ上部のドロップダウンから選択します。利用可能なカラムファミリー: `default`、`users`、`products`、`orders`、`config`、`logs`。

#### データの表示

選択したカラムファミリーのキーとバリューがテーブル形式で表示されます。

#### 検索

検索ボックスにキーワードを入力するとリアルタイムでフィルタリングされます。

#### 追加・編集

1. 「新規追加」ボタンをクリック (または既存行の「編集」ボタン)
2. モーダルダイアログでキーとバリューを入力
3. 「保存」ボタンをクリック

JSON 編集機能: 「フォーマット」、「圧縮」、リアルタイム検証。

#### 削除

- **個別削除**: 各行の「削除」ボタンをクリック
- **全削除**: 「全削除」ボタン (確認ダイアログあり)

#### エクスポート・インポート

- **エクスポート**: 現在のカラムファミリーを JSON 形式でダウンロード
- **インポート**: JSON ファイルを選択し、カラムファミリーに書き戻し

インポートファイルの形式:
```json
{
  "key1": "value1",
  "key2": {"name": "example", "age": 30},
  "key3": "value3"
}
```

## 機能一覧

- ネイティブデスクトップウィンドウ (ブラウザ不要)
- `rocksdict` 直接アクセス — HTTP・CORS・ポート競合なし
- リアルタイム検索・フィルタリング
- JSON 構文検証、フォーマット、圧縮
- データのインポート / エクスポート (JSON)
- 複数カラムファミリー対応
- ソート切り替え、更新、自動更新 (10秒間隔)
- トースト通知
- 日本語 / 英語切り替え
- カスタムスクロールバー、アニメーション、カラーコーディング

## セキュリティに関する注意

- このツールは**開発環境**での使用を想定しています
- `rocksdict` でデータベースを直接開きます。ネットワーク越しに公開しないローカルファイルとして扱ってください。

## トラブルシューティング

### ウィンドウが起動しない / pywebview がバックエンドを見つけられない

Linux: WebKit2GTK ランタイムをインストールしてください (例: openSUSE は `zypper install python3-webkit2gtk` 等)。`gobject-introspection` や `cairo` 関連のシステムパッケージも必要な場合があります。

Windows: WebView2 ランタイムをインストールしてください。

### `WebKitJavascriptError: Unsupported result type (601)` という警告

WebKit2GTK 環境での pywebview ブリッジ初期化時に出る無害な内部メッセージで、機能には影響しません。

### データベースが見つからない

```bash
python3 Create_RocksDB.py ./sample_rocksdb
```

### データが表示されない

1. カラムファミリーが正しく選択されているか確認
2. データベースに実際にデータが存在するか確認
3. (pywebview ビルドが対応している場合) DevTools のコンソールを確認

### JSON 編集時のエラー

- ダブルクォート (`"`) を使用してください (シングルクォートは無効)
- 末尾のカンマに注意

## ライセンス

このプロジェクトは、教育・開発目的で自由に使用できます。

## 貢献

バグ報告や機能改善の提案を歓迎します。

---

<br>

**Enjoy using RocksDB GUI Viewer!**
