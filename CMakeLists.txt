cmake_minimum_required(VERSION 3.16)
# RocksDBViewerのビルド設定
# Qt6/QMLアプリ本体、単体テスト、サンプルDB生成ツール、インストールとRPMパッケージを定義する
project(RocksDBViewer VERSION 1.2.5 LANGUAGES CXX)

# コンパイラ設定
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6コンポーネントを検索する
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 LinguistTools)

# RocksDBライブラリとヘッダを検索する
# ROCKSDB_ROOT配下のみを検索し、システム側のRocksDBを誤って採用しない
find_library(ROCKSDB_LIBRARY NAMES rocksdb PATHS "${ROCKSDB_ROOT}/lib64" "${ROCKSDB_ROOT}/lib" NO_DEFAULT_PATH)
find_path(ROCKSDB_INCLUDE_DIR NAMES rocksdb/db.h PATHS "${ROCKSDB_ROOT}/include" NO_DEFAULT_PATH)

if(NOT ROCKSDB_LIBRARY)
    message(FATAL_ERROR "RocksDB library not found at ${ROCKSDB_ROOT}/lib64")
endif()
if(NOT ROCKSDB_INCLUDE_DIR)
    message(FATAL_ERROR "RocksDB headers not found at ${ROCKSDB_ROOT}/include")
endif()

message(STATUS "RocksDB library: ${ROCKSDB_LIBRARY}")
message(STATUS "RocksDB include: ${ROCKSDB_INCLUDE_DIR}")

# ソースファイル一覧
set(SOURCES
    src/main.cpp
    src/backend/RocksDBBackend.cpp
    src/backend/EntryModel.cpp
    src/backend/FilterProxyModel.cpp
    src/utils/JsonUtils.cpp
    src/utils/I18nManager.cpp
    src/utils/FileSystemModel.cpp
    src/utils/SettingsMigration.cpp
    src/utils/WindowGeometry.cpp
)

# ヘッダーファイル一覧
set(HEADERS
    src/backend/RocksDBBackend.h
    src/backend/EntryModel.h
    src/backend/FilterProxyModel.h
    src/utils/JsonUtils.h
    src/utils/I18nManager.h
    src/utils/FileSystemModel.h
    src/utils/SettingsMigration.h
    src/utils/SettingsKeys.h
    src/utils/WindowGeometry.h
)

# QMLモジュールに登録するQMLファイル
# main.qmlは画面全体のレイアウト、Theme.qmlは配色定数、components/配下は画面を構成する各部品
set(QML_FILES
    qml/main.qml
    qml/Theme.qml
    qml/components/HeaderBar.qml
    qml/components/DatabasePanel.qml
    qml/components/ControlBar.qml
    qml/components/DataTable.qml
    qml/components/EditModal.qml
    qml/components/ToastManager.qml
    qml/components/WelcomePage.qml
    qml/components/StatsPanel.qml
    qml/components/SkeletonMenu.qml
    qml/components/FilePickerDialog.qml
)

# 翻訳ファイル
set(TS_FILES
    i18n/rocksdbviewer_ja.ts
    i18n/rocksdbviewer_en.ts
)

# モジュール名と実行ファイル名
# QMLモジュールURI
set(QML_MODULE_NAME "RocksDBViewerApp")
set(EXECUTABLE_NAME "RocksDBViewer")

# 実行ファイルターゲットを作成する
qt_add_executable(${EXECUTABLE_NAME}
    WIN32
    ${SOURCES}
    ${HEADERS}
)

qt_policy(SET QTP0004 NEW)

# QMLモジュールを登録する
# RESOURCE_PREFIXを[/]にするとQMLファイルはqrc:/RocksDBViewerApp/配下に配置される
# NO_CACHEGENは開発中のQMLキャッシュ生成を省略する
qt_add_qml_module(${EXECUTABLE_NAME}
    URI ${QML_MODULE_NAME}
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCE_PREFIX /
    NO_CACHEGEN
)

# QMLモジュールとは独立したアイコンリソースを追加する
qt6_add_resources(${EXECUTABLE_NAME} "icons"
    PREFIX "/RocksDBViewerApp"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        assets/RocksDBViewer@128.png
        assets/RocksDBViewer@256.png
        assets/RocksDBViewer@512.png
)

# 翻訳ファイルを組み込む
# lupdate/lreleaseが自動実行され、.qmファイルはqrcの:/i18n/配下へ配置される
qt_add_translations(${EXECUTABLE_NAME}
    TS_FILES ${TS_FILES}
)

# テスト基盤
# BUILD_TESTINGが有効な場合のみ単体テストをビルドする
option(BUILD_TESTING "Build unit tests (requires Qt6::Test)" ON)
if(BUILD_TESTING)
    # tests/へ進む前にQt6Testを検索する
    # Testコンポーネントがない環境では設定全体を失敗させずテストを無効にする
    # ホストへqt6-testまたは同等パッケージを導入するとテストを再び有効化できる
    find_package(Qt6 COMPONENTS Test QUIET)
    if(NOT Qt6Test_FOUND)
        message(STATUS "Qt6Test not found; disabling BUILD_TESTING")
        set(BUILD_TESTING OFF CACHE BOOL "Build unit tests" FORCE)
    else()
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

# サンプルDB生成ツール
# 手動テストと耐久性テストで使うサンプルDB生成ツール
add_executable(create_sample_db EXCLUDE_FROM_ALL tools/create_sample_db.cpp)
target_include_directories(create_sample_db PRIVATE ${ROCKSDB_INCLUDE_DIR})
target_link_libraries(create_sample_db PRIVATE ${ROCKSDB_LIBRARY})
if(WIN32)
    target_link_libraries(create_sample_db PRIVATE shlwapi rpcrt4)
endif()

# インクルードパスとバージョン定義
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    src
    ${ROCKSDB_INCLUDE_DIR}
)

target_compile_definitions(RocksDBViewer PRIVATE
    ROCKSDBVIEWER_VERSION="${PROJECT_VERSION}"
)

# ライブラリをリンクする
if(WIN32)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        ${ROCKSDB_LIBRARY}
        shlwapi
        rpcrt4
    )
else()
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        ${ROCKSDB_LIBRARY}
    )
endif()

# インストール設定
if(NOT DEFINED CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR "bin")
endif()

# 実行ファイルをインストールする
install(TARGETS ${EXECUTABLE_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT WIN32)
    # .desktopファイルを設定してインストールする
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/RocksDBViewer.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/RocksDBViewer.desktop
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/RocksDBViewer.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )

    # 複数サイズのアイコンをhicolorテーマへインストールする
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/RocksDBViewer@128.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps
        RENAME RocksDBViewer.png
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/RocksDBViewer@256.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME RocksDBViewer.png
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/RocksDBViewer@512.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps
        RENAME RocksDBViewer.png
    )

    # RPMパッケージ設定
    # CPackのRPM設定
    set(CPACK_PACKAGE_NAME "RocksDBViewer")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "Presire")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Desktop GUI for browsing and editing RocksDB databases")
    set(CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-core >= 6.5, qt6-gui >= 6.5, qt6-quick >= 6.5, qt6-quickcontrols2 >= 6.5")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/presire/RocksDBViewer")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "A native desktop application for browsing and editing RocksDB databases, built with Qt6/QML and C++20.")
    include(CPack)
endif()
