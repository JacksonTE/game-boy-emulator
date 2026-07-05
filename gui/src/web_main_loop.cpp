#include "web_main_loop.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>

#include "display_utilities.h"
#include "input_events.h"

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

void start_emscripten_main_loop(EmscriptenLoopState& loop_state)
{
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &loop_state, false, emscripten_resize_callback);
    emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, &loop_state, false, emscripten_fullscreen_change_callback);
    emscripten_set_main_loop_arg(emscripten_main_loop_iteration, &loop_state, 0, false);
}

#endif
