cmake_minimum_required(VERSION 3.30.5)

project(emulate-game-boy-project
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(emulate-game-boy-lib
    core/game_boy.cpp
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
