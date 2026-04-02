cmake_minimum_required(VERSION 3.25)
project(uintDebugger VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UINTDEBUGGER_GITHUB_OWNER "uintptr")
set(UINTDEBUGGER_GITHUB_REPOSITORY "uintDebugger")
set(UINTDEBUGGER_UPDATE_MANIFEST_ASSET "uintDebugger-update-manifest.json")
set(UINTDEBUGGER_UPDATE_USER_AGENT "uintDebugger-updater")

if(NOT WIN32)
    message(FATAL_ERROR "uintDebugger only supports Windows")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network)
qt_standard_project_setup()

add_subdirectory(qtUintDebugger)
add_subdirectory(uintDebuggerUpdater)
