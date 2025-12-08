#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "gui_state_types.h"

struct PersistentSettings
{
    KeyBindings key_bindings{};

    int selected_colour_palette_index{};
    uint32_t custom_colour_palette[4]{};

    int selected_fast_forward_speed_index{};

    std::string loaded_game_rom_path{};
    std::string loaded_boot_rom_path{};
};

bool save_settings_to_file(const PersistentSettings& settings);

bool load_settings_from_file(PersistentSettings& settings);

void apply_loaded_settings(
    const PersistentSettings& settings,
    KeyBindings& key_bindings,
    MenuProperties& menu_properties,
    GraphicsController& graphics_controller,
    EmulationController& emulation_controller);

PersistentSettings gather_current_settings(
    const KeyBindings& key_bindings,
    const MenuProperties& menu_properties,
    const GraphicsController& graphics_controller,
    const std::string& game_rom_path,
    const std::string& boot_rom_path);