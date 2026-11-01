cmake_minimum_required(VERSION 3.16)
project(fitd_gles2_probe LANGUAGES C CXX)
# Use with an SDL2 Android Gradle template; the SDL2 target must exist.
add_library(main SHARED gles2_probe.cpp)
target_compile_features(main PRIVATE cxx_std_11)
target_link_libraries(main PRIVATE SDL2 GLESv2 EGL log android)
target_compile_options(main PRIVATE -O2 -DNDEBUG)
