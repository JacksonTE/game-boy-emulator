#pragma once

#include <atomic>
#include <cstdint>

#include "gui_state_types.h"

constexpr const char* COLOUR_PALETTE_LABELS[] =
{
    "Sage",
    "Greyscale",
    "Classic",
    "Custom"
};

constexpr const char* FAST_FORWARD_SPEED_LABELS[] =
{
    "1.50x",
    "1.75x",
    "2.00x",
    "2.25x",
    "2.50x",
    "2.75x",
    "3.00x",
    "3.25x",
    "3.50x",
    "3.75x",
    "4.00x"
};

void render_main_menu_bar(
    const uint8_t currently_published_frame_buffer_index,
    GameBoyEmulator::Emulator& game_boy_emulator,
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
    GameBoyEmulator::Emulator& game_boy_emulator,
    MenuProperties& menu_properties,
    GraphicsController& graphics_controller);

void render_error_message_popup(
    FileLoadingStatus& file_loading_status,
    std::atomic<bool>& is_emulation_paused_atomic,
    std::string& error_message);

ImVec4 get_imvec4_from_abgr(uint32_t abgr);

void imgui_spaced_separator();
