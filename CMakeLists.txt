cmake_minimum_required(VERSION 3.28.3)

project(game-boy-emulator-project LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

enable_testing()

add_subdirectory(emulator)
add_subdirectory(gui)
add_subdirectory(tests)
