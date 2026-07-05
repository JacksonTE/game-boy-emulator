#include <atomic>
#include <exception>
#include <iostream>
#include <SDL3/SDL.h>
#include <stop_token>
#include <string>
#include <thread>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <gtk/gtk.h>
#endif

#include "display_utilities.h"
#include "emulator.h"
#include "input_events.h"
#include "raii_wrappers.h"
#include "gui_state_types.h"
#include "persistent_settings.h"

struct WebThreadPacingState
{
    std::atomic<uint64_t> target_published_frame_sequence_number_atomic{};
};

static void run_emulator_core(
    std::stop_token stop_token,
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    std::atomic<bool>& did_exception_occur_atomic,
    std::exception_ptr& exception_pointer
#ifdef __EMSCRIPTEN__
    , WebThreadPacingState* web_thread_pacing_state
#endif
)
{
    try
    {
#ifdef __EMSCRIPTEN__
        web_thread_pacing_state->target_published_frame_sequence_number_atomic.store(
            game_boy_emulator.get_published_frame_sequence_number_thread_safe(),
            std::memory_order_release);
#else
        constexpr double FRAME_DURATION_SECONDS = 0.01674;
        const uint64_t counter_ticks_per_second = SDL_GetPerformanceFrequency();
        const uint64_t counter_ticks_per_frame_rounded = static_cast<uint64_t>(FRAME_DURATION_SECONDS * counter_ticks_per_second + 0.5);

        uint64_t next_frame_counter_tick = SDL_GetPerformanceCounter();
        uint8_t previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();
#endif

        while (!stop_token.stop_requested())
        {
            if (!game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() ||
                emulation_controller.is_emulation_paused_atomic.load(std::memory_order_acquire))
            {
                SDL_Delay(0);
#ifdef __EMSCRIPTEN__
                web_thread_pacing_state->target_published_frame_sequence_number_atomic.store(
                    game_boy_emulator.get_published_frame_sequence_number_thread_safe(),
                    std::memory_order_release);
#else
                next_frame_counter_tick = SDL_GetPerformanceCounter();
                previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();
#endif
                continue;
            }

#ifdef __EMSCRIPTEN__
            const uint64_t target_published_frame_sequence_number =
                web_thread_pacing_state->target_published_frame_sequence_number_atomic.load(std::memory_order_acquire);

            if (game_boy_emulator.get_published_frame_sequence_number_thread_safe() >= target_published_frame_sequence_number)
            {
                SDL_Delay(0);
                continue;
            }

            while (!stop_token.stop_requested() &&
                   game_boy_emulator.get_published_frame_sequence_number_thread_safe() < target_published_frame_sequence_number)
            {
                game_boy_emulator.execute_next_instruction();
            }
#else
            game_boy_emulator.execute_next_instruction();

            const uint8_t currently_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index_thread_safe();

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
                {
                    next_frame_counter_tick = current_counter_tick;
                }
            }
#endif
        }
    }
    catch (...)
    {
        exception_pointer = std::current_exception();
        did_exception_occur_atomic.store(true, std::memory_order_release);
    }
}

// Allows menu bar and the rendering rectangle to resize responsively while window size is being changed
static bool resize_event_watch_callback(void* render_context_data, SDL_Event* sdl_event)
{
    if (sdl_event->type == SDL_EVENT_WINDOW_EXPOSED ||
        sdl_event->type == SDL_EVENT_WINDOW_RESIZED)
    {
        RenderContext* render_context = static_cast<RenderContext*>(render_context_data);
        update_imgui_scale_by_resolution(render_context->sdl_window);
        render_frame(*render_context);
    }
    return true;
}

#ifdef __EMSCRIPTEN__

// Holds all state that the emscripten main loop callback needs
struct EmscriptenLoopState
{
    GameBoyEmulator::Emulator* game_boy_emulator;
    EmulationController* emulation_controller;
    WebThreadPacingState* web_thread_pacing_state;
    FileLoadingStatus* file_loading_status;
    MenuAndCursorDisplayStatus* menu_and_cursor_display_status;
    KeyPressedStates* key_pressed_states;
    KeyBindings* key_bindings;
    MenuProperties* menu_properties;
    RenderContext* render_context;
    std::atomic<bool>* did_emulator_core_exception_occur_atomic;
    std::exception_ptr* emulator_core_exception_pointer;
    PersistentSettings* persistent_settings;
    bool* should_stop_emulation;
    std::string* error_message;
    SDL_Window* sdl_window;
    uint64_t target_published_frame_sequence_number;
};

static void sync_emscripten_window_to_viewport(
    EmscriptenLoopState* state,
    int viewport_w,
    int viewport_h)
{
    int sdl_w, sdl_h;
    SDL_GetWindowSize(state->sdl_window, &sdl_w, &sdl_h);

    if (viewport_w != sdl_w || viewport_h != sdl_h)
    {
        SDL_SetWindowSize(state->sdl_window, viewport_w, viewport_h);
    }

    update_imgui_scale_by_resolution(state->sdl_window);
    render_frame(*state->render_context);
}

static EM_BOOL emscripten_resize_callback(int, const EmscriptenUiEvent* ui_event, void* user_data)
{
    EmscriptenLoopState* state = static_cast<EmscriptenLoopState*>(user_data);

    const int viewport_w = ui_event->windowInnerWidth;
    const int viewport_h = ui_event->windowInnerHeight;

    sync_emscripten_window_to_viewport(state, viewport_w, viewport_h);

    return EM_FALSE;
}

static EM_BOOL emscripten_fullscreen_change_callback(int, const EmscriptenFullscreenChangeEvent*, void* user_data)
{
    EmscriptenLoopState* state = static_cast<EmscriptenLoopState*>(user_data);

    const int viewport_w = EM_ASM_INT({ return window.innerWidth; });
    const int viewport_h = EM_ASM_INT({ return window.innerHeight; });

    sync_emscripten_window_to_viewport(state, viewport_w, viewport_h);

    return EM_FALSE;
}

static void emscripten_main_loop_iteration(void* arg)
{
    EmscriptenLoopState* state = static_cast<EmscriptenLoopState*>(arg);

    if (state->did_emulator_core_exception_occur_atomic->load(std::memory_order_acquire))
    {
        emscripten_cancel_main_loop();
        std::rethrow_exception(*state->emulator_core_exception_pointer);
    }

    handle_sdl_events(
        *state->game_boy_emulator,
        *state->emulation_controller,
        *state->file_loading_status,
        *state->menu_and_cursor_display_status,
        *state->key_pressed_states,
        *state->key_bindings,
        *state->menu_properties,
        state->sdl_window,
        &state->persistent_settings->loaded_game_rom_path,
        &state->persistent_settings->loaded_boot_rom_path,
        *state->should_stop_emulation,
        *state->error_message);

    consume_pending_web_file_selection(
        *state->game_boy_emulator,
        *state->emulation_controller,
        *state->file_loading_status,
        *state->menu_and_cursor_display_status,
        state->sdl_window,
        &state->persistent_settings->loaded_game_rom_path,
        &state->persistent_settings->loaded_boot_rom_path,
        *state->error_message);

    if (*state->should_stop_emulation)
    {
        emscripten_cancel_main_loop();
        return;
    }

    if (!state->game_boy_emulator->is_game_rom_loaded_in_memory_thread_safe() ||
        state->emulation_controller->is_emulation_paused_atomic.load(std::memory_order_acquire))
    {
        state->target_published_frame_sequence_number =
            state->game_boy_emulator->get_published_frame_sequence_number_thread_safe();
        state->web_thread_pacing_state->target_published_frame_sequence_number_atomic.store(
            state->target_published_frame_sequence_number,
            std::memory_order_release);
        render_frame(*state->render_context);
        return;
    }

    const double target_emulation_speed = state->emulation_controller->is_fast_forward_enabled_atomic.load(std::memory_order_acquire)
        ? state->emulation_controller->target_fast_forward_multiplier_atomic.load(std::memory_order_acquire)
        : 1.0;
    const uint64_t frames_to_advance = std::max<uint64_t>(1, static_cast<uint64_t>(target_emulation_speed));
    state->target_published_frame_sequence_number += frames_to_advance;
    state->web_thread_pacing_state->target_published_frame_sequence_number_atomic.store(
        state->target_published_frame_sequence_number,
        std::memory_order_release);

    render_frame(*state->render_context);
}

#endif

int main()
{
    try
    {
        ResourceAcquisitionIsInitialization::SdlInitializerRaii sdl_initializer{SDL_INIT_VIDEO};

#ifdef __EMSCRIPTEN__
        const int initial_window_w = EM_ASM_INT({ return window.innerWidth; });
        const int initial_window_h = EM_ASM_INT({ return window.innerHeight; });
#else
        const int adaptive_window_scale = get_initial_window_scale_for_display();
        const int initial_window_w = DISPLAY_WIDTH_PIXELS * adaptive_window_scale;
        const int initial_window_h = DISPLAY_HEIGHT_PIXELS * adaptive_window_scale;
#endif
        ResourceAcquisitionIsInitialization::SdlWindowRaii sdl_window
        {
            "Emulate Game Boy",
            initial_window_w,
            initial_window_h,
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

        update_imgui_scale_by_resolution(sdl_window.get());

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
        gtk_init(NULL, NULL);
#endif
#ifndef __EMSCRIPTEN__
        ResourceAcquisitionIsInitialization::NfdInitializerRaii nfd_initializer{};
#endif

        GameBoyEmulator::Emulator game_boy_emulator{};
        EmulationController emulation_controller{};
        std::atomic<bool> did_emulator_core_exception_occur_atomic{};
        std::exception_ptr emulator_core_exception_pointer{};
        WebThreadPacingState web_thread_pacing_state{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(game_boy_emulator),
            std::ref(emulation_controller),
            std::ref(did_emulator_core_exception_occur_atomic),
            std::ref(emulator_core_exception_pointer)
#ifdef __EMSCRIPTEN__
            , &web_thread_pacing_state
#endif
        };

        FileLoadingStatus file_loading_status{};
        MenuAndCursorDisplayStatus menu_and_cursor_display_status{};
        FrameDiagnosticsState frame_diagnostics_state{};
#ifndef __EMSCRIPTEN__
        frame_diagnostics_state.performance_counter_frequency = SDL_GetPerformanceFrequency();
#endif
        constexpr uint32_t initial_custom_colour_palette[4] =
        {
            get_abgr_value_for_current_endianness(0xFF, 0xEF, 0xE0, 0x90),
            get_abgr_value_for_current_endianness(0xFF, 0xD8, 0xB4, 0x00),
            get_abgr_value_for_current_endianness(0xFF, 0xB6, 0x77, 0x00),
            get_abgr_value_for_current_endianness(0xFF, 0x5E, 0x04, 0x03)
        };
        GraphicsController graphics_controller
        {
            SAGE_COLOUR_PALETTE,
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
            initial_custom_colour_palette,
            sdl_texture.get(),
        };
        KeyPressedStates key_pressed_states{};
        MenuProperties menu_properties{};
        KeyBindings key_bindings{};
        set_emulation_screen_blank(graphics_controller);

        uint8_t previously_published_frame_buffer_index = 0;
        std::string error_message = "";
        bool should_stop_emulation = false;

        PersistentSettings persistent_settings{};
        if (load_settings_from_file(persistent_settings))
        {
            apply_loaded_settings(
                persistent_settings,
                key_bindings,
                menu_properties,
                graphics_controller,
                emulation_controller);

            if (!persistent_settings.loaded_boot_rom_path.empty() &&
                std::filesystem::exists(persistent_settings.loaded_boot_rom_path))
            {
                game_boy_emulator.try_to_load_file_to_memory(
                    persistent_settings.loaded_boot_rom_path.c_str(),
                    GameBoyEmulator::FileType::BootROM,
                    error_message);
            }

            if (!persistent_settings.loaded_game_rom_path.empty() &&
                std::filesystem::exists(persistent_settings.loaded_game_rom_path))
            {
                if (game_boy_emulator.try_to_load_file_to_memory(
                        persistent_settings.loaded_game_rom_path.c_str(),
                        GameBoyEmulator::FileType::GameROM,
                        error_message))
                {
                    game_boy_emulator.try_load_save_file(std::filesystem::path(SDL_GetBasePath()));
                    game_boy_emulator.reset_state();
                    SDL_SetWindowTitle(
                        sdl_window.get(),
                        std::string("Emulate Game Boy - " + game_boy_emulator.get_loaded_game_rom_title_thread_safe()).c_str());
                }
            }
        }

        RenderContext render_context
        {
            &game_boy_emulator,
            &emulation_controller,
            &file_loading_status,
            &menu_and_cursor_display_status,
            &frame_diagnostics_state,
            &graphics_controller,
            &menu_properties,
            &key_bindings,
            sdl_renderer.get(),
            sdl_texture.get(),
            sdl_window.get(),
            &previously_published_frame_buffer_index,
            &persistent_settings.loaded_game_rom_path,
            &persistent_settings.loaded_boot_rom_path,
            false,
            &should_stop_emulation,
            &error_message
        };
        ResourceAcquisitionIsInitialization::SdlEventWatchRaii event_watch
        {
            resize_event_watch_callback,
            &render_context
        };

#ifdef __EMSCRIPTEN__
        EmscriptenLoopState loop_state
        {
            &game_boy_emulator,
            &emulation_controller,
            &web_thread_pacing_state,
            &file_loading_status,
            &menu_and_cursor_display_status,
            &key_pressed_states,
            &key_bindings,
            &menu_properties,
            &render_context,
            &did_emulator_core_exception_occur_atomic,
            &emulator_core_exception_pointer,
            &persistent_settings,
            &should_stop_emulation,
            &error_message,
            sdl_window.get(),
            game_boy_emulator.get_published_frame_sequence_number_thread_safe()
        };
        // 0 = use requestAnimationFrame (browser controls frame rate)
        // false = don't block (return control to browser immediately)
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &loop_state, false, emscripten_resize_callback);
        emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &loop_state, false, emscripten_fullscreen_change_callback);
        emscripten_set_main_loop_arg(emscripten_main_loop_iteration, &loop_state, 0, false);
        // main() returns here but the loop continues via the browser event loop
        // The RAII destructors must NOT run yet — Emscripten handles cleanup
        emscripten_exit_with_live_runtime();
#else
        while (!should_stop_emulation)
        {
            if (did_emulator_core_exception_occur_atomic.load(std::memory_order_acquire))
            {
                std::rethrow_exception(emulator_core_exception_pointer);
            }

            handle_sdl_events(
                game_boy_emulator,
                emulation_controller,
                file_loading_status,
                menu_and_cursor_display_status,
                key_pressed_states,
                key_bindings,
                menu_properties,
                sdl_window.get(),
                &persistent_settings.loaded_game_rom_path,
                &persistent_settings.loaded_boot_rom_path,
                should_stop_emulation,
                error_message);

            render_frame(render_context);
        }

        game_boy_emulator.try_save_save_file(std::filesystem::path(SDL_GetBasePath()));

        const PersistentSettings settings_to_save =
            gather_current_settings(
                key_bindings,
                menu_properties,
                graphics_controller,
                persistent_settings.loaded_game_rom_path,
                persistent_settings.loaded_boot_rom_path);
        save_settings_to_file(settings_to_save);
        return 0;
#endif
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << ", exiting.\n";
        return 1;
    }
}
