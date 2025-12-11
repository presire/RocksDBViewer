# RocksDB GUI Viewer

RocksDBデータベースをブラウザから操作するためのGUIツールです。REST API経由でRocksDBにアクセスし、データの参照・編集・削除などを直感的に行えます。

## 概要

このツールは以下のコンポーネントで構成されています：

1. **Create_RocksDB.py** - サンプルデータベース作成スクリプト (初回セットアップ用)
2. **RocksDB_API_Server.py** - RocksDBにアクセスするためのREST APIサーバ (Python/Flask)
3. **RocksDBViewer.html** - データを操作するためのWebインターフェース
4. **rocksdb-manager.js** - UIとAPIサーバの連携を管理するJavaScriptコード

## セットアップ

### 必要な環境

- Python 3.7以上
- 最新のWebブラウザ (Chrome、Firefox、Safari等)

### Pythonパッケージのインストール

```bash
pip3 install flask flask-cors rocksdict
```

### ファイル構成

```
project/
├── Create_RocksDB.py          # サンプルデータベース作成スクリプト
├── RocksDB_API_Server.py      # APIサーバ
├── RocksDBViewer.html         # HTMLインターフェース
├── js/
│   └── rocksdb-manager.js    # JavaScriptコード
├── css/
│   ├── core.css              # 基本スタイル (必要に応じて)
│   └── tailwind.css          # Tailwind CSSスタイル (必要に応じて)
└── sample_rocksdb/            # RocksDBデータベース (自動生成)
```

**注意**  
CSSファイルは、HTMLファイル内で参照されていますが、Tailwindのユーティリティクラスを使用しているため、CDN経由で読み込むことも可能です。

## 使い方

### 0. サンプルデータベースの作成 (初回のみ)

サンプルデータで試したい場合は、付属の`Create_RocksDB.py`スクリプトを使用してサンプルデータベースを作成できます。

```bash
# デフォルトのパス (./sample_rocksdb) にサンプルデータベースを作成
python3 Create_RocksDB.py

# カスタムパスにサンプルデータベースを作成
python3 Create_RocksDB.py /path/to/your/rocksdb
```

**作成されるサンプルデータ:**
- `default` - アプリケーション基本情報と設定 (7レコード)
- `users` - ユーザー情報 (5レコード)
- `products` - 商品データ (7レコード)
- `orders` - 注文履歴 (5レコード)
- `config` - システム設定 (17レコード)
- `logs` - アプリケーションログ (20レコード)

スクリプト実行後、以下のように表示されます：

```
サンプルデータベースの作成が完了しました！
データベースパス: /absolute/path/to/sample_rocksdb

作成されたカラムファミリー:
   • default      :   7 レコード
   • users        :   5 レコード
   • products     :   7 レコード
   • orders       :   5 レコード
   • config       :  17 レコード
   • logs         :  20 レコード
```

**注意**  
既存のデータベースが存在する場合は、上書き確認のプロンプトが表示されます。

### 1. APIサーバの起動

まず、RocksDB APIサーバを起動します。

```bash
# デフォルトのパス (./sample_rocksdb)を使用する場合
python3 RocksDB_API_Server.py

# カスタムデータベースパスを指定する場合
python3 RocksDB_API_Server.py /path/to/your/rocksdb
```

サーバが正常に起動すると、以下のように表示されます：

```
============================================================
RocksDB REST APIサーバ
============================================================

データベースパス: /absolute/path/to/sample_rocksdb
サーバURL: http://localhost:5000

利用可能なエンドポイント:
  GET    /api/health                     - ヘルスチェック
  GET    /api/column-families            - カラムファミリー一覧
  GET    /api/data/<cf>                  - 全データ取得
  ...

サーバを停止するには: Ctrl+C
============================================================
```

### 2. HTMLビューアーを開く

1. `RocksDBViewer.html` をWebブラウザで開きます
2. 「APIサーバURL」フィールドに `http://localhost:5000` を入力 (デフォルトで設定済み)
3. 「サーバに接続」ボタンをクリック

接続が成功すると、画面右上のステータスが「接続済み」に変わり、メインコンテンツが表示されます。

### 3. データの操作

#### カラムファミリーの選択

画面上部のドロップダウンメニューから、操作したいカラムファミリーを選択します。

利用可能なカラムファミリー：
- `default` - デフォルトのカラムファミリー
- `users` - ユーザー情報
- `products` - 製品情報
- `orders` - 注文情報
- `config` - 設定情報
- `logs` - ログ情報

#### データの表示

選択したカラムファミリーのすべてのキーと値がテーブル形式で表示されます。

#### データの検索

検索ボックスにキーワードを入力すると、キーまたは値に含まれるデータをリアルタイムでフィルタリングします。

#### データの追加・編集

1. 「新規追加」ボタンをクリック (または既存データの「編集」ボタンをクリック)
2. モーダルダイアログでキーと値を入力
3. 「保存」ボタンをクリック

**JSON編集機能:**
- 「フォーマット」ボタン - JSONを整形して見やすく表示
- 「圧縮」ボタン - JSONを1行に圧縮
- リアルタイムJSON検証 - 入力中に構文エラーをチェック

#### データの削除

- **個別削除**: 各データ行の「削除」ボタンをクリック
- **全削除**: 「全削除」ボタンで選択中のカラムファミリーのすべてのデータを削除

#### データのエクスポート

「エクスポート」ボタンをクリックすると、選択中のカラムファミリーのデータをJSON形式でダウンロードできます。

#### データのインポート

1. 「インポート」ボタンをクリック
2. JSON形式のファイルを選択
3. データが自動的にインポートされます

**インポートファイルの形式:**
```json
{
  "key1": "value1",
  "key2": {"name": "example", "age": 30},
  "key3": "value3"
}
```

## API エンドポイント

APIサーバは以下のエンドポイントを提供します：

### ヘルスチェック
```
GET /api/health
```
サーバの状態とデータベース情報を取得

### カラムファミリー一覧
```
GET /api/column-families
```
利用可能なカラムファミリーのリストを取得

### データ取得
```
GET /api/data/<column_family>
GET /api/data/<column_family>?search=keyword
GET /api/data/<column_family>/<key>
```
データの取得 (全体、検索、キー指定)

### データ追加・更新
```
POST /api/data/<column_family>
Content-Type: application/json

{
  "key": "my-key",
  "value": "my-value"
}
```

### データ削除
```
DELETE /api/data/<column_family>/<key>
DELETE /api/data/<column_family>/clear
```
個別削除または全削除

### エクスポート
```
GET /api/export/<column_family>
```
JSON形式でデータをエクスポート

### インポート
```
POST /api/import/<column_family>
Content-Type: application/json

{
  "key1": "value1",
  "key2": "value2"
}
```

## 使用例

### 例1: ユーザー情報の管理

```json
// キー: user:1001
// 値:
{
  "id": 1001,
  "name": "山田太郎",
  "email": "taro@example.com",
  "role": "admin"
}
```

### 例2: 設定情報の保存

```json
// キー: config:app
// 値:
{
  "app_name": "MyApp",
  "version": "1.0.0",
  "debug_mode": false
}
```

### 例3: シンプルなキャッシュデータ

```
キー: cache:session:abc123
値: {"user_id": 42, "expires": 1234567890}
```

## 機能一覧

### 主な機能

- ✅ リアルタイム検索・フィルタリング
- ✅ JSON構文検証とフォーマット
- ✅ データのインポート/エクスポート (JSON形式)
- ✅ 複数カラムファミリーのサポート
- ✅ レスポンシブデザイン (モバイル対応)
- ✅ クリップボードへのコピー機能
- ✅ トースト通知による操作フィードバック
- ✅ データの追加・編集・削除
- ✅ カスタムスクロールバー

### UI機能

- アニメーション効果による滑らかなユーザー体験
- カラーコーディングによる視覚的フィードバック
- 長いデータの自動切り詰め表示
- JSONデータの自動検出とバッジ表示

## セキュリティに関する注意

- このツールは**開発環境**での使用を想定しています
- 本番環境で使用する場合は、以下の対策を実施してください：
  - 認証・認可の実装
  - HTTPS通信の使用
  - CORS設定の見直し
  - 入力値の検証強化
  - APIレート制限の実装

## トラブルシューティング

### サーバに接続できない

**原因1**: APIサーバが起動していない

```bash
# サーバを起動してください
python3 RocksDB_API_Server.py
```

**原因2**: URLが間違っている

- デフォルトURL: `http://localhost:5000`
- ポート番号が別の場合は適切に変更してください

**原因3**: CORSエラー

- APIサーバでCORSが有効になっていることを確認
- ブラウザのコンソールでエラーメッセージを確認

### データベースが見つからない

```bash
# サンプルデータベースを作成
python3 Create_RocksDB.py ./Example

# または、デフォルトパスで作成
python3 Create_RocksDB.py
```

### データが表示されない

1. カラムファミリーが正しく選択されているか確認
2. データベースにデータが存在するか確認
3. ブラウザのコンソールでエラーログを確認

### JSON編集時のエラー

- JSON形式が正しいか確認してください
- ダブルクォート (`"`) を使用してください (シングルクォートは不可)
- 末尾のカンマに注意してください

## ライセンス

このプロジェクトは、教育・開発目的で自由に使用できます。

## 貢献

バグ報告や機能改善の提案を歓迎します。

## サポート

問題が発生した場合は、以下を確認してください。
1. Pythonとパッケージが正しくインストールされているか
2. データベースパスが正しいか
3. TCP 5000番ポートが他のプログラムで使用されていないか
4. ブラウザのJavaScriptコンソールのエラーメッセージ

---

<br>

**Enjoy using RocksDB GUI Viewer!**
