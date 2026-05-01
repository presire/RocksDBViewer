#!/usr/bin/env python3
"""
RocksDB sample database creation script

This script creates a sample RocksDB database. It uses RocksDB's native
Column Family feature so that all data is contained in a single directory
(only sample_rocksdb is generated).

Required packages:
    pip install rocksdict

Usage:
    python Create_RocksDB.py [DATABASE_PATH]

    By default, the database is created at ./sample_rocksdb.
"""

from __future__ import annotations

import json
import os
import random
import sys
from datetime import datetime, timedelta

from rocksdict import Options, Rdict

NEEDED_CFS = ['default', 'users', 'products', 'orders', 'config', 'logs']


def write_default(cf: Rdict) -> None:
    print("📦 Writing data to the default column family...")
    cf["app_name"] = "RocksDB Sample Application"
    cf["version"] = "1.0.0"
    cf["created_at"] = datetime.now().isoformat()
    cf["description"] = "This is a sample RocksDB database for testing purposes"
    cf["config:max_connections"] = "100"
    cf["config:timeout"] = "30"
    cf["config:retry_count"] = "3"
    print("   ✓ Default data created")


def write_users(cf: Rdict) -> None:
    print("👥 Writing data to the users column family...")
    users = [
        {
            "id": "user001", "name": "Taro Yamada", "email": "yamada@example.com",
            "age": 28, "role": "admin",
            "created_at": (datetime.now() - timedelta(days=365)).isoformat(),
        },
        {
            "id": "user002", "name": "Hanako Suzuki", "email": "suzuki@example.com",
            "age": 32, "role": "user",
            "created_at": (datetime.now() - timedelta(days=180)).isoformat(),
        },
        {
            "id": "user003", "name": "Jiro Sato", "email": "sato@example.com",
            "age": 25, "role": "user",
            "created_at": (datetime.now() - timedelta(days=90)).isoformat(),
        },
        {
            "id": "user004", "name": "Misaki Tanaka", "email": "tanaka@example.com",
            "age": 29, "role": "moderator",
            "created_at": (datetime.now() - timedelta(days=120)).isoformat(),
        },
        {
            "id": "user005", "name": "Kenta Ito", "email": "ito@example.com",
            "age": 35, "role": "user",
            "created_at": (datetime.now() - timedelta(days=60)).isoformat(),
        },
    ]
    for user in users:
        cf[user["id"]] = json.dumps(user, ensure_ascii=False)
    print(f"   ✓ Created {len(users)} user records")


def write_products(cf: Rdict) -> None:
    print("🛍️  Writing data to the products column family...")
    products = [
        {"id": "prod001", "name": "Laptop",          "category": "Electronics", "price": 89800, "stock": 15,  "description": "High-performance laptop with a 15.6-inch display"},
        {"id": "prod002", "name": "Wireless Mouse",  "category": "Electronics", "price": 2980,  "stock": 50,  "description": "Silent wireless mouse"},
        {"id": "prod003", "name": "Office Chair",    "category": "Furniture",   "price": 24800, "stock": 8,   "description": "Ergonomic office chair"},
        {"id": "prod004", "name": "USB Drive 64GB",  "category": "Electronics", "price": 1480,  "stock": 100, "description": "High-speed USB flash drive"},
        {"id": "prod005", "name": "Desk Lamp",       "category": "Furniture",   "price": 4980,  "stock": 25,  "description": "Dimmable LED desk lamp"},
        {"id": "prod006", "name": "Keyboard",        "category": "Electronics", "price": 8900,  "stock": 30,  "description": "Mechanical keyboard with RGB lighting"},
        {"id": "prod007", "name": "Monitor Stand",   "category": "Furniture",   "price": 3200,  "stock": 20,  "description": "Height-adjustable monitor stand"},
    ]
    for product in products:
        cf[product["id"]] = json.dumps(product, ensure_ascii=False)
    print(f"   ✓ Created {len(products)} product records")


def write_orders(cf: Rdict) -> None:
    print("📋 Writing data to the orders column family...")
    orders = [
        {
            "id": "order001", "user_id": "user001",
            "products": [{"product_id": "prod001", "quantity": 1, "price": 89800}],
            "total_amount": 89800, "status": "completed",
            "order_date": (datetime.now() - timedelta(days=30)).isoformat(),
        },
        {
            "id": "order002", "user_id": "user002",
            "products": [
                {"product_id": "prod002", "quantity": 2, "price": 2980},
                {"product_id": "prod004", "quantity": 3, "price": 1480},
            ],
            "total_amount": 10400, "status": "completed",
            "order_date": (datetime.now() - timedelta(days=25)).isoformat(),
        },
        {
            "id": "order003", "user_id": "user003",
            "products": [{"product_id": "prod003", "quantity": 1, "price": 24800}],
            "total_amount": 24800, "status": "shipped",
            "order_date": (datetime.now() - timedelta(days=5)).isoformat(),
        },
        {
            "id": "order004", "user_id": "user004",
            "products": [
                {"product_id": "prod005", "quantity": 1, "price": 4980},
                {"product_id": "prod007", "quantity": 1, "price": 3200},
            ],
            "total_amount": 8180, "status": "processing",
            "order_date": (datetime.now() - timedelta(days=2)).isoformat(),
        },
        {
            "id": "order005", "user_id": "user005",
            "products": [{"product_id": "prod006", "quantity": 1, "price": 8900}],
            "total_amount": 8900, "status": "pending",
            "order_date": datetime.now().isoformat(),
        },
    ]
    for order in orders:
        cf[order["id"]] = json.dumps(order, ensure_ascii=False)
    print(f"   ✓ Created {len(orders)} order records")


def write_config(cf: Rdict) -> None:
    print("⚙️  Writing data to the config column family...")
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
        "ui:items_per_page": "20",
    }
    for key, value in configs.items():
        cf[key] = value
    print(f"   ✓ Created {len(configs)} config records")


def write_logs(cf: Rdict) -> None:
    print("📝 Writing data to the logs column family...")
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
        "Backup completed",
    ]
    count = 20
    for i in range(count):
        log_time = datetime.now() - timedelta(hours=random.randint(0, 48))
        log_entry = {
            "timestamp": log_time.isoformat(),
            "level": random.choice(log_levels),
            "message": random.choice(log_messages),
            "source": f"module_{random.randint(1, 5)}",
            "user_id": f"user{random.randint(1, 5):03d}" if random.random() > 0.3 else None,
        }
        log_key = f"log_{log_time.strftime('%Y%m%d_%H%M%S')}_{i:03d}"
        cf[log_key] = json.dumps(log_entry, ensure_ascii=False)
    print(f"   ✓ Created {count} log records")


def open_db_with_cfs(db_path: str) -> tuple[Rdict, list[str]]:
    """Open the database considering existing CFs and create any missing ones."""
    if os.path.exists(db_path):
        existing = Rdict.list_cf(db_path)
    else:
        existing = ['default']

    db = Rdict(db_path, column_families={name: Options() for name in existing})
    for cf_name in NEEDED_CFS:
        if cf_name not in existing:
            db.create_column_family(cf_name, Options())
            existing.append(cf_name)
    return db, existing


def print_summary(db: Rdict, db_path: str) -> None:
    print()
    print("=" * 60)
    print("✅ Sample database creation completed!")
    print("=" * 60)
    print()
    print(f"📁 Database path: {db_path}")
    print()
    print("📊 Column families created:")

    for cf_name in NEEDED_CFS:
        try:
            cf = db if cf_name == 'default' else db.get_column_family(cf_name)
            count = len(list(cf.keys()))
            print(f"   • {cf_name:12s} : {count:3d} records")
        except Exception as e:
            print(f"   • {cf_name:12s} : error ({e})")

    print()
    print("=" * 60)


def main() -> None:
    db_path = sys.argv[1] if len(sys.argv) > 1 else "./sample_rocksdb"
    db_path = os.path.abspath(db_path)

    print()
    print("=" * 60)
    print("🔨 Creating RocksDB sample database")
    print("=" * 60)
    print()
    print(f"📁 Target: {db_path}")
    print()

    if os.path.exists(db_path):
        print("⚠️  Warning: the database already exists")
        response = input("Overwrite? (yes/no): ")
        if response.lower() not in ['yes', 'y']:
            print("Aborted")
            return
        print()

    try:
        db, _ = open_db_with_cfs(db_path)
        try:
            write_default(db)
            write_users(db.get_column_family('users'))
            write_products(db.get_column_family('products'))
            write_orders(db.get_column_family('orders'))
            write_config(db.get_column_family('config'))
            write_logs(db.get_column_family('logs'))

            print_summary(db, db_path)
        finally:
            db.close()
    except Exception as e:
        print()
        print("=" * 60)
        print("❌ An error occurred")
        print("=" * 60)
        print(f"Error: {e}")
        print()
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
