#include <backends/imgui_impl_sdl3.h>

#include "display_utilities.h"

SDL_FRect get_sized_emulation_rectangle(
    SDL_Renderer* sdl_renderer,
    SDL_Window* sdl_window)
{
    const bool is_fullscreen_enabled = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    float space_reserved_for_menu_bar = 0.0f;

    if (!is_fullscreen_enabled)
    {
        int renderer_output_width, renderer_output_height;
        SDL_GetRenderOutputSize(sdl_renderer, &renderer_output_width, &renderer_output_height);

        const float current_scale_x = static_cast<float>(renderer_output_width) / static_cast<float>(DISPLAY_WIDTH_PIXELS);
        const float current_scale_y = static_cast<float>(renderer_output_height) / static_cast<float>(DISPLAY_HEIGHT_PIXELS);

        const int renderer_integer_scaling_factor = std::max(1, std::min(int(current_scale_x), int(current_scale_y)));

        space_reserved_for_menu_bar = (ImGui::GetFrameHeight() / static_cast<float>(renderer_integer_scaling_factor));
    }

    return SDL_FRect
    {
        0.0f,
        space_reserved_for_menu_bar,
        static_cast<float>(DISPLAY_WIDTH_PIXELS),
        static_cast<float>(DISPLAY_HEIGHT_PIXELS) - space_reserved_for_menu_bar
    };
}

void set_emulation_screen_blank(GraphicsController& graphics_controller)
{
    for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
    {
        graphics_controller.abgr_pixel_buffer[i] = graphics_controller.active_colour_palette[0];
    }
    SDL_UpdateTexture(
        graphics_controller.sdl_texture,
        nullptr,
        graphics_controller.abgr_pixel_buffer.get(),
        DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
}

void update_colour_palette(
    GameBoyEmulator::Emulator& game_boy_emulator,
    GraphicsController& graphics_controller,
    const uint8_t currently_published_frame_buffer_index)
{
    if (game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe())
    {
        auto const& pixel_frame_buffer = game_boy_emulator.get_pixel_frame_buffer(currently_published_frame_buffer_index);

        for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
        {
            graphics_controller.abgr_pixel_buffer[i] = graphics_controller.active_colour_palette[pixel_frame_buffer[i]];
        }
        SDL_UpdateTexture(
            graphics_controller.sdl_texture,
            nullptr,
            graphics_controller.abgr_pixel_buffer.get(),
            DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
    }
    else
        set_emulation_screen_blank(graphics_controller);
}

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
sdl_logical_presentation_imgui_workaround_t sdl_logical_presentation_imgui_workaround_pre_frame(SDL_Renderer* sdl_renderer)
{
    sdl_logical_presentation_imgui_workaround_t logical_values{};

    SDL_GetRenderLogicalPresentation(
        sdl_renderer,
        &logical_values.sdl_renderer_logical_width,
        &logical_values.sdl_renderer_logical_height,
        &logical_values.sdl_renderer_logical_presentation_mode);

    SDL_SetRenderLogicalPresentation(sdl_renderer,
        logical_values.sdl_renderer_logical_width,
        logical_values.sdl_renderer_logical_height,
        SDL_LOGICAL_PRESENTATION_DISABLED);

    return logical_values;
}

// Used in workaround for https://github.com/ocornut/imgui/issues/8339
void sdl_logical_presentation_imgui_workaround_post_frame(
    SDL_Renderer* sdl_renderer,
    sdl_logical_presentation_imgui_workaround_t logical_values)
{
    SDL_SetRenderLogicalPresentation(
        sdl_renderer,
        logical_values.sdl_renderer_logical_width,
        logical_values.sdl_renderer_logical_height,
        logical_values.sdl_renderer_logical_presentation_mode);
}
