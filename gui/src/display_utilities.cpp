#include <backends/imgui_impl_sdlrenderer3.h>
#include <iostream>

#include "display_utilities.h"
#include "imgui_rendering.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef __EMSCRIPTEN__
static void update_frame_diagnostics(
    FrameDiagnosticsState& frame_diagnostics_state,
    uint64_t current_published_frame_sequence_number)
{
    if (frame_diagnostics_state.performance_counter_frequency == 0)
    {
        return;
    }

    const uint64_t current_frame_counter = SDL_GetPerformanceCounter();
    if (frame_diagnostics_state.previous_frame_counter == 0)
    {
        frame_diagnostics_state.previous_frame_counter = current_frame_counter;
        frame_diagnostics_state.previous_published_frame_sequence_number = current_published_frame_sequence_number;
        return;
    }

    const double frame_time_seconds = static_cast<double>(current_frame_counter - frame_diagnostics_state.previous_frame_counter)
        / static_cast<double>(frame_diagnostics_state.performance_counter_frequency);
    frame_diagnostics_state.previous_frame_counter = current_frame_counter;

    const double frame_time_ms = frame_time_seconds * 1000.0;
    frame_diagnostics_state.sample_elapsed_seconds += frame_time_seconds;
    frame_diagnostics_state.sample_worst_frame_time_ms = std::max(frame_diagnostics_state.sample_worst_frame_time_ms, frame_time_ms);
    frame_diagnostics_state.sample_frame_count++;

    if (current_published_frame_sequence_number > frame_diagnostics_state.previous_published_frame_sequence_number)
    {
        const uint64_t published_frame_delta =
            current_published_frame_sequence_number - frame_diagnostics_state.previous_published_frame_sequence_number;
        frame_diagnostics_state.sample_emulator_frame_count += static_cast<uint32_t>(published_frame_delta);
        if (published_frame_delta > 1)
        {
            frame_diagnostics_state.sample_skipped_frame_count += static_cast<uint32_t>(published_frame_delta - 1);
        }
    }

    frame_diagnostics_state.previous_published_frame_sequence_number = current_published_frame_sequence_number;

    if (frame_diagnostics_state.sample_elapsed_seconds >= FrameDiagnosticsState::SAMPLE_WINDOW_SECONDS)
    {
        frame_diagnostics_state.displayed_worst_frame_time_ms = frame_diagnostics_state.sample_worst_frame_time_ms;
        frame_diagnostics_state.displayed_emulator_frame_rate =
            static_cast<double>(frame_diagnostics_state.sample_emulator_frame_count) / frame_diagnostics_state.sample_elapsed_seconds;
        frame_diagnostics_state.displayed_skipped_frame_count = frame_diagnostics_state.sample_skipped_frame_count;
        frame_diagnostics_state.has_completed_sample = true;
        frame_diagnostics_state.sample_elapsed_seconds = 0.0;
        frame_diagnostics_state.sample_worst_frame_time_ms = 0.0;
        frame_diagnostics_state.sample_frame_count = 0;
        frame_diagnostics_state.sample_emulator_frame_count = 0;
        frame_diagnostics_state.sample_skipped_frame_count = 0;
    }
}

#endif

static bool is_cursor_currently_visible()
{
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT(
    {
        return Module.is_web_cursor_visible ? 1 : 0;
    }) != 0;
#else
    return SDL_CursorVisible();
#endif
}

static void set_cursor_visible(const bool is_visible)
{
#ifdef __EMSCRIPTEN__
    EM_ASM(
    {
        Module.setWebCursorVisible(!!$0);
    },
    is_visible ? 1 : 0);
#else
    if (is_visible)
    {
        SDL_ShowCursor();
    }
    else
    {
        SDL_HideCursor();
    }
#endif
}

int get_initial_window_scale_for_display()
{
    const SDL_DisplayMode* display_mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (display_mode != nullptr)
    {
        constexpr float BASE_PIXEL_HEIGHT_FOR_SCALING = 1440.0f;
        const float display_height = static_cast<float>(display_mode->h);
        const float scale_multiplier = display_height / BASE_PIXEL_HEIGHT_FOR_SCALING;

        return static_cast<int>(DEFAULT_INITIAL_WINDOW_SCALE * scale_multiplier + 0.5f);
    }
    return DEFAULT_INITIAL_WINDOW_SCALE;
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
    {
        set_emulation_screen_blank(graphics_controller);
    }
}

bool should_main_menu_bar_and_cursor_be_visible(
    const GameBoyEmulator::Emulator& game_boy_emulator,
    const EmulationController& emulation_controller,
    MenuAndCursorDisplayStatus& menu_and_cursor_display_status,
    bool is_custom_palette_editor_open,
    bool is_keybinds_editor_open,
    SDL_Window* sdl_window)
{
    (void)sdl_window;
    if (!game_boy_emulator.is_game_rom_loaded_in_memory_thread_safe() ||
        emulation_controller.is_emulation_paused_atomic.load(std::memory_order_acquire) ||
        is_keybinds_editor_open ||
        is_custom_palette_editor_open)
    {
        menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden
            = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
        return true;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float main_menu_bar_height_pixels = ImGui::GetFrameHeight() * io.DisplayFramebufferScale.y;
    const bool is_mouse_in_window =
        ImGui::IsMousePosValid(&io.MousePos) &&
        io.MousePos.x >= 0.0f &&
        io.MousePos.y >= 0.0f &&
        io.MousePos.x < io.DisplaySize.x &&
        io.MousePos.y < io.DisplaySize.y;

    if (is_mouse_in_window)
    {
        if (menu_and_cursor_display_status.cursor_changes_to_ignore_count != 0)
        {
            menu_and_cursor_display_status.cursor_changes_to_ignore_count--;
            return false;
        }
        else if (menu_and_cursor_display_status.is_main_menu_bar_hovered ||
                io.MousePos.y <= main_menu_bar_height_pixels ||
                io.MouseDelta.x != 0.0f ||
                io.MouseDelta.y != 0.0f)
        {
            menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden
                = MAIN_MENU_BAR_AND_CURSOR_HIDE_DELAY_SECONDS;
            return true;
        }
    }

    if (menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden > 0.0f)
    {
        menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden -= ImGui::GetIO().DeltaTime;
    }
    return (menu_and_cursor_display_status.seconds_until_main_menu_bar_and_cursor_hidden > 0.0f);
}

void update_imgui_scale_by_resolution(SDL_Window* sdl_window)
{
    constexpr float BASE_PIXEL_HEIGHT_FOR_FONT_SCALING = 1440.0f;

#ifdef __EMSCRIPTEN__
    const double browser_window_height = EM_ASM_DOUBLE(
    {
        return window.innerHeight * (window.devicePixelRatio || 1);
    });

    if (browser_window_height <= 0.0)
    {
        return;
    }

    const float display_height = static_cast<float>(browser_window_height);
    const double browser_inner_height = EM_ASM_DOUBLE({ return window.innerHeight || 0; });
    const double browser_device_pixel_ratio = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1; });
    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(sdl_window, &window_width, &window_height);
#else
    SDL_DisplayID display_id = SDL_GetDisplayForWindow(sdl_window);
    const SDL_DisplayMode* display_mode = SDL_GetCurrentDisplayMode(display_id);

    if (display_mode == nullptr)
    {
        return;
    }

    const float display_height = static_cast<float>(display_mode->h);
#endif
    const float font_scale = display_height / BASE_PIXEL_HEIGHT_FOR_FONT_SCALING;

#ifdef __EMSCRIPTEN__
    static float previous_logged_display_height = -1.0f;
    static float previous_logged_font_scale = -1.0f;
    static int previous_logged_window_width = -1;
    static int previous_logged_window_height = -1;

    if (previous_logged_display_height != display_height ||
        previous_logged_font_scale != font_scale ||
        previous_logged_window_width != window_width ||
        previous_logged_window_height != window_height)
    {
        previous_logged_display_height = display_height;
        previous_logged_font_scale = font_scale;
        previous_logged_window_width = window_width;
        previous_logged_window_height = window_height;

        std::cout << "[web_scale] viewport_pixel_height=" << browser_window_height
                  << " inner_height=" << browser_inner_height
                  << " device_pixel_ratio=" << browser_device_pixel_ratio
                  << " sdl_window_width=" << window_width
                  << " sdl_window_height=" << window_height
                  << " display_height=" << display_height
                  << " font_scale=" << font_scale << "\n";
    }
#endif

    ImGuiStyle& style = ImGui::GetStyle();

    constexpr float BASE_FRAME_PADDING_X = 6.0f;
    constexpr float BASE_FRAME_PADDING_Y = 6.0f;
    constexpr float BASE_ITEM_SPACING_X = 8.0f;
    constexpr float BASE_ITEM_SPACING_Y = 4.0f;
    constexpr float BASE_ITEM_INNER_SPACING_X = 4.0f;
    constexpr float BASE_ITEM_INNER_SPACING_Y = 6.0f;
    constexpr float BASE_WINDOW_PADDING_X = 8.0f;
    constexpr float BASE_WINDOW_PADDING_Y = 10.0f;

    style.FramePadding = ImVec2(BASE_FRAME_PADDING_X * font_scale, BASE_FRAME_PADDING_Y * font_scale);
    style.ItemSpacing = ImVec2(BASE_ITEM_SPACING_X * font_scale, BASE_ITEM_SPACING_Y * font_scale);
    style.ItemInnerSpacing = ImVec2(BASE_ITEM_INNER_SPACING_X * font_scale, BASE_ITEM_INNER_SPACING_Y * font_scale);
    style.WindowPadding = ImVec2(BASE_WINDOW_PADDING_X * font_scale, BASE_WINDOW_PADDING_Y * font_scale);
    style.FontScaleMain = font_scale;
}

void render_frame(RenderContext& context)
{
    if (context.is_currently_rendering)
    {
        return;
    }

    int window_width, window_height;
    SDL_GetWindowSize(context.sdl_window, &window_width, &window_height);
    if (window_width <= 0 || window_height <= 0)
    {
        return;
    }

    context.is_currently_rendering = true;
    const uint8_t currently_published_frame_buffer_index = context.game_boy_emulator->get_published_frame_buffer_index_thread_safe();
    const uint64_t current_published_frame_sequence_number = context.game_boy_emulator->get_published_frame_sequence_number_thread_safe();

#ifndef __EMSCRIPTEN__
    update_frame_diagnostics(*context.frame_diagnostics_state, current_published_frame_sequence_number);
#endif

    if (current_published_frame_sequence_number != *context.previously_published_frame_sequence_number)
    {
        auto const& pixel_frame_buffer = context.game_boy_emulator->get_pixel_frame_buffer(currently_published_frame_buffer_index);

        for (int i = 0; i < DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS; i++)
        {
            context.graphics_controller->abgr_pixel_buffer[i] = context.graphics_controller->active_colour_palette[pixel_frame_buffer[i]];
        }
        SDL_UpdateTexture(
            context.sdl_texture,
            nullptr,
            context.graphics_controller->abgr_pixel_buffer.get(),
            DISPLAY_WIDTH_PIXELS * sizeof(uint32_t));
        *context.previously_published_frame_buffer_index = currently_published_frame_buffer_index;
        *context.previously_published_frame_sequence_number = current_published_frame_sequence_number;
    }

    SDL_RenderClear(context.sdl_renderer);
    SDL_FRect emulation_screen_rectangle =
        SDL_FRect
        {
            0.0f,
            0.0f,
            static_cast<float>(DISPLAY_WIDTH_PIXELS),
            static_cast<float>(DISPLAY_HEIGHT_PIXELS)
        };
    SDL_RenderTexture(context.sdl_renderer, context.sdl_texture, nullptr, &emulation_screen_rectangle);

    sdl_logical_presentation_imgui_workaround_t logical_values
        = sdl_logical_presentation_imgui_workaround_pre_frame(context.sdl_renderer);

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();

    if (should_main_menu_bar_and_cursor_be_visible(
            *context.game_boy_emulator,
            *context.emulation_controller,
            *context.menu_and_cursor_display_status,
            context.menu_properties->is_custom_palette_editor_open,
            context.menu_properties->keybinds_editor_state.is_open,
            context.sdl_window))
    {
        if (!is_cursor_currently_visible())
        {
            set_cursor_visible(true);
        }
        render_main_menu_bar(
            currently_published_frame_buffer_index,
            *context.game_boy_emulator,
            *context.emulation_controller,
            *context.file_loading_status,
            *context.menu_and_cursor_display_status,
#ifndef __EMSCRIPTEN__
            *context.frame_diagnostics_state,
#endif
            *context.graphics_controller,
            *context.menu_properties,
            *context.key_bindings,
            context.sdl_window,
            context.loaded_game_rom_path,
            context.loaded_boot_rom_path,
            *context.should_stop_emulation,
            *context.error_message);
    }
    else if (is_cursor_currently_visible())
    {
        set_cursor_visible(false);
    }

    render_auxiliary_windows(
        currently_published_frame_buffer_index,
        *context.game_boy_emulator,
        *context.file_loading_status,
        context.emulation_controller->is_emulation_paused_atomic,
        *context.graphics_controller,
        *context.menu_properties,
        *context.key_bindings,
        *context.error_message
#ifndef __EMSCRIPTEN__
        , *context.frame_diagnostics_state
#endif
    );

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), context.sdl_renderer);
    sdl_logical_presentation_imgui_workaround_post_frame(context.sdl_renderer, logical_values);

    SDL_RenderPresent(context.sdl_renderer);
    context.is_currently_rendering = false;
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
