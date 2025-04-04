add_library(emulate-game-boy-core-library
    "emulator.cpp"
    "cpu.cpp"
    "memory_management_unit.cpp"
)

target_include_directories(emulate-game-boy-core-library PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
