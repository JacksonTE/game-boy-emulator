cmake_minimum_required(VERSION 3.30.5)

project(emulate-game-boy-project VERSION 1.0 LANGUAGES CXX)

add_executable(emulate-game-boy src/main.cpp
							    src/game_boy.cpp
								src/cpu.cpp
								src/memory.cpp)

target_compile_features(emulate-game-boy PRIVATE cxx_std_20)

target_include_directories(emulate-game-boy PRIVATE include/emulate-game-boy)
