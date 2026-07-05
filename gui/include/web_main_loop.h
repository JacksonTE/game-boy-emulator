#pragma once

#include <atomic>
#include <exception>
#include <string>
#include <SDL3/SDL.h>

#include "emulator.h"
#include "gui_state_types.h"
#include "persistent_settings.h"

struct WebThreadPacingState
{
    std::atomic<uint64_t> target_published_frame_sequence_number_atomic{};
};

#ifdef __EMSCRIPTEN__

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

void sync_emscripten_canvas_to_viewport(SDL_Window* sdl_window, int viewport_w, int viewport_h);

void start_emscripten_main_loop(EmscriptenLoopState& loop_state);

#endif
