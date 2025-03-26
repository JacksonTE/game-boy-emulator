cmake_minimum_required(VERSION 3.30.5)

project(emulate-gb-project VERSION 1.0 LANGUAGES CXX)

add_executable(emulate-gb src/main.cpp src/game_boy.cpp src/cpu.cpp src/memory.cpp)

target_compile_features(emulate-gb PRIVATE cxx_std_20)

target_include_directories(emulate-gb PRIVATE include/emulate-gb)
