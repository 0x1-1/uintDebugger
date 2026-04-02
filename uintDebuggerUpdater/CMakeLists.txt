cmake_minimum_required(VERSION 3.25)

# updater.exe: WinAPI-only bootstrap that copies update files and launches uintDebugger.exe
# Intentionally has no Qt dependency to keep it small and dependency-free.
add_executable(updater main.cpp)

target_compile_definitions(updater PRIVATE
    WIN32
    _WINDOWS
    UNICODE
    _UNICODE
)

target_link_libraries(updater PRIVATE
    kernel32
    user32
)
