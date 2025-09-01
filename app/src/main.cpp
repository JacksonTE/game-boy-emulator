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

#include "application_utilities.h"
#include "emulator.h"
#include "imgui_utilities.h"
#include "raii_wrappers.h"
#include "state_data_types.h"

static void run_emulator_core(
    std::stop_token stop_token,
    EmulationController& emulation_controller,
    std::atomic<bool>& did_exception_occur_atomic,
    std::exception_ptr& exception_pointer
)
{
    try
    {
        constexpr double FRAME_DURATION_SECONDS = 0.01674;
        const uint64_t counter_ticks_per_second = SDL_GetPerformanceFrequency();
        const uint64_t counter_ticks_per_frame_rounded = static_cast<uint64_t>(FRAME_DURATION_SECONDS * counter_ticks_per_second + 0.5);

        uint64_t next_frame_counter_tick = SDL_GetPerformanceCounter();
        uint8_t previously_published_frame_buffer_index = emulation_controller.game_boy_emulator.get_published_frame_buffer_index_thread_safe();

        while (!stop_token.stop_requested())
        {
            if (!emulation_controller.game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() ||
                emulation_controller.is_emulation_paused_atomic.load(std::memory_order_acquire))
            {
                SDL_Delay(0);
                continue;
            }
            emulation_controller.game_boy_emulator.step_central_processing_unit_single_instruction();

            const uint8_t currently_published_frame_buffer_index = emulation_controller.game_boy_emulator.get_published_frame_buffer_index_thread_safe();

            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;

                double target_emulation_speed = emulation_controller.is_fast_forward_enabled_atomic.load(std::memory_order_acquire)
                    ? emulation_controller.target_fast_forward_multiplier_atomic.load(std::memory_order_acquire)
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
        ResourceAcquisitionIsInitialization::SdlRendererRaii sdl_renderer
        {
            sdl_window,
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS
        };
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

        EmulationController emulation_controller{};
        std::atomic<bool> did_emulator_core_exception_occur_atomic{};
        std::exception_ptr emulator_core_exception_pointer{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(emulation_controller),
            std::ref(did_emulator_core_exception_occur_atomic),
            std::ref(emulator_core_exception_pointer),
        };

        FileLoadingStatus file_loading_status{};
        FullscreenDisplayStatus fullscreen_display_status{};
        constexpr uint32_t initial_custom_colour_palette[4] =
        {
            get_abgr_value_for_current_endianness(0xFF, 0xEF, 0xE0, 0x90),
            get_abgr_value_for_current_endianness(0xFF, 0xD8, 0xB4, 0x00),
            get_abgr_value_for_current_endianness(0xFF, 0xB6, 0x77, 0x00),
            get_abgr_value_for_current_endianness(0xFF, 0x5E, 0x04, 0x03)
        };
        GraphicsController graphics_controller{
            SAGE_COLOUR_PALETTE,
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
            initial_custom_colour_palette,
            sdl_texture.get(),
        };
        KeyPressedStates key_pressed_states{};
        MenuProperties menu_properties{};
        set_emulation_screen_blank(graphics_controller);

        uint8_t previously_published_frame_buffer_index = 0;
        std::string error_message = "";
        bool should_stop_emulation = false;

        while (!should_stop_emulation)
        {
            if (did_emulator_core_exception_occur_atomic.load(std::memory_order_acquire))
            {
                std::rethrow_exception(emulator_core_exception_pointer);
            }

            handle_sdl_events(
                emulation_controller,
                file_loading_status,
                fullscreen_display_status,
                key_pressed_states,
                sdl_window.get(),
                should_stop_emulation,
                error_message);

            const uint8_t currently_published_frame_buffer_index = emulation_controller.game_boy_emulator.get_published_frame_buffer_index_thread_safe();
            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                auto const& pixel_frame_buffer = emulation_controller.game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);

                for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
                {
                    graphics_controller.abgr_pixel_buffer[i] = graphics_controller.active_colour_palette[pixel_frame_buffer[i]];
                }
                SDL_UpdateTexture(sdl_texture.get(), nullptr, graphics_controller.abgr_pixel_buffer.get(), DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;
            }

            SDL_RenderClear(sdl_renderer.get());
            SDL_FRect emulation_screen_rectangle = get_sized_emulation_rectangle(sdl_renderer.get(), sdl_window.get());
            SDL_RenderTexture(sdl_renderer.get(), sdl_texture.get(), nullptr, &emulation_screen_rectangle);

            sdl_logical_presentation_imgui_workaround_t logical_values = sdl_logical_presentation_imgui_workaround_pre_frame(sdl_renderer.get());
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            fullscreen_display_status.are_main_menu_bar_and_cursor_visible =
                should_main_menu_bar_and_cursor_be_visible(emulation_controller, fullscreen_display_status, sdl_window.get());
            if (fullscreen_display_status.are_main_menu_bar_and_cursor_visible)
            {
                if (!SDL_CursorVisible())
                {
                    SDL_ShowCursor();
                }
                render_main_menu_bar(
                    currently_published_frame_buffer_index,
                    emulation_controller,
                    file_loading_status,
                    fullscreen_display_status,
                    graphics_controller,
                    menu_properties,
                    sdl_window.get(),
                    should_stop_emulation,
                    error_message);
            }
            else if (SDL_CursorVisible())
            {
                SDL_HideCursor();
            }

            render_custom_colour_palette_editor(
                currently_published_frame_buffer_index,
                emulation_controller.game_boy_emulator,
                menu_properties,
                graphics_controller);

            render_error_message_popup(
                file_loading_status,
                emulation_controller.is_emulation_paused_atomic,
                error_message);

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
