#pragma once

#include <cstdint>

#include "emulator.h"
#include "gui_state_types.h"

constexpr float MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS = 2.5f;

constexpr int DEFAULT_INITIAL_WINDOW_SCALE = 4;
constexpr uint8_t DISPLAY_WIDTH_PIXELS = 160;
constexpr uint8_t DISPLAY_HEIGHT_PIXELS = 144;

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

int get_initial_window_scale_for_display();

void set_emulation_screen_blank(GraphicsController& graphics_controller);

void update_colour_palette(
    GameBoyEmulator::Emulator& game_boy_emulator,
    GraphicsController& graphics_controller,
    const uint8_t currently_published_frame_buffer_index);

bool should_main_menu_bar_and_cursor_be_visible(
    const GameBoyEmulator::Emulator& game_boy_emulator,
    const EmulationController& emulation_controller,
    MenuAndCursorDisplayStatus& fullscreen_display_status,
    bool is_custom_palette_editor_open,
    bool is_keybinds_editor_open);

void update_imgui_scale_by_resolution(SDL_Window* sdl_window);

#ifdef __EMSCRIPTEN__
struct web_viewport_metrics_t
{
    int css_viewport_width{};
    int css_viewport_height{};
    int pixel_viewport_width{};
    int pixel_viewport_height{};
    double device_pixel_ratio{1.0};
};

double get_web_device_pixel_ratio();

web_viewport_metrics_t get_web_viewport_metrics();
#endif

void render_frame(RenderContext& context);

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
