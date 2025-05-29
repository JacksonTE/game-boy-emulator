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
#include "raii_wrappers.h"

constexpr uint8_t RIGHT_DIRECTION_PAD_FLAG_MASK = 1 << 0;
constexpr uint8_t LEFT_DIRECTION_PAD_FLAG_MASK = 1 << 1;
constexpr uint8_t UP_DIRECTION_PAD_FLAG_MASK = 1 << 2;
constexpr uint8_t DOWN_DIRECTION_PAD_FLAG_MASK = 1 << 3;

constexpr uint8_t A_BUTTON_FLAG_MASK = 1 << 0;
constexpr uint8_t B_BUTTON_FLAG_MASK = 1 << 1;
constexpr uint8_t SELECT_BUTTON_FLAG_MASK = 1 << 2;
constexpr uint8_t START_BUTTON_FLAG_MASK = 1 << 3;

constexpr bool BUTTON_PRESSED_STATE = false;
constexpr bool BUTTON_RELEASED_STATE = true;

constexpr uint8_t DISPLAY_WIDTH_PIXELS = 160;
constexpr uint8_t DISPLAY_HEIGHT_PIXELS = 144;
constexpr int INITIAL_WINDOW_SCALE = 5;

static void run_emulator_core(
    std::stop_token stop_token,
    GameBoyCore::Emulator &game_boy_emulator,
    std::atomic<bool> &is_emulation_paused,
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

                next_frame_counter_tick += counter_ticks_per_frame_rounded;
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

static constexpr uint32_t light_green_colour_palette[4] =
{
    get_abgr_value_for_current_endianness(0xff, 0xd0, 0xf8, 0xe0),
    get_abgr_value_for_current_endianness(0xff, 0x70, 0xc0, 0x88),
    get_abgr_value_for_current_endianness(0xff, 0x56, 0x68, 0x34),
    get_abgr_value_for_current_endianness(0xff, 0x20, 0x18, 0x08)
};

static constexpr uint32_t original_green_colour_palette[4] =
{
    get_abgr_value_for_current_endianness(0xff, 0x0f, 0xbc, 0x9b),
    get_abgr_value_for_current_endianness(0xff, 0x0f, 0xac, 0x8b),
    get_abgr_value_for_current_endianness(0xff, 0x30, 0x62, 0x30),
    get_abgr_value_for_current_endianness(0xff, 0x0f, 0x38, 0x0f)
};

static constexpr uint32_t greyscale_colour_palette[4] =
{
    get_abgr_value_for_current_endianness(0xff, 0xff, 0xff, 0xff),
    get_abgr_value_for_current_endianness(0xff, 0xaa, 0xaa, 0xaa),
    get_abgr_value_for_current_endianness(0xff, 0x55, 0x55, 0x55),
    get_abgr_value_for_current_endianness(0xff, 0x00, 0x00, 0x00)
};

static const char *colour_palette_names[] =
{
    "Light Green",
    "Original Green",
    "Greyscale"
};


static void set_emulation_screen_blank(const uint32_t *active_colour_palette, uint32_t *abgr_pixel_buffer,  SDL_Texture *sdl_texture)
{
    for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
    {
        abgr_pixel_buffer[i] = active_colour_palette[0];
    }
    SDL_UpdateTexture(sdl_texture, nullptr, abgr_pixel_buffer, DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
}

static bool try_load_file_to_memory_with_dialog(
    GameBoyCore::Emulator &game_boy_emulator,
    bool is_bootrom_file,
    std::atomic<bool> &is_emulation_paused,
    std::string &error_message)
{
    const bool previous_pause_state = is_emulation_paused.load(std::memory_order_acquire);
    is_emulation_paused.store(true, std::memory_order_release);

    nfdopendialogu8args_t open_dialog_arguments{};
    nfdu8filteritem_t filters[] =
    {
        {is_bootrom_file ? "Game Boy Boot ROM" : "Game Boy ROMs", "gb,bin,rom"}
    };
    open_dialog_arguments.filterList = filters;
    open_dialog_arguments.filterCount = 1;

    nfdchar_t *rom_path = nullptr;
    nfdresult_t result = NFD_OpenDialogU8_With(&rom_path, &open_dialog_arguments);
    bool is_operation_successful = false;

    if (result == NFD_OKAY)
    {
        if (game_boy_emulator.try_load_file_to_memory(rom_path, is_bootrom_file, error_message))
        {
            if (game_boy_emulator.is_bootrom_loaded_in_memory_thread_safe())
            {
                game_boy_emulator.reset_state(true);
            }
            else
            {
                game_boy_emulator.set_post_boot_state();
            }
            is_operation_successful = true;
        }
        NFD_FreePathU8(rom_path);
    }
    else if (result == NFD_CANCEL)
    {
        is_operation_successful = true;
    }
    else if (result == NFD_ERROR)
    {
        std::cerr << "NFD error: " << NFD_GetError() << "\n";
        error_message = NFD_GetError();
    }
    is_emulation_paused.store(previous_pause_state, std::memory_order_release);
    return is_operation_successful;
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

        GameBoyCore::Emulator game_boy_emulator{};
        std::atomic<bool> is_emulation_paused{};
        std::atomic<bool> did_emulator_core_exception_occur{};
        std::exception_ptr emulator_core_exception_pointer{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(game_boy_emulator),
            std::ref(is_emulation_paused),
            std::ref(did_emulator_core_exception_occur),
            std::ref(emulator_core_exception_pointer),
        };

        uint8_t previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

        const uint32_t *active_colour_palette = light_green_colour_palette;
        int selected_colour_palette_index = 0;

        std::unique_ptr<uint32_t[]> abgr_pixel_buffer = std::make_unique<uint32_t[]>(static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS));
        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());

        std::string error_message;
        bool did_error_occur = false;
        bool stop_emulating = false;

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
                        const bool key_pressed_state = (sdl_event.type == SDL_EVENT_KEY_DOWN ? BUTTON_PRESSED_STATE : BUTTON_RELEASED_STATE);

                        switch (sdl_event.key.key)
                        {
                            case SDLK_X:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(A_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_Z:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(B_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_BACKSPACE:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(SELECT_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_RETURN:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(START_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_RIGHT:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(RIGHT_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_LEFT:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(LEFT_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_UP:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(UP_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_DOWN:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(DOWN_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
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
                        did_error_occur = !try_load_file_to_memory_with_dialog(game_boy_emulator, false, std::ref(is_emulation_paused), error_message);
                    }
                    if (ImGui::MenuItem("Load Boot ROM (Optional)"))
                    {
                        did_error_occur = !try_load_file_to_memory_with_dialog(game_boy_emulator, true, std::ref(is_emulation_paused), error_message);
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
                                active_colour_palette = original_green_colour_palette;
                                break;
                            case 2:
                                active_colour_palette = greyscale_colour_palette;
                                break;
                        }
                        if (!game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                        {
                            set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Emulation"))
                {
                    const bool current_pause_state = is_emulation_paused.load(std::memory_order_acquire);

                    if (ImGui::MenuItem(current_pause_state ? "Unpause" : "Pause", "", false))
                    {
                        is_emulation_paused.store(!current_pause_state, std::memory_order_release);
                    }
                    if (ImGui::MenuItem("Unload Game ROM", "", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
                    {
                        game_boy_emulator.unload_game_rom_from_memory_thread_safe();
                        game_boy_emulator.reset_state(true);
                        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());
                    }
                    if (ImGui::MenuItem("Unload Boot ROM", "", false, game_boy_emulator.is_bootrom_loaded_in_memory_thread_safe()))
                    {
                        game_boy_emulator.unload_bootrom_from_memory_thread_safe();

                        if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                        {
                            game_boy_emulator.set_post_boot_state();
                        }
                        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Reset", "", false, game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
                    {
                        if (game_boy_emulator.is_bootrom_loaded_in_memory_thread_safe())
                        {
                            game_boy_emulator.reset_state(true);
                        }
                        else
                        {
                            game_boy_emulator.set_post_boot_state();
                        }
                        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());
                    }
                    ImGui::EndMenu();
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

            if (did_error_occur)
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
                    is_emulation_paused.store(false, std::memory_order_release);
                    ImGui::CloseCurrentPopup();
                    did_error_occur = false;
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
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Error: " << exception.what() << ", exiting.\n";
        return 1;
    }   
}
