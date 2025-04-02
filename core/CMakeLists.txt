add_library(emulate-game-boy-lib
    "emulator.cpp"
    "cpu.cpp"
    "memory_management_unit.cpp"
)

target_include_directories(emulate-game-boy-lib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
