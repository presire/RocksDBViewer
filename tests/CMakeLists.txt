# RocksDBViewerの単体テスト
# QTestバイナリを作成してCTestへ登録する
# Issue#2のformat_version保持とopenedWithDefaultsの警告通知を検証する

# Qt6Testを検索する
find_package(Qt6 REQUIRED COMPONENTS Core Gui Test)

# テストソース一覧
# アプリケーション本体のRocksDBBackendとSettingsMigrationを実DBのままリンクする
set(TEST_SOURCES
    test_rocksdbbackend.cpp
    ../src/backend/RocksDBBackend.cpp
    ../src/utils/SettingsMigration.cpp
)

# ウィンドウジオメトリ補正の単体テストを作成してCTestへ登録する
qt_add_executable(test_windowgeometry
    test_windowgeometry.cpp
    ../src/utils/WindowGeometry.cpp
    ../src/utils/WindowGeometry.h
)
target_include_directories(test_windowgeometry PRIVATE ../src)
target_link_libraries(test_windowgeometry PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME test_windowgeometry COMMAND test_windowgeometry)

# テストヘッダ一覧
set(TEST_HEADERS
    ../src/backend/RocksDBBackend.h
    ../src/utils/SettingsMigration.h
    ../src/utils/SettingsKeys.h
)

# テストターゲットを作成する
qt_add_executable(test_rocksdbbackend
    ${TEST_SOURCES}
    ${TEST_HEADERS}
)

# インクルードパスとバージョン定義
target_include_directories(test_rocksdbbackend PRIVATE
    ../src
    ${ROCKSDB_INCLUDE_DIR}
)

target_compile_definitions(test_rocksdbbackend PRIVATE
    ROCKSDBVIEWER_VERSION="test"
)

# ライブラリをリンクする
target_link_libraries(test_rocksdbbackend PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    ${ROCKSDB_LIBRARY}
)

if(WIN32)
    target_link_libraries(test_rocksdbbackend PRIVATE shlwapi rpcrt4)
endif()

# CTestへ登録する
# QTEST_MAINマクロがQtTest標準のmainを提供する
add_test(NAME test_rocksdbbackend COMMAND test_rocksdbbackend)
