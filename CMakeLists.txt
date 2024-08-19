cmake_minimum_required(VERSION 3.16)

project(VideoTransform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set the Qt6 installation path
set(Qt6_DIR "/opt/homebrew/Cellar/qt/6.7.0_1/lib/cmake/Qt6")

# Find Qt packages
find_package(Qt6 COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Concurrent REQUIRED)

# FFmpeg detection
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/ffmpegwrapper.cpp
    src/ffmpegwrapper.h
)

add_executable(VideoTransform ${PROJECT_SOURCES})

target_link_libraries(VideoTransform PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent
    PkgConfig::FFMPEG
)

target_include_directories(VideoTransform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    /opt/homebrew/include
    /opt/homebrew/opt/qt/include
    /opt/homebrew/Cellar/qt/6.7.0_1/include
)

# Print Qt6 version and location for debugging
message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Qt6 dir: ${Qt6_DIR}")