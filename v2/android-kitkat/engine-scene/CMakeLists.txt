cmake_minimum_required(VERSION 3.16)
project(fitd_android_armv7_actual_camera_preview LANGUAGES C CXX)

if(NOT SDL2_ROOT OR NOT SDL2_INCLUDE_DIR OR NOT FITD_UPSTREAM_DIR)
    message(FATAL_ERROR "Require SDL2_ROOT, SDL2_INCLUDE_DIR and FITD_UPSTREAM_DIR")
endif()
if(NOT EXISTS "${FITD_UPSTREAM_DIR}/FitdLib/unpack.cpp")
    message(FATAL_ERROR "Missing verified upstream FITD source tree")
endif()

# Use the original FITD engine PAK reader/decompressor unchanged.
add_library(fitd_scene_core STATIC
    "${FITD_UPSTREAM_DIR}/FitdLib/unpack.cpp"
    "${FITD_UPSTREAM_DIR}/FitdLib/pak.cpp"
    "${FITD_UPSTREAM_DIR}/FitdLib/cosTable.cpp"
)
set_target_properties(fitd_scene_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(fitd_scene_core PRIVATE "${FITD_UPSTREAM_DIR}/FitdLib")
target_compile_features(fitd_scene_core PRIVATE cxx_std_17)
target_compile_options(fitd_scene_core PRIVATE -O2 -DNDEBUG -fno-strict-aliasing)

# This generates libmain.so for SDLActivity. It displays actual CAMERA.PAK
# background frames but is not yet the full interactive FITD simulation.
add_library(main SHARED fitd_scene_preview.cpp)
target_include_directories(main PRIVATE
    "${FITD_UPSTREAM_DIR}/FitdLib"
    "${SDL2_INCLUDE_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/.."
)
target_compile_features(main PRIVATE cxx_std_17)
target_compile_options(main PRIVATE -O2 -DNDEBUG -fno-strict-aliasing)
target_link_libraries(main PRIVATE fitd_scene_core
    "${SDL2_ROOT}/libSDL2.so" GLESv2 EGL android log z)
target_link_options(main PRIVATE "-Wl,--no-undefined")
