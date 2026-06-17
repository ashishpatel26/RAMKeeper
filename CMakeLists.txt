cmake_minimum_required(VERSION 3.20)
project(RAMKeeper VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/main.cpp
    src/tray.cpp
    src/cleaner.cpp
    src/stats.cpp
    src/config.cpp
    src/settings.cpp
    src/history.cpp
    src/status.cpp
    res/resources.rc
)

add_executable(RAMKeeper WIN32 ${SOURCES})

target_include_directories(RAMKeeper PRIVATE include)

if(MSVC)
    set_property(TARGET RAMKeeper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(RAMKeeper PRIVATE /W4 /O2 /WX- /utf-8)
    target_link_options(RAMKeeper PRIVATE /MANIFEST:NO /OPT:REF /OPT:ICF)
else()
    target_compile_options(RAMKeeper PRIVATE -O2 -Wall -Wextra)
    target_link_options(RAMKeeper PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
endif()

target_link_libraries(RAMKeeper PRIVATE
    kernel32
    user32
    shell32
    psapi
    advapi32
    gdi32
    dwmapi
)

if(NOT MSVC)
    target_link_options(RAMKeeper PRIVATE $<$<CONFIG:Release>:-s>)
endif()

install(TARGETS RAMKeeper DESTINATION .)
