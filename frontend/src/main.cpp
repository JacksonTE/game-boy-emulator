#include <atomic>
#include <exception>
#include <filesystem>
#include <iostream>
#include <SDL3/SDL.h>
#include <stop_token>
#include <string>
#include <thread>

#include "emulator.h"
#include "sdl_wrappers.h"

static void run_emulator_core(std::stop_token stop_token, std::exception_ptr &exception_pointer, std::atomic<bool> &did_exception_occur)
{
    try
    {
        GameBoyCore::Emulator game_boy_emulator{};

        std::filesystem::path rom_path = std::filesystem::path(PROJECT_ROOT) /
            "tests" / "data" / "gbmicrotest" / "bin" / "win0_scx3_a.gb";

        if (!game_boy_emulator.try_load_file_to_memory(2 * GameBoyCore::ROM_BANK_SIZE, rom_path, false))
        {
            throw std::runtime_error("Error: unable to initialize Game Boy with provided rom path, exiting.");
        }

        std::filesystem::path bootrom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "dmg_boot.bin";

        if (!game_boy_emulator.try_load_bootrom(bootrom_path))
        {
            throw std::runtime_error("Error: unable to initialize Game Boy with provided bootrom path, exiting.");
        }

        while (!stop_token.stop_requested())
        {
            const uint8_t test_result_byte = game_boy_emulator.read_byte_from_memory(0xff80);
            const uint8_t test_expected_result_byte = game_boy_emulator.read_byte_from_memory(0xff81);
            const uint8_t test_pass_fail_byte = game_boy_emulator.read_byte_from_memory(0xff82);

            if (test_pass_fail_byte == 0xff)
            {
                throw std::runtime_error
                (
                    std::string("Test failed with result ")
                        + std::to_string(static_cast<int>(test_result_byte))
                        + ". Expected result was "
                        + std::to_string(static_cast<int>(test_expected_result_byte))
                        + "\n"
                );
            }
            else if (test_pass_fail_byte == 0x01)
            {
                throw std::runtime_error(
                    std::string("Test passed with result ")
                        + std::to_string(static_cast<int>(test_result_byte))
                );
            }

            game_boy_emulator.step_cpu_single_instruction();
        }
    }
    catch (...)
    {
        exception_pointer = std::current_exception();
        did_exception_occur.store(true, std::memory_order_release);
    }
}

int main()
{
    try
    {
        SdlResourceAcquisitionIsInitialization::Initializer sdl_initializer{SDL_INIT_VIDEO};
        SdlResourceAcquisitionIsInitialization::Window sdl_window{"Emulate Game Boy", 160, 144, SDL_WINDOW_RESIZABLE};
        SdlResourceAcquisitionIsInitialization::Renderer sdl_renderer{sdl_window};
        SdlResourceAcquisitionIsInitialization::Texture sdl_texture
        {
            sdl_renderer,
            SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING,
            160,
            144,
        };

        std::exception_ptr emulator_core_exception_pointer{};
        std::atomic<bool> did_emulator_core_exception_occur_atomic{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(emulator_core_exception_pointer),
            std::ref(did_emulator_core_exception_occur_atomic)
        };

        while (true)
        {
            if (did_emulator_core_exception_occur_atomic.load(std::memory_order_acquire))
            {
                std::rethrow_exception(emulator_core_exception_pointer);
            }
        }
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Error: " << exception.what() << ", exiting.\n";
        return 1;
    }   
}
