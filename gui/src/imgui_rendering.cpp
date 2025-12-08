#include "display_utilities.h"
#include "imgui_rendering.h"
#include "input_events.h"

void render_main_menu_bar(
    const uint8_t currently_published_frame_buffer_index,
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    GraphicsController& graphics_controller,
    MenuProperties& menu_properties,
    KeyBindings& key_bindings,
    SDL_Window* sdl_window,
    std::string* loaded_game_rom_path,
    std::string* loaded_boot_rom_path,
    bool& should_stop_emulation,
    std::string& error_message)
{
    const bool is_fullscreen_enabled = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    const bool is_fast_forward_enabled = emulation_controller.is_fast_forward_enabled_atomic.load(std::memory_order_acquire);
    const bool is_emulation_paused = emulation_controller.is_emulation_paused_atomic.load(std::memory_order_acquire);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem(
                    "Load Game ROM",
                    get_keybind_label(key_bindings.load_game_rom).c_str()))
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
            ImGui::Spacing();
            if (ImGui::MenuItem("Load Boot ROM (Optional)"))
            {
                try_load_file_to_memory_with_dialog(
                    GameBoyEmulator::FileType::BootROM,
                    game_boy_emulator,
                    emulation_controller,
                    file_loading_status,
                    menu_and_cursor_display_status,
                    sdl_window,
                    loaded_boot_rom_path,
                    error_message);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem(
                    "Unload Game ROM",
                    "",
                    false,
                    game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                set_emulation_screen_blank(graphics_controller);
                SDL_SetWindowTitle(sdl_window, std::string("Emulate Game Boy").c_str());
                game_boy_emulator.unload_game_rom_from_memory_thread_safe();
                *loaded_game_rom_path = "";
                game_boy_emulator.reset_state();
                emulation_controller.is_fast_forward_enabled_atomic.store(false, std::memory_order_release);
                emulation_controller.is_emulation_paused_atomic.store(false, std::memory_order_release);
            }
            ImGui::Spacing();
            if (ImGui::MenuItem(
                    "Unload Boot ROM",
                    "",
                    false,
                    game_boy_emulator.is_boot_rom_loaded_in_memory_thread_safe()))
            {
                emulation_controller.is_emulation_paused_atomic.store(true, std::memory_order_release);
                game_boy_emulator.unload_boot_rom_from_memory_thread_safe();
                *loaded_boot_rom_path = "";

                if (game_boy_emulator.is_boot_rom_mapped_in_memory() &&
                    game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
                {
                    game_boy_emulator.reset_state();
                }
                emulation_controller.is_emulation_paused_atomic.store(is_emulation_paused);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem("Quit", "[Alt+F4]"))
            {
                should_stop_emulation = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Video"))
        {
            ImGui::SeparatorText("Colour Palette");
            if (ImGui::Combo(
                    "##Colour Palette",
                    &menu_properties.selected_colour_palette_combobox_index,
                    COLOUR_PALETTE_LABELS,
                    IM_ARRAYSIZE(COLOUR_PALETTE_LABELS)))
            {
                switch (menu_properties.selected_colour_palette_combobox_index)
                {
                    case 0:
                        graphics_controller.active_colour_palette = SAGE_COLOUR_PALETTE;
                        break;
                    case 1:
                        graphics_controller.active_colour_palette = GREYSCALE_COLOUR_PALETTE;
                        break;
                    case 2:
                        graphics_controller.active_colour_palette = CLASSIC_COLOUR_PALETTE;
                        break;
                    case 3:
                        graphics_controller.active_colour_palette = graphics_controller.custom_colour_palette;
                        break;
                }
                update_colour_palette(
                    game_boy_emulator,
                    graphics_controller,
                    currently_published_frame_buffer_index);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem("Update Custom Palette"))
            {
                menu_properties.is_custom_palette_editor_open = true;
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem(
                    is_fullscreen_enabled ? "Exit Fullscreen" : "Fullscreen",
                    get_keybind_label(key_bindings.fullscreen).c_str()))
            {
                toggle_fullscreen_enabled_state(
                    menu_and_cursor_display_status,
                    sdl_window);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Input"))
        {
            ImGui::Spacing();
            if (ImGui::MenuItem("Configure Keybinds"))
            {
                menu_properties.keybinds_editor_state.is_open = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Emulation"))
        {
            ImGui::SeparatorText("Fast-Foward Speed");
            if (ImGui::Combo(
                    "##Fast-Foward Speed",
                    &menu_properties.selected_fast_emulation_speed_index,
                    FAST_FORWARD_SPEED_LABELS,
                    IM_ARRAYSIZE(FAST_FORWARD_SPEED_LABELS)))
            {
                const double emulation_speed_multiplier = menu_properties.selected_fast_emulation_speed_index * 0.25 + 1.5;
                emulation_controller.target_fast_forward_multiplier_atomic.store(emulation_speed_multiplier, std::memory_order_release);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem(
                    is_fast_forward_enabled ? "Disable Fast-Forward" : "Enable Fast-Forward",
                    get_keybind_label(key_bindings.fast_forward).c_str(),
                    false,
                    game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                toggle_fast_forward_enabled_state(
                    emulation_controller.is_fast_forward_enabled_atomic,
                    menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden);
            }
            ImGui::Spacing();
            if (ImGui::MenuItem(
                    is_emulation_paused ? "Unpause" : "Pause",
                    get_keybind_label(key_bindings.pause).c_str(),
                    false,
                    game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                toggle_emulation_paused_state(
                    emulation_controller.is_emulation_paused_atomic,
                    menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem(
                    "Reset",
                    get_keybind_label(key_bindings.reset).c_str(),
                    false,
                    game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                game_boy_emulator.reset_state();
                emulation_controller.is_emulation_paused_atomic.store(false, std::memory_order_release);
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
        menu_and_cursor_display_status.is_main_menu_bar_hovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
            menu_and_cursor_display_status.cursor_changes_to_ignore_count == 0;
        ImGui::EndMainMenuBar();
    }
}

void render_custom_colour_palette_editor(
    const uint8_t currently_published_frame_buffer_index,
    GameBoyEmulator::Emulator& game_boy_emulator,
    MenuProperties& menu_properties,
    GraphicsController& graphics_controller)
{
    if (menu_properties.is_custom_palette_editor_open)
    {
        ImGui::OpenPopup("Custom Palette");
    }
    if (ImGui::BeginPopupModal("Custom Palette", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const float font_scale = ImGui::GetStyle().FontScaleMain;
        const float base_button_width = 160.0f;
        const float scaled_button_width = base_button_width * font_scale;
        ImGui::SetNextWindowSizeConstraints(ImVec2(scaled_button_width + ImGui::GetStyle().WindowPadding.x * 2.0f, 0), ImVec2(FLT_MAX, FLT_MAX));

        for (int i = 0; i < 4; i++)
        {
            menu_properties.selected_custom_colour_palette_colours[i] =
                get_imvec4_from_abgr(graphics_controller.custom_colour_palette[i]);

            std::string colour_label = std::string("Colour ") + std::to_string(i);
            if (ImGui::ColorEdit4(
                    colour_label.c_str(),
                    reinterpret_cast<float*>(&menu_properties.selected_custom_colour_palette_colours[i]),
                    ImGuiColorEditFlags_NoInputs))
            {
                update_colour_palette(
                    game_boy_emulator,
                    graphics_controller,
                    currently_published_frame_buffer_index);
            }
            const ImVec4& new_colour = menu_properties.selected_custom_colour_palette_colours[i];
            const uint8_t new_alpha = static_cast<uint8_t>(new_colour.w * 255.0f + 0.5f);
            const uint8_t new_blue = static_cast<uint8_t>(new_colour.z * 255.0f + 0.5f);
            const uint8_t new_green = static_cast<uint8_t>(new_colour.y * 255.0f + 0.5f);
            const uint8_t new_red = static_cast<uint8_t>(new_colour.x * 255.0f + 0.5f);
            graphics_controller.custom_colour_palette[i] = get_abgr_value_for_current_endianness(
                new_alpha,
                new_blue,
                new_green,
                new_red);
        }
        if (ImGui::Button("Close", ImVec2(scaled_button_width, 0)))
        {
            menu_properties.is_custom_palette_editor_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void render_keybinds_editor(
    MenuProperties& menu_properties,
    KeyBindings& key_bindings)
{
    constexpr float BASE_COLUMN_WIDTH = 200.0f;
    const float font_scale = ImGui::GetStyle().FontScaleMain;
    const float scaled_emulator_controls_column_width = BASE_COLUMN_WIDTH * font_scale;
    const float scaled_game_boy_controls_column_width = scaled_emulator_controls_column_width * 0.8f;

    if (menu_properties.keybinds_editor_state.is_open)
    {
        ImGui::OpenPopup("Configure Keybinds");
        const float total_width = scaled_game_boy_controls_column_width + scaled_emulator_controls_column_width +
                                  ImGui::GetStyle().WindowPadding.x * 2.0f +
                                  ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(total_width, 0), ImVec2(total_width, FLT_MAX));
    }

    if (ImGui::BeginPopupModal("Configure Keybinds", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Click a button to rebind, then press a key");
        imgui_spaced_separator();

        if (ImGui::BeginTable(
                "KeybindsTable",
                2,
                ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Game Boy Controls", ImGuiTableColumnFlags_WidthFixed, scaled_game_boy_controls_column_width);
            ImGui::TableSetupColumn("Emulator Controls", ImGuiTableColumnFlags_WidthFixed, scaled_emulator_controls_column_width);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("Game Boy Controls");
            imgui_spaced_separator();
            ImGui::TableNextColumn();
            ImGui::Text("Emulator Controls");
            imgui_spaced_separator();
            ImGui::TableNextRow();

            const char* gameboy_labels[] =
            {
                "Up",
                "Down",
                "Left",
                "Right",
                "A",
                "B",
                "Start",
                "Select"
            };
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
            const float gameboy_label_width = 57.5f * font_scale;
            const float emulator_label_width = 130.0f * font_scale;

            ImVec2 original_frame_padding = ImGui::GetStyle().FramePadding;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(original_frame_padding.x, original_frame_padding.y * 0.5f));
            ImGui::TableNextColumn();

            for (int i = 0; i < 8; i++)
            {
                ImGui::Text("%s:", gameboy_labels[i]);
                ImGui::SameLine(gameboy_label_width);

                const std::string button_id = std::string("##gb_") + std::to_string(i);

                const char* key_name = (*gameboy_keys[i] == SDLK_UNKNOWN)
                    ? "[Unbound]"
                    : SDL_GetKeyName(*gameboy_keys[i]);

                if (menu_properties.keybinds_editor_state.is_waiting_for_key && 
                    menu_properties.keybinds_editor_state.selected_control_type == KeybindsEditorState::ControlType::GameBoy &&
                    menu_properties.keybinds_editor_state.editing_index == i)
                {
                    ImGui::Button("Press a key...", ImVec2(-FLT_MIN, 0));
                }
                else if (ImGui::Button((std::string(key_name) + button_id).c_str(), ImVec2(-FLT_MIN, 0)))
                {
                    menu_properties.keybinds_editor_state.is_waiting_for_key = true;
                    menu_properties.keybinds_editor_state.editing_index = i;
                    menu_properties.keybinds_editor_state.selected_control_type = KeybindsEditorState::ControlType::GameBoy;
                }
            }
            ImGui::TableNextColumn();

            const char* emulator_labels[] = 
            {
                "Load Game ROM",
                "Fast-Forward",
                "Pause", "Reset",
                "Fullscreen"
            };
            SDL_Keycode* emulator_keys[] =
            {
                &key_bindings.load_game_rom,
                &key_bindings.fast_forward,
                &key_bindings.pause,
                &key_bindings.reset,
                &key_bindings.fullscreen
            };

            for (int i = 0; i < 5; i++)
            {
                ImGui::Text("%s:", emulator_labels[i]);
                ImGui::SameLine(emulator_label_width);

                std::string button_id = std::string("##emu_") + std::to_string(i);

                const char* key_name = (*emulator_keys[i] == SDLK_UNKNOWN)
                    ? "[Unbound]"
                    : SDL_GetKeyName(*emulator_keys[i]);

                if (menu_properties.keybinds_editor_state.is_waiting_for_key && 
                    menu_properties.keybinds_editor_state.selected_control_type == KeybindsEditorState::ControlType::Emulation &&
                    menu_properties.keybinds_editor_state.editing_index == i)
                {
                    ImGui::Button("Press a key...", ImVec2(-FLT_MIN, 0));
                }
                else if (ImGui::Button((std::string(key_name) + button_id).c_str(), ImVec2(-FLT_MIN, 0)))
                {
                    menu_properties.keybinds_editor_state.is_waiting_for_key = true;
                    menu_properties.keybinds_editor_state.editing_index = i;
                    menu_properties.keybinds_editor_state.selected_control_type = KeybindsEditorState::ControlType::Emulation;
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndTable();
        }
        imgui_spaced_separator();
        const float available_width = ImGui::GetContentRegionAvail().x;
        const float button_width = (available_width - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (ImGui::Button("Reset to Defaults", ImVec2(button_width, 0)))
        {
            menu_properties.keybinds_editor_state.is_waiting_for_key = false;
            key_bindings.reset_to_defaults();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(button_width, 0)))
        {
            menu_properties.keybinds_editor_state.is_open = false;
            menu_properties.keybinds_editor_state.is_waiting_for_key = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void render_error_message_popup(
    FileLoadingStatus& file_loading_status,
    std::atomic<bool>& is_emulation_paused_atomic,
    std::string& error_message)
{
    if (file_loading_status.did_rom_loading_error_occur)
    {
        is_emulation_paused_atomic.store(true, std::memory_order_release);
        const float maximum_error_popup_width = ImGui::GetIO().DisplaySize.x * 0.4f;
        const float error_message_width = ImGui::CalcTextSize(error_message.c_str()).x;
        const float minimum_error_popup_width =
            std::min(maximum_error_popup_width, error_message_width + ImGui::GetStyle().WindowPadding.x * 2.0f);

        ImGui::SetNextWindowSizeConstraints(ImVec2(minimum_error_popup_width, 0), ImVec2(maximum_error_popup_width, FLT_MAX));
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f));

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
            is_emulation_paused_atomic.store(file_loading_status.is_emulation_paused_before_rom_loading, std::memory_order_release);
            ImGui::CloseCurrentPopup();
            file_loading_status.did_rom_loading_error_occur = false;
            error_message = "";
        }
        ImGui::EndPopup();
    }
}

std::string get_keybind_label(SDL_Keycode key)
{
    if (key == SDLK_UNKNOWN)
    {
        return "";
    }
    return std::string("[") + SDL_GetKeyName(key) + "]";
}

ImVec4 get_imvec4_from_abgr(uint32_t abgr)
{
    const uint8_t alpha = (abgr >> 24) & 0xFF;
    const uint8_t blue = (abgr >> 16) & 0xFF;
    const uint8_t green = (abgr >> 8) & 0xFF;
    const uint8_t red = abgr & 0xFF;
    return ImVec4(red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f);
}

void imgui_spaced_separator()
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}
