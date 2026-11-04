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
    fitd_android_entry.cpp
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


# Phase 2: compile all original gameplay, actor, collision, script, save,
# scene and PAK modules as real ARMv7 object code. The few files below
# require SDL3, BGFX, ImGui editor or unavailable OpenAL/MP3 headers and
# will be replaced with Android-specific implementations in later phases.
file(GLOB FITD_RUNTIME_ALL_SOURCES "${FITD_UPSTREAM_DIR}/FitdLib/*.cpp")
set(FITD_RUNTIME_EXCLUDED
    bgfxGlue.cpp
    rendererBGFX.cpp
    imguiBGFX.cpp
    osystemSDL.cpp
    input.cpp
    osystemAL_mp3.cpp
    debugger.cpp
    debugFont.cpp
)
foreach(excluded IN LISTS FITD_RUNTIME_EXCLUDED)
    list(REMOVE_ITEM FITD_RUNTIME_ALL_SOURCES
         "${FITD_UPSTREAM_DIR}/FitdLib/${excluded}")
endforeach()
list(LENGTH FITD_RUNTIME_ALL_SOURCES FITD_RUNTIME_SOURCE_COUNT)
message(STATUS "FITD genuine gameplay and resource module sources: ${FITD_RUNTIME_SOURCE_COUNT}")
add_library(fitd_android_gameplay_core_objects OBJECT
    ${FITD_RUNTIME_ALL_SOURCES}
    fitd_sdl2_platform.cpp
    fitd_sdl2_input.cpp
    fitd_android_entry.cpp
)
set_target_properties(fitd_android_gameplay_core_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
target_compile_features(fitd_android_gameplay_core_objects PRIVATE cxx_std_17)
target_include_directories(fitd_android_gameplay_core_objects PRIVATE
    "${FITD_UPSTREAM_DIR}/FitdLib"
    "${SDL2_INCLUDE_DIR}"
    "${FMT_INCLUDE_DIR}"
    "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bgfx/include"
    "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bx/include"
    "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bimg/include"
    "${FITD_UPSTREAM_DIR}/ThirdParty/soloud.cmake/soloud/include"
    "${CMAKE_CURRENT_LIST_DIR}/.."
    "${CMAKE_CURRENT_LIST_DIR}/../engine-gameplay"
)
target_compile_options(fitd_android_gameplay_core_objects PRIVATE
    -O2 -DNDEBUG -fno-strict-aliasing -fvisibility=hidden)
target_compile_definitions(fitd_android_gameplay_core_objects PRIVATE
    FMT_HEADER_ONLY=1 BGFX_CONFIG_MULTITHREADED=0)


# Phase 3: attempt a TRUE installable engine library. Uses the original
# FitdMain/PlayWorld and all non-debug gameplay source modules with our GLES2
# compatibility renderer instead of upstream BGFX/SDL3. Sound is deliberately
# silent for this first boot prototype; never treat it as release-ready.
if(FITD_LINK_REAL_ANDROID_GAME)
    if(NOT SDL2_ROOT OR NOT EXISTS "${SDL2_ROOT}/libSDL2.so")
        message(FATAL_ERROR "FITD_LINK_REAL_ANDROID_GAME requires SDL2_ROOT/libSDL2.so")
    endif()
    set(FITD_BOOT_SOURCES ${FITD_RUNTIME_ALL_SOURCES})
    list(REMOVE_ITEM FITD_BOOT_SOURCES
        "${FITD_UPSTREAM_DIR}/FitdLib/osystemAL.cpp")
    add_library(main SHARED
        ${FITD_BOOT_SOURCES}
        fitd_sdl2_platform.cpp
        fitd_sdl2_input.cpp
        fitd_android_entry.cpp
        fitd_gles2_renderer.cpp
        fitd_audio_silent.cpp
    )
    target_compile_features(main PRIVATE cxx_std_17)
    target_include_directories(main PRIVATE
        "${FITD_UPSTREAM_DIR}/FitdLib"
        "${SDL2_INCLUDE_DIR}"
        "${FMT_INCLUDE_DIR}"
        "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bgfx/include"
        "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bx/include"
        "${FITD_UPSTREAM_DIR}/ThirdParty/bgfx.cmake/bimg/include"
        "${CMAKE_CURRENT_LIST_DIR}/.."
        "${CMAKE_CURRENT_LIST_DIR}/../engine-gameplay"
    )
    target_compile_options(main PRIVATE
        -O2 -DNDEBUG -fno-strict-aliasing -fvisibility=hidden)
    target_compile_definitions(main PRIVATE
        FMT_HEADER_ONLY=1 BGFX_CONFIG_MULTITHREADED=0)
    target_link_libraries(main PRIVATE
        "${SDL2_ROOT}/libSDL2.so"
        GLESv2 EGL android log z)
    target_link_options(main PRIVATE "-Wl,--no-undefined")
endif()
