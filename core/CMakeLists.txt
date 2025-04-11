add_library(emulate-game-boy-core-library
    "cpu.cpp"
    "emulator.cpp"
    "machine_cycle_interaction.cpp"
    "memory_management_unit.cpp"
    "pixel_processing_unit.cpp"
    "timer.cpp"
)

target_include_directories(emulate-game-boy-core-library PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
