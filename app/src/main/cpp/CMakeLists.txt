cmake_minimum_required(VERSION 3.22.1)

project("ndk_visualizer")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Strict compiler optimizations for ARMv8-A (Tensor G5 / Pixel 10 Pro)
add_compile_options(
    -O3
    -march=armv8-a+crypto
    -flto
    -fexceptions
    -frtti
    -Wall
    -Wextra
)

# Find NDK libraries
find_library(log-lib log)
find_library(android-lib android)
find_library(gles3-lib GLESv3)
find_library(egl-lib EGL)

# Oboe (FetchContent)
include(FetchContent)
FetchContent_Declare(
    oboe
    GIT_REPOSITORY https://github.com/google/oboe
    GIT_TAG main
)
FetchContent_MakeAvailable(oboe)

# LiteRT (TensorFlow Lite) - Assuming standard location or we point to it
# In a real setup, this would be provided via AAR or manual path.
# We will define the include paths and link against the shared library.
# set(LITERT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/litert)
# set(LITERT_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/${ANDROID_ABI})

# add_library(litert SHARED IMPORTED)
# set_target_properties(litert PROPERTIES IMPORTED_LOCATION ${LITERT_LIB_DIR}/libtensorflowlite_jni.so)

add_library(${CMAKE_PROJECT_NAME} SHARED
    audio_capture.cpp
    # tpu_inference.cpp
    engine.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # ${LITERT_INCLUDE_DIR}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    ${log-lib}
    ${android-lib}
    ${gles3-lib}
    ${egl-lib}
    oboe
    # litert
)
