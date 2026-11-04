cmake_minimum_required(VERSION 3.16)
project(fitd_android_armv7_original_body_preview LANGUAGES C CXX)

if(NOT SDL2_ROOT OR NOT SDL2_INCLUDE_DIR OR NOT FITD_UPSTREAM_DIR)
    message(FATAL_ERROR "Require SDL2_ROOT, SDL2_INCLUDE_DIR and FITD_UPSTREAM_DIR")
endif()
if(NOT EXISTS "${FITD_UPSTREAM_DIR}/FitdLib/unpack.cpp")
    message(FATAL_ERROR "Missing pinned source for original FITD PAK decoder")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/fitd_scene_model_preview.cpp")
    message(FATAL_ERROR "Run engine-character/patch_scene07.py before CMake configure")
endif()

add_library(fitd_model_core STATIC
    "${FITD_UPSTREAM_DIR}/FitdLib/unpack.cpp"
    "${FITD_UPSTREAM_DIR}/FitdLib/pak.cpp"
    "${FITD_UPSTREAM_DIR}/FitdLib/cosTable.cpp"
)
set_target_properties(fitd_model_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(fitd_model_core PRIVATE cxx_std_17)
target_include_directories(fitd_model_core PRIVATE "${FITD_UPSTREAM_DIR}/FitdLib")
target_compile_options(fitd_model_core PRIVATE -O2 -DNDEBUG -fno-strict-aliasing)

add_library(main SHARED
    "${CMAKE_CURRENT_BINARY_DIR}/fitd_scene_model_preview.cpp"
    fitd_character_preview.cpp
)
target_compile_features(main PRIVATE cxx_std_17)
target_include_directories(main PRIVATE
    "${SDL2_INCLUDE_DIR}"
    "${FITD_UPSTREAM_DIR}/FitdLib"
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../engine-gameplay"
    "${CMAKE_CURRENT_LIST_DIR}/.."
)
target_compile_options(main PRIVATE -O2 -DNDEBUG -fno-strict-aliasing)
target_link_libraries(main PRIVATE fitd_model_core
    "${SDL2_ROOT}/libSDL2.so" GLESv2 EGL android log z)
target_link_options(main PRIVATE "-Wl,--no-undefined")
