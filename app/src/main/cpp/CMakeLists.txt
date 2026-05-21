cmake_minimum_required(VERSION 3.22.1)

project("vulkan_visualizer")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find NDK libraries
find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)
find_library(media-lib mediandk)

# Oboe (FetchContent)
include(FetchContent)
FetchContent_Declare(
    oboe
    GIT_REPOSITORY https://github.com/google/oboe
    GIT_TAG main
)
FetchContent_MakeAvailable(oboe)

# Define our library
add_library(${CMAKE_PROJECT_NAME} SHARED
    audio_capture.cpp
    engine.cpp
    video_decoder.cpp
    vulkan_renderer.cpp
)

# Apply optimizations and warnings ONLY to our target
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -O3
    -march=armv8-a+crypto
    -flto
    -fexceptions
    -frtti
    -Wall
    -Wextra
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
    ${media-lib}
    oboe
)
