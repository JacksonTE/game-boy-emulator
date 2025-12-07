#pragma once

#include <algorithm>
#include <atomic>
#include <backends/imgui_impl_sdl3.h>
#include <cstdint>
#include <SDL3/SDL.h>

#include "emulator.h"

struct EmulationController
{
    std::atomic<bool> is_emulation_paused_atomic{};
    std::atomic<bool> is_fast_forward_enabled_atomic{};
    std::atomic<double> target_fast_forward_multiplier_atomic{1.5};
};

struct FileLoadingStatus
{
    bool did_rom_loading_error_occur{};
    bool is_emulation_paused_before_rom_loading{};
};

struct MenuAndCursorDisplayStatus
{
    static constexpr int MAX_CURSOR_CHANGES_TO_IGNORE = 5;
    bool is_main_menu_bar_hovered{};
    int cursor_changes_to_ignore_count{};
    float seconds_until_main_menu_bar_and_cursor_hidden{};
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

struct KeyBindings
{
    SDL_Keycode button_up = SDLK_UP;
    SDL_Keycode button_down = SDLK_DOWN;
    SDL_Keycode button_left = SDLK_LEFT;
    SDL_Keycode button_right = SDLK_RIGHT;
    SDL_Keycode button_a = SDLK_X;
    SDL_Keycode button_b = SDLK_Z;
    SDL_Keycode button_start = SDLK_RETURN;
    SDL_Keycode button_select = SDLK_BACKSPACE;

    SDL_Keycode load_rom = SDLK_O;
    SDL_Keycode fast_forward = SDLK_SPACE;
    SDL_Keycode pause = SDLK_ESCAPE;
    SDL_Keycode reset = SDLK_R;
    SDL_Keycode fullscreen = SDLK_F11;

    void reset_to_defaults()
    {
        button_up = SDLK_UP;
        button_down = SDLK_DOWN;
        button_left = SDLK_LEFT;
        button_right = SDLK_RIGHT;
        button_a = SDLK_X;
        button_b = SDLK_Z;
        button_start = SDLK_RETURN;
        button_select = SDLK_BACKSPACE;

        load_rom = SDLK_O;
        fast_forward = SDLK_SPACE;
        pause = SDLK_ESCAPE;
        reset = SDLK_R;
        fullscreen = SDLK_F11;
    }
};

struct KeybindsEditorState
{
    enum class ControlType
    {
        GameBoy,
        Emulation
    };

    bool is_open{};
    bool is_waiting_for_key{};
    int editing_index = -1;
    ControlType selected_control_type{};
};

struct MenuProperties
{
    ImVec4 selected_custom_colour_palette_colours[4]{};
    bool is_custom_palette_editor_open{};
    int selected_colour_palette_combobox_index{};
    int selected_fast_emulation_speed_index{};
    KeybindsEditorState keybinds_editor_state{};
};

struct RenderContext
{
    GameBoyEmulator::Emulator* game_boy_emulator{};
    EmulationController* emulation_controller{};
    FileLoadingStatus* file_loading_status{};
    MenuAndCursorDisplayStatus* menu_and_cursor_display_status{};
    GraphicsController* graphics_controller{};
    MenuProperties* menu_properties{};
    KeyBindings* key_bindings{};
    SDL_Renderer* sdl_renderer{};
    SDL_Texture* sdl_texture{};
    SDL_Window* sdl_window{};
    uint8_t* previously_published_frame_buffer_index{};
    bool* should_stop_emulation{};
    std::string* error_message{};
    bool is_currently_rendering = false;
};
