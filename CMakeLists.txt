cmake_minimum_required(VERSION 3.16)
project(RocksDBViewer VERSION 1.2.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 path
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 LinguistTools)

# RocksDB
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

set(SOURCES
    src/main.cpp
    src/backend/RocksDBBackend.cpp
    src/backend/EntryModel.cpp
    src/backend/FilterProxyModel.cpp
    src/utils/JsonUtils.cpp
    src/utils/I18nManager.cpp
    src/utils/FileSystemModel.cpp
    src/utils/SettingsMigration.cpp
)

set(HEADERS
    src/backend/RocksDBBackend.h
    src/backend/EntryModel.h
    src/backend/FilterProxyModel.h
    src/utils/JsonUtils.h
    src/utils/I18nManager.h
    src/utils/FileSystemModel.h
    src/utils/SettingsMigration.h
    src/utils/SettingsKeys.h
)

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

# Translation files
set(TS_FILES
    i18n/rocksdbviewer_ja.ts
    i18n/rocksdbviewer_en.ts
)

set(QML_MODULE_NAME "RocksDBViewerApp")
set(EXECUTABLE_NAME "RocksDBViewer")

qt_add_executable(${EXECUTABLE_NAME}
    WIN32
    ${SOURCES}
    ${HEADERS}
)

qt_policy(SET QTP0004 NEW)

qt_add_qml_module(${EXECUTABLE_NAME}
    URI ${QML_MODULE_NAME}
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCE_PREFIX /
    NO_CACHEGEN
)

# アイコンリソースを別途追加 (QMLモジュールとは独立)
qt6_add_resources(${EXECUTABLE_NAME} "icons"
    PREFIX "/RocksDBViewerApp"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        assets/RocksDBViewer@128.png
        assets/RocksDBViewer@256.png
        assets/RocksDBViewer@512.png
)

qt_add_translations(${EXECUTABLE_NAME}
    TS_FILES ${TS_FILES}
)

# ----------------------------------------------------------------------------
# Test infrastructure
# ----------------------------------------------------------------------------
option(BUILD_TESTING "Build unit tests (requires Qt6::Test)" ON)
if(BUILD_TESTING)
    # Probe for Qt6Test before entering tests/. Some CI images ship Qt6
    # without the Test component; in that case silently disable tests instead
    # of failing the entire configure step. Install qt6-test (or equivalent)
    # to re-enable tests on the host.
    find_package(Qt6 COMPONENTS Test QUIET)
    if(NOT Qt6Test_FOUND)
        message(STATUS "Qt6Test not found; disabling BUILD_TESTING")
        set(BUILD_TESTING OFF CACHE BOOL "Build unit tests" FORCE)
    else()
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

# Sample database generator (used by manual testing and the durability script)
add_executable(create_sample_db EXCLUDE_FROM_ALL tools/create_sample_db.cpp)
target_include_directories(create_sample_db PRIVATE ${ROCKSDB_INCLUDE_DIR})
target_link_libraries(create_sample_db PRIVATE ${ROCKSDB_LIBRARY})
if(WIN32)
    target_link_libraries(create_sample_db PRIVATE shlwapi rpcrt4)
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE
    src
    ${ROCKSDB_INCLUDE_DIR}
)

target_compile_definitions(RocksDBViewer PRIVATE
    ROCKSDBVIEWER_VERSION="${PROJECT_VERSION}"
)

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

if(NOT DEFINED CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR "bin")
endif()

# Install executable
install(TARGETS ${EXECUTABLE_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT WIN32)
    # Configure and install .desktop file
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/RocksDBViewer.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/RocksDBViewer.desktop
        @ONLY
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/RocksDBViewer.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )

    # Install icon files to hicolor theme (multiple sizes)
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

    # CPack RPM configuration
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
