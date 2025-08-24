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

constexpr int INITIAL_WINDOW_SCALE = 5;
constexpr uint8_t DISPLAY_WIDTH_PIXELS = 160;
constexpr uint8_t DISPLAY_HEIGHT_PIXELS = 144;

constexpr float MAIN_MENU_BAR_HIDE_DELAY_SECONDS = 2.0f;

static bool try_load_file_to_memory_with_dialog(
    GameBoyCore::FileType file_type,
    SDL_Window* sdl_window,
    GameBoyCore::Emulator& game_boy_emulator,
    std::atomic<bool>& is_emulation_paused_atomic,
    bool& is_emulation_paused_before_rom_loading,
    bool& did_rom_loading_error_occur,
    std::string& error_message)
{
    is_emulation_paused_before_rom_loading = is_emulation_paused_atomic.load(std::memory_order_acquire);
    is_emulation_paused_atomic.store(true, std::memory_order_release);

    nfdopendialogu8args_t open_dialog_arguments{};
    nfdu8filteritem_t filters[] =
    {
        {file_type == GameBoyCore::FileType::BootROM ? "Game Boy Boot ROMs" : "Game Boy ROMs", "gb,gbc,bin,rom"}
    };
    open_dialog_arguments.filterList = filters;
    open_dialog_arguments.filterCount = 1;

    nfdchar_t* rom_path = nullptr;
    NFD_GetNativeWindowFromSDLWindow(sdl_window, &open_dialog_arguments.parentWindow);

    nfdresult_t result = NFD_OpenDialogU8_With(&rom_path, &open_dialog_arguments);
    bool is_operation_successful = false;

    if (result == NFD_OKAY)
    {
        if (game_boy_emulator.try_load_file_to_memory(rom_path, file_type, error_message))
        {
            if (file_type == GameBoyCore::FileType::GameROM)
            {
                game_boy_emulator.reset_state();
            }
            is_operation_successful = true;
        }
        NFD_FreePathU8(rom_path);
    }
    else if (result == NFD_ERROR)
    {
        std::cerr << "NFD error: " << NFD_GetError() << "\n";
        error_message = NFD_GetError();
    }

    if (is_operation_successful)
    {
        if (file_type == GameBoyCore::FileType::GameROM)
        {
            SDL_SetWindowTitle(sdl_window, std::string("Emulate Game Boy - " + game_boy_emulator.get_loaded_game_rom_title_thread_safe()).c_str());
        }
        is_emulation_paused_atomic.store(false, std::memory_order_release);
    }
    else
    {
        did_rom_loading_error_occur = (error_message != "");
        is_emulation_paused_atomic.store(is_emulation_paused_before_rom_loading, std::memory_order_release);
    }
    return is_operation_successful;
}

static void toggle_fullscreen_enabled_state(SDL_Window* sdl_window, float& main_menu_bar_seconds_remaining_until_hidden)
{
    const bool was_window_fullscreen = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(sdl_window, !was_window_fullscreen);
    main_menu_bar_seconds_remaining_until_hidden = MAIN_MENU_BAR_HIDE_DELAY_SECONDS;
}

static void toggle_emulation_paused_state(std::atomic<bool>& is_emulation_paused_atomic, float& main_menu_bar_seconds_remaining_until_hidden)
{
    const bool was_emulation_paused = is_emulation_paused_atomic.load(std::memory_order_acquire);
    is_emulation_paused_atomic.store(!was_emulation_paused, std::memory_order_release);
    main_menu_bar_seconds_remaining_until_hidden = MAIN_MENU_BAR_HIDE_DELAY_SECONDS;
}

static void toggle_fast_forward_enabled_state(std::atomic<bool>& is_fast_forward_enabled_atomic, float& main_menu_bar_seconds_remaining_until_hidden)
{
    const bool was_fast_forward_enabled = is_fast_forward_enabled_atomic.load(std::memory_order_acquire);
    is_fast_forward_enabled_atomic.store(!was_fast_forward_enabled, std::memory_order_release);
    main_menu_bar_seconds_remaining_until_hidden = MAIN_MENU_BAR_HIDE_DELAY_SECONDS;
}

static void handle_sdl_events(
    bool& stop_emulating,
    bool& did_rom_loading_error_occur,
    bool& is_emulation_paused_before_rom_loading,
    bool& was_fast_forward_key_previously_pressed,
    bool& was_pause_key_previously_pressed,
    bool& was_reset_key_previously_pressed,
    bool& was_fullscreen_key_previously_pressed,
    float& main_menu_bar_seconds_remaining_until_hidden,
    std::atomic<bool>& is_emulation_paused_atomic,
    std::atomic<bool>& is_fast_forward_enabled_atomic,
    GameBoyCore::Emulator& game_boy_emulator,
    SDL_Window* sdl_window,
    std::string& error_message)
{
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        ImGui_ImplSDL3_ProcessEvent(&sdl_event);

        switch (sdl_event.type)
        {
            case SDL_EVENT_QUIT:
                stop_emulating = true;
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                if (did_rom_loading_error_occur)
                {
                    break;
                }
                const bool is_key_pressed = (sdl_event.type == SDL_EVENT_KEY_DOWN);

                switch (sdl_event.key.key)
                {
                    case SDLK_F11:
                    {
                        if (is_key_pressed && !was_fullscreen_key_previously_pressed)
                        {
                            toggle_fullscreen_enabled_state(sdl_window, main_menu_bar_seconds_remaining_until_hidden);
                        }
                        was_fullscreen_key_previously_pressed = is_key_pressed;
                        break;
                    }
                    case SDLK_O:
                        if (is_key_pressed)
                        {
                            try_load_file_to_memory_with_dialog(
                                GameBoyCore::FileType::GameROM,
                                sdl_window,
                                game_boy_emulator,
                                std::ref(is_emulation_paused_atomic),
                                is_emulation_paused_before_rom_loading,
                                did_rom_loading_error_occur,
                                error_message
                            );
                        }
                        break;
                }
                if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                {
                    switch (sdl_event.key.key)
                    {
                        case SDLK_SPACE:
                            if (is_key_pressed && !was_fast_forward_key_previously_pressed)
                            {
                                toggle_fast_forward_enabled_state(is_fast_forward_enabled_atomic, main_menu_bar_seconds_remaining_until_hidden);
                            }
                            was_fast_forward_key_previously_pressed = is_key_pressed;
                            break;
                        case SDLK_ESCAPE:
                            if (is_key_pressed && !was_pause_key_previously_pressed)
                            {
                                toggle_emulation_paused_state(is_emulation_paused_atomic, main_menu_bar_seconds_remaining_until_hidden);
                            }
                            was_pause_key_previously_pressed = is_key_pressed;
                            break;
                        case SDLK_R:
                            if (is_key_pressed && !was_reset_key_previously_pressed)
                            {
                                game_boy_emulator.reset_state();
                            }
                            was_reset_key_previously_pressed = is_key_pressed;
                            break;
                        case SDLK_W:
                            game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(GameBoyCore::UP_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                            break;
                        case SDLK_A:
                            game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(GameBoyCore::LEFT_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                            break;
                        case SDLK_S:
                            game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(GameBoyCore::DOWN_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                            break;
                        case SDLK_D:
                            game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(GameBoyCore::RIGHT_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                            break;
                        case SDLK_APOSTROPHE:
                            game_boy_emulator.update_button_pressed_state_thread_safe(GameBoyCore::A_BUTTON_FLAG_MASK, is_key_pressed);
                            break;
                        case SDLK_PERIOD:
                            game_boy_emulator.update_button_pressed_state_thread_safe(GameBoyCore::B_BUTTON_FLAG_MASK, is_key_pressed);
                            break;
                        case SDLK_RETURN:
                            game_boy_emulator.update_button_pressed_state_thread_safe(GameBoyCore::START_BUTTON_FLAG_MASK, is_key_pressed);
                            break;
                        case SDLK_RSHIFT:
                            game_boy_emulator.update_button_pressed_state_thread_safe(GameBoyCore::SELECT_BUTTON_FLAG_MASK, is_key_pressed);
                            break;
                    }
                }
            }
            break;
        }
    }
}

static bool is_main_menu_bar_visible(
    SDL_Window* sdl_window,
    bool& is_main_menu_hovered,
    float& main_menu_bar_seconds_remaining_until_hidden,
    const std::atomic<bool>& is_emulation_paused_atomic,
    const GameBoyCore::Emulator& game_boy_emulator
)
{
    const bool is_window_fullscreen = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    if (!is_window_fullscreen || !game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() || is_emulation_paused_atomic.load(std::memory_order_acquire))
    {
        return true;
    }

    float mouse_y_position_in_window;
    SDL_GetGlobalMouseState(nullptr, &mouse_y_position_in_window);
    const float main_menu_bar_height_pixels = ImGui::GetFrameHeight() * ImGui::GetIO().DisplayFramebufferScale.y;

    if (is_main_menu_hovered || mouse_y_position_in_window <= main_menu_bar_height_pixels)
    {
        main_menu_bar_seconds_remaining_until_hidden = MAIN_MENU_BAR_HIDE_DELAY_SECONDS;
        return true;
    }

    if (main_menu_bar_seconds_remaining_until_hidden > 0.0f)
    {
        main_menu_bar_seconds_remaining_until_hidden -= ImGui::GetIO().DeltaTime;
    }
    return (main_menu_bar_seconds_remaining_until_hidden > 0.0f);
}

static SDL_FRect get_sized_emulation_rectangle(SDL_Renderer* sdl_renderer, SDL_Window* sdl_window)
{
    const bool is_window_fullscreen = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    float space_reserved_for_menu_bar = 0.0f;

    if (!is_window_fullscreen)
    {
        int renderer_output_width, renderer_output_height;
        SDL_GetRenderOutputSize(sdl_renderer, &renderer_output_width, &renderer_output_height);

        const float current_scale_x = static_cast<float>(renderer_output_width) / static_cast<float>(DISPLAY_WIDTH_PIXELS);
        const float current_scale_y = static_cast<float>(renderer_output_height) / static_cast<float>(DISPLAY_HEIGHT_PIXELS);

        const int renderer_integer_scaling_factor = std::max(1, std::min(int(current_scale_x), int(current_scale_y)));

        space_reserved_for_menu_bar = (ImGui::GetFrameHeight() / static_cast<float>(renderer_integer_scaling_factor));
    }

    return SDL_FRect
    {
        0.0f,
        space_reserved_for_menu_bar,
        static_cast<float>(DISPLAY_WIDTH_PIXELS),
        static_cast<float>(DISPLAY_HEIGHT_PIXELS) - space_reserved_for_menu_bar
    };
}

static void set_emulation_screen_blank(const uint32_t* active_colour_palette, uint32_t* abgr_pixel_buffer, SDL_Texture* sdl_texture)
{
    for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
    {
        abgr_pixel_buffer[i] = active_colour_palette[0];
    }
    SDL_UpdateTexture(sdl_texture, nullptr, abgr_pixel_buffer, DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
}

static constexpr uint32_t get_abgr_value_for_current_endianness(uint8_t alpha, uint8_t blue, uint8_t green, uint8_t red)
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

static constexpr uint32_t SAGE_COLOUR_PALETTE[4] =
{
    get_abgr_value_for_current_endianness(0xFF, 0xD0, 0xF8, 0xE0),
    get_abgr_value_for_current_endianness(0xFF, 0x70, 0xC0, 0x88),
    get_abgr_value_for_current_endianness(0xFF, 0x56, 0x68, 0x34),
    get_abgr_value_for_current_endianness(0xFF, 0x20, 0x18, 0x08)
};

static constexpr uint32_t GREYSCALE_COLOUR_PALETTE[4] =
{
    get_abgr_value_for_current_endianness(0xFF, 0xFF, 0xFF, 0xFF),
    get_abgr_value_for_current_endianness(0xFF, 0xAA, 0xAA, 0xAA),
    get_abgr_value_for_current_endianness(0xFF, 0x55, 0x55, 0x55),
    get_abgr_value_for_current_endianness(0xFF, 0x00, 0x00, 0x00)
};

static constexpr uint32_t CLASSIC_COLOUR_PALETTE[4] =
{
    get_abgr_value_for_current_endianness(0xFF, 0x0F, 0xBC, 0x9B),
    get_abgr_value_for_current_endianness(0xFF, 0x0F, 0xAC, 0x8B),
    get_abgr_value_for_current_endianness(0xFF, 0x30, 0x62, 0x30),
    get_abgr_value_for_current_endianness(0xFF, 0x0F, 0x38, 0x0F)
};

static uint32_t custom_colour_palette[4] =
{
    get_abgr_value_for_current_endianness(0xFF, 0xEF, 0xE0, 0x90),
    get_abgr_value_for_current_endianness(0xFF, 0xD8, 0xB4, 0x00),
    get_abgr_value_for_current_endianness(0xFF, 0xB6, 0x77, 0x00),
    get_abgr_value_for_current_endianness(0xFF, 0x5E, 0x04, 0x03)
};

static ImVec4 selected_custom_colour_palette_colours[4] =
{
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
};

static ImVec4 get_imvec4_from_abgr(uint32_t abgr)
{
    const uint8_t alpha = (abgr >> 24) & 0xFF;
    const uint8_t blue = (abgr >> 16) & 0xFF;
    const uint8_t green = (abgr >> 8) & 0xFF;
    const uint8_t red = abgr & 0xFF;
    return ImVec4(red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f);
}

static constexpr const char* COLOUR_PALETTE_LABELS[] =
{
    "Sage",
    "Greyscale",
    "Classic",
    "Custom"
};

static constexpr const char* FAST_FORWARD_SPEED_LABELS[] =
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

static void update_colour_palette(
    GameBoyCore::Emulator& game_boy_emulator,
    const uint32_t*& active_colour_palette,
    const uint8_t currently_published_frame_buffer_index,
    uint32_t* abgr_pixel_buffer,
    SDL_Texture* sdl_texture)
{
    if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
    {
        auto const& pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);

        for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
        {
            abgr_pixel_buffer[i] = active_colour_palette[pixel_frame_buffer[i]];
        }
        SDL_UpdateTexture(sdl_texture, nullptr, abgr_pixel_buffer, DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
    }
    else
        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer, sdl_texture);
}

static void imgui_spaced_separator()
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

static void render_main_menu_bar(
    bool& stop_emulating,
    bool& is_emulation_paused_before_rom_loading,
    bool& did_rom_loading_error_occur,
    bool& is_custom_palette_editor_open,
    int& selected_colour_palette_combobox_index,
    int& selected_fast_emulation_speed_index,
    float& main_menu_bar_seconds_remaining_until_hidden,
    bool& is_main_menu_hovered,
    const uint32_t*& active_colour_palette,
    const uint8_t currently_published_frame_buffer_index,
    uint32_t* abgr_pixel_buffer,
    std::atomic<bool>& is_emulation_paused_atomic,
    std::atomic<bool>& is_fast_forward_enabled_atomic,
    std::atomic<double>& target_fast_forward_multiplier_atomic,
    SDL_Window* sdl_window,
    SDL_Texture* sdl_texture,
    GameBoyCore::Emulator& game_boy_emulator,
    std::string& error_message)
{
    const bool is_window_fullscreen = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    const bool is_fast_forward_enabled = is_fast_forward_enabled_atomic.load(std::memory_order_acquire);
    const bool is_emulation_paused = is_emulation_paused_atomic.load(std::memory_order_acquire);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem("Load Game ROM", "[O]"))
            {
                try_load_file_to_memory_with_dialog(
                    GameBoyCore::FileType::GameROM,
                    sdl_window,
                    game_boy_emulator,
                    std::ref(is_emulation_paused_atomic),
                    is_emulation_paused_before_rom_loading,
                    did_rom_loading_error_occur,
                    error_message
                );
            }
            ImGui::Spacing();
            if (ImGui::MenuItem("Load Boot ROM (Optional)"))
            {
                try_load_file_to_memory_with_dialog(
                    GameBoyCore::FileType::BootROM,
                    sdl_window,
                    game_boy_emulator,
                    std::ref(is_emulation_paused_atomic),
                    is_emulation_paused_before_rom_loading,
                    did_rom_loading_error_occur,
                    error_message
                );
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem("Unload Game ROM", "", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer, sdl_texture);
                SDL_SetWindowTitle(sdl_window, std::string("Emulate Game Boy").c_str());
                game_boy_emulator.unload_game_rom_from_memory_thread_safe();
                game_boy_emulator.reset_state();
                is_fast_forward_enabled_atomic.store(false, std::memory_order_release);
                is_emulation_paused_atomic.store(false, std::memory_order_release);
            }
            ImGui::Spacing();
            if (ImGui::MenuItem("Unload Boot ROM", "", false, game_boy_emulator.is_boot_rom_loaded_in_memory_thread_safe()))
            {
                is_emulation_paused_atomic.store(true, std::memory_order_release);
                game_boy_emulator.unload_boot_rom_from_memory_thread_safe();

                if (game_boy_emulator.is_boot_rom_mapped_in_memory() && game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                {
                    game_boy_emulator.reset_state();
                }
                is_emulation_paused_atomic.store(is_emulation_paused);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem("Quit", "[Alt+F4]"))
            {
                stop_emulating = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Video"))
        {
            ImGui::SeparatorText("Colour Palette");
            if (ImGui::Combo("##Colour Palette", &selected_colour_palette_combobox_index, COLOUR_PALETTE_LABELS, IM_ARRAYSIZE(COLOUR_PALETTE_LABELS)))
            {
                switch (selected_colour_palette_combobox_index)
                {
                    case 0:
                        active_colour_palette = SAGE_COLOUR_PALETTE;
                        break;
                    case 1:
                        active_colour_palette = GREYSCALE_COLOUR_PALETTE;
                        break;
                    case 2:
                        active_colour_palette = CLASSIC_COLOUR_PALETTE;
                        break;
                    case 3:
                        active_colour_palette = custom_colour_palette;
                        break;
                }
                update_colour_palette(
                    game_boy_emulator,
                    active_colour_palette,
                    currently_published_frame_buffer_index,
                    abgr_pixel_buffer,
                    sdl_texture
                );
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem("Update Custom Palette"))
            {
                is_custom_palette_editor_open = true;
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem(is_window_fullscreen ? "Exit Fullscreen" : "Fullscreen", "[F11]"))
            {
                toggle_fullscreen_enabled_state(sdl_window, main_menu_bar_seconds_remaining_until_hidden);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation"))
        {
            ImGui::SeparatorText("Fast-Foward Speed");
            if (ImGui::Combo("##Fast-Foward Speed", &selected_fast_emulation_speed_index, FAST_FORWARD_SPEED_LABELS, IM_ARRAYSIZE(FAST_FORWARD_SPEED_LABELS)))
            {
                const double emulation_speed_multiplier = selected_fast_emulation_speed_index * 0.25 + 1.5;
                target_fast_forward_multiplier_atomic.store(emulation_speed_multiplier, std::memory_order_release);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem(is_fast_forward_enabled ? "Disable Fast-Forward" : "Enable Fast-Forward", "[Space]", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                toggle_fast_forward_enabled_state(is_fast_forward_enabled_atomic, main_menu_bar_seconds_remaining_until_hidden);
            }
            ImGui::Spacing();
            if (ImGui::MenuItem(is_emulation_paused ? "Unpause" : "Pause", "[Esc]", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                toggle_emulation_paused_state(is_emulation_paused_atomic, main_menu_bar_seconds_remaining_until_hidden);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem("Reset", "[R]", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                game_boy_emulator.reset_state();
                is_emulation_paused_atomic.store(false, std::memory_order_release);
            }
            ImGui::EndMenu();
        }
        if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
        {
            if (is_emulation_paused)
            {
                ImGui::TextDisabled("[Emulation Paused]");
            }
            if (is_fast_forward_enabled)
            {
                ImGui::TextDisabled("[Fast-Forward Enabled]");
            }
        }
        is_main_menu_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        ImGui::EndMainMenuBar();
    }
}

static void render_custom_colour_palette_editor(
    GameBoyCore::Emulator& game_boy_emulator,
    bool& is_custom_palette_editor_open,
    const uint8_t currently_published_frame_buffer_index,
    const uint32_t* active_colour_palette,
    int& selected_colour_palette_combobox_index,
    uint32_t* abgr_pixel_buffer,
    SDL_Texture* sdl_texture)
{
    if (is_custom_palette_editor_open)
    {
        ImGui::OpenPopup("Custom Palette");
    }
    if (ImGui::BeginPopupModal("Custom Palette", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        for (int i = 0; i < 4; i++)
        {
            selected_custom_colour_palette_colours[i] = get_imvec4_from_abgr(custom_colour_palette[i]);
            std::string colour_label = std::string("Colour ") + std::to_string(i);
            if (ImGui::ColorEdit4(colour_label.c_str(), reinterpret_cast<float *>(&selected_custom_colour_palette_colours[i]), ImGuiColorEditFlags_NoInputs))
            {
                update_colour_palette(
                    game_boy_emulator,
                    active_colour_palette,
                    currently_published_frame_buffer_index,
                    abgr_pixel_buffer,
                    sdl_texture
                );
            }
            auto& colour = selected_custom_colour_palette_colours[i];
            uint8_t alpha = static_cast<uint8_t>(colour.w * 255.0f + 0.5f);
            uint8_t blue = static_cast<uint8_t>(colour.z * 255.0f + 0.5f);
            uint8_t green = static_cast<uint8_t>(colour.y * 255.0f + 0.5f);
            uint8_t red = static_cast<uint8_t>(colour.x * 255.0f + 0.5f);
            custom_colour_palette[i] = get_abgr_value_for_current_endianness(alpha, blue, green, red);
        }
        if (ImGui::Button("OK", ImVec2(160, 0)))
        {
            is_custom_palette_editor_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void render_error_message_popup(
    bool& did_rom_loading_error_occur,
    bool& is_emulation_paused_before_rom_loading,
    std::atomic<bool>& is_emulation_paused_atomic,
    std::string& error_message
)
{
    if (did_rom_loading_error_occur)
    {
        is_emulation_paused_atomic.store(true, std::memory_order_release);
        const float maximum_error_popup_width = ImGui::GetIO().DisplaySize.x * 0.4f;
        const float error_message_width = ImGui::CalcTextSize(error_message.c_str()).x;
        const float minimum_error_popup_width = std::min(maximum_error_popup_width, error_message_width + ImGui::GetStyle().WindowPadding.x * 2.0f);
        ImGui::SetNextWindowSizeConstraints(ImVec2(minimum_error_popup_width, 0), ImVec2(maximum_error_popup_width, FLT_MAX));
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Error");
    }
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextWrapped("%s", error_message.c_str());
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        {
            is_emulation_paused_atomic.store(is_emulation_paused_before_rom_loading, std::memory_order_release);
            ImGui::CloseCurrentPopup();
            did_rom_loading_error_occur = false;
            error_message = "";
        }
        ImGui::EndPopup();
    }
}

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
struct sdl_logical_presentation_imgui_workaround_t
{
    int sdl_renderer_logical_width{};
    int sdl_renderer_logical_height{};
    SDL_RendererLogicalPresentation sdl_renderer_logical_presentation_mode{};
};

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
static sdl_logical_presentation_imgui_workaround_t sdl_logical_presentation_imgui_workaround_pre_frame(SDL_Renderer* sdl_renderer)
{
    sdl_logical_presentation_imgui_workaround_t logical_values{};
    SDL_GetRenderLogicalPresentation(sdl_renderer, &logical_values.sdl_renderer_logical_width, &logical_values.sdl_renderer_logical_height, &logical_values.sdl_renderer_logical_presentation_mode);
    SDL_SetRenderLogicalPresentation(sdl_renderer, logical_values.sdl_renderer_logical_width, logical_values.sdl_renderer_logical_height, SDL_LOGICAL_PRESENTATION_DISABLED);
    return logical_values;
}

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
static void sdl_logical_presentation_imgui_workaround_post_frame(SDL_Renderer* sdl_renderer, sdl_logical_presentation_imgui_workaround_t logical_values)
{
    SDL_SetRenderLogicalPresentation(sdl_renderer, logical_values.sdl_renderer_logical_width, logical_values.sdl_renderer_logical_height, logical_values.sdl_renderer_logical_presentation_mode);
}
