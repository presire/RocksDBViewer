#include <rocksdb/db.h>
#include <iostream>
#include <vector>
#include <memory>

/**
 * @brief サンプルRocksDBデータベースを生成する
 *
 * 指定パスに[default]、[users]、[products]、[orders]、[config]、[logs]の6カラムファミリーを持つDBを作成する
 * 既存DBがない場合は新規作成し、各カラムファミリーへテスト用JSONデータを書き込む
 *
 * @param argc コマンドライン引数の数
 * @param argv コマンドライン引数の配列
 * @note argv[1]で出力先DBパスを指定できる
 * @return 成功時は0、失敗時は1
 */
int main(int argc, char* argv[]) {
    std::string db_path = (argc > 1) ? argv[1] : "sample_rocksdb";

    std::vector<std::string> cf_names = {"default", "users", "products", "orders", "config", "logs"};

    rocksdb::Options options;
    options.create_if_missing = true;

    std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
    for (const auto& name : cf_names) {
        descriptors.emplace_back(name, rocksdb::ColumnFamilyOptions());
    }

    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    std::unique_ptr<rocksdb::DB> db;
    rocksdb::Status s = rocksdb::DB::Open(rocksdb::DBOptions(), db_path, descriptors, &handles, &db);

    if (!s.ok()) {
        std::cerr << "Initial open failed (expected for new DB): " << s.ToString() << std::endl;
        // 新規DBを作成する
        s = rocksdb::DB::Open(options, db_path, &db);
        if (!s.ok()) {
            std::cerr << "Failed to create DB: " << s.ToString() << std::endl;
            return 1;
        }

        // 不足するカラムファミリーを作成する
        for (const auto& name : cf_names) {
            if (name == "default") continue;
            rocksdb::ColumnFamilyHandle* cf;
            s = db->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), name, &cf);
            if (s.ok()) {
                handles.push_back(cf);
            } else {
                std::cerr << "Failed to create CF " << name << ": " << s.ToString() << std::endl;
            }
        }

        // すべてのカラムファミリーを指定して再接続する
        for (auto* h : handles) {
            db->DestroyColumnFamilyHandle(h);
        }

        db->Close();
        db.reset();
        handles.clear();

        s = rocksdb::DB::Open(rocksdb::DBOptions(), db_path, descriptors, &handles, &db);
        if (!s.ok()) {
            std::cerr << "Failed to re-open DB with CFs: " << s.ToString() << std::endl;
            return 1;
        }
    }

    /**
     * @brief 指定名のカラムファミリーハンドルを取得する
     * @param name カラムファミリー名
     * @return 対応するハンドルポインタ
     * @note 見つからない場合は既定のカラムファミリーを返す
     */
    auto getHandle = [&](const std::string& name) -> rocksdb::ColumnFamilyHandle* {
        for (auto* h : handles) {
            if (h->GetName() == name) return h;
        }
        return db->DefaultColumnFamily();
    };

    // 既定カラムファミリーへサンプルデータを書き込む
    db->Put(rocksdb::WriteOptions(), getHandle("default"), "app_name", "RocksDB Sample Application");
    db->Put(rocksdb::WriteOptions(), getHandle("default"), "version", "1.0.0");
    db->Put(rocksdb::WriteOptions(), getHandle("default"), "description", "This is a sample RocksDB database for testing purposes");

    // [users]へサンプルデータを書き込む
    db->Put(rocksdb::WriteOptions(), getHandle("users"), "user001", R"({"id":"user001","name":"Taro Yamada","email":"yamada@example.com","age":28,"role":"admin"})");
    db->Put(rocksdb::WriteOptions(), getHandle("users"), "user002", R"({"id":"user002","name":"Hanako Suzuki","email":"suzuki@example.com","age":32,"role":"user"})");
    db->Put(rocksdb::WriteOptions(), getHandle("users"), "user003", R"({"id":"user003","name":"Jiro Sato","email":"sato@example.com","age":25,"role":"user"})");

    // [products]へサンプルデータを書き込む
    db->Put(rocksdb::WriteOptions(), getHandle("products"), "prod001", R"({"id":"prod001","name":"Laptop","category":"Electronics","price":89800,"stock":15})");
    db->Put(rocksdb::WriteOptions(), getHandle("products"), "prod002", R"({"id":"prod002","name":"Wireless Mouse","category":"Electronics","price":2980,"stock":50})");

    // [orders]へサンプルデータを書き込む
    db->Put(rocksdb::WriteOptions(), getHandle("orders"), "order001", R"({"id":"order001","user_id":"user001","total_amount":89800,"status":"completed"})");
    db->Put(rocksdb::WriteOptions(), getHandle("orders"), "order002", R"({"id":"order002","user_id":"user002","total_amount":10400,"status":"completed"})");

    // [config]へサンプルデータを書き込む
    db->Put(rocksdb::WriteOptions(), getHandle("config"), "system:debug_mode", "true");
    db->Put(rocksdb::WriteOptions(), getHandle("config"), "system:log_level", "INFO");
    db->Put(rocksdb::WriteOptions(), getHandle("config"), "ui:theme", "light");

    // [logs]へサンプルデータを書き込む
    db->Put(rocksdb::WriteOptions(), getHandle("logs"), "log_20240101_000001", R"({"timestamp":"2024-01-01T00:00:01","level":"INFO","message":"Application started"})");
    db->Put(rocksdb::WriteOptions(), getHandle("logs"), "log_20240101_000002", R"({"timestamp":"2024-01-01T00:00:02","level":"DEBUG","message":"Database connection established"})");

    for (auto* h : handles) {
        db->DestroyColumnFamilyHandle(h);
    }

    db->Close();

    std::cout << "Sample database created at: " << db_path << std::endl;
    return 0;
}
