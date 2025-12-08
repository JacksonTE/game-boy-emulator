#include "persistent_settings.h"
#include "display_utilities.h"

#include <iostream>
#include <sstream>

template<typename T>
static void write_key_value_pair(std::ofstream& file, const std::string& key, const T& value)
{
    file << key << "=" << value << "\n";
}

bool save_settings_to_file(const PersistentSettings& settings)
{
    try
    {
        const auto settings_path = std::filesystem::path(SDL_GetBasePath()) / "settings.cfg";
        std::ofstream file(settings_path);
        if (!file.is_open())
        {
            return false;
        }

        file << "[KeyBindings]\n";
        write_key_value_pair(file, "button_up",     static_cast<int>(settings.key_bindings.button_up));
        write_key_value_pair(file, "button_down",   static_cast<int>(settings.key_bindings.button_down));
        write_key_value_pair(file, "button_left",   static_cast<int>(settings.key_bindings.button_left));
        write_key_value_pair(file, "button_right",  static_cast<int>(settings.key_bindings.button_right));
        write_key_value_pair(file, "button_a",      static_cast<int>(settings.key_bindings.button_a));
        write_key_value_pair(file, "button_b",      static_cast<int>(settings.key_bindings.button_b));
        write_key_value_pair(file, "button_start",  static_cast<int>(settings.key_bindings.button_start));
        write_key_value_pair(file, "button_select", static_cast<int>(settings.key_bindings.button_select));
        write_key_value_pair(file, "load_game_rom", static_cast<int>(settings.key_bindings.load_game_rom));
        write_key_value_pair(file, "fast_forward",  static_cast<int>(settings.key_bindings.fast_forward));
        write_key_value_pair(file, "pause",         static_cast<int>(settings.key_bindings.pause));
        write_key_value_pair(file, "reset",         static_cast<int>(settings.key_bindings.reset));
        write_key_value_pair(file, "fullscreen",    static_cast<int>(settings.key_bindings.fullscreen));

        file << "\n[Video]\n";
        write_key_value_pair(file, "selected_colour_palette_index", settings.selected_colour_palette_index);
        write_key_value_pair(file, "custom_colour_palette_0",         settings.custom_colour_palette[0]);
        write_key_value_pair(file, "custom_colour_palette_1",         settings.custom_colour_palette[1]);
        write_key_value_pair(file, "custom_colour_palette_2",         settings.custom_colour_palette[2]);
        write_key_value_pair(file, "custom_colour_palette_3",         settings.custom_colour_palette[3]);

        file << "\n[Emulation]\n";
        write_key_value_pair(file, "selected_fast_forward_speed_index", settings.selected_fast_forward_speed_index);

        file << "\n[LoadedFiles]\n";
        write_key_value_pair(file, "loaded_game_rom_path", settings.loaded_game_rom_path);
        write_key_value_pair(file, "loaded_boot_rom_path", settings.loaded_boot_rom_path);

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error saving settings: " << e.what() << "\n";
        return false;
    }
}

static std::string trim(const std::string& str)
{
    const auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        return "";
    }
    const auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool load_settings_from_file(PersistentSettings& settings)
{
    try
    {
        const auto settings_path = std::filesystem::path(SDL_GetBasePath()) / "settings.cfg";
        std::ifstream file(settings_path);
        if (!file.is_open())
        {
            return false;
        }

        std::string line;
        std::string current_section;

        while (std::getline(file, line))
        {
            line = trim(line);
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            if (line[0] == '[' && line.back() == ']')
            {
                current_section = line.substr(1, line.size() - 2);
                continue;
            }

            const auto equals_pos = line.find('=');
            if (equals_pos == std::string::npos)
            {
                continue;
            }

            const std::string key = trim(line.substr(0, equals_pos));
            const std::string value = trim(line.substr(equals_pos + 1));

            if (current_section == "KeyBindings")
            {
                const int keycode = std::stoi(value);
                if (key == "button_up")
                {
                    settings.key_bindings.button_up = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "button_down")
                {
                    settings.key_bindings.button_down = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "button_left")
                {
                    settings.key_bindings.button_left = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "button_right")
                {
                    settings.key_bindings.button_right = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "button_a")
                {
                    settings.key_bindings.button_a = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "button_b")
                {
                    settings.key_bindings.button_b = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "button_start")
                {
                    settings.key_bindings.button_start = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "button_select")
                {
                    settings.key_bindings.button_select = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "load_game_rom")
                {
                    settings.key_bindings.load_game_rom = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "fast_forward")
                {
                    settings.key_bindings.fast_forward = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "pause")
                {
                    settings.key_bindings.pause = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "reset")
                {
                    settings.key_bindings.reset = static_cast<SDL_Keycode>(keycode);
                }
                else if (key == "fullscreen")
                {
                    settings.key_bindings.fullscreen = static_cast<SDL_Keycode>(keycode);
                }
            }
            else if (current_section == "Video")
            {
                if (key == "selected_colour_palette_index")
                {
                    settings.selected_colour_palette_index = std::stoi(value);
                }
                else if (key == "custom_colour_palette_0")
                {
                    settings.custom_colour_palette[0] = static_cast<uint32_t>(std::stoul(value));
                }
                else if (key == "custom_colour_palette_1")
                {
                    settings.custom_colour_palette[1] = static_cast<uint32_t>(std::stoul(value));
                }
                else if (key == "custom_colour_palette_2")
                {
                    settings.custom_colour_palette[2] = static_cast<uint32_t>(std::stoul(value));
                }
                else if (key == "custom_colour_palette_3")
                {
                    settings.custom_colour_palette[3] = static_cast<uint32_t>(std::stoul(value));
                }
            }
            else if (current_section == "Emulation")
            {
                if (key == "selected_fast_forward_speed_index")
                {
                    settings.selected_fast_forward_speed_index = std::stoi(value);
                }
            }
            else if (current_section == "LoadedFiles")
            {
                if (key == "loaded_game_rom_path")
                {
                    settings.loaded_game_rom_path = value;
                }
                else if (key == "loaded_boot_rom_path")
                {
                    settings.loaded_boot_rom_path = value;
                }
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error loading settings: " << e.what() << "\n";
        return false;
    }
}

void apply_loaded_settings(
    const PersistentSettings& settings,
    KeyBindings& key_bindings,
    MenuProperties& menu_properties,
    GraphicsController& graphics_controller,
    EmulationController& emulation_controller)
{
    key_bindings = settings.key_bindings;

    menu_properties.selected_colour_palette_combobox_index = settings.selected_colour_palette_index;
    menu_properties.selected_fast_emulation_speed_index = settings.selected_fast_forward_speed_index;

    for (int i = 0; i < 4; i++)
    {
        graphics_controller.custom_colour_palette[i] = settings.custom_colour_palette[i];
    }

    switch (settings.selected_colour_palette_index)
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
    const double emulation_speed_multiplier 
        = settings.selected_fast_forward_speed_index * 0.25 + 1.5;

    emulation_controller.target_fast_forward_multiplier_atomic
        .store(emulation_speed_multiplier, std::memory_order_release);
}

PersistentSettings gather_current_settings(
    const KeyBindings& key_bindings,
    const MenuProperties& menu_properties,
    const GraphicsController& graphics_controller,
    const std::string& game_rom_path,
    const std::string& boot_rom_path)
{
    PersistentSettings settings{};
    settings.key_bindings = key_bindings;
    settings.selected_colour_palette_index = menu_properties.selected_colour_palette_combobox_index;
    settings.selected_fast_forward_speed_index = menu_properties.selected_fast_emulation_speed_index;

    for (int i = 0; i < 4; i++)
    {
        settings.custom_colour_palette[i] = graphics_controller.custom_colour_palette[i];
    }
    settings.loaded_game_rom_path = game_rom_path;
    settings.loaded_boot_rom_path = boot_rom_path;
    return settings;
}
