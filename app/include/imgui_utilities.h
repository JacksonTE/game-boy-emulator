#pragma once

#include <atomic>
#include <cstdint>

#include "state_data_types.h"

void render_main_menu_bar(
    const uint8_t currently_published_frame_buffer_index,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    FullscreenDisplayStatus& fullscreen_display_status,
    GraphicsController& graphics_controller,
    MenuProperties& menu_properties,
    SDL_Window* sdl_window,
    bool& should_stop_emulation,
    std::string& error_message);

void render_custom_colour_palette_editor(
    const uint8_t currently_published_frame_buffer_index,
    GameBoyCore::Emulator& game_boy_emulator,
    MenuProperties& menu_properties,
    GraphicsController& graphics_controller);

void render_error_message_popup(
    FileLoadingStatus& file_loading_status,
    std::atomic<bool>& is_emulation_paused_atomic,
    std::string& error_message);

void imgui_spaced_separator();
