#pragma once

#include <atomic>
#include <backends/imgui_impl_sdl3.h>
#include <bit>
#include <cstdint>
#include <nfd.h>
#include <SDL3/SDL.h>
#include <string>

#include "emulator.h"
#include "nfd_sdl3.h"
#include "state_data_types.h"

constexpr int INITIAL_WINDOW_SCALE = 5;
constexpr uint8_t DISPLAY_WIDTH_PIXELS = 160;
constexpr uint8_t DISPLAY_HEIGHT_PIXELS = 144;

constexpr float MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS = 2.5f;

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

constexpr uint32_t get_abgr_value_for_current_endianness(
    uint8_t alpha,
    uint8_t blue,
    uint8_t green,
    uint8_t red)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return (static_cast<uint32_t>(red)) |
               (static_cast<uint32_t>(green) << 8) |
               (static_cast<uint32_t>(blue) << 16) |
               (static_cast<uint32_t>(alpha) << 24);
    }
    else
    {
        return (static_cast<uint32_t>(alpha)) |
               (static_cast<uint32_t>(blue) << 8) |
               (static_cast<uint32_t>(green) << 16) |
               (static_cast<uint32_t>(red) << 24);
    }
}

constexpr uint32_t SAGE_COLOUR_PALETTE[4] =
{
    get_abgr_value_for_current_endianness(0xFF, 0xD0, 0xF8, 0xE0),
    get_abgr_value_for_current_endianness(0xFF, 0x70, 0xC0, 0x88),
    get_abgr_value_for_current_endianness(0xFF, 0x56, 0x68, 0x34),
    get_abgr_value_for_current_endianness(0xFF, 0x20, 0x18, 0x08)
};

constexpr uint32_t GREYSCALE_COLOUR_PALETTE[4] =
{
    get_abgr_value_for_current_endianness(0xFF, 0xFF, 0xFF, 0xFF),
    get_abgr_value_for_current_endianness(0xFF, 0xAA, 0xAA, 0xAA),
    get_abgr_value_for_current_endianness(0xFF, 0x55, 0x55, 0x55),
    get_abgr_value_for_current_endianness(0xFF, 0x00, 0x00, 0x00)
};

constexpr uint32_t CLASSIC_COLOUR_PALETTE[4] =
{
    get_abgr_value_for_current_endianness(0xFF, 0x0F, 0xBC, 0x9B),
    get_abgr_value_for_current_endianness(0xFF, 0x0F, 0xAC, 0x8B),
    get_abgr_value_for_current_endianness(0xFF, 0x30, 0x62, 0x30),
    get_abgr_value_for_current_endianness(0xFF, 0x0F, 0x38, 0x0F)
};

ImVec4 get_imvec4_from_abgr(uint32_t abgr);

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
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    FullscreenDisplayStatus& fullscreen_display_status,
    KeyPressedStates& key_pressed_states,
    SDL_Window* sdl_window,
    bool& should_stop_emulation,
    std::string& error_message);

bool should_main_menu_bar_and_cursor_be_visible(
    const EmulationController& emulation_controller,
    FullscreenDisplayStatus& fullscreen_display_status,
    SDL_Window* sdl_window);

SDL_FRect get_sized_emulation_rectangle(
    SDL_Renderer* sdl_renderer,
    SDL_Window* sdl_window);

void set_emulation_screen_blank(GraphicsController& graphics_controller);

void update_colour_palette(
    GameBoyCore::Emulator& game_boy_emulator,
    GraphicsController& graphics_controller,
    const uint8_t currently_published_frame_buffer_index);

bool try_load_file_to_memory_with_dialog(
    GameBoyCore::FileType file_type,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    SDL_Window* sdl_window,
    std::string& error_message);

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
struct sdl_logical_presentation_imgui_workaround_t
{
    int sdl_renderer_logical_width{};
    int sdl_renderer_logical_height{};
    SDL_RendererLogicalPresentation sdl_renderer_logical_presentation_mode{};
};

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
sdl_logical_presentation_imgui_workaround_t sdl_logical_presentation_imgui_workaround_pre_frame(SDL_Renderer* sdl_renderer);

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
void sdl_logical_presentation_imgui_workaround_post_frame(
    SDL_Renderer* sdl_renderer,
    sdl_logical_presentation_imgui_workaround_t logical_values);
