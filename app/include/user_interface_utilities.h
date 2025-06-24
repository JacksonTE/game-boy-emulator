#pragma once

#include <atomic>
#include <bit>
#include <cstdint>
#include <nfd.h>
#include <SDL3/SDL.h>
#include <string>

#include "emulator.h"
#include "nfd_sdl3.h"

constexpr int INITIAL_WINDOW_SCALE = 5;
constexpr uint8_t DISPLAY_WIDTH_PIXELS = 160;
constexpr uint8_t DISPLAY_HEIGHT_PIXELS = 144;

static bool try_load_file_to_memory_with_dialog(
    bool is_bootrom_file,
    SDL_Window *sdl_window,
    GameBoyCore::Emulator &game_boy_emulator,
    std::atomic<bool> &is_emulation_paused,
    bool &pre_rom_loading_pause_state,
    bool &did_rom_loading_error_occur,
    std::string &error_message)
{
    const bool previous_pause_state = is_emulation_paused.load(std::memory_order_acquire);
    is_emulation_paused.store(true, std::memory_order_release);

    nfdopendialogu8args_t open_dialog_arguments{};
    nfdu8filteritem_t filters[] =
    {
        {is_bootrom_file ? "Game Boy Boot ROMs" : "Game Boy ROMs", "gb,gbc,bin,rom"}
    };
    open_dialog_arguments.filterList = filters;
    open_dialog_arguments.filterCount = 1;

    nfdchar_t *rom_path = nullptr;
    NFD_GetNativeWindowFromSDLWindow(sdl_window, &open_dialog_arguments.parentWindow);

    nfdresult_t result = NFD_OpenDialogU8_With(&rom_path, &open_dialog_arguments);
    bool is_operation_successful = false;

    if (result == NFD_OKAY)
    {
        if (game_boy_emulator.try_load_file_to_memory(rom_path, is_bootrom_file, error_message))
        {
            if (game_boy_emulator.is_bootrom_loaded_in_memory_thread_safe())
            {
                game_boy_emulator.reset_state(true);
            }
            else
            {
                game_boy_emulator.set_post_boot_state();
            }
            is_operation_successful = true;
        }
        NFD_FreePathU8(rom_path);
    }
    else if (result == NFD_ERROR)
    {
        std::cerr << "NFD error: " << NFD_GetError() << "\n";
        error_message = NFD_GetError();
    }

    if (is_operation_successful)
    {
        if (!is_bootrom_file)
        {
            SDL_SetWindowTitle(sdl_window, std::string("Emulate Game Boy - " + game_boy_emulator.get_loaded_game_rom_title()).c_str());
        }
        is_emulation_paused.store(false, std::memory_order_release);
    }
    else
    {
        if (error_message != "")
        {
            pre_rom_loading_pause_state = previous_pause_state;
            did_rom_loading_error_occur = true;
        }
        is_emulation_paused.store(previous_pause_state, std::memory_order_release);
    }
    return is_operation_successful;
}

static void set_emulation_screen_blank(const uint32_t *active_colour_palette, uint32_t *abgr_pixel_buffer, SDL_Texture *sdl_texture)
{
    for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
    {
        abgr_pixel_buffer[i] = active_colour_palette[0];
    }
    SDL_UpdateTexture(sdl_texture, nullptr, abgr_pixel_buffer, DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
}

static constexpr uint32_t get_abgr_value_for_current_endianness(uint8_t alpha, uint8_t blue, uint8_t green, uint8_t red)
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return (static_cast<uint32_t>(red)) |
               (static_cast<uint32_t>(green) << 8) |
               (static_cast<uint32_t>(blue) << 16) |
               (static_cast<uint32_t>(alpha) << 24);
    }
    else
    {
        return (static_cast<uint32_t>(alpha)) |
               (static_cast<uint32_t>(blue) << 8) |
               (static_cast<uint32_t>(green) << 16) |
               (static_cast<uint32_t>(red) << 24);
    }
}

static constexpr uint32_t light_green_colour_palette[4] =
{
    get_abgr_value_for_current_endianness(0xff, 0xd0, 0xf8, 0xe0),
    get_abgr_value_for_current_endianness(0xff, 0x70, 0xc0, 0x88),
    get_abgr_value_for_current_endianness(0xff, 0x56, 0x68, 0x34),
    get_abgr_value_for_current_endianness(0xff, 0x20, 0x18, 0x08)
};

static constexpr uint32_t greyscale_colour_palette[4] =
{
    get_abgr_value_for_current_endianness(0xff, 0xff, 0xff, 0xff),
    get_abgr_value_for_current_endianness(0xff, 0xaa, 0xaa, 0xaa),
    get_abgr_value_for_current_endianness(0xff, 0x55, 0x55, 0x55),
    get_abgr_value_for_current_endianness(0xff, 0x00, 0x00, 0x00)
};

static constexpr uint32_t original_green_colour_palette[4] =
{
    get_abgr_value_for_current_endianness(0xff, 0x0f, 0xbc, 0x9b),
    get_abgr_value_for_current_endianness(0xff, 0x0f, 0xac, 0x8b),
    get_abgr_value_for_current_endianness(0xff, 0x30, 0x62, 0x30),
    get_abgr_value_for_current_endianness(0xff, 0x0f, 0x38, 0x0f)
};

static const char *colour_palette_names[] =
{
    "Light Green",
    "Greyscale",
    "Original Green"
};

static const char *fast_forward_speed_names[] =
{
    "1.50x",
    "1.75x",
    "2.00x",
    "2.25x",
    "2.50x",
    "2.75x",
    "3.00x",
    "3.25x",
    "3.50x",
    "3.75x",
    "4.00x"
};
