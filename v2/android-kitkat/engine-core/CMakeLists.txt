cmake_minimum_required(VERSION 3.16)
project(fitd_android_armv7_engine_core LANGUAGES C CXX)

if(NOT SDL2_ROOT OR NOT SDL2_INCLUDE_DIR OR NOT FITD_UPSTREAM_DIR)
    message(FATAL_ERROR "Need SDL2_ROOT, SDL2_INCLUDE_DIR, FITD_UPSTREAM_DIR")
endif()
if(NOT EXISTS "${SDL2_ROOT}/libSDL2.so")
    message(FATAL_ERROR "Missing SDL2 native library")
endif()
if(NOT EXISTS "${FITD_UPSTREAM_DIR}/FitdLib/unpack.cpp")
    message(FATAL_ERROR "Missing pinned original FITD source")
endif()

# Real, un-reimplemented FITD C++ engine modules; not a graphics-only mockup.
add_library(fitd_engine_core STATIC
    "${FITD_UPSTREAM_DIR}/FitdLib/unpack.cpp"
    "${FITD_UPSTREAM_DIR}/FitdLib/pak.cpp"
    "${FITD_UPSTREAM_DIR}/FitdLib/cosTable.cpp"
)
set_target_properties(fitd_engine_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(fitd_engine_core PUBLIC cxx_std_17)
target_include_directories(fitd_engine_core PRIVATE "${FITD_UPSTREAM_DIR}/FitdLib")
target_compile_options(fitd_engine_core PRIVATE -O2 -DNDEBUG -fvisibility=hidden)

add_library(main SHARED
    "${CMAKE_CURRENT_BINARY_DIR}/fitd_engine_probe.cpp"
    fitd_core_selftest.cpp
)
target_include_directories(main PRIVATE
    "${SDL2_INCLUDE_DIR}"
    "${FITD_UPSTREAM_DIR}/FitdLib"
    "${CMAKE_CURRENT_LIST_DIR}/.."
)
target_compile_features(main PRIVATE cxx_std_17)
target_compile_options(main PRIVATE -O2 -DNDEBUG -fvisibility=hidden)
target_link_libraries(main PRIVATE fitd_engine_core
    "${SDL2_ROOT}/libSDL2.so" GLESv2 EGL android log z)
target_link_options(main PRIVATE "-Wl,--no-undefined")
