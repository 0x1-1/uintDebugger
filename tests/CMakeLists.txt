# tests/CMakeLists.txt
# CTest targets for uintDebugger automated smoke-tests.
# Both executables are plain console apps — no install step needed.

enable_testing()

# ---------------------------------------------------------------------------
# test_updater_paths
#   Standalone Win32 test: no Qt dependency.
#   Validates UpdaterSafety::IsPathSafe and IsSourceRegularFile from
#   uintDebuggerUpdater/PathSafety.h using real temp files.
#   Entry point: wmain (Unicode console, MSVC default).
# ---------------------------------------------------------------------------
add_executable(test_updater_paths test_updater_paths.cpp)

target_include_directories(test_updater_paths PRIVATE
    ${CMAKE_SOURCE_DIR}/uintDebuggerUpdater
)

# Console subsystem (not GUI); wmain is the wide-char console entry point.
set_target_properties(test_updater_paths PROPERTIES
    WIN32_EXECUTABLE FALSE
)

# kernel32/user32 are implicit MSVC link defaults; std::filesystem is in the
# C++ runtime — no extra target_link_libraries needed.

add_test(NAME UpdaterPaths COMMAND test_updater_paths)

# Qt6 is already found by the root CMakeLists, but explicitly request Core
# here so this subdirectory remains independently buildable.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

# ---------------------------------------------------------------------------
# test_updater_qt_paths
#   Qt-side updater path validation test. Catches separator-normalization
#   regressions in the in-app updater manifest/download checks.
# ---------------------------------------------------------------------------
add_executable(test_updater_qt_paths test_updater_qt_paths.cpp)

target_link_libraries(test_updater_qt_paths PRIVATE Qt6::Core)

target_include_directories(test_updater_qt_paths PRIVATE
    ${CMAKE_SOURCE_DIR}/uintDebuggerUpdater/UpdaterWidget
)

set_target_properties(test_updater_qt_paths PROPERTIES
    WIN32_EXECUTABLE FALSE
)

add_test(NAME UpdaterQtPaths COMMAND test_updater_qt_paths)

# ---------------------------------------------------------------------------
# test_ndb_schema
#   Qt XML roundtrip test: verifies schemaVersion attribute handling.
#   Uses QXmlStreamReader/Writer (Qt6::Core) — no QApplication required.
# ---------------------------------------------------------------------------
add_executable(test_ndb_schema test_ndb_schema.cpp)

target_link_libraries(test_ndb_schema PRIVATE Qt6::Core)

set_target_properties(test_ndb_schema PROPERTIES
    WIN32_EXECUTABLE FALSE
)

add_test(NAME NdbSchema COMMAND test_ndb_schema)
