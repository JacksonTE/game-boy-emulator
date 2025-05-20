#include <atomic>
#include <bit>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <SDL3/SDL.h>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>

#include "emulator.h"
#include "raii_wrappers.h"

constexpr uint8_t RIGHT_DIRECTION_PAD_FLAG_MASK = 1 << 0;
constexpr uint8_t LEFT_DIRECTION_PAD_FLAG_MASK = 1 << 1;
constexpr uint8_t UP_DIRECTION_PAD_FLAG_MASK = 1 << 2;
constexpr uint8_t DOWN_DIRECTION_PAD_FLAG_MASK = 1 << 3;

constexpr uint8_t A_BUTTON_FLAG_MASK = 1 << 0;
constexpr uint8_t B_BUTTON_FLAG_MASK = 1 << 1;
constexpr uint8_t SELECT_BUTTON_FLAG_MASK = 1 << 2;
constexpr uint8_t START_BUTTON_FLAG_MASK = 1 << 3;

constexpr bool BUTTON_PRESSED_STATE = false;
constexpr bool BUTTON_RELEASED_STATE = true;

constexpr uint8_t DISPLAY_WIDTH_PIXELS = 160;
constexpr uint8_t DISPLAY_HEIGHT_PIXELS = 144;
constexpr int WINDOW_SCALE = 6;

static void run_emulator_core(std::stop_token stop_token, GameBoyCore::Emulator &game_boy_emulator, std::exception_ptr &exception_pointer, std::atomic<bool> &did_exception_occur)
{
    try
    {
        constexpr double frame_duration_seconds = 0.01674;
        const uint64_t counter_ticks_per_second = SDL_GetPerformanceFrequency();
        const uint64_t counter_ticks_per_frame_rounded = static_cast<uint64_t>(frame_duration_seconds * counter_ticks_per_second + 0.5);

        uint64_t next_frame_counter_tick = SDL_GetPerformanceCounter();
        uint8_t previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

        while (!stop_token.stop_requested())
        {
            game_boy_emulator.step_central_processing_unit_single_instruction();

            const uint8_t currently_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;

                next_frame_counter_tick += counter_ticks_per_frame_rounded;
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
        }
    }
    catch (...)
    {
        exception_pointer = std::current_exception();
        did_exception_occur.store(true, std::memory_order_release);
    }
}

static constexpr uint32_t pack_abgr(uint8_t alpha, uint8_t blue, uint8_t green, uint8_t red)
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

static constexpr uint32_t game_boy_colour_palette[4] =
{
    pack_abgr(0xff, 0xff, 0xff, 0xff), // white
    pack_abgr(0xff, 0xaa, 0xaa, 0xaa), // light gray
    pack_abgr(0xff, 0x55, 0x55, 0x55), // dark gray
    pack_abgr(0xff, 0x00, 0x00, 0x00)  // black
};

int main()
{
    try
    {
        ResourceAcquisitionIsInitialization::SdlInitializerRaii sdl_initializer{SDL_INIT_VIDEO};
        ResourceAcquisitionIsInitialization::SdlWindowRaii sdl_window{
            "Emulate Game Boy",
            DISPLAY_WIDTH_PIXELS * WINDOW_SCALE,
            DISPLAY_HEIGHT_PIXELS * WINDOW_SCALE,
            SDL_WINDOW_RESIZABLE
        };
        ResourceAcquisitionIsInitialization::SdlRendererRaii sdl_renderer{sdl_window};
        SDL_SetRenderLogicalPresentation(
            sdl_renderer.get(),
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
        );
        if (!SDL_SetRenderVSync(sdl_renderer.get(), 1))
        {
            std::cerr << "VSync unable to be used: " << SDL_GetError() << "\n";
        }
        ResourceAcquisitionIsInitialization::SdlTextureRaii sdl_texture
        {
            sdl_renderer,
            SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_STREAMING,
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
        };
        SDL_SetTextureScaleMode(sdl_texture.get(), SDL_SCALEMODE_NEAREST);

        ResourceAcquisitionIsInitialization::ImGuiContextRaii imgui_context{sdl_window.get(), sdl_renderer.get()};

        GameBoyCore::Emulator game_boy_emulator{};

        auto bootrom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "dmg_boot.bin";
        auto rom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "Dr. Mario (JU) (V1.1).gb";
        //auto rom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "Tetris (JUE) (V1.1) [!].gb";
        //auto rom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "dmg-acid2.gb";
        //auto rom_path = std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "gbmicrotest" / "bin" / "400-dma.gb";

        if (!game_boy_emulator.try_load_bootrom(bootrom_path))
        {
            throw std::runtime_error("Error: unable to initialize Game Boy with provided bootrom path, exiting.");
        }
        if (!game_boy_emulator.try_load_file_to_memory(2 * GameBoyCore::ROM_BANK_SIZE, rom_path, false))
        {
            throw std::runtime_error("Error: unable to initialize Game Boy with provided rom path, exiting.");
        }

        uint8_t previously_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

        std::unique_ptr<uint32_t[]> abgr_pixel_buffer = std::make_unique<uint32_t[]>(static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS));
        std::fill_n(abgr_pixel_buffer.get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);

        std::exception_ptr emulator_core_exception_pointer{};
        std::atomic<bool> did_emulator_core_exception_occur{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(game_boy_emulator),
            std::ref(emulator_core_exception_pointer),
            std::ref(did_emulator_core_exception_occur)
        };

        bool stop_emulating = false;
        while (!stop_emulating)
        {
            if (did_emulator_core_exception_occur.load(std::memory_order_acquire))
            {
                std::rethrow_exception(emulator_core_exception_pointer);
            }

            SDL_Event sdl_event;
            while (SDL_PollEvent(&sdl_event))
            {
                ImGui_ImplSDL3_ProcessEvent(&sdl_event);

                switch (sdl_event.type)
                {
                    case SDL_EVENT_QUIT:
                        stop_emulating = true;
                        break;
                    case SDL_EVENT_KEY_DOWN:
                    case SDL_EVENT_KEY_UP:
                    {
                        const bool key_pressed_state = (sdl_event.type == SDL_EVENT_KEY_DOWN ? BUTTON_PRESSED_STATE : BUTTON_RELEASED_STATE);

                        switch (sdl_event.key.key)
                        {
                            case SDLK_D:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(A_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_W:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(B_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_RSHIFT:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(SELECT_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_RETURN:
                            case SDLK_KP_ENTER:
                                game_boy_emulator.update_joypad_button_pressed_state_thread_safe(START_BUTTON_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_RIGHT:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(RIGHT_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_LEFT:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(LEFT_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_UP:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(UP_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
                            case SDLK_DOWN:
                                game_boy_emulator.update_joypad_direction_pad_pressed_state_thread_safe(DOWN_DIRECTION_PAD_FLAG_MASK, key_pressed_state);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                }
            }

            const uint8_t currently_published_frame_buffer_index = game_boy_emulator.get_published_frame_buffer_index();

            if (currently_published_frame_buffer_index != previously_published_frame_buffer_index)
            {
                auto const &pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);
                previously_published_frame_buffer_index = currently_published_frame_buffer_index;

                for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
                {
                    abgr_pixel_buffer[i] = game_boy_colour_palette[pixel_frame_buffer[i]];
                }
                SDL_UpdateTexture(sdl_texture.get(), nullptr, abgr_pixel_buffer.get(), DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
            }
            SDL_RenderClear(sdl_renderer.get());
            SDL_RenderTexture(sdl_renderer.get(), sdl_texture.get(), nullptr, nullptr);
            
            {
                // imgui
            }

            SDL_RenderPresent(sdl_renderer.get());
        }
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Error: " << exception.what() << ", exiting.\n";
        return 1;
    }   
}
