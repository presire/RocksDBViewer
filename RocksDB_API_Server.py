#!/usr/bin/env python3
"""
RocksDB REST APIサーバ

このサーバは、RocksDBデータベースへのアクセスを提供するREST APIを実装します。
ブラウザベースのHTMLアプリケーションから、HTTPリクエストを通じてRocksDBを
操作できるようにします。

必要なパッケージ:
    pip install flask flask-cors rocksdict

使用方法:
    python RocksDB_API_Server.py [データベースパス]
    
    デフォルトでは、http://localhost:5000 でサーバが起動します。
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
from rocksdict import Rdict
import json
import sys
import os
from typing import Dict, List, Optional

# Flaskアプリケーションの作成
app = Flask(__name__)

# CORS（Cross-Origin Resource Sharing）を有効化
# これにより、ブラウザから異なるオリジンのAPIを呼び出せるようになります
CORS(app)

# グローバル変数：データベースパス
DB_PATH = "./sample_rocksdb"

# 使用可能なカラムファミリーのリスト
# 実際の環境では、これを動的に取得することもできます
COLUMN_FAMILIES = ['default', 'users', 'products', 'orders', 'config', 'logs']


def get_db(column_family: str = 'default') -> Rdict:
    """
    指定されたカラムファミリーのRocksDBインスタンスを取得
    
    Args:
        column_family: カラムファミリー名
        
    Returns:
        Rdictインスタンス
    """
    global DB_PATH
    if column_family == 'default':
        return Rdict(DB_PATH)
    else:
        return Rdict(f"{DB_PATH}:{column_family}")


def check_column_family_exists(column_family: str) -> bool:
    """
    カラムファミリーが存在するかチェック
    
    Args:
        column_family: カラムファミリー名
        
    Returns:
        存在する場合True、存在しない場合False
    """
    try:
        # データベースのパスを構築
        if column_family == 'default':
            db_path = DB_PATH
        else:
            db_path = f"{DB_PATH}:{column_family}"

        # ディレクトリの存在確認のみ行う (データベースを開かない)
        return os.path.exists(db_path)
    except Exception:
        return False


@app.route('/api/health', methods=['GET'])
def health_check():
    """
    ヘルスチェックエンドポイント
    サーバが正常に動作しているか確認するために使用します
    """
    return jsonify({
        'status': 'ok',
        'database_path': os.path.abspath(DB_PATH),
        'database_exists': os.path.exists(DB_PATH)
    })


@app.route('/api/column-families', methods=['GET'])
def list_column_families():
    """
    カラムファミリーの一覧を取得
    
    Returns:
        カラムファミリー名のリスト
    """
    try:
        # 実際に存在するカラムファミリーのみをフィルタリング
        existing_families = [
            cf for cf in COLUMN_FAMILIES 
            if check_column_family_exists(cf)
        ]
        
        return jsonify({
            'status': 'success',
            'column_families': existing_families
        })
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/api/data/<column_family>', methods=['GET'])
def get_all_data(column_family: str):
    """
    指定されたカラムファミリーの全データを取得
    
    Args:
        column_family: カラムファミリー名
        
    Query Parameters:
        search: 検索キーワード（オプション）
        
    Returns:
        キーと値のペアのリスト
    """
    try:
        # 検索キーワードを取得（オプション）
        search_term = request.args.get('search', '').lower()
        
        # データベースを開く
        db = get_db(column_family)
        
        # 全データを取得
        data = {}
        for key in db.keys():
            value = db[key]
            
            # 検索フィルタリング
            if search_term:
                if search_term in key.lower() or search_term in str(value).lower():
                    data[key] = value
            else:
                data[key] = value
        
        db.close()
        
        return jsonify({
            'status': 'success',
            'column_family': column_family,
            'data': data,
            'count': len(data)
        })
        
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/api/data/<column_family>/<key>', methods=['GET'])
def get_data_by_key(column_family: str, key: str):
    """
    指定されたキーのデータを取得
    
    Args:
        column_family: カラムファミリー名
        key: データのキー
        
    Returns:
        キーに対応する値
    """
    try:
        db = get_db(column_family)
        
        if key in db:
            value = db[key]
            db.close()
            
            return jsonify({
                'status': 'success',
                'key': key,
                'value': value
            })
        else:
            db.close()
            return jsonify({
                'status': 'error',
                'message': f"Key '{key}' not found"
            }), 404
            
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/api/data/<column_family>', methods=['POST'])
def put_data(column_family: str):
    """
    データを追加または更新
    
    Args:
        column_family: カラムファミリー名
        
    Request Body:
        {
            "key": "キー名",
            "value": "値"
        }
        
    Returns:
        成功メッセージ
    """
    try:
        # リクエストボディからデータを取得
        data = request.get_json()
        
        if not data or 'key' not in data or 'value' not in data:
            return jsonify({
                'status': 'error',
                'message': 'key and value are required'
            }), 400
        
        key = data['key']
        value = data['value']
        
        # データベースに保存
        db = get_db(column_family)
        db[key] = value
        db.close()
        
        return jsonify({
            'status': 'success',
            'message': f"Data saved successfully",
            'key': key
        })
        
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/api/data/<column_family>/<key>', methods=['DELETE'])
def delete_data(column_family: str, key: str):
    """
    指定されたキーのデータを削除
    
    Args:
        column_family: カラムファミリー名
        key: データのキー
        
    Returns:
        成功メッセージ
    """
    try:
        db = get_db(column_family)
        
        if key in db:
            del db[key]
            db.close()
            
            return jsonify({
                'status': 'success',
                'message': f"Key '{key}' deleted successfully"
            })
        else:
            db.close()
            return jsonify({
                'status': 'error',
                'message': f"Key '{key}' not found"
            }), 404
            
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/api/data/<column_family>/clear', methods=['DELETE'])
def clear_column_family(column_family: str):
    """
    カラムファミリーの全データを削除
    
    Args:
        column_family: カラムファミリー名
        
    Returns:
        成功メッセージ
    """
    try:
        db = get_db(column_family)
        
        # 全キーを取得して削除
        keys = list(db.keys())
        for key in keys:
            del db[key]
        
        db.close()
        
        return jsonify({
            'status': 'success',
            'message': f"All data in '{column_family}' deleted successfully",
            'deleted_count': len(keys)
        })
        
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/api/export/<column_family>', methods=['GET'])
def export_data(column_family: str):
    """
    カラムファミリーのデータをJSON形式でエクスポート
    
    Args:
        column_family: カラムファミリー名
        
    Returns:
        JSON形式のデータ
    """
    try:
        db = get_db(column_family)
        
        # 全データを取得
        data = {}
        for key in db.keys():
            value = db[key]
            
            # JSONとして解析できる場合はパース
            try:
                data[key] = json.loads(value)
            except (json.JSONDecodeError, TypeError):
                data[key] = value
        
        db.close()
        
        return jsonify({
            'status': 'success',
            'column_family': column_family,
            'data': data
        })
        
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


@app.route('/api/import/<column_family>', methods=['POST'])
def import_data(column_family: str):
    """
    JSON形式のデータをインポート
    
    Args:
        column_family: カラムファミリー名
        
    Request Body:
        {
            "key1": "value1",
            "key2": "value2",
            ...
        }
        
    Returns:
        成功メッセージ
    """
    try:
        # リクエストボディからデータを取得
        data = request.get_json()
        
        if not data or not isinstance(data, dict):
            return jsonify({
                'status': 'error',
                'message': 'Invalid data format. Expected JSON object.'
            }), 400
        
        # データベースに保存
        db = get_db(column_family)
        
        count = 0
        for key, value in data.items():
            # 値がオブジェクトの場合はJSON文字列に変換
            if isinstance(value, (dict, list)):
                db[key] = json.dumps(value, ensure_ascii=False)
            else:
                db[key] = str(value)
            count += 1
        
        db.close()
        
        return jsonify({
            'status': 'success',
            'message': f"{count} records imported successfully",
            'imported_count': count
        })
        
    except Exception as e:
        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500


def print_usage():
    """使用方法を表示"""
    print("=" * 60)
    print("RocksDB REST APIサーバ")
    print("=" * 60)
    print()
    print("使用方法:")
    print("  python RocksDB_API_Server.py [データベースパス]")
    print()
    print("オプション:")
    print("  データベースパス: RocksDBデータベースのパス")
    print("                   （省略時: ./sample_rocksdb）")
    print()
    print("例:")
    print("  python RocksDB_API_Server.py")
    print("  python RocksDB_API_Server.py ./my_database")
    print()


if __name__ == '__main__':
    # コマンドライン引数からデータベースパスを取得
    if len(sys.argv) > 1:
        DB_PATH = sys.argv[1]

    # データベースパスを絶対パスに変換して正規化
    DB_PATH = os.path.abspath(os.path.expanduser(DB_PATH))

    # 末尾のスラッシュを削除
    DB_PATH = DB_PATH.rstrip('/')
    
    # データベースが存在するか確認
    if not os.path.exists(DB_PATH):
        print(f"エラー: データベースが見つかりません: {DB_PATH}")
        print()
        print("データベースを作成するには:")
        print(f"  python Create_RocksDB.py {DB_PATH}")
        print()
        sys.exit(1)
    
    print("=" * 60)
    print("RocksDB REST APIサーバ")
    print("=" * 60)
    print()
    print(f"データベースパス: {DB_PATH}")
    print(f"サーバURL: http://localhost:5000")
    print()
    print("利用可能なエンドポイント:")
    print("  GET    /api/health                     - ヘルスチェック")
    print("  GET    /api/column-families            - カラムファミリー一覧")
    print("  GET    /api/data/<cf>                  - 全データ取得")
    print("  GET    /api/data/<cf>/<key>            - 特定キーのデータ取得")
    print("  POST   /api/data/<cf>                  - データ追加/更新")
    print("  DELETE /api/data/<cf>/<key>            - データ削除")
    print("  DELETE /api/data/<cf>/clear            - 全データ削除")
    print("  GET    /api/export/<cf>                - データエクスポート")
    print("  POST   /api/import/<cf>                - データインポート")
    print()
    print("ブラウザでHTMLを開いて、サーバURL (http://localhost:5000) を")
    print("指定してください。")
    print()
    print("サーバを停止するには: Ctrl+C")
    print("=" * 60)
    print()
    
    # Flaskサーバを起動
    # debug=Trueにすると、コード変更時に自動リロードされます
    app.run(host='0.0.0.0', port=5000, debug=True)
