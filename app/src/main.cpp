#include <atomic>
#include <bit>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <nfd.h>
#include <SDL3/SDL.h>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>

#include "emulator.h"
#include "user_interface_utilities.h"
#include "raii_wrappers.h"

static void run_emulator_core(
    std::stop_token stop_token,
    GameBoyCore::Emulator &game_boy_emulator,
    std::atomic<bool> &is_emulation_paused,
    std::atomic<bool> &is_fast_emulation_enabled,
    std::atomic<double> &target_fast_emulation_speed,
    std::atomic<bool> &did_exception_occur,
    std::exception_ptr &exception_pointer)
{
    try
    {
        constexpr double frame_duration_seconds = 0.01674;
        const uint64_t counter_ticks_per_second = SDL_GetPerformanceFrequency();
        const uint64_t counter_ticks_per_frame_rounded = static_cast<uint64_t>(frame_duration_seconds * counter_ticks_per_second + 0.5);

        uint64_t next_frame_counter_tick = SDL_GetPerformanceCounter();
        uint8_t previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

        while (!stop_token.stop_requested())
        {
            if (!game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() ||
                is_emulation_paused.load(std::memory_order_acquire))
            {
                continue;
            }
            game_boy_emulator.step_central_processing_unit_single_instruction();

            const uint8_t currently_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;

                double target_emulation_speed = is_fast_emulation_enabled.load(std::memory_order_acquire) 
                    ? target_fast_emulation_speed.load(std::memory_order_acquire)
                    : 1.0;
                next_frame_counter_tick += counter_ticks_per_frame_rounded / target_emulation_speed;
                const uint64_t current_counter_tick = SDL_GetPerformanceCounter();

                if (next_frame_counter_tick > current_counter_tick)
                {
                    const uint64_t delay_in_nanoseconds = (next_frame_counter_tick - current_counter_tick) * 1'000'000'000ull / counter_ticks_per_second;
                    SDL_DelayPrecise(delay_in_nanoseconds);
                }
                else
                    next_frame_counter_tick = current_counter_tick;
            }
        }
    }
    catch (...)
    {
        exception_pointer = std::current_exception();
        did_exception_occur.store(true, std::memory_order_release);
    }
}

int main()
{
    try
    {
        ResourceAcquisitionIsInitialization::SdlInitializerRaii sdl_initializer{SDL_INIT_VIDEO};
        ResourceAcquisitionIsInitialization::SdlWindowRaii sdl_window
        {
            "Emulate Game Boy",
            DISPLAY_WIDTH_PIXELS * INITIAL_WINDOW_SCALE,
            DISPLAY_HEIGHT_PIXELS * INITIAL_WINDOW_SCALE,
            SDL_WINDOW_RESIZABLE
        };
        ResourceAcquisitionIsInitialization::SdlRendererRaii sdl_renderer{sdl_window};
        ResourceAcquisitionIsInitialization::SdlTextureRaii sdl_texture
        {
            sdl_renderer,
            SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_STREAMING,
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
        };
        ResourceAcquisitionIsInitialization::ImGuiContextRaii imgui_context{sdl_window.get(), sdl_renderer.get()};

        SDL_SetRenderLogicalPresentation
        (
            sdl_renderer.get(),
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
        );
        if (!SDL_SetRenderVSync(sdl_renderer.get(), 1))
        {
            std::cerr << "VSync unable to be used: " << SDL_GetError() << "\n";
        }
        SDL_SetTextureScaleMode(sdl_texture.get(), SDL_SCALEMODE_NEAREST);

        if (NFD_Init() != NFD_OKAY)
        {
            std::cerr << "Error: NativeFileDialogExtended was unable to be initialized, exiting.\n";
            return 1;
        }
        NFD_SetDisplayPropertiesFromSDLWindowForLinuxWayland(sdl_window.get());

        GameBoyCore::Emulator game_boy_emulator{};
        std::atomic<bool> is_emulation_paused{};

        std::atomic<bool> is_fast_forward_enabled{};
        std::atomic<double> target_fast_emulation_speed{1.5};
        int selected_fast_emulation_speed_index = 0;

        std::atomic<bool> did_emulator_core_exception_occur{};
        std::exception_ptr emulator_core_exception_pointer{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(game_boy_emulator),
            std::ref(is_emulation_paused),
            std::ref(is_fast_forward_enabled),
            std::ref(target_fast_emulation_speed),
            std::ref(did_emulator_core_exception_occur),
            std::ref(emulator_core_exception_pointer),
        };

        const uint32_t *active_colour_palette = light_green_colour_palette;
        int selected_colour_palette_index = 0;

        uint8_t previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

        std::unique_ptr<uint32_t[]> abgr_pixel_buffer = std::make_unique<uint32_t[]>(static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS));
        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());

        std::string error_message = "";
        bool pre_rom_loading_pause_state = false;
        bool did_rom_loading_error_occur = false;
        bool stop_emulating = false;

        bool was_fast_forward_key_previously_pressed = false;
        bool was_pause_key_previously_pressed = false;
        bool was_reset_key_previously_pressed = false;

        while (!stop_emulating)
        {
            if (did_emulator_core_exception_occur.load(std::memory_order_acquire))
            {
                std::rethrow_exception(emulator_core_exception_pointer);
            }

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

                        if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                        {
                            switch (sdl_event.key.key)
                            {
                                case SDLK_SPACE:
                                    if (is_key_pressed && !was_fast_forward_key_previously_pressed)
                                    {
                                        is_fast_forward_enabled.store(!is_fast_forward_enabled.load(std::memory_order_acquire), std::memory_order_release);
                                    }
                                    was_fast_forward_key_previously_pressed = is_key_pressed;
                                    break;
                                case SDLK_ESCAPE:
                                    if (is_key_pressed && !was_pause_key_previously_pressed)
                                    {
                                        is_emulation_paused.store(!is_emulation_paused.load(std::memory_order_acquire), std::memory_order_release);
                                    }
                                    was_pause_key_previously_pressed = is_key_pressed;
                                    break;
                                case SDLK_R:
                                    if (is_key_pressed && !was_reset_key_previously_pressed)
                                    {
                                        if (game_boy_emulator.is_bootrom_loaded_in_memory_thread_safe())
                                        {
                                            game_boy_emulator.reset_state(true);
                                        }
                                        else
                                        {
                                            game_boy_emulator.set_post_boot_state();
                                        }
                                        is_emulation_paused.store(false, std::memory_order_release);
                                    }
                                    was_reset_key_previously_pressed = is_key_pressed;
                                    break;
                                case SDLK_W:
                                    game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(GameBoyCore::UP_DIRECTION_PAD_FLAG_MASK, is_key_pressed);
                                    break;
                                case SDLK_A:
                                    game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(GameBoyCore::LEFT_DIRECTION_PAD_FLAG_MASK, is_key_pressed);
                                    break;
                                case SDLK_S:
                                    game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(GameBoyCore::DOWN_DIRECTION_PAD_FLAG_MASK, is_key_pressed);
                                    break;
                                case SDLK_D:
                                    game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(GameBoyCore::RIGHT_DIRECTION_PAD_FLAG_MASK, is_key_pressed);
                                    break;
                                case SDLK_APOSTROPHE:
                                    game_boy_emulator.update_joypad_button_pressed_state_thread_safe(GameBoyCore::A_BUTTON_FLAG_MASK, is_key_pressed);
                                    break;
                                case SDLK_PERIOD:
                                    game_boy_emulator.update_joypad_button_pressed_state_thread_safe(GameBoyCore::B_BUTTON_FLAG_MASK, is_key_pressed);
                                    break;
                                case SDLK_RETURN:
                                    game_boy_emulator.update_joypad_button_pressed_state_thread_safe(GameBoyCore::START_BUTTON_FLAG_MASK, is_key_pressed);
                                    break;
                                case SDLK_RSHIFT:
                                    game_boy_emulator.update_joypad_button_pressed_state_thread_safe(GameBoyCore::SELECT_BUTTON_FLAG_MASK, is_key_pressed);
                                    break;
                            }
                        }
                        if (sdl_event.key.key == SDLK_O && is_key_pressed)
                        {
                            try_load_file_to_memory_with_dialog(false, sdl_window.get(), game_boy_emulator, std::ref(is_emulation_paused), pre_rom_loading_pause_state, did_rom_loading_error_occur, error_message);
                        }
                    }
                    break;
                }
            }

            const uint8_t currently_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                auto const &pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);

                for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
                {
                    abgr_pixel_buffer[i] = active_colour_palette[pixel_frame_buffer[i]];
                }
                SDL_UpdateTexture(sdl_texture.get(), nullptr, abgr_pixel_buffer.get(), DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;
            }
            SDL_RenderClear(sdl_renderer.get());

            int renderer_output_width, renderer_output_height;
            SDL_GetRenderOutputSize(sdl_renderer.get(), &renderer_output_width, &renderer_output_height);
            float current_scale_x = static_cast<float>(renderer_output_width) / static_cast<float>(DISPLAY_WIDTH_PIXELS);
            float current_scale_y = static_cast<float>(renderer_output_height) / static_cast<float>(DISPLAY_HEIGHT_PIXELS);
            int current_integer_scale = std::max(1, std::min(int(current_scale_x), int(current_scale_y)));

            float menu_bar_logical_height = ImGui::GetFrameHeight() / static_cast<float>(current_integer_scale);
            SDL_FRect emulation_screen_rectangle
            {
                0.0f,
                menu_bar_logical_height,
                static_cast<float>(DISPLAY_WIDTH_PIXELS),
                static_cast<float>(DISPLAY_HEIGHT_PIXELS) - menu_bar_logical_height
            };
            SDL_RenderTexture(sdl_renderer.get(), sdl_texture.get(), nullptr, &emulation_screen_rectangle);

            // Workaround for https://github.com/ocornut/imgui/issues/8339
            int sdl_renderer_logical_width, sdl_renderer_logical_height;
            SDL_RendererLogicalPresentation sdl_renderer_logical_presentation_mode;
            SDL_GetRenderLogicalPresentation(sdl_renderer.get(), &sdl_renderer_logical_width, &sdl_renderer_logical_height, &sdl_renderer_logical_presentation_mode);
            SDL_SetRenderLogicalPresentation(sdl_renderer.get(), sdl_renderer_logical_width, sdl_renderer_logical_height, SDL_LOGICAL_PRESENTATION_DISABLED);

            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Load Game ROM"))
                    {
                        try_load_file_to_memory_with_dialog(false, sdl_window.get(), game_boy_emulator, std::ref(is_emulation_paused), pre_rom_loading_pause_state, did_rom_loading_error_occur, error_message);
                    }
                    if (ImGui::MenuItem("Load Boot ROM (Optional)"))
                    {
                        try_load_file_to_memory_with_dialog(true, sdl_window.get(), game_boy_emulator, std::ref(is_emulation_paused), pre_rom_loading_pause_state, did_rom_loading_error_occur, error_message);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Quit"))
                    {
                        stop_emulating = true;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Video"))
                {
                    ImGui::SeparatorText("Colour Palette");
                    if (ImGui::Combo("##Colour Palette", &selected_colour_palette_index, colour_palette_names, IM_ARRAYSIZE(colour_palette_names)))
                    {
                        switch (selected_colour_palette_index)
                        {
                            case 0:
                                active_colour_palette = light_green_colour_palette;
                                break;
                            case 1:
                                active_colour_palette = greyscale_colour_palette;
                                break;
                            case 2:
                                active_colour_palette = original_green_colour_palette;
                                break;
                        }
                        if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                        {
                            auto const &pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);

                            for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
                            {
                                abgr_pixel_buffer[i] = active_colour_palette[pixel_frame_buffer[i]];
                            }
                            SDL_UpdateTexture(sdl_texture.get(), nullptr, abgr_pixel_buffer.get(), DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
                        }
                        else
                            set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());
                    }
                    ImGui::EndMenu();
                }
                const bool current_fast_forward_enable_state = is_fast_forward_enabled.load(std::memory_order_acquire);
                const bool current_pause_state = is_emulation_paused.load(std::memory_order_acquire);
                if (ImGui::BeginMenu("Emulation"))
                {
                    ImGui::SeparatorText("Fast-Foward Speed");
                    if (ImGui::Combo("##Fast-Foward Speed", &selected_fast_emulation_speed_index, fast_forward_speed_names, IM_ARRAYSIZE(fast_forward_speed_names)))
                    {
                        target_fast_emulation_speed = selected_fast_emulation_speed_index * 0.25 + 1.5;
                    }
                    ImGui::Spacing();
                    if (ImGui::MenuItem(current_fast_forward_enable_state ? "Disable Fast-Forwarding" : "Enable Fast-Forwarding", "Space", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
                    {
                        is_fast_forward_enabled.store(!current_fast_forward_enable_state, std::memory_order_release);
                    }
                    if (ImGui::MenuItem(current_pause_state ? "Unpause" : "Pause", "Esc", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
                    {
                        is_emulation_paused.store(!current_pause_state, std::memory_order_release);
                    }
                    if (ImGui::MenuItem("Unload Game ROM", "", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
                    {
                        SDL_SetWindowTitle(sdl_window.get(), std::string("Emulate Game Boy").c_str());
                        game_boy_emulator.unload_game_rom_from_memory_thread_safe();
                        game_boy_emulator.reset_state(true);
                        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());
                        is_emulation_paused.store(false, std::memory_order_release);
                    }
                    if (ImGui::MenuItem("Unload Boot ROM", "", false, game_boy_emulator.is_bootrom_loaded_in_memory_thread_safe()))
                    {
                        game_boy_emulator.unload_bootrom_from_memory_thread_safe();

                        if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                        {
                            game_boy_emulator.set_post_boot_state();
                        }
                        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());
                        is_emulation_paused.store(false, std::memory_order_release);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Reset", "R", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
                    {
                        if (game_boy_emulator.is_bootrom_loaded_in_memory_thread_safe())
                        {
                            game_boy_emulator.reset_state(true);
                        }
                        else
                        {
                            game_boy_emulator.set_post_boot_state();
                        }
                        is_emulation_paused.store(false, std::memory_order_release);
                    }
                    ImGui::EndMenu();
                }
                if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                {
                    if (current_pause_state)
                    {
                        ImGui::TextDisabled("[Emulation Paused]");
                    }
                    if (current_fast_forward_enable_state)
                    {
                        ImGui::TextDisabled("[Fast-Forward Enabled]");
                    }
                }
                ImGui::EndMainMenuBar();
            }

            const float error_popup_x_position = ImGui::GetIO().DisplaySize.x * 0.5f;
            const float error_popup_y_position = ImGui::GetIO().DisplaySize.y * 0.5f;
            ImGui::SetNextWindowPos(ImVec2(error_popup_x_position, error_popup_y_position), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

            const float maximum_error_popup_width = ImGui::GetIO().DisplaySize.x * 0.4f;
            const float error_message_width = ImGui::CalcTextSize(error_message.c_str()).x;
            const float window_x_padding = ImGui::GetStyle().WindowPadding.x * 2.0f;
            const float minimum_error_popup_width = std::min(maximum_error_popup_width, error_message_width + window_x_padding);
            ImGui::SetNextWindowSizeConstraints(ImVec2(minimum_error_popup_width, 0), ImVec2(maximum_error_popup_width, FLT_MAX));

            if (did_rom_loading_error_occur)
            {
                is_emulation_paused.store(true, std::memory_order_release);
                ImGui::OpenPopup("Error");
            }
            if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar))
            {
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                ImGui::TextWrapped(error_message.c_str());
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                ImGui::Separator();
                if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                {
                    is_emulation_paused.store(pre_rom_loading_pause_state, std::memory_order_release);
                    ImGui::CloseCurrentPopup();
                    did_rom_loading_error_occur = false;
                    error_message = "";
                }
                ImGui::EndPopup();
            }

            ImGui::Render();
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl_renderer.get());

            // Workaround for https://github.com/ocornut/imgui/issues/8339
            SDL_SetRenderLogicalPresentation(sdl_renderer.get(), sdl_renderer_logical_width, sdl_renderer_logical_height, sdl_renderer_logical_presentation_mode);

            SDL_RenderPresent(sdl_renderer.get());
        }
        NFD_Quit();
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Error: " << exception.what() << ", exiting.\n";
        NFD_Quit();
        return 1;
    }   
}
