# Unit tests for RocksDB Viewer
#
# Builds the QTest binary and registers it with CTest. Tests exercise the
# Issue #2 fix (format_version preservation across edits) and the fallback
# warning surface (openedWithDefaults).

find_package(Qt6 REQUIRED COMPONENTS Core Gui Test)

set(TEST_SOURCES
    test_rocksdbbackend.cpp
    ../src/backend/RocksDBBackend.cpp
    ../src/utils/SettingsMigration.cpp
)

set(TEST_HEADERS
    ../src/backend/RocksDBBackend.h
    ../src/utils/SettingsMigration.h
    ../src/utils/SettingsKeys.h
)

qt_add_executable(test_rocksdbbackend
    ${TEST_SOURCES}
    ${TEST_HEADERS}
)

target_include_directories(test_rocksdbbackend PRIVATE
    ../src
    ${ROCKSDB_INCLUDE_DIR}
)

target_compile_definitions(test_rocksdbbackend PRIVATE
    ROCKSDBVIEWER_VERSION="test"
)

target_link_libraries(test_rocksdbbackend PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    ${ROCKSDB_LIBRARY}
)

if(WIN32)
    target_link_libraries(test_rocksdbbackend PRIVATE shlwapi rpcrt4)
endif()

# QTest uses QtTest's built-in main via QTEST_MAIN macro, so no custom main needed.
add_test(NAME test_rocksdbbackend COMMAND test_rocksdbbackend)
