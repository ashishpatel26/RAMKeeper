cmake_minimum_required(VERSION 3.20)
project(RAMKeeper VERSION 1.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/main.cpp
    src/tray.cpp
    src/cleaner.cpp
    src/stats.cpp
    src/config.cpp
    src/settings.cpp
    res/resources.rc
)

add_executable(RAMKeeper WIN32 ${SOURCES})

target_include_directories(RAMKeeper PRIVATE include)

# Static CRT — no VCRUNTIME / MSVCRT dependency
if(MSVC)
    set_property(TARGET RAMKeeper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(RAMKeeper PRIVATE /W4 /O2 /WX- /utf-8)
    # Manifest already embedded via resources.rc (ID=1) — suppress linker manifest to avoid duplicate
    target_link_options(RAMKeeper PRIVATE /MANIFEST:NO /OPT:REF /OPT:ICF)
else()
    # MinGW
    target_compile_options(RAMKeeper PRIVATE -O2 -Wall -Wextra)
    target_link_options(RAMKeeper PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
endif()

target_link_libraries(RAMKeeper PRIVATE
    kernel32
    user32
    shell32
    psapi
    advapi32
)

# Strip symbols in release (MinGW)
if(NOT MSVC)
    target_link_options(RAMKeeper PRIVATE $<$<CONFIG:Release>:-s>)
endif()

install(TARGETS RAMKeeper DESTINATION .)
