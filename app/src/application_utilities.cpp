#include <backends/imgui_impl_sdl3.h>

#include "application_utilities.h"

ImVec4 get_imvec4_from_abgr(uint32_t abgr)
{
    const uint8_t alpha = (abgr >> 24) & 0xFF;
    const uint8_t blue = (abgr >> 16) & 0xFF;
    const uint8_t green = (abgr >> 8) & 0xFF;
    const uint8_t red = abgr & 0xFF;
    return ImVec4(red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f);
}

void toggle_emulation_paused_state(
    std::atomic<bool>& is_emulation_paused_atomic,
    float& seconds_remaining_until_main_menu_bar_and_cursor_hidden)
{
    const bool was_emulation_paused = is_emulation_paused_atomic.load(std::memory_order_acquire);
    is_emulation_paused_atomic.store(!was_emulation_paused, std::memory_order_release);
    seconds_remaining_until_main_menu_bar_and_cursor_hidden = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
}

void toggle_fast_forward_enabled_state(
    std::atomic<bool>& is_fast_forward_enabled_atomic,
    float& seconds_remaining_until_main_menu_bar_and_cursor_hidden)
{
    const bool was_fast_forward_enabled = is_fast_forward_enabled_atomic.load(std::memory_order_acquire);
    is_fast_forward_enabled_atomic.store(!was_fast_forward_enabled, std::memory_order_release);
    seconds_remaining_until_main_menu_bar_and_cursor_hidden = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
}

void toggle_fullscreen_enabled_state(
    float& seconds_remaining_until_main_menu_bar_and_cursor_hidden,
    SDL_Window* sdl_window)
{
    const bool was_fullscreen_enabled = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(sdl_window, !was_fullscreen_enabled);
    seconds_remaining_until_main_menu_bar_and_cursor_hidden = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
}

void handle_sdl_events(
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    FullscreenDisplayStatus& fullscreen_display_status,
    KeyPressedStates& key_pressed_states,
    SDL_Window* sdl_window,
    bool& should_stop_emulation,
    std::string& error_message)
{
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        ImGui_ImplSDL3_ProcessEvent(&sdl_event);

        switch (sdl_event.type)
        {
            case SDL_EVENT_QUIT:
                should_stop_emulation = true;
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                if (file_loading_status.did_rom_loading_error_occur)
                {
                    break;
                }
                const bool is_key_pressed = (sdl_event.type == SDL_EVENT_KEY_DOWN);

                switch (sdl_event.key.key)
                {
                    case SDLK_F11:
                    {
                        if (is_key_pressed && !key_pressed_states.was_fullscreen_key_previously_pressed)
                        {
                            toggle_fullscreen_enabled_state(
                                fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden,
                                sdl_window);
                        }
                        key_pressed_states.was_fullscreen_key_previously_pressed = is_key_pressed;
                        break;
                    }
                    case SDLK_O:
                        if (is_key_pressed)
                        {
                            try_load_file_to_memory_with_dialog(
                                GameBoyCore::FileType::GameROM,
                                emulation_controller,
                                file_loading_status,
                                sdl_window,
                                error_message);
                        }
                        break;
                }
                if (emulation_controller.game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                {
                    switch (sdl_event.key.key)
                    {
                        case SDLK_SPACE:
                            if (is_key_pressed && !key_pressed_states.was_fast_forward_key_previously_pressed)
                            {
                                toggle_fast_forward_enabled_state(
                                    emulation_controller.is_fast_forward_enabled_atomic,
                                    fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden);
                            }
                            key_pressed_states.was_fast_forward_key_previously_pressed = is_key_pressed;
                            break;
                        case SDLK_ESCAPE:
                            if (is_key_pressed && !key_pressed_states.was_pause_key_previously_pressed)
                            {
                                toggle_emulation_paused_state(
                                    emulation_controller.is_emulation_paused_atomic,
                                    fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden);
                            }
                            key_pressed_states.was_pause_key_previously_pressed = is_key_pressed;
                            break;
                        case SDLK_R:
                            if (is_key_pressed && !key_pressed_states.was_reset_key_previously_pressed)
                            {
                                emulation_controller.game_boy_emulator.reset_state();
                            }
                            key_pressed_states.was_reset_key_previously_pressed = is_key_pressed;
                            break;
                        case SDLK_W:
                            emulation_controller.game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                                GameBoyCore::UP_DPAD_DIRECTION_FLAG_MASK,
                                is_key_pressed);
                            break;
                        case SDLK_A:
                            emulation_controller.game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                                GameBoyCore::LEFT_DPAD_DIRECTION_FLAG_MASK,
                                is_key_pressed);
                            break;
                        case SDLK_S:
                            emulation_controller.game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                                GameBoyCore::DOWN_DPAD_DIRECTION_FLAG_MASK,
                                is_key_pressed);
                            break;
                        case SDLK_D:
                            emulation_controller.game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                                GameBoyCore::RIGHT_DPAD_DIRECTION_FLAG_MASK,
                                is_key_pressed);
                            break;
                        case SDLK_APOSTROPHE:
                            emulation_controller.game_boy_emulator.update_button_pressed_state_thread_safe(
                                GameBoyCore::A_BUTTON_FLAG_MASK,
                                is_key_pressed);
                            break;
                        case SDLK_PERIOD:
                            emulation_controller.game_boy_emulator.update_button_pressed_state_thread_safe(
                                GameBoyCore::B_BUTTON_FLAG_MASK,
                                is_key_pressed);
                            break;
                        case SDLK_RETURN:
                            emulation_controller.game_boy_emulator.update_button_pressed_state_thread_safe(
                                GameBoyCore::START_BUTTON_FLAG_MASK,
                                is_key_pressed);
                            break;
                        case SDLK_RSHIFT:
                            emulation_controller.game_boy_emulator.update_button_pressed_state_thread_safe(
                                GameBoyCore::SELECT_BUTTON_FLAG_MASK,
                                is_key_pressed);
                            break;
                    }
                }
            }
            break;
        }
    }
}

bool should_main_menu_bar_and_cursor_be_visible(
    const EmulationController& emulation_controller,
    FullscreenDisplayStatus& fullscreen_display_status,
    SDL_Window* sdl_window)
{
    const bool is_fullscreen_enabled = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    if (!is_fullscreen_enabled ||
        !emulation_controller.game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() ||
        emulation_controller.is_emulation_paused_atomic.load(std::memory_order_acquire))
    {
        return true;
    }

    float mouse_y_position_in_window;
    SDL_GetGlobalMouseState(nullptr, &mouse_y_position_in_window);
    const float main_menu_bar_height_pixels = ImGui::GetFrameHeight() * ImGui::GetIO().DisplayFramebufferScale.y;
    ImGuiIO& io = ImGui::GetIO();
    if (SDL_GetMouseFocus() == sdl_window)
    {
        if (fullscreen_display_status.is_main_menu_bar_hovered ||
            mouse_y_position_in_window <= main_menu_bar_height_pixels ||
            io.MouseDelta.x != 0.0f ||
            io.MouseDelta.y != 0.0f)
        {
            fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
            return true;
        }
    }

    if (fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden > 0.0f)
    {
        fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden -= ImGui::GetIO().DeltaTime;
    }
    return (fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden > 0.0f);
}

SDL_FRect get_sized_emulation_rectangle(
    SDL_Renderer* sdl_renderer,
    SDL_Window* sdl_window)
{
    const bool is_fullscreen_enabled = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    float space_reserved_for_menu_bar = 0.0f;

    if (!is_fullscreen_enabled)
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

void set_emulation_screen_blank(GraphicsController& graphics_controller)
{
    for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
    {
        graphics_controller.abgr_pixel_buffer[i] = graphics_controller.active_colour_palette[0];
    }
    SDL_UpdateTexture(
        graphics_controller.sdl_texture,
        nullptr,
        graphics_controller.abgr_pixel_buffer.get(),
        DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
}

void update_colour_palette(
    GameBoyCore::Emulator& game_boy_emulator,
    GraphicsController& graphics_controller,
    const uint8_t currently_published_frame_buffer_index)
{
    if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
    {
        auto const& pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);

        for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
        {
            graphics_controller.abgr_pixel_buffer[i] = graphics_controller.active_colour_palette[pixel_frame_buffer[i]];
        }
        SDL_UpdateTexture(
            graphics_controller.sdl_texture,
            nullptr,
            graphics_controller.abgr_pixel_buffer.get(),
            DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
    }
    else
        set_emulation_screen_blank(graphics_controller);
}

bool try_load_file_to_memory_with_dialog(
    GameBoyCore::FileType file_type,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    SDL_Window* sdl_window,
    std::string& error_message)
{
    file_loading_status.is_emulation_paused_before_rom_loading = emulation_controller.is_emulation_paused_atomic.load(std::memory_order_acquire);
    emulation_controller.is_emulation_paused_atomic.store(true, std::memory_order_release);

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
        if (emulation_controller.game_boy_emulator.try_load_file_to_memory(rom_path, file_type, error_message))
        {
            if (file_type == GameBoyCore::FileType::GameROM)
            {
                emulation_controller.game_boy_emulator.reset_state();
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
            SDL_SetWindowTitle(
                sdl_window,
                std::string("Emulate Game Boy - " + emulation_controller.game_boy_emulator.get_loaded_game_rom_title_thread_safe()).c_str());
        }
        emulation_controller.is_emulation_paused_atomic.store(false, std::memory_order_release);
    }
    else
    {
        file_loading_status.did_rom_loading_error_occur = (error_message != "");
        emulation_controller.is_emulation_paused_atomic.store(
            file_loading_status.is_emulation_paused_before_rom_loading,
            std::memory_order_release);
    }
    return is_operation_successful;
}

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
sdl_logical_presentation_imgui_workaround_t sdl_logical_presentation_imgui_workaround_pre_frame(SDL_Renderer* sdl_renderer)
{
    sdl_logical_presentation_imgui_workaround_t logical_values{};

    SDL_GetRenderLogicalPresentation(
        sdl_renderer,
        &logical_values.sdl_renderer_logical_width,
        &logical_values.sdl_renderer_logical_height,
        &logical_values.sdl_renderer_logical_presentation_mode);

    SDL_SetRenderLogicalPresentation(sdl_renderer,
        logical_values.sdl_renderer_logical_width,
        logical_values.sdl_renderer_logical_height,
        SDL_LOGICAL_PRESENTATION_DISABLED);

    return logical_values;
}

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
void sdl_logical_presentation_imgui_workaround_post_frame(
    SDL_Renderer* sdl_renderer,
    sdl_logical_presentation_imgui_workaround_t logical_values)
{
    SDL_SetRenderLogicalPresentation(
        sdl_renderer,
        logical_values.sdl_renderer_logical_width,
        logical_values.sdl_renderer_logical_height,
        logical_values.sdl_renderer_logical_presentation_mode);
}
