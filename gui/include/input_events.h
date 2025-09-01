#pragma once

#include <atomic>
#include <SDL3/SDL.h>

#include "emulator.h"
#include "gui_state_types.h"

constexpr float MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS = 2.5f;

bool should_main_menu_bar_and_cursor_be_visible(
    GameBoyEmulator::Emulator& game_boy_emulator,
    const EmulationController& emulation_controller,
    FullscreenDisplayStatus& fullscreen_display_status,
    SDL_Window* sdl_window);

bool try_load_file_to_memory_with_dialog(
    GameBoyEmulator::FileType file_type,
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    SDL_Window* sdl_window,
    std::string& error_message);

void toggle_emulation_paused_state(
    std::atomic<bool>& is_emulation_paused_atomic,
    float& seconds_remaining_until_main_menu_bar_and_cursor_hidden);

void toggle_fast_forward_enabled_state(
    std::atomic<bool>& is_fast_forward_enabled_atomic,
    float& seconds_remaining_until_main_menu_bar_and_cursor_hidden);

void toggle_fullscreen_enabled_state(
    float& seconds_remaining_until_main_menu_bar_and_cursor_hidden,
    SDL_Window* sdl_window);

void handle_sdl_events(
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    FullscreenDisplayStatus& fullscreen_display_status,
    KeyPressedStates& key_pressed_states,
    SDL_Window* sdl_window,
    bool& should_stop_emulation,
    std::string& error_message);
