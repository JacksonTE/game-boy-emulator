#include <algorithm>
#include <cstdint>
#include <memory>

#include "bitwise_utilities.h"
#include "pixel_processing_unit.h"

namespace GameBoyEmulator
{

void PixelSliceFetcher::reset_state()
{
    current_step = PixelSliceFetcherStep::GetTileId;
    tile_index = 0;
    tile_row_low = 0;
    tile_row_high = 0;
    is_in_first_dot_of_current_step = true;
    is_enabled = false;
}

void BackgroundPixelSliceFetcher::reset_state()
{
    PixelSliceFetcher::reset_state();
    is_enabled = true;
    tile_row.fill(BackgroundPixel{});
    fetcher_mode = FetcherMode::BackgroundMode;
    fetcher_x = 0;
}

PixelProcessingUnit::PixelProcessingUnit(std::function<void(uint8_t)> request_interrupt)
    : request_interrupt_callback{request_interrupt}
{
    video_ram = std::make_unique<uint8_t[]>(VIDEO_RAM_SIZE);
    std::fill_n(video_ram.get(), VIDEO_RAM_SIZE, 0);

    object_attribute_memory = std::make_unique<uint8_t[]>(OBJECT_ATTRIBUTE_MEMORY_SIZE);
    std::fill_n(object_attribute_memory.get(), OBJECT_ATTRIBUTE_MEMORY_SIZE, 0);

    pixel_frame_buffers[0] = std::make_unique<uint8_t[]>(static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS));
    pixel_frame_buffers[1] = std::make_unique<uint8_t[]>(static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS));
    std::fill_n(pixel_frame_buffers[0].get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);
    std::fill_n(pixel_frame_buffers[1].get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);
}

void PixelProcessingUnit::reset_state()
{
    std::fill_n(video_ram.get(), VIDEO_RAM_SIZE, 0);
    std::fill_n(object_attribute_memory.get(), OBJECT_ATTRIBUTE_MEMORY_SIZE, 0);

    std::fill_n(pixel_frame_buffers[in_progress_frame_index].get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);
    publish_new_frame();

    scy_viewport_y_position = 0;
    scx_viewport_x_position = 0;
    lyc_lcd_y_coordinate_compare = 0;
    dma_object_attribute_memory_direct_memory_access = 0;
    bgp_background_palette = 0x00;
    obp0_object_palette_0 = 0xFF;
    obp1_object_palette_1 = 0xFF;
    wy_window_y_position = 0;
    wx_window_x_position_plus_7 = 0;
    is_oam_dma_in_progress = false;

    lcdc_lcd_control = 0;
    stat_lcd_status = 0b10000000;
    ly_lcd_y_coordinate = 0;
    lx_internal_lcd_x_coordinate_plus_8 = 0;
    wlc_internal_window_line_counter = 0;

    previous_mode = PixelProcessingUnitMode::HorizontalBlank;
    current_mode = PixelProcessingUnitMode::HorizontalBlank;
    current_scanline_dot_number = 0;
    is_in_frame_after_lcd_enable = false;
    is_in_first_scanline_after_lcd_enable = false;
    is_in_first_dot_of_current_step = true;
    is_window_enabled_for_scanline = false;

    stat_value_after_spurious_interrupt = 0;
    did_spurious_stat_interrupt_occur = false;
    are_stat_interrupts_blocked = false;
    is_stat_interrupt_pending = false;
    previous_ly_lcd_y_coordinate = 0;
    previous_lyc_lcd_y_coordinate_compare = 0;
    hblank_mode_delay_dots = 0;
    is_vram_blocked = false;
    is_oam_blocked = false;
    was_wy_condition_triggered_this_frame = false;

    scanline_selected_objects.clear();
    current_object_index = 0;

    scanline_pixels_to_discard_from_dummy_fetch_count = 8;
    scanline_pixels_to_discard_from_scrolling_count = -1;

    background_fetcher.reset_state();
    object_fetcher.reset_state();

    background_pixel_shift_register.clear();
    object_pixel_shift_register.clear();
}

void PixelProcessingUnit::set_post_boot_state()
{
    reset_state();

    previous_mode = PixelProcessingUnitMode::VerticalBlank;
    current_mode = PixelProcessingUnitMode::VerticalBlank;
    current_scanline_dot_number = 395;

    lcdc_lcd_control = 0x91;
    stat_lcd_status = 0x85;
    dma_object_attribute_memory_direct_memory_access = 0xFF;
    bgp_background_palette = 0xFC;
}

uint8_t PixelProcessingUnit::get_published_frame_buffer_index_thread_safe() const
{
    return published_frame_index_atomic.load(std::memory_order_acquire);
}

uint64_t PixelProcessingUnit::get_published_frame_sequence_number_thread_safe() const
{
    return published_frame_sequence_number_atomic.load(std::memory_order_acquire);
}

std::unique_ptr<uint8_t[]>& PixelProcessingUnit::get_pixel_frame_buffer(uint8_t index)
{
    return pixel_frame_buffers[index];
}

uint8_t PixelProcessingUnit::read_lcdc_lcd_control() const
{
    return lcdc_lcd_control;
}

void PixelProcessingUnit::write_lcdc_lcd_control(uint8_t value)
{
    const bool was_lcd_enable_bit_previously_set = is_bit_set(lcdc_lcd_control, 7);
    const bool will_lcd_enable_bit_be_set = is_bit_set(value, 7);

    if (will_lcd_enable_bit_be_set && !was_lcd_enable_bit_previously_set)
    {
        is_in_frame_after_lcd_enable = true;
        is_in_first_scanline_after_lcd_enable = true;
        is_in_first_dot_of_current_step = true;
        current_scanline_dot_number = 0;
        previous_lyc_lcd_y_coordinate_compare = lyc_lcd_y_coordinate_compare;
        set_bit(stat_lcd_status, 2, lyc_lcd_y_coordinate_compare == 0);
    }
    else if (!will_lcd_enable_bit_be_set && was_lcd_enable_bit_previously_set)
    {
        std::fill_n(pixel_frame_buffers[in_progress_frame_index].get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);
        publish_new_frame();

        ly_lcd_y_coordinate = 0;
        previous_ly_lcd_y_coordinate = 0;
        wlc_internal_window_line_counter = 0;
        hblank_mode_delay_dots = 0;
        is_vram_blocked = false;
        is_oam_blocked = false;
        scanline_selected_objects.clear();
        was_wy_condition_triggered_this_frame = false;
        switch_to_mode(PixelProcessingUnitMode::HorizontalBlank);
        previous_mode = PixelProcessingUnitMode::HorizontalBlank;
    }
    lcdc_lcd_control = value;
}

uint8_t PixelProcessingUnit::read_stat_lcd_status() const
{
    return (stat_lcd_status & 0b11111100) | static_cast<uint8_t>(previous_mode);
}

void PixelProcessingUnit::write_stat_lcd_status(uint8_t value)
{
    const uint8_t new_stat_value = (value & 0b01111000) | (stat_lcd_status & 0b10000111);
    const bool is_the_ly_equal_to_lyc_flag_set = is_bit_set(stat_lcd_status, 2);

    if (previous_mode != PixelProcessingUnitMode::PixelTransfer || is_the_ly_equal_to_lyc_flag_set)
    {
        stat_value_after_spurious_interrupt = new_stat_value;
        did_spurious_stat_interrupt_occur = true;
        stat_lcd_status = 0xFF; // TODO eventually confirm spurious interrupt timing with Ocean’s Road Rash and Vic Tokai’s Xerd no Densetsu
    }
    else
    {
        stat_lcd_status = new_stat_value;
    }
}

uint8_t PixelProcessingUnit::read_ly_lcd_y_coordinate() const
{
    return ly_lcd_y_coordinate;
}

uint8_t PixelProcessingUnit::read_byte_video_ram(uint16_t memory_address) const
{
    if (is_vram_blocked)
    {
        return 0xFF;
    }
    const uint16_t local_address = memory_address - VIDEO_RAM_START;
    return video_ram[local_address];
}

void PixelProcessingUnit::write_byte_video_ram(uint16_t memory_address, uint8_t value)
{
    if (is_vram_blocked)
    {
        return;
    }
    const uint16_t local_address = memory_address - VIDEO_RAM_START;
    video_ram[local_address] = value;
}

uint8_t PixelProcessingUnit::read_byte_object_attribute_memory(uint16_t memory_address, bool is_access_unrestricted) const
{
    if (is_oam_dma_in_progress)
    {
        return 0xFF;
    }

    if (!is_access_unrestricted && is_oam_blocked)
    {
        return 0xFF;
    }
    const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
    return object_attribute_memory[local_address];
}

void PixelProcessingUnit::write_byte_object_attribute_memory(uint16_t memory_address, uint8_t value, bool is_access_unrestricted)
{
    if (!is_access_unrestricted)
    {
        if (is_oam_dma_in_progress)
        {
            return;
        }

        if (is_oam_blocked)
        {
            return;
        }
    }
    const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
    object_attribute_memory[local_address] = value;
}

void PixelProcessingUnit::step_one_clock_cycle()
{
    const bool is_lcd_enable_bit_set = is_bit_set(lcdc_lcd_control, 7);
    if (is_lcd_enable_bit_set)
    {
        current_scanline_dot_number++;
        if (hblank_mode_delay_dots > 0)
        {
            hblank_mode_delay_dots--;
            is_vram_blocked = false;
            is_oam_blocked = false;
        }

        switch (current_mode)
        {
            case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
                step_object_attribute_memory_scan_forward_one_dot();
                break;
            case PixelProcessingUnitMode::PixelTransfer:
                step_pixel_transfer_forward_one_dot();
                break;
            case PixelProcessingUnitMode::HorizontalBlank:
                step_horizontal_blank_forward_one_dot();
                break;
            case PixelProcessingUnitMode::VerticalBlank:
                step_vertical_blank_forward_one_dot();
                break;
        }
        update_previous_mode_for_current_dot();
        trigger_stat_interrupts();
        // These corrections affect mode visibility after STAT interrupts are checked for this dot.
        if ((previous_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan && is_oam_dma_in_progress) ||
            (current_mode == PixelProcessingUnitMode::VerticalBlank && ly_lcd_y_coordinate == 0 && current_scanline_dot_number >= 454))
        {
            previous_mode = PixelProcessingUnitMode::HorizontalBlank;
        }
        previous_ly_lcd_y_coordinate = ly_lcd_y_coordinate;
        previous_lyc_lcd_y_coordinate_compare = lyc_lcd_y_coordinate_compare;
    }

    if (did_spurious_stat_interrupt_occur)
    {
        stat_lcd_status = (stat_lcd_status & 0b10000111) | stat_value_after_spurious_interrupt;
        did_spurious_stat_interrupt_occur = false;
        stat_value_after_spurious_interrupt = 0x00;
    }
}

void PixelProcessingUnit::update_previous_mode_for_current_dot()
{
    previous_mode = current_mode;
    if (current_scanline_dot_number == 79 &&
        (current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan || is_in_first_scanline_after_lcd_enable))
    {
        previous_mode = PixelProcessingUnitMode::PixelTransfer;
    }
    if (current_mode == PixelProcessingUnitMode::HorizontalBlank && current_scanline_dot_number == 455)
    {
        previous_mode = (ly_lcd_y_coordinate == FIRST_SCANLINE_OF_VERTICAL_BLANK)
            ? PixelProcessingUnitMode::VerticalBlank
            : PixelProcessingUnitMode::ObjectAttributeMemoryScan;
    }
    if (hblank_mode_delay_dots > 0)
    {
        previous_mode = PixelProcessingUnitMode::PixelTransfer;
    }
}

void PixelProcessingUnit::step_object_attribute_memory_scan_forward_one_dot()
{
    is_oam_blocked = current_scanline_dot_number < 76 || current_scanline_dot_number >= 78;
    is_vram_blocked = current_scanline_dot_number >= 78;
    if (is_in_first_dot_of_current_step)
    {
        is_in_first_dot_of_current_step = false;
        return;
    }

    if (scanline_selected_objects.size() < MAX_OBJECTS_PER_LINE)
    {
        const uint16_t object_start_global_address = OBJECT_ATTRIBUTE_MEMORY_START + (2 * current_scanline_dot_number) - 4;
        const bool is_access_unrestricted = true;
        ObjectAttributes current_object
        {
            object_start_global_address,
            read_byte_object_attribute_memory(object_start_global_address, is_access_unrestricted),
            read_byte_object_attribute_memory(object_start_global_address + 1, is_access_unrestricted),
            read_byte_object_attribute_memory(object_start_global_address + 2, is_access_unrestricted),
            read_byte_object_attribute_memory(object_start_global_address + 3, is_access_unrestricted),
        };

        const uint8_t object_height = is_bit_set(lcdc_lcd_control, 2) ? 16 : 8;
        const int16_t object_lowest_lcd_y_coordinate = current_object.y_position - 16;
        const bool does_object_intersect_with_scanline = ly_lcd_y_coordinate >= object_lowest_lcd_y_coordinate &&
                                                         ly_lcd_y_coordinate < object_lowest_lcd_y_coordinate + object_height;
        if (does_object_intersect_with_scanline)
        {
            scanline_selected_objects.push_back(current_object);
        }
    }
    is_in_first_dot_of_current_step = true;

    if (current_scanline_dot_number == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS)
    {
        std::stable_sort(scanline_selected_objects.begin(), scanline_selected_objects.end(), 
            [](const ObjectAttributes& a, const ObjectAttributes& b)
            {
                return a.x_position < b.x_position; 
            });
        switch_to_mode(PixelProcessingUnitMode::PixelTransfer);
    }
}

void PixelProcessingUnit::step_pixel_transfer_forward_one_dot()
{
    const uint8_t dot_number_for_dummy_push = OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS + 4;

    if (current_scanline_dot_number == dot_number_for_dummy_push - 1)
    {
        scanline_pixels_to_discard_from_scrolling_count = scx_viewport_x_position % 8;
    }

    if (current_scanline_dot_number < dot_number_for_dummy_push)
    {
        return;
    }
    else if (current_scanline_dot_number == dot_number_for_dummy_push)
    {
        const auto& tile_row_to_discard = background_fetcher.tile_row;
        background_pixel_shift_register.load_new_tile_row(tile_row_to_discard);
    }

    if (background_fetcher.is_enabled &&
        background_fetcher.fetcher_mode == FetcherMode::BackgroundMode &&
        is_window_enabled_for_scanline && was_wy_condition_triggered_this_frame &&
        lx_internal_lcd_x_coordinate_plus_8 == wx_window_x_position_plus_7 + 1)
    {
        background_pixel_shift_register.clear();
        background_fetcher.reset_state();
        background_fetcher.fetcher_mode = FetcherMode::WindowMode;
    }
    if (background_fetcher.is_enabled &&
        background_fetcher.fetcher_mode == FetcherMode::WindowMode &&
        background_fetcher.fetcher_x <= 14)
    {
        background_fetcher.fetcher_x++;
    }

    step_fetchers_forward_one_dot(); 

    const bool should_draw_or_discard_pixels = !object_fetcher.is_enabled && !background_pixel_shift_register.is_empty();
    if (should_draw_or_discard_pixels)
    {
        const BackgroundPixel next_background_pixel = background_pixel_shift_register.shift_out();
        const ObjectPixel next_object_pixel = object_pixel_shift_register.shift_out();

        if (scanline_pixels_to_discard_from_scrolling_count == -1)
        {
            scanline_pixels_to_discard_from_scrolling_count = (background_fetcher.fetcher_mode == FetcherMode::BackgroundMode || wx_window_x_position_plus_7 == 0)
                ? scx_viewport_x_position % 8
                : 0;
        }
        if (scanline_pixels_to_discard_from_scrolling_count > 0)
        {
            scanline_pixels_to_discard_from_scrolling_count--;
            return;
        }
        if (scanline_pixels_to_discard_from_dummy_fetch_count > 0)
        {
            lx_internal_lcd_x_coordinate_plus_8++;
            scanline_pixels_to_discard_from_dummy_fetch_count--;
            return;
        }

        const bool are_background_and_window_enabled = is_bit_set(lcdc_lcd_control, 0);
        uint8_t pixel_with_palette_applied = 0x00;

        if (scanline_selected_objects.size() == 0 ||
            (are_background_and_window_enabled &&
             (!is_object_display_enabled() ||
              (next_object_pixel.is_priority_bit_set && next_background_pixel.colour_index != 0) ||
              next_object_pixel.colour_index == 0)))
        {
            const uint8_t colour_index = are_background_and_window_enabled
                ? next_background_pixel.colour_index
                : 0b00;
            const uint8_t palette_colour_position = colour_index << 1;
            pixel_with_palette_applied = (bgp_background_palette & (0b11 << palette_colour_position)) >> palette_colour_position;
        }
        else
        {
            const uint8_t palette_colour_position = next_object_pixel.colour_index << 1;
            const uint8_t palette = next_object_pixel.is_palette_bit_set
                ? obp1_object_palette_1
                : obp0_object_palette_0;
            pixel_with_palette_applied = (palette & (0b11 << palette_colour_position)) >> palette_colour_position;
        }
        const uint16_t pixel_address = static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * ly_lcd_y_coordinate) + (lx_internal_lcd_x_coordinate_plus_8 - 8);
        pixel_frame_buffers[in_progress_frame_index][pixel_address] = pixel_with_palette_applied;

        background_fetcher.fetcher_x++;
        if (++lx_internal_lcd_x_coordinate_plus_8 == 168)
        {
            switch_to_mode(PixelProcessingUnitMode::HorizontalBlank);
            if (is_in_first_scanline_after_lcd_enable)
            {
                hblank_mode_delay_dots = 2;
            }
            else
            {
                is_vram_blocked = false;
                is_oam_blocked = false;
            }
        }
    }
}

void PixelProcessingUnit::step_horizontal_blank_forward_one_dot()
{
    if (current_scanline_dot_number == 454)
    {
        ly_lcd_y_coordinate++;
        is_oam_blocked = ly_lcd_y_coordinate < FIRST_SCANLINE_OF_VERTICAL_BLANK;
    }
    if (current_scanline_dot_number < SCANLINE_DURATION_DOTS)
    {
        if (is_in_first_scanline_after_lcd_enable && 
            current_scanline_dot_number == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS)
        {
            switch_to_mode(PixelProcessingUnitMode::PixelTransfer);
        }
        return;
    }

    if (background_fetcher.fetcher_mode == FetcherMode::WindowMode)
    {
        wlc_internal_window_line_counter++;
    }
    current_scanline_dot_number = 0;

    if (ly_lcd_y_coordinate == FIRST_SCANLINE_OF_VERTICAL_BLANK)
    {
        switch_to_mode(PixelProcessingUnitMode::VerticalBlank);
    }
    else
    {
        is_in_first_scanline_after_lcd_enable = false;
        switch_to_mode(PixelProcessingUnitMode::ObjectAttributeMemoryScan);
    }
}

void PixelProcessingUnit::step_vertical_blank_forward_one_dot()
{
    if (ly_lcd_y_coordinate == FINAL_SCANLINE_OF_FRAME && current_scanline_dot_number == FINAL_SCANLINE_EARLY_LY_RESET_DOT_NUMBER)
    {
        ly_lcd_y_coordinate = 0;
        wlc_internal_window_line_counter = 0;
    }
    if (current_scanline_dot_number == 454)
    {
        if (ly_lcd_y_coordinate != 0)
        {
            ly_lcd_y_coordinate++;
        }
        else
        {
            is_oam_blocked = true;
        }
    }
    if (current_scanline_dot_number < SCANLINE_DURATION_DOTS)
    {
        return;
    }

    if (ly_lcd_y_coordinate == 0)
    {
        was_wy_condition_triggered_this_frame = false;
        switch_to_mode(PixelProcessingUnitMode::ObjectAttributeMemoryScan);
    }
    current_scanline_dot_number = 0;
}

void PixelProcessingUnit::trigger_stat_interrupts()
{
    const bool is_final_scanline = (current_mode == PixelProcessingUnitMode::VerticalBlank) && (ly_lcd_y_coordinate == 0);

    const bool is_comparison_enabled = (is_final_scanline || current_scanline_dot_number != 454) &&
        !(is_final_scanline && current_scanline_dot_number >= 3 && current_scanline_dot_number < 7);

    const bool is_ly_equal_to_lyc = is_comparison_enabled && (previous_ly_lcd_y_coordinate == previous_lyc_lcd_y_coordinate_compare);

    const bool is_lyc_interrupt_select_enabled = is_bit_set(stat_lcd_status, 6);

    // Mode 2 is an entry pulse, rather than a level held throughout the OAM scan.
    const bool is_oam_interrupt_dot =
        (current_mode == PixelProcessingUnitMode::HorizontalBlank && current_scanline_dot_number == 453) ||
        (current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan && current_scanline_dot_number == 0 && ly_lcd_y_coordinate == 0);
    if (is_oam_interrupt_dot)
    {
        const bool should_oam_interrupt_trigger = is_bit_set(stat_lcd_status, 5) || (is_lyc_interrupt_select_enabled && is_ly_equal_to_lyc);
        is_stat_interrupt_pending = should_oam_interrupt_trigger && !are_stat_interrupts_blocked;

        if (ly_lcd_y_coordinate != 143 && is_lyc_interrupt_select_enabled &&
            previous_lyc_lcd_y_coordinate_compare == ly_lcd_y_coordinate + 1)
        {
            are_stat_interrupts_blocked = should_oam_interrupt_trigger;
        }
    }

    if (is_stat_interrupt_pending)
    {
        request_interrupt_callback(INTERRUPT_FLAG_STAT_MASK);
        is_stat_interrupt_pending = false;
    }
    else
    {
        const bool is_first_oam_scan = is_in_first_scanline_after_lcd_enable &&
            current_scanline_dot_number < OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS;
        const bool is_leaving_vertical_blank = is_final_scanline && current_scanline_dot_number >= 454;
        bool should_stat_interrupt_trigger = false;
        if (is_lyc_interrupt_select_enabled && is_ly_equal_to_lyc)
        {
            should_stat_interrupt_trigger = true;
        }
        if (!is_first_oam_scan && !is_leaving_vertical_blank)
        {
            if (previous_mode == PixelProcessingUnitMode::HorizontalBlank && is_bit_set(stat_lcd_status, 3))
            {
                should_stat_interrupt_trigger = true;
            }
            if (previous_mode == PixelProcessingUnitMode::VerticalBlank && (stat_lcd_status & 0b00110000) != 0)
            {
                should_stat_interrupt_trigger = true;
            }
        }
        is_stat_interrupt_pending = should_stat_interrupt_trigger && !are_stat_interrupts_blocked;
        are_stat_interrupts_blocked = should_stat_interrupt_trigger;
    }
    set_bit(stat_lcd_status, 2, is_ly_equal_to_lyc);
}

void PixelProcessingUnit::switch_to_mode(PixelProcessingUnitMode new_mode)
{
    switch (new_mode)
    {
        case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
            scanline_selected_objects.clear();
            current_object_index = 0;
            is_in_first_dot_of_current_step = true;
            is_oam_blocked = true;
            break;
        case PixelProcessingUnitMode::PixelTransfer:
            is_vram_blocked = true;
            is_oam_blocked = true;
            if (!was_wy_condition_triggered_this_frame)
            {
                was_wy_condition_triggered_this_frame = (wy_window_y_position == ly_lcd_y_coordinate);
            }
            is_window_enabled_for_scanline = is_bit_set(lcdc_lcd_control, 5);

            scanline_pixels_to_discard_from_dummy_fetch_count = 8;
            scanline_pixels_to_discard_from_scrolling_count = -1;

            lx_internal_lcd_x_coordinate_plus_8 = 0;
            background_fetcher.reset_state();
            object_fetcher.reset_state();
            background_pixel_shift_register.clear();
            object_pixel_shift_register.clear();
            break;
        case PixelProcessingUnitMode::HorizontalBlank:
            break;
        case PixelProcessingUnitMode::VerticalBlank:
            if (is_in_frame_after_lcd_enable)
            {
                is_in_frame_after_lcd_enable = false;
                std::fill_n(pixel_frame_buffers[in_progress_frame_index].get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);
            }
            publish_new_frame();
            request_interrupt_callback(INTERRUPT_FLAG_VERTICAL_BLANK_MASK);
            break;
    }
    current_mode = new_mode;
}

void PixelProcessingUnit::step_fetchers_forward_one_dot()
{
    if (background_fetcher.is_enabled)
    {
        step_background_fetcher_forward_one_dot();
    }
    else
    {
        step_object_fetcher_forward_one_dot();
    }

    if (is_object_display_enabled())
    {
        if (!object_fetcher.is_enabled)
        {
            object_fetcher.is_enabled = is_next_object_hit();
        }
        if (object_fetcher.is_enabled &&
            background_fetcher.is_enabled &&
            background_fetcher.current_step == PixelSliceFetcherStep::PushPixels &&
            !background_pixel_shift_register.is_empty())
        {
            background_fetcher.is_enabled = false;
        }
    }
    else
    {
        object_fetcher.is_enabled = false;
    }
}

void PixelProcessingUnit::step_background_fetcher_forward_one_dot()
{
    if (background_fetcher.current_step == PixelSliceFetcherStep::PushPixels && background_pixel_shift_register.is_empty())
    {
        background_pixel_shift_register.load_new_tile_row(background_fetcher.tile_row);
        background_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
    }

    switch (background_fetcher.current_step)
    {
        case PixelSliceFetcherStep::GetTileId:
            if (!background_fetcher.is_in_first_dot_of_current_step)
            {
                background_fetcher.tile_index = get_background_fetcher_tile_id();
                background_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
            }
            background_fetcher.is_in_first_dot_of_current_step = !background_fetcher.is_in_first_dot_of_current_step;
            break;
        case PixelSliceFetcherStep::GetTileRowLow:
            if (!background_fetcher.is_in_first_dot_of_current_step)
            {
                background_fetcher.tile_row_low = get_background_fetcher_tile_row_byte(0);
                background_fetcher.current_step = PixelSliceFetcherStep::GetTileRowHigh;
            }
            background_fetcher.is_in_first_dot_of_current_step = !background_fetcher.is_in_first_dot_of_current_step;
            break;
        case PixelSliceFetcherStep::GetTileRowHigh:
            if (!background_fetcher.is_in_first_dot_of_current_step)
            {
                background_fetcher.tile_row_high = get_background_fetcher_tile_row_byte(1);

                for (int i = 0; i < PIXELS_PER_TILE_ROW; i++)
                {
                    background_fetcher.tile_row[i] = BackgroundPixel{get_pixel_colour_id(background_fetcher, PIXELS_PER_TILE_ROW - 1 - i)};
                }
                background_fetcher.current_step = PixelSliceFetcherStep::PushPixels;
            }
            background_fetcher.is_in_first_dot_of_current_step = !background_fetcher.is_in_first_dot_of_current_step;
            break;
    }
}

uint8_t PixelProcessingUnit::get_background_fetcher_tile_id() const
{
    uint16_t tile_id_address = static_cast<uint16_t>(0b10011 << 11);
    uint16_t tile_map_area_bit = 0x0000;

    switch (background_fetcher.fetcher_mode)
    {
        case FetcherMode::BackgroundMode:
            tile_map_area_bit = is_bit_set(lcdc_lcd_control, 3) ? (1 << 10) : 0;
            tile_id_address |= (static_cast<uint8_t>(ly_lcd_y_coordinate + scy_viewport_y_position) >> 3) << 5;
            tile_id_address |= static_cast<uint8_t>(lx_internal_lcd_x_coordinate_plus_8 + scx_viewport_x_position) >> 3;
            break;
        case FetcherMode::WindowMode:
            tile_map_area_bit = is_bit_set(lcdc_lcd_control, 6) ? (1 << 10) : 0;
            tile_id_address |= (wlc_internal_window_line_counter >> 3) << 5;
            tile_id_address |= (background_fetcher.fetcher_x >> 3);
            break;
    }
    tile_id_address |= tile_map_area_bit;
    const uint16_t local_address = tile_id_address - VIDEO_RAM_START;
    return video_ram[local_address];
}

uint8_t PixelProcessingUnit::get_background_fetcher_tile_row_byte(uint8_t offset) const
{
    uint16_t tile_row_address = static_cast<uint16_t>((1 << 15)) | (background_fetcher.tile_index << 4) | offset;

    if (!is_bit_set(lcdc_lcd_control, 4) && !is_bit_set(background_fetcher.tile_index, 7))
    {
        tile_row_address |= (1 << 12);
    }
    tile_row_address |= ((background_fetcher.fetcher_mode == FetcherMode::BackgroundMode
        ? (ly_lcd_y_coordinate + scy_viewport_y_position)
        : wlc_internal_window_line_counter) << 1) & 0b1110;

    const uint16_t local_address = tile_row_address - VIDEO_RAM_START;
    return video_ram[local_address];
}

void PixelProcessingUnit::step_object_fetcher_forward_one_dot()
{
    switch (object_fetcher.current_step)
    {
        case PixelSliceFetcherStep::GetTileId:
            if (!object_fetcher.is_in_first_dot_of_current_step)
            {
                const bool is_access_unrestricted = true;
                get_current_object().tile_index = read_byte_object_attribute_memory(get_current_object().object_start_global_address + 2, is_access_unrestricted);
                get_current_object().flags = read_byte_object_attribute_memory(get_current_object().object_start_global_address + 3, is_access_unrestricted);

                const bool is_object_double_height = is_bit_set(lcdc_lcd_control, 2);
                if (is_object_double_height)
                {
                    const bool is_flipped_vertically = is_bit_set(get_current_object().flags, 6);

                    set_bit(
                        get_current_object().tile_index,
                        0, 
                        ly_lcd_y_coordinate < get_current_object().y_position - 8 == is_flipped_vertically);
                }
                object_fetcher.tile_index = get_current_object().tile_index;
                object_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
            }
            object_fetcher.is_in_first_dot_of_current_step = !object_fetcher.is_in_first_dot_of_current_step;
            break;
        case PixelSliceFetcherStep::GetTileRowLow:
            if (!object_fetcher.is_in_first_dot_of_current_step)
            {
                object_fetcher.tile_row_low = get_object_fetcher_tile_row_byte(0);
                object_fetcher.current_step = PixelSliceFetcherStep::GetTileRowHigh;
            }
            object_fetcher.is_in_first_dot_of_current_step = !object_fetcher.is_in_first_dot_of_current_step;
            break;
        case PixelSliceFetcherStep::GetTileRowHigh:
            if (!object_fetcher.is_in_first_dot_of_current_step)
            {
                object_fetcher.tile_row_high = get_object_fetcher_tile_row_byte(1);
                object_fetcher.current_step = PixelSliceFetcherStep::PushPixels;
            }
            object_fetcher.is_in_first_dot_of_current_step = !object_fetcher.is_in_first_dot_of_current_step;
            break;
    }

    if (object_fetcher.current_step == PixelSliceFetcherStep::PushPixels)
    {
        for (uint8_t i = 0; i < PIXELS_PER_TILE_ROW; i++)
        {
            if (object_pixel_shift_register[i].colour_index == 0b00)
            {
                object_pixel_shift_register[i].colour_index = get_pixel_colour_id(object_fetcher, PIXELS_PER_TILE_ROW - 1 - i);
                object_pixel_shift_register[i].is_priority_bit_set = is_bit_set(get_current_object().flags, 7);
                object_pixel_shift_register[i].is_palette_bit_set = is_bit_set(get_current_object().flags, 4);
            }
        }
        current_object_index++;

        if (!(is_object_display_enabled() && is_next_object_hit()))
        {
            background_fetcher.is_enabled = true;
            object_fetcher.is_enabled = false;
        }
        object_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
    }
}

uint8_t PixelProcessingUnit::get_object_fetcher_tile_row_byte(uint8_t offset)
{
    const bool is_flipped_vertically = is_bit_set(get_current_object().flags, 6);
    const uint8_t tile_row_address_bits_1_to_3 = ly_lcd_y_coordinate - get_current_object().y_position;

    uint16_t tile_row_address = static_cast<uint16_t>((1 << 15) | (object_fetcher.tile_index << 4) | offset);
    tile_row_address |= ((is_flipped_vertically
        ? ~tile_row_address_bits_1_to_3
        : tile_row_address_bits_1_to_3) << 1) & 0b1110;

    const uint16_t local_address = tile_row_address - VIDEO_RAM_START;
    const uint8_t tile_row_byte = video_ram[local_address];

    return is_bit_set(get_current_object().flags, 5)
        ? get_byte_horizontally_flipped(tile_row_byte)
        : tile_row_byte;
}

void PixelProcessingUnit::publish_new_frame()
{
    published_frame_index_atomic.store(in_progress_frame_index, std::memory_order_release);
    published_frame_sequence_number_atomic.fetch_add(1, std::memory_order_release);
    in_progress_frame_index = 1 - in_progress_frame_index;
}

bool PixelProcessingUnit::is_object_display_enabled() const
{
    return is_bit_set(lcdc_lcd_control, 1);
}

bool PixelProcessingUnit::is_next_object_hit() const
{
    return current_object_index < scanline_selected_objects.size() &&
           scanline_selected_objects[current_object_index].x_position == lx_internal_lcd_x_coordinate_plus_8;
}

ObjectAttributes& PixelProcessingUnit::get_current_object()
{
    return scanline_selected_objects[current_object_index];
}

uint8_t PixelProcessingUnit::get_pixel_colour_id(PixelSliceFetcher pixel_slice_fetcher, uint8_t bit_position) const
{
    const uint8_t low_bit = (pixel_slice_fetcher.tile_row_low >> bit_position) & 1;
    const uint8_t high_bit = (pixel_slice_fetcher.tile_row_high >> bit_position) & 1;
    return (high_bit << 1) | low_bit;
}

} // namespace GameBoyEmulator
