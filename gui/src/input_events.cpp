#include "display_utilities.h"
#include "input_events.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include "nfd_sdl3.h"
#endif

#ifdef __EMSCRIPTEN__
namespace
{

enum class WebFileSelectionResult
{
    None,
    Waiting,
    Ready,
    Cancelled,
    Error
};

struct WebPendingFileSelection
{
    std::atomic<WebFileSelectionResult> result{WebFileSelectionResult::None};
    std::atomic<GameBoyEmulator::FileType> file_type{GameBoyEmulator::FileType::GameROM};
};

WebPendingFileSelection web_pending_file_selection{};

constexpr const char* WEB_SELECTED_GAME_ROM_PATH = "/tmp/web-selected-game-rom.gb";
constexpr const char* WEB_SELECTED_BOOT_ROM_PATH = "/tmp/web-selected-boot-rom.bin";

const char* get_web_selected_rom_path(const GameBoyEmulator::FileType file_type)
{
    return file_type == GameBoyEmulator::FileType::BootROM
        ? WEB_SELECTED_BOOT_ROM_PATH
        : WEB_SELECTED_GAME_ROM_PATH;
}

} // namespace

extern "C"
{

EMSCRIPTEN_KEEPALIVE void set_pending_web_file_selection_result(const int file_type, const int result)
{
    web_pending_file_selection.file_type.store(static_cast<GameBoyEmulator::FileType>(file_type), std::memory_order_release);
    web_pending_file_selection.result.store(static_cast<WebFileSelectionResult>(result), std::memory_order_release);
}

}

#endif

static void finish_failed_rom_loading_operation(
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    const std::string& error_message)
{
    file_loading_status.did_rom_loading_error_occur = !error_message.empty();
    emulation_controller.is_emulation_paused_atomic.store(
        file_loading_status.is_emulation_paused_before_rom_loading,
        std::memory_order_release);
}

static bool try_load_file_to_memory_from_path(
    const std::filesystem::path& file_path,
    GameBoyEmulator::FileType file_type,
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    SDL_Window* sdl_window,
    std::string* loaded_rom_path,
    std::string& error_message)
{
    bool is_operation_successful = false;

    if (file_type == GameBoyEmulator::FileType::GameROM)
    {
        game_boy_emulator.try_save_save_file(std::filesystem::path(SDL_GetBasePath()));
    }

    if (game_boy_emulator.try_to_load_file_to_memory(file_path, file_type, error_message))
    {
        if (file_type == GameBoyEmulator::FileType::GameROM)
        {
            game_boy_emulator.reset_state();
            game_boy_emulator.try_load_save_file(std::filesystem::path(SDL_GetBasePath()));
        }
        *loaded_rom_path = file_path.string();
        is_operation_successful = true;
    }

    if (is_operation_successful)
    {
        file_loading_status.did_rom_loading_error_occur = false;
        if (file_type == GameBoyEmulator::FileType::GameROM)
        {
            SDL_SetWindowTitle(
                sdl_window,
                std::string("Emulate Game Boy - " + game_boy_emulator.get_loaded_game_rom_title_thread_safe()).c_str());
        }
        emulation_controller.is_emulation_paused_atomic.store(false, std::memory_order_release);
        menu_and_cursor_display_status.cursor_changes_to_ignore_count =
            menu_and_cursor_display_status.MAX_CURSOR_CHANGES_TO_IGNORE;
        menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden = 0.0f;
    }
    else
    {
        finish_failed_rom_loading_operation(emulation_controller, file_loading_status, error_message);
    }

    return is_operation_successful;
}

bool try_load_file_to_memory_with_dialog(
    GameBoyEmulator::FileType file_type,
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    SDL_Window* sdl_window,
    std::string* loaded_rom_path,
    std::string& error_message)
{
    error_message.clear();
    file_loading_status.did_rom_loading_error_occur = false;
    file_loading_status.is_emulation_paused_before_rom_loading =
        emulation_controller.is_emulation_paused_atomic.exchange(true, std::memory_order_acq_rel);

#ifdef __EMSCRIPTEN__
    web_pending_file_selection.file_type.store(file_type, std::memory_order_release);
    web_pending_file_selection.result.store(WebFileSelectionResult::Waiting, std::memory_order_release);

    const int did_open_picker = EM_ASM_INT(
        {
            if (Module.openRomFilePicker)
            {
                Module.openRomFilePicker($0);
                return 1;
            }
            return 0;
        },
        static_cast<int>(file_type));

    if (!did_open_picker)
    {
        web_pending_file_selection.result.store(WebFileSelectionResult::None, std::memory_order_release);
        error_message = "Web file picker is unavailable.";
        finish_failed_rom_loading_operation(emulation_controller, file_loading_status, error_message);
    }

    return false;
#else
    nfdopendialogu8args_t open_dialog_arguments{};
    nfdu8filteritem_t filters[] =
    {
        {file_type == GameBoyEmulator::FileType::BootROM ? "Game Boy Boot ROMs" : "Game Boy ROMs", "gb,gbc,bin,rom"}
    };
    open_dialog_arguments.filterList = filters;
    open_dialog_arguments.filterCount = 1;

    nfdchar_t* rom_path = nullptr;
    NFD_GetNativeWindowFromSDLWindow(sdl_window, &open_dialog_arguments.parentWindow);

    nfdresult_t result = NFD_OpenDialogU8_With(&rom_path, &open_dialog_arguments);
    bool is_operation_successful = false;

    if (result == NFD_OKAY)
    {
        is_operation_successful = try_load_file_to_memory_from_path(
            rom_path,
            file_type,
            game_boy_emulator,
            emulation_controller,
            file_loading_status,
            menu_and_cursor_display_status,
            sdl_window,
            loaded_rom_path,
            error_message);
        NFD_FreePathU8(rom_path);
    }
    else if (result == NFD_ERROR)
    {
        std::cerr << "NFD error: " << NFD_GetError() << "\n";
        error_message = NFD_GetError();
    }

    if (result != NFD_OKAY)
    {
        finish_failed_rom_loading_operation(emulation_controller, file_loading_status, error_message);
    }
    return is_operation_successful;
#endif
}

#ifdef __EMSCRIPTEN__
void consume_pending_web_file_selection(
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    SDL_Window* sdl_window,
    std::string* loaded_game_rom_path,
    std::string* loaded_boot_rom_path,
    std::string& error_message)
{
    const WebFileSelectionResult result =
        web_pending_file_selection.result.exchange(WebFileSelectionResult::None, std::memory_order_acq_rel);

    if (result == WebFileSelectionResult::None || result == WebFileSelectionResult::Waiting)
    {
        if (result == WebFileSelectionResult::Waiting)
        {
            web_pending_file_selection.result.store(WebFileSelectionResult::Waiting, std::memory_order_release);
        }
        return;
    }

    const GameBoyEmulator::FileType file_type = web_pending_file_selection.file_type.load(std::memory_order_acquire);

    if (result == WebFileSelectionResult::Cancelled)
    {
        emulation_controller.is_emulation_paused_atomic.store(
            file_loading_status.is_emulation_paused_before_rom_loading,
            std::memory_order_release);
        return;
    }

    if (result == WebFileSelectionResult::Error)
    {
        error_message = "Failed to read the selected file in the browser.";
        finish_failed_rom_loading_operation(emulation_controller, file_loading_status, error_message);
        return;
    }

    try_load_file_to_memory_from_path(
        get_web_selected_rom_path(file_type),
        file_type,
        game_boy_emulator,
        emulation_controller,
        file_loading_status,
        menu_and_cursor_display_status,
        sdl_window,
        file_type == GameBoyEmulator::FileType::BootROM ? loaded_boot_rom_path : loaded_game_rom_path,
        error_message);
}
#endif

void toggle_emulation_paused_state(
    std::atomic<bool>& is_emulation_paused_atomic,
    float& seconds_until_main_menu_bar_and_cursor_hidden)
{
    const bool was_emulation_paused = is_emulation_paused_atomic.load(std::memory_order_acquire);
    is_emulation_paused_atomic.store(!was_emulation_paused, std::memory_order_release);
    seconds_until_main_menu_bar_and_cursor_hidden = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
}

void toggle_fast_forward_enabled_state(
    std::atomic<bool>& is_fast_forward_enabled_atomic,
    float& seconds_until_main_menu_bar_and_cursor_hidden)
{
    const bool was_fast_forward_enabled = is_fast_forward_enabled_atomic.load(std::memory_order_acquire);
    is_fast_forward_enabled_atomic.store(!was_fast_forward_enabled, std::memory_order_release);
    seconds_until_main_menu_bar_and_cursor_hidden = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
}

void toggle_fullscreen_enabled_state(
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    SDL_Window* sdl_window)
{
#ifndef __EMSCRIPTEN__
    float mouse_x_monitor_coordinate, mouse_y_monitor_coordinate;
    SDL_GetGlobalMouseState(&mouse_x_monitor_coordinate, &mouse_y_monitor_coordinate);
#endif

    const bool was_fullscreen_enabled = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(sdl_window, !was_fullscreen_enabled);

#ifndef __EMSCRIPTEN__
    SDL_WarpMouseGlobal(mouse_x_monitor_coordinate, mouse_y_monitor_coordinate);
#endif

    menu_and_cursor_display_status.cursor_changes_to_ignore_count =
        menu_and_cursor_display_status.MAX_CURSOR_CHANGES_TO_IGNORE;
}

void clear_duplicate_keybind(
    KeyBindings& key_bindings,
    SDL_Keycode new_key,
    SDL_Keycode* current_binding)
{
    SDL_Keycode* all_keybinds[] =
    {
        &key_bindings.button_up,
        &key_bindings.button_down,
        &key_bindings.button_left,
        &key_bindings.button_right,
        &key_bindings.button_a,
        &key_bindings.button_b,
        &key_bindings.button_start,
        &key_bindings.button_select,
        &key_bindings.load_game_rom,
        &key_bindings.fast_forward,
        &key_bindings.pause,
        &key_bindings.reset,
        &key_bindings.fullscreen
    };

    for (SDL_Keycode* keybind : all_keybinds)
    {
        if (keybind == current_binding)
        {
            continue;
        }

        if (*keybind == new_key)
        {
            *keybind = SDLK_UNKNOWN;
        }
    }
}

void handle_sdl_events(
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    KeyPressedStates& key_pressed_states,
    KeyBindings& key_bindings,
    MenuProperties& menu_properties,
    SDL_Window* sdl_window,
    std::string* loaded_game_rom_path,
    std::string* loaded_boot_rom_path,
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
                const SDL_Keycode key = sdl_event.key.key;

                if (key == SDLK_UNKNOWN)
                {
                    break;
                }
                const bool is_key_pressed = (sdl_event.type == SDL_EVENT_KEY_DOWN);

                if (menu_properties.keybinds_editor_state.is_waiting_for_key && is_key_pressed)
                {
#ifdef __EMSCRIPTEN__
                    if (key == SDLK_ESCAPE || key == SDLK_F11)
                    {
                        break;
                    }
#endif
                    SDL_Keycode* gameboy_keys[] =
                    {
                        &key_bindings.button_up,
                        &key_bindings.button_down,
                        &key_bindings.button_left,
                        &key_bindings.button_right,
                        &key_bindings.button_a,
                        &key_bindings.button_b,
                        &key_bindings.button_start,
                        &key_bindings.button_select
                    };
                    SDL_Keycode* emulator_keys[] =
                    {
                        &key_bindings.load_game_rom,
                        &key_bindings.fast_forward,
                        &key_bindings.pause,
                        &key_bindings.reset,
                        &key_bindings.fullscreen
                    };
                    SDL_Keycode* target_binding =
                        (menu_properties.keybinds_editor_state.selected_control_type == KeybindsEditorState::ControlType::GameBoy)
                        ? gameboy_keys[menu_properties.keybinds_editor_state.editing_index]
                        : emulator_keys[menu_properties.keybinds_editor_state.editing_index];

                    clear_duplicate_keybind(key_bindings, key, target_binding);
                    *target_binding = key;

                    menu_properties.keybinds_editor_state.is_waiting_for_key = false;
                    menu_properties.keybinds_editor_state.editing_index = -1;
                    break;
                }

                if (key == key_bindings.fullscreen)
                {
                    if (is_key_pressed && !key_pressed_states.was_fullscreen_key_previously_pressed)
                    {
                        toggle_fullscreen_enabled_state(menu_and_cursor_display_status, sdl_window);
                    }
                    key_pressed_states.was_fullscreen_key_previously_pressed = is_key_pressed;
                }
                else if (key == key_bindings.load_game_rom && is_key_pressed)
                {
                    try_load_file_to_memory_with_dialog(
                        GameBoyEmulator::FileType::GameROM,
                        game_boy_emulator,
                        emulation_controller,
                        file_loading_status,
                        menu_and_cursor_display_status,
                        sdl_window,
                        loaded_game_rom_path,
                        error_message);
                }

                if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                {
                    if (key == key_bindings.fast_forward)
                    {
                        if (is_key_pressed && !key_pressed_states.was_fast_forward_key_previously_pressed)
                        {
                            toggle_fast_forward_enabled_state(
                                emulation_controller.is_fast_forward_enabled_atomic,
                                menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden);
                        }
                        key_pressed_states.was_fast_forward_key_previously_pressed = is_key_pressed;
                    }
                    else if (key == key_bindings.pause)
                    {
                        if (is_key_pressed && !key_pressed_states.was_pause_key_previously_pressed)
                        {
                            toggle_emulation_paused_state(
                                emulation_controller.is_emulation_paused_atomic,
                                menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden);
                        }
                        key_pressed_states.was_pause_key_previously_pressed = is_key_pressed;
                    }
                    else if (key == key_bindings.reset)
                    {
                        if (is_key_pressed && !key_pressed_states.was_reset_key_previously_pressed)
                        {
                            emulation_controller.is_emulation_paused_atomic.store(true, std::memory_order_release);
                            game_boy_emulator.reset_state();
                            emulation_controller.is_emulation_paused_atomic.store(false, std::memory_order_release);
                        }
                        key_pressed_states.was_reset_key_previously_pressed = is_key_pressed;
                    }
                    else if (key == key_bindings.button_up)
                    {
                        game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                            GameBoyEmulator::UP_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                    }
                    else if (key == key_bindings.button_left)
                    {
                        game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                            GameBoyEmulator::LEFT_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                    }
                    else if (key == key_bindings.button_down)
                    {
                        game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                            GameBoyEmulator::DOWN_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                    }
                    else if (key == key_bindings.button_right)
                    {
                        game_boy_emulator.update_dpad_direction_pressed_state_thread_safe(
                            GameBoyEmulator::RIGHT_DPAD_DIRECTION_FLAG_MASK, is_key_pressed);
                    }
                    else if (key == key_bindings.button_a)
                    {
                        game_boy_emulator.update_button_pressed_state_thread_safe(
                            GameBoyEmulator::A_BUTTON_FLAG_MASK, is_key_pressed);
                    }
                    else if (key == key_bindings.button_b)
                    {
                        game_boy_emulator.update_button_pressed_state_thread_safe(
                            GameBoyEmulator::B_BUTTON_FLAG_MASK, is_key_pressed);
                    }
                    else if (key == key_bindings.button_start)
                    {
                        game_boy_emulator.update_button_pressed_state_thread_safe(
                            GameBoyEmulator::START_BUTTON_FLAG_MASK, is_key_pressed);
                    }
                    else if (key == key_bindings.button_select)
                    {
                        game_boy_emulator.update_button_pressed_state_thread_safe(
                            GameBoyEmulator::SELECT_BUTTON_FLAG_MASK, is_key_pressed);
                    }
                }
            }
            break;
        }
    }
}
