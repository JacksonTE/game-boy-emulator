#include <backends/imgui_impl_sdlrenderer3.h>

#include "display_utilities.h"
#include "imgui_rendering.h"

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

        const float menu_bar_height = ImGui::GetFrameHeight();
        const float emulation_height = static_cast<float>(renderer_output_height) - menu_bar_height;

        const float current_scale_x = static_cast<float>(renderer_output_width) / static_cast<float>(DISPLAY_WIDTH_PIXELS);
        const float current_scale_y = emulation_height / static_cast<float>(DISPLAY_HEIGHT_PIXELS);
        const int renderer_integer_scaling_factor = std::max(
            1,
            std::min(static_cast<int>(current_scale_x), static_cast<int>(current_scale_y)));

        space_reserved_for_menu_bar = menu_bar_height / static_cast<float>(renderer_integer_scaling_factor);
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

bool should_main_menu_bar_and_cursor_be_visible(
    GameBoyEmulator::Emulator& game_boy_emulator,
    const EmulationController& emulation_controller,
    FullscreenDisplayStatus& fullscreen_display_status,
    SDL_Window* sdl_window)
{
    const bool is_fullscreen_enabled = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN);
    if (!is_fullscreen_enabled ||
        !game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() ||
        emulation_controller.is_emulation_paused_atomic.load(std::memory_order_acquire))
    {
        return true;
    }

    float mouse_y_position_in_window;
    SDL_GetGlobalMouseState(nullptr, &mouse_y_position_in_window);
    const float main_menu_bar_height_pixels = ImGui::GetFrameHeight() * ImGui::GetIO().DisplayFramebufferScale.y;
    ImGuiIO& io = ImGui::GetIO();
    if (SDL_GetMouseFocus() == sdl_window)
    {
        if (fullscreen_display_status.is_main_menu_bar_hovered ||
            mouse_y_position_in_window <= main_menu_bar_height_pixels ||
            io.MouseDelta.x != 0.0f ||
            io.MouseDelta.y != 0.0f)
        {
            fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
            return true;
        }
    }

    if (fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden > 0.0f)
    {
        fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden -= ImGui::GetIO().DeltaTime;
    }
    return (fullscreen_display_status.seconds_remaining_until_main_menu_bar_and_cursor_hidden > 0.0f);
}

void render_frame(RenderContext& context)
{
    const uint8_t currently_published_frame_buffer_index = context.game_boy_emulator->get_published_frame_buffer_index_thread_safe();
    if (currently_published_frame_buffer_index != *context.previously_published_frame_buffer_index)
    {
        auto const& pixel_frame_buffer = context.game_boy_emulator->get_pixel_frame_buffer(currently_published_frame_buffer_index);

        for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
        {
            context.graphics_controller->abgr_pixel_buffer[i] = context.graphics_controller->active_colour_palette[pixel_frame_buffer[i]];
        }
        SDL_UpdateTexture(context.sdl_texture, nullptr, context.graphics_controller->abgr_pixel_buffer.get(), DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
        *context.previously_published_frame_buffer_index = currently_published_frame_buffer_index;
    }

    SDL_RenderClear(context.sdl_renderer);
    SDL_FRect emulation_screen_rectangle = get_sized_emulation_rectangle(context.sdl_renderer, context.sdl_window);
    SDL_RenderTexture(context.sdl_renderer, context.sdl_texture, nullptr, &emulation_screen_rectangle);

    sdl_logical_presentation_imgui_workaround_t logical_values = sdl_logical_presentation_imgui_workaround_pre_frame(context.sdl_renderer);
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (should_main_menu_bar_and_cursor_be_visible(
        *context.game_boy_emulator,
        *context.emulation_controller,
        *context.fullscreen_display_status,
        context.sdl_window))
    {
        if (!SDL_CursorVisible())
        {
            SDL_ShowCursor();
        }
        render_main_menu_bar(
            currently_published_frame_buffer_index,
            *context.game_boy_emulator,
            *context.emulation_controller,
            *context.file_loading_status,
            *context.fullscreen_display_status,
            *context.graphics_controller,
            *context.menu_properties,
            context.sdl_window,
            *context.should_stop_emulation,
            *context.error_message);
    }
    else if (SDL_CursorVisible())
    {
        SDL_HideCursor();
    }

    render_custom_colour_palette_editor(
        currently_published_frame_buffer_index,
        *context.game_boy_emulator,
        *context.menu_properties,
        *context.graphics_controller);

    render_error_message_popup(
        *context.file_loading_status,
        context.emulation_controller->is_emulation_paused_atomic,
        *context.error_message);

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), context.sdl_renderer);
    sdl_logical_presentation_imgui_workaround_post_frame(context.sdl_renderer, logical_values);

    SDL_RenderPresent(context.sdl_renderer);
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
