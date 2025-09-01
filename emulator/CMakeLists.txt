add_library(game-boy-emulator
    "src/central_processing_unit.cpp"
    "src/emulator.cpp"
    "src/game_cartridge_slot.cpp"
    "src/internal_timer.cpp"
    "src/memory_bank_controllers.cpp"
    "src/memory_management_unit.cpp"
    "src/pixel_processing_unit.cpp")

target_include_directories(game-boy-emulator PUBLIC
    "include")
