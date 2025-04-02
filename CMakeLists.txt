cmake_minimum_required(VERSION 3.30.5)

project(emulate-game-boy-project
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_library(emulate-game-boy-lib
    core/emulator.cpp
    core/cpu.cpp
    core/memory_management_unit.cpp
)
target_include_directories(emulate-game-boy-lib PUBLIC
    include
)

add_executable(emulate-game-boy 
    app/main.cpp
)
target_link_libraries(emulate-game-boy PUBLIC 
    emulate-game-boy-lib
)

add_subdirectory(tests)
