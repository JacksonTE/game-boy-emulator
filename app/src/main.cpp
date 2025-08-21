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

#ifdef __linux__
#include <gtk/gtk.h>
#endif

#include "emulator.h"
#include "user_interface_utilities.h"
#include "raii_wrappers.h"

static void run_emulator_core(
    std::stop_token stop_token,
    GameBoyCore::Emulator& game_boy_emulator,
    std::atomic<bool>& is_emulation_paused_atomic,
    std::atomic<bool>& is_fast_emulation_enabled_atomic,
    std::atomic<double>& target_fast_emulation_speed_atomic,
    std::atomic<bool>& did_exception_occur_atomic,
    std::exception_ptr& exception_pointer)
{
    try
    {
        constexpr double frame_duration_seconds = 0.01674;
        const uint64_t counter_ticks_per_second = SDL_GetPerformanceFrequency();
        const uint64_t counter_ticks_per_frame_rounded = static_cast<uint64_t>(frame_duration_seconds * counter_ticks_per_second + 0.5);

        uint64_t next_frame_counter_tick = SDL_GetPerformanceCounter();
        uint8_t previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();

        while (!stop_token.stop_requested())
        {
            if (!game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() ||
                is_emulation_paused_atomic.load(std::memory_order_acquire))
            {
                continue;
            }
            game_boy_emulator.step_central_processing_unit_single_instruction();

            const uint8_t currently_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();

            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;

                double target_emulation_speed = is_fast_emulation_enabled_atomic.load(std::memory_order_acquire)
                    ? target_fast_emulation_speed_atomic.load(std::memory_order_acquire)
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
        did_exception_occur_atomic.store(true, std::memory_order_release);
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

#ifdef __linux__
        gtk_init(NULL, NULL);
#endif
        if (NFD_Init() != NFD_OKAY)
        {
            std::cerr << "Error: NativeFileDialogExtended was unable to be initialized, exiting.\n";
            return 1;
        }

        GameBoyCore::Emulator game_boy_emulator{};
        std::atomic<bool> is_emulation_paused_atomic{};

        std::atomic<bool> is_fast_forward_enabled_atomic{};
        std::atomic<double> target_fast_emulation_speed_atomic{1.5};
        int selected_fast_emulation_speed_index = 0;

        std::atomic<bool> did_emulator_core_exception_occur_atomic{};
        std::exception_ptr emulator_core_exception_pointer{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(game_boy_emulator),
            std::ref(is_emulation_paused_atomic),
            std::ref(is_fast_forward_enabled_atomic),
            std::ref(target_fast_emulation_speed_atomic),
            std::ref(did_emulator_core_exception_occur_atomic),
            std::ref(emulator_core_exception_pointer),
        };

        const uint32_t* active_colour_palette = sage_colour_palette;
        int selected_colour_palette_index = 0;

        uint8_t previously_published_frame_buffer_index = 0;
        uint8_t currently_published_frame_buffer_index = 0;

        std::unique_ptr<uint32_t[]> abgr_pixel_buffer = std::make_unique<uint32_t[]>(static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS));
        set_emulation_screen_blank(active_colour_palette, abgr_pixel_buffer.get(), sdl_texture.get());

        std::string error_message = "";
        bool is_emulation_paused_before_rom_loading = false;
        bool did_rom_loading_error_occur = false;
        bool is_custom_palette_editor_open = false;
        bool stop_emulating = false;

        bool was_fast_forward_key_previously_pressed = false;
        bool was_pause_key_previously_pressed = false;
        bool was_reset_key_previously_pressed = false;

        while (!stop_emulating)
        {
            if (did_emulator_core_exception_occur_atomic.load(std::memory_order_acquire))
            {
                std::rethrow_exception(emulator_core_exception_pointer);
            }

            handle_sdl_events(
                stop_emulating,
                did_rom_loading_error_occur,
                is_emulation_paused_before_rom_loading,
                was_fast_forward_key_previously_pressed,
                was_pause_key_previously_pressed,
                was_reset_key_previously_pressed,
                is_emulation_paused_atomic,
                is_fast_forward_enabled_atomic,
                game_boy_emulator,
                sdl_window.get(),
                error_message
            );

            currently_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();

            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                auto const& pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);

                for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
                {
                    abgr_pixel_buffer[i] = active_colour_palette[pixel_frame_buffer[i]];
                }
                SDL_UpdateTexture(sdl_texture.get(), nullptr, abgr_pixel_buffer.get(), DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;
            }

            SDL_RenderClear(sdl_renderer.get());
            SDL_FRect emulation_screen_rectangle = get_sized_emulation_rectangle(sdl_renderer.get());
            SDL_RenderTexture(sdl_renderer.get(), sdl_texture.get(), nullptr, &emulation_screen_rectangle);

            sdl_logical_presentation_imgui_workaround_t logical_values = sdl_logical_presentation_imgui_workaround_pre_frame(sdl_renderer.get());
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            render_main_menu_bar(
                stop_emulating,
                is_emulation_paused_before_rom_loading,
                did_rom_loading_error_occur,
                is_custom_palette_editor_open,
                selected_colour_palette_index,
                selected_fast_emulation_speed_index,
                active_colour_palette,
                currently_published_frame_buffer_index,
                abgr_pixel_buffer.get(),
                is_emulation_paused_atomic,
                is_fast_forward_enabled_atomic,
                target_fast_emulation_speed_atomic,
                sdl_window.get(),
                sdl_texture.get(),
                game_boy_emulator,
                error_message
            );
            render_custom_colour_palette_editor(
                game_boy_emulator,
                is_custom_palette_editor_open,
                currently_published_frame_buffer_index,
                active_colour_palette,
                selected_colour_palette_index,
                abgr_pixel_buffer.get(),
                sdl_texture.get()
            );
            render_error_message_popup(
                did_rom_loading_error_occur,
                is_emulation_paused_before_rom_loading,
                is_emulation_paused_atomic,
                error_message
            );

            ImGui::Render();
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl_renderer.get());
            sdl_logical_presentation_imgui_workaround_post_frame(sdl_renderer.get(), logical_values);

            SDL_RenderPresent(sdl_renderer.get());
        }
        NFD_Quit();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << ", exiting.\n";
        NFD_Quit();
        return 1;
    }   
}
