add_library(emulate-game-boy-core-library
    "central_processing_unit.cpp"
    "emulator.cpp"
    "memory_management_unit.cpp"
    "pixel_processing_unit.cpp"
    "timer.cpp"
)

target_include_directories(emulate-game-boy-core-library PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
