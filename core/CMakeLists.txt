add_library(emulate-game-boy-core-library
    "src/central_processing_unit.cpp"
    "src/emulator.cpp"
    "src/memory_management_unit.cpp"
    "src/pixel_processing_unit.cpp"
    "src/timer.cpp"
)

target_include_directories(emulate-game-boy-core-library
PUBLIC
    "${CMAKE_SOURCE_DIR}/core/include"
)
