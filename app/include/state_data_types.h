#pragma once

#include <algorithm>
#include <atomic>
#include <backends/imgui_impl_sdl3.h>
#include <cstdint>
#include <SDL3/SDL.h>

#include "emulator.h"

struct EmulationController
{
    GameBoyCore::Emulator game_boy_emulator{};
    std::atomic<bool> is_emulation_paused_atomic{};
    std::atomic<bool> is_fast_forward_enabled_atomic{};
    std::atomic<double> target_fast_forward_multiplier_atomic{1.5};
};

struct FileLoadingStatus
{
    bool did_rom_loading_error_occur{};
    bool is_emulation_paused_before_rom_loading{};
};

struct FullscreenDisplayStatus
{
    bool are_main_menu_bar_and_cursor_visible{};
    bool is_main_menu_bar_hovered{};
    float seconds_remaining_until_main_menu_bar_and_cursor_hidden{};
};

struct GraphicsController
{
    GraphicsController(
        const uint32_t(&initial_colour_palette)[4],
        uint8_t display_width_pixels,
        uint8_t display_height_pixels,
        const uint32_t (&initial_custom_colour_palette)[4],
        SDL_Texture* sdl_texture_pointer)
        : active_colour_palette{initial_colour_palette},
          sdl_texture{sdl_texture_pointer}
    {
        std::copy(
            std::begin(initial_custom_colour_palette),
            std::end(initial_custom_colour_palette),
            custom_colour_palette);

        abgr_pixel_buffer =
            std::make_unique<uint32_t[]>(static_cast<uint16_t>(display_width_pixels * display_height_pixels));
    }

    const uint32_t* active_colour_palette;
    std::unique_ptr<uint32_t[]> abgr_pixel_buffer;
    uint32_t custom_colour_palette[4];
    SDL_Texture* sdl_texture;
};

struct KeyPressedStates
{
    bool was_fast_forward_key_previously_pressed{};
    bool was_fullscreen_key_previously_pressed{};
    bool was_pause_key_previously_pressed{};
    bool was_reset_key_previously_pressed{};
};

struct MenuProperties
{
    ImVec4 selected_custom_colour_palette_colours[4]{};
    bool is_custom_palette_editor_open{};
    int selected_colour_palette_combobox_index{};
    int selected_fast_emulation_speed_index{};
};
