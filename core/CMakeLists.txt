add_library(emulate-game-boy-core-library
    "src/central_processing_unit.cpp"
    "src/emulator.cpp"
    "src/internal_timer.cpp"
    "src/memory_management_unit.cpp"
    "src/pixel_processing_unit.cpp"
    "src/game_cartridge_slot.cpp"
)

target_include_directories(emulate-game-boy-core-library
PUBLIC
    "include"
)
