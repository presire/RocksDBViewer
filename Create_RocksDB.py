#!/usr/bin/env python3
"""
RocksDBサンプルデータベース作成スクリプト

このスクリプトは、RocksDBのサンプルデータベースを作成します。
複数のカラムファミリーにサンプルデータを格納し、
RocksDBViewerアプリケーションでの動作確認に使用できます。

必要なパッケージ:
    pip install rocksdict

使用方法:
    python Create_RocksDB.py [データベースパス]
    
    デフォルトでは、./sample_rocksdb にデータベースが作成されます。
"""

from rocksdict import Rdict
import json
import sys
import os
from datetime import datetime, timedelta
import random


def create_default_data(db_path: str):
    """
    デフォルトカラムファミリーにサンプルデータを作成
    
    Args:
        db_path: データベースのパス
    """
    print("📦 デフォルトカラムファミリーにデータを作成中...")
    
    db = Rdict(db_path)
    
    # 基本的なキー・バリューペア
    db["app_name"] = "RocksDB Sample Application"
    db["version"] = "1.0.0"
    db["created_at"] = datetime.now().isoformat()
    db["description"] = "This is a sample RocksDB database for testing purposes"
    
    # 設定データ
    db["config:max_connections"] = "100"
    db["config:timeout"] = "30"
    db["config:retry_count"] = "3"
    
    db.close()
    print("   ✓ デフォルトデータを作成しました")


def create_users_data(db_path: str):
    """
    usersカラムファミリーにサンプルデータを作成
    
    Args:
        db_path: データベースのパス
    """
    print("👥 usersカラムファミリーにデータを作成中...")
    
    db = Rdict(f"{db_path}:users")
    
    # ユーザーデータ（JSON形式）
    users = [
        {
            "id": "user001",
            "name": "山田太郎",
            "email": "yamada@example.com",
            "age": 28,
            "role": "admin",
            "created_at": (datetime.now() - timedelta(days=365)).isoformat()
        },
        {
            "id": "user002",
            "name": "鈴木花子",
            "email": "suzuki@example.com",
            "age": 32,
            "role": "user",
            "created_at": (datetime.now() - timedelta(days=180)).isoformat()
        },
        {
            "id": "user003",
            "name": "佐藤次郎",
            "email": "sato@example.com",
            "age": 25,
            "role": "user",
            "created_at": (datetime.now() - timedelta(days=90)).isoformat()
        },
        {
            "id": "user004",
            "name": "田中美咲",
            "email": "tanaka@example.com",
            "age": 29,
            "role": "moderator",
            "created_at": (datetime.now() - timedelta(days=120)).isoformat()
        },
        {
            "id": "user005",
            "name": "伊藤健太",
            "email": "ito@example.com",
            "age": 35,
            "role": "user",
            "created_at": (datetime.now() - timedelta(days=60)).isoformat()
        }
    ]
    
    for user in users:
        user_id = user["id"]
        db[user_id] = json.dumps(user, ensure_ascii=False)
    
    db.close()
    print(f"   ✓ {len(users)}件のユーザーデータを作成しました")


def create_products_data(db_path: str):
    """
    productsカラムファミリーにサンプルデータを作成
    
    Args:
        db_path: データベースのパス
    """
    print("🛍️  productsカラムファミリーにデータを作成中...")
    
    db = Rdict(f"{db_path}:products")
    
    # 商品データ（JSON形式）
    products = [
        {
            "id": "prod001",
            "name": "ノートパソコン",
            "category": "Electronics",
            "price": 89800,
            "stock": 15,
            "description": "高性能ノートパソコン、15.6インチディスプレイ"
        },
        {
            "id": "prod002",
            "name": "ワイヤレスマウス",
            "category": "Electronics",
            "price": 2980,
            "stock": 50,
            "description": "静音設計のワイヤレスマウス"
        },
        {
            "id": "prod003",
            "name": "オフィスチェア",
            "category": "Furniture",
            "price": 24800,
            "stock": 8,
            "description": "エルゴノミクスデザインのオフィスチェア"
        },
        {
            "id": "prod004",
            "name": "USBメモリ 64GB",
            "category": "Electronics",
            "price": 1480,
            "stock": 100,
            "description": "高速転送対応USBメモリ"
        },
        {
            "id": "prod005",
            "name": "デスクライト",
            "category": "Furniture",
            "price": 4980,
            "stock": 25,
            "description": "調光機能付きLEDデスクライト"
        },
        {
            "id": "prod006",
            "name": "キーボード",
            "category": "Electronics",
            "price": 8900,
            "stock": 30,
            "description": "メカニカルキーボード、RGB対応"
        },
        {
            "id": "prod007",
            "name": "モニタースタンド",
            "category": "Furniture",
            "price": 3200,
            "stock": 20,
            "description": "高さ調節可能なモニタースタンド"
        }
    ]
    
    for product in products:
        product_id = product["id"]
        db[product_id] = json.dumps(product, ensure_ascii=False)
    
    db.close()
    print(f"   ✓ {len(products)}件の商品データを作成しました")


def create_orders_data(db_path: str):
    """
    ordersカラムファミリーにサンプルデータを作成
    
    Args:
        db_path: データベースのパス
    """
    print("📋 ordersカラムファミリーにデータを作成中...")
    
    db = Rdict(f"{db_path}:orders")
    
    # 注文データ（JSON形式）
    orders = [
        {
            "id": "order001",
            "user_id": "user001",
            "products": [
                {"product_id": "prod001", "quantity": 1, "price": 89800}
            ],
            "total_amount": 89800,
            "status": "completed",
            "order_date": (datetime.now() - timedelta(days=30)).isoformat()
        },
        {
            "id": "order002",
            "user_id": "user002",
            "products": [
                {"product_id": "prod002", "quantity": 2, "price": 2980},
                {"product_id": "prod004", "quantity": 3, "price": 1480}
            ],
            "total_amount": 10400,
            "status": "completed",
            "order_date": (datetime.now() - timedelta(days=25)).isoformat()
        },
        {
            "id": "order003",
            "user_id": "user003",
            "products": [
                {"product_id": "prod003", "quantity": 1, "price": 24800}
            ],
            "total_amount": 24800,
            "status": "shipped",
            "order_date": (datetime.now() - timedelta(days=5)).isoformat()
        },
        {
            "id": "order004",
            "user_id": "user004",
            "products": [
                {"product_id": "prod005", "quantity": 1, "price": 4980},
                {"product_id": "prod007", "quantity": 1, "price": 3200}
            ],
            "total_amount": 8180,
            "status": "processing",
            "order_date": (datetime.now() - timedelta(days=2)).isoformat()
        },
        {
            "id": "order005",
            "user_id": "user005",
            "products": [
                {"product_id": "prod006", "quantity": 1, "price": 8900}
            ],
            "total_amount": 8900,
            "status": "pending",
            "order_date": datetime.now().isoformat()
        }
    ]
    
    for order in orders:
        order_id = order["id"]
        db[order_id] = json.dumps(order, ensure_ascii=False)
    
    db.close()
    print(f"   ✓ {len(orders)}件の注文データを作成しました")


def create_config_data(db_path: str):
    """
    configカラムファミリーにサンプルデータを作成
    
    Args:
        db_path: データベースのパス
    """
    print("⚙️  configカラムファミリーにデータを作成中...")
    
    db = Rdict(f"{db_path}:config")
    
    # 設定データ
    configs = {
        "system:maintenance_mode": "false",
        "system:debug_mode": "true",
        "system:log_level": "INFO",
        "api:rate_limit": "1000",
        "api:timeout": "30",
        "api:retry_attempts": "3",
        "cache:ttl": "3600",
        "cache:max_size": "10000",
        "email:smtp_host": "smtp.example.com",
        "email:smtp_port": "587",
        "email:from_address": "noreply@example.com",
        "security:session_timeout": "1800",
        "security:max_login_attempts": "5",
        "security:password_min_length": "8",
        "ui:theme": "light",
        "ui:language": "ja",
        "ui:items_per_page": "20"
    }
    
    for key, value in configs.items():
        db[key] = value
    
    db.close()
    print(f"   ✓ {len(configs)}件の設定データを作成しました")


def create_logs_data(db_path: str):
    """
    logsカラムファミリーにサンプルデータを作成
    
    Args:
        db_path: データベースのパス
    """
    print("📝 logsカラムファミリーにデータを作成中...")
    
    db = Rdict(f"{db_path}:logs")
    
    # ログレベル
    log_levels = ["INFO", "WARNING", "ERROR", "DEBUG"]
    log_messages = [
        "Application started successfully",
        "User authentication successful",
        "Database connection established",
        "Cache cleared",
        "Configuration reloaded",
        "API request processed",
        "Session timeout",
        "Failed login attempt detected",
        "Memory usage: 75%",
        "Backup completed"
    ]
    
    # ログデータを生成
    for i in range(20):
        log_time = datetime.now() - timedelta(hours=random.randint(0, 48))
        log_entry = {
            "timestamp": log_time.isoformat(),
            "level": random.choice(log_levels),
            "message": random.choice(log_messages),
            "source": f"module_{random.randint(1, 5)}",
            "user_id": f"user{random.randint(1, 5):03d}" if random.random() > 0.3 else None
        }
        
        log_key = f"log_{log_time.strftime('%Y%m%d_%H%M%S')}_{i:03d}"
        db[log_key] = json.dumps(log_entry, ensure_ascii=False)
    
    db.close()
    print(f"   ✓ 20件のログデータを作成しました")


def print_summary(db_path: str):
    """
    作成したデータベースのサマリーを表示
    
    Args:
        db_path: データベースのパス
    """
    print()
    print("=" * 60)
    print("✅ サンプルデータベースの作成が完了しました！")
    print("=" * 60)
    print()
    print(f"📁 データベースパス: {db_path}")
    print()
    print("📊 作成されたカラムファミリー:")
    
    column_families = ['default', 'users', 'products', 'orders', 'config', 'logs']
    
    for cf in column_families:
        try:
            if cf == 'default':
                db = Rdict(db_path)
            else:
                db = Rdict(f"{db_path}:{cf}")
            
            count = len(list(db.keys()))
            db.close()
            print(f"   • {cf:12s} : {count:3d} レコード")
        except Exception as e:
            print(f"   • {cf:12s} : エラー ({str(e)})")
    
    print()
    print("🚀 次のステップ:")
    print("   1. APIサーバを起動:")
    print(f"      python RocksDB_API_Server.py {db_path}")
    print()
    print("   2. ブラウザでRocksDBViewer.htmlを開く")
    print()
    print("   3. サーバURL (http://localhost:5000) を指定して接続")
    print()
    print("=" * 60)


def main():
    """メイン処理"""
    # コマンドライン引数からデータベースパスを取得
    db_path = "./sample_rocksdb"
    if len(sys.argv) > 1:
        db_path = sys.argv[1]
    
    # 絶対パスに変換
    db_path = os.path.abspath(db_path)
    
    print()
    print("=" * 60)
    print("🔨 RocksDBサンプルデータベース作成")
    print("=" * 60)
    print()
    print(f"📁 作成先: {db_path}")
    print()
    
    # データベースが既に存在する場合は警告
    if os.path.exists(db_path):
        print("⚠️  警告: データベースが既に存在します")
        response = input("上書きしますか？ (yes/no): ")
        if response.lower() not in ['yes', 'y']:
            print("処理を中止しました")
            return
        print()
    
    try:
        # 各カラムファミリーにデータを作成
        create_default_data(db_path)
        create_users_data(db_path)
        create_products_data(db_path)
        create_orders_data(db_path)
        create_config_data(db_path)
        create_logs_data(db_path)
        
        # サマリーを表示
        print_summary(db_path)
        
    except Exception as e:
        print()
        print("=" * 60)
        print("❌ エラーが発生しました")
        print("=" * 60)
        print(f"エラー: {str(e)}")
        print()
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
