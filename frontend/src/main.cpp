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
        while (!stop_token.stop_requested())
        {
            if (!game_boy_emulator.is_frame_ready_thread_safe())
            {
                game_boy_emulator.step_central_processing_unit_single_instruction();
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
        ResourceAcquisitionIsInitialization::SdlTextureRaii sdl_texture
        {
            sdl_renderer,
            SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_STREAMING,
            DISPLAY_WIDTH_PIXELS,
            DISPLAY_HEIGHT_PIXELS,
        };
        SDL_SetTextureScaleMode(sdl_texture.get(), SDL_SCALEMODE_NEAREST);

        ResourceAcquisitionIsInitialization::ImGuiContextRaii imgui_context{ sdl_window.get(), sdl_renderer.get() };

        GameBoyCore::Emulator game_boy_emulator{};

        auto bootrom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "dmg_boot.bin";
        auto rom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "Dr. Mario (JU) (V1.1).gb";
        //auto rom_path = std::filesystem::path(PROJECT_ROOT) / "bootrom" / "Tetris (JUE) (V1.1) [!].gb";
        //auto rom_path = std::filesystem::path(PROJECT_ROOT) / "tests" / "data" / "gbmicrotest" / "bin" / "400-dma.gb";

        if (!game_boy_emulator.try_load_bootrom(bootrom_path))
        {
            throw std::runtime_error("Error: unable to initialize Game Boy with provided bootrom path, exiting.");
        }
        if (!game_boy_emulator.try_load_file_to_memory(2 * GameBoyCore::ROM_BANK_SIZE, rom_path, false))
        {
            throw std::runtime_error("Error: unable to initialize Game Boy with provided rom path, exiting.");
        }

        std::exception_ptr emulator_core_exception_pointer{};
        std::atomic<bool> did_emulator_core_exception_occur{};
        std::jthread emulator_thread
        {
            run_emulator_core,
            std::ref(game_boy_emulator),
            std::ref(emulator_core_exception_pointer),
            std::ref(did_emulator_core_exception_occur)
        };

        std::unique_ptr<uint32_t[]> abgr_pixel_buffer = std::make_unique<uint32_t[]>(static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS));
        std::fill_n(abgr_pixel_buffer.get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);

        const uint64_t frame_duration_nanoseconds = 16'740'000ull;
        const uint64_t performance_counter_frequency = SDL_GetPerformanceFrequency();
        uint64_t frames_per_second_last_update_time = SDL_GetPerformanceCounter();
        int frames_since_last_update = 0;
        double frames_per_second = 0.0;

        bool stop_emulating = false;

        while (!stop_emulating)
        {
            if (did_emulator_core_exception_occur.load(std::memory_order_acquire))
            {
                std::rethrow_exception(emulator_core_exception_pointer);
            }

            SDL_Event sdl_event{};
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

            if (game_boy_emulator.is_frame_ready_thread_safe())
            {
                uint64_t frame_start_time = SDL_GetPerformanceCounter();

                auto const &pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer();

                for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
                {
                    abgr_pixel_buffer[i] = game_boy_colour_palette[pixel_frame_buffer[i]];
                }
                game_boy_emulator.clear_frame_ready_thread_safe();

                SDL_UpdateTexture(
                    sdl_texture.get(),
                    nullptr,
                    abgr_pixel_buffer.get(),
                    DISPLAY_WIDTH_PIXELS * sizeof(uint32_t)
                );
                SDL_RenderClear(sdl_renderer.get());
                SDL_RenderTexture(sdl_renderer.get(), sdl_texture.get(), nullptr, nullptr);
                SDL_RenderPresent(sdl_renderer.get());

                const uint64_t time_after_rendering = SDL_GetPerformanceCounter();
                const uint64_t elapsed_nanoseconds = (time_after_rendering - frame_start_time) * 1'000'000'000 / performance_counter_frequency;
                if (elapsed_nanoseconds < frame_duration_nanoseconds)
                {
                    SDL_DelayPrecise(frame_duration_nanoseconds - elapsed_nanoseconds);
                }

                frames_since_last_update++;
                const uint64_t time_after_frame_delay = SDL_GetPerformanceCounter();
                if (time_after_frame_delay - frames_per_second_last_update_time >= performance_counter_frequency)
                {
                    frames_per_second = frames_since_last_update * static_cast<double>(performance_counter_frequency) / static_cast<double>(time_after_frame_delay - frames_per_second_last_update_time);
                    frames_since_last_update = 0;
                    frames_per_second_last_update_time = time_after_frame_delay;

                    std::ostringstream output_string_stream;
                    output_string_stream << "Emulate Game Boy - FPS: "
                                         << std::fixed << std::setprecision(2)
                                         << frames_per_second;
                    SDL_SetWindowTitle(sdl_window.get(), output_string_stream.str().c_str());
                }
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
