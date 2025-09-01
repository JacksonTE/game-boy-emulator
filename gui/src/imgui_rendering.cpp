#include <backends/imgui_impl_sdl3.h>

#include "display_utilities.h"
#include "imgui_rendering.h"
#include "input_events.h"

void render_main_menu_bar(
    const uint8_t currently_published_frame_buffer_index,
    GameBoyEmulator::Emulator& game_boy_emulator,
    EmulationController& emulation_controller,
    FileLoadingStatus& file_loading_status,
    FullscreenDisplayStatus& fullscreen_display_status,
    GraphicsController& graphics_controller,
    MenuProperties& menu_properties,
    SDL_Window* sdl_window,
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
            if (ImGui::MenuItem("Load Game ROM", "[O]"))
            {
                try_load_file_to_memory_with_dialog(
                    GameBoyEmulator::FileType::GameROM,
                    game_boy_emulator,
                    emulation_controller,
                    file_loading_status,
                    sdl_window,
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
                    sdl_window,
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
            if (ImGui::MenuItem(is_fullscreen_enabled ? "Exit Fullscreen" : "Fullscreen", "[F11]"))
            {
                toggle_fullscreen_enabled_state(
                    fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden,
                    sdl_window);
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
                "[Space]",
                false,
                game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                toggle_fast_forward_enabled_state(
                    emulation_controller.is_fast_forward_enabled_atomic,
                    fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden);
            }
            ImGui::Spacing();
            if (ImGui::MenuItem(
                is_emulation_paused ? "Unpause" : "Pause",
                "[Esc]",
                false,
                game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe()))
            {
                toggle_emulation_paused_state(
                    emulation_controller.is_emulation_paused_atomic,
                    fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden);
            }
            imgui_spaced_separator();
            if (ImGui::MenuItem(
                "Reset",
                "[R]",
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
        fullscreen_display_status.is_main_menu_bar_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
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
        if (ImGui::Button("OK", ImVec2(160, 0)))
        {
            menu_properties.is_custom_palette_editor_open = false;
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
