#pragma once

#include <atomic>
#include <SDL3/SDL.h>

#include "emulator.h"
#include "gui_state_types.h"

bool try_load_file_to_memory_with_dialog(
    GameBoyEmulator::FileType file_type,
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    SDL_Window* sdl_window,
    std::string* loaded_rom_path,
    std::string& error_message);

#ifdef __EMSCRIPTEN__
void consume_pending_web_file_selection(
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    SDL_Window* sdl_window,
    std::string* loaded_game_rom_path,
    std::string* loaded_boot_rom_path,
    std::string& error_message);
#endif

void toggle_emulation_paused_state(
    std::atomic<bool>& is_emulation_paused_atomic,
    float& seconds_until_main_menu_bar_and_cursor_hidden);

void toggle_fast_forward_enabled_state(
    std::atomic<bool>& is_fast_forward_enabled_atomic,
    float& seconds_until_main_menu_bar_and_cursor_hidden);

void toggle_fullscreen_enabled_state(
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    SDL_Window* sdl_window);

void clear_duplicate_keybind(
    KeyBindings& key_bindings,
    SDL_Keycode new_key,
    SDL_Keycode* current_binding);

void handle_sdl_events(
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& fullscreen_display_status,
    KeyPressedStates& key_pressed_states,
    KeyBindings& key_bindings,
    MenuProperties& menu_properties,
    SDL_Window* sdl_window,
    std::string* loaded_game_rom_path,
    std::string* loaded_boot_rom_path,
    bool& should_stop_emulation,
    std::string& error_message);
