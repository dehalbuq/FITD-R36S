cmake_minimum_required(VERSION 3.16)
project(FITD_API19_real_gameplay_core_compile LANGUAGES CXX)

if(NOT FITD_UPSTREAM_DIR OR NOT SDL2_INCLUDE_DIR OR NOT FMT_INCLUDE_DIR)
    message(FATAL_ERROR "Set FITD_UPSTREAM_DIR, SDL2_INCLUDE_DIR and FMT_INCLUDE_DIR")
endif()

# Compile OBJECT code from the original FitdMain() / PlayWorld() game loop
# alongside the Android SDL2 platform. This is a compile gate, not an APK:
# the OpenGL ES 2.0 full renderer, shaders, audio and link are next.
add_library(fitd_real_runtime_objects OBJECT
    fitd_sdl2_platform.cpp
    fitd_sdl2_input.cpp
    "${FITD_UPSTREAM_DIR}/FitdLib/main.cpp"
    "${FITD_UPSTREAM_DIR}/FitdLib/mainLoop.cpp"
)
set_target_properties(fitd_real_runtime_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(fitd_real_runtime_objects PRIVATE cxx_std_17)
target_include_directories(fitd_real_runtime_objects PRIVATE
    "${FITD_UPSTREAM_DIR}/FitdLib"
    "${SDL2_INCLUDE_DIR}"
    "${FMT_INCLUDE_DIR}"
    "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bgfx/include"
    "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bx/include"
    "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bimg/include"
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_LIST_DIR}/../engine-gameplay"
)
target_compile_options(fitd_real_runtime_objects PRIVATE
    -O2 -DNDEBUG -fno-strict-aliasing -fvisibility=hidden
)
target_compile_definitions(fitd_real_runtime_objects PRIVATE
    FMT_HEADER_ONLY=1 BGFX_CONFIG_MULTITHREADED=0
)
