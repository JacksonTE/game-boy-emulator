#include <algorithm>
#include <cstdint>
#include <memory>

#include "bitwise_utilities.h"
#include "pixel_processing_unit.h"

namespace GameBoyCore
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
    : request_interrupt_callback{ request_interrupt }
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

    viewport_y_position_scy = 0;
    viewport_x_position_scx = 0;
    lcd_y_coordinate_compare_lyc = 0;
    object_attribute_memory_direct_memory_access_dma = 0;
    background_palette_bgp = 0;
    object_palette_0_obp0 = 0;
    object_palette_1_obp1 = 0;
    window_y_position_wy = 0;
    window_x_position_plus_7_wx = 0;
    is_oam_dma_in_progress = false;

    lcd_control_lcdc = 0;
    lcd_status_stat = 0b10000000;
    lcd_y_coordinate_ly = 0;
    internal_lcd_x_coordinate_plus_8_lx = 0;
    internal_window_line_counter_wlc = 0;

    previous_mode = PixelProcessingUnitMode::HorizontalBlank;
    current_mode = PixelProcessingUnitMode::HorizontalBlank;
    current_scanline_dot_number = 0;
    is_in_first_scanline_after_lcd_enable = false;
    is_in_first_dot_of_current_step = true;
    is_window_enabled_for_scanline = false;

    stat_value_after_spurious_interrupt = 0;
    did_spurious_stat_interrupt_occur = false;
    are_stat_interrupts_blocked = false;
    did_scan_line_end_during_this_machine_cycle = false;

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
    background_palette_bgp = 0xfc;

    previous_mode = PixelProcessingUnitMode::VerticalBlank;
    current_mode = PixelProcessingUnitMode::VerticalBlank;
    current_scanline_dot_number = 0x018c;

    lcd_control_lcdc = 0x91;
    lcd_status_stat = 0x85;
    object_attribute_memory_direct_memory_access_dma = 0xff;
    background_palette_bgp = 0xfc;
}

uint8_t PixelProcessingUnit::get_published_frame_buffer_index() const
{
    return published_frame_index.load(std::memory_order_acquire);
}

std::unique_ptr<uint8_t[]> &PixelProcessingUnit::get_pixel_frame_buffer(uint8_t index)
{
    return pixel_frame_buffers[index];
}

uint8_t PixelProcessingUnit::read_lcd_control_lcdc() const
{
    return lcd_control_lcdc;
}

void PixelProcessingUnit::write_lcd_control_lcdc(uint8_t value)
{
    const bool was_lcd_enable_bit_previously_set = is_bit_set(lcd_control_lcdc, 7);
    const bool will_lcd_enable_bit_be_set = is_bit_set(value, 7);

    if (will_lcd_enable_bit_be_set && !was_lcd_enable_bit_previously_set)
    {
        is_in_frame_after_lcd_enable = true;
        is_in_first_scanline_after_lcd_enable = true;
        is_in_first_dot_of_current_step = true;
        current_scanline_dot_number = 0;
    }
    else if (!will_lcd_enable_bit_be_set && was_lcd_enable_bit_previously_set)
    {
        std::fill_n(pixel_frame_buffers[in_progress_frame_index].get(), static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * DISPLAY_HEIGHT_PIXELS), 0);
        publish_new_frame();

        lcd_y_coordinate_ly = 0;
        internal_window_line_counter_wlc = 0;
        switch_to_mode(PixelProcessingUnitMode::HorizontalBlank);
    }
    lcd_control_lcdc = value;
}

uint8_t PixelProcessingUnit::read_lcd_status_stat() const
{
    return (lcd_status_stat & 0b11111100) | static_cast<uint8_t>(previous_mode);
}

void PixelProcessingUnit::write_lcd_status_stat(uint8_t value)
{
    const uint8_t new_stat_value = (value & 0b01111000) | (lcd_status_stat & 0b10000111);

    const bool is_ly_equal_to_lyc_flag_set = is_bit_set(lcd_status_stat, 2);
    if (previous_mode != PixelProcessingUnitMode::PixelTransfer || is_ly_equal_to_lyc_flag_set)
    {
        stat_value_after_spurious_interrupt = new_stat_value;
        did_spurious_stat_interrupt_occur = true;
        lcd_status_stat = 0xff; // TODO eventually confirm spurious interrupt timing with Ocean’s Road Rash and Vic Tokai’s Xerd no Densetsu
    }
    else
        lcd_status_stat = new_stat_value;
}

uint8_t PixelProcessingUnit::read_lcd_y_coordinate_ly() const
{
    return lcd_y_coordinate_ly;
}

uint8_t PixelProcessingUnit::read_byte_video_ram(uint16_t memory_address) const
{
    const bool is_lcd_enable_bit_set = is_bit_set(lcd_control_lcdc, 7);

    if (is_lcd_enable_bit_set &&
        (previous_mode == PixelProcessingUnitMode::PixelTransfer ||
         (previous_mode != PixelProcessingUnitMode::HorizontalBlank && current_mode == PixelProcessingUnitMode::PixelTransfer)))
    {
        return 0xff;
    }
    const uint16_t local_address = memory_address - VIDEO_RAM_START;
    return video_ram[local_address];
}

void PixelProcessingUnit::write_byte_video_ram(uint16_t memory_address, uint8_t value)
{
    const bool is_lcd_enable_bit_set = is_bit_set(lcd_control_lcdc, 7);

    if (is_lcd_enable_bit_set && previous_mode == PixelProcessingUnitMode::PixelTransfer)
    {
        return;
    }
    const uint16_t local_address = memory_address - VIDEO_RAM_START;
    video_ram[local_address] = value;
}

uint8_t PixelProcessingUnit::read_byte_object_attribute_memory(uint16_t memory_address) const
{
    const bool is_lcd_enable_bit_set = is_bit_set(lcd_control_lcdc, 7);

    if (is_lcd_enable_bit_set &&
        (is_oam_dma_in_progress ||
         (current_mode != PixelProcessingUnitMode::VerticalBlank &&
         (current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan || previous_mode != PixelProcessingUnitMode::HorizontalBlank))))
    {
        return 0xff;
    }
    const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
    return object_attribute_memory[local_address];
}

void PixelProcessingUnit::write_byte_object_attribute_memory(uint16_t memory_address, uint8_t value, bool is_access_for_oam_dma)
{
    const bool is_lcd_enable_bit_set = is_bit_set(lcd_control_lcdc, 7);

    if (is_lcd_enable_bit_set &&
        ((is_oam_dma_in_progress && !is_access_for_oam_dma) ||
         previous_mode == PixelProcessingUnitMode::PixelTransfer ||
         (previous_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan && current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan)))
    {
        return;
    }
    const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
    object_attribute_memory[local_address] = value;
}

void PixelProcessingUnit::step_single_machine_cycle()
{
    previous_mode = current_mode;

    const bool is_lcd_enable_bit_set = is_bit_set(lcd_control_lcdc, 7);
    if (!is_lcd_enable_bit_set)
        return;

    were_stat_interrupts_handled_early = false;

    for (uint8_t i = 0; i < DOTS_PER_MACHINE_CYCLE; i++)
    {
        current_scanline_dot_number++;

        switch (current_mode)
        {
            case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
                step_object_attribute_memory_scan_single_dot();
                break;
            case PixelProcessingUnitMode::PixelTransfer:
                step_pixel_transfer_single_dot();

                if (current_mode == PixelProcessingUnitMode::HorizontalBlank && i < DOTS_PER_MACHINE_CYCLE - 1)
                {
                    trigger_stat_interrupts();
                    were_stat_interrupts_handled_early = true;
                    previous_mode = PixelProcessingUnitMode::HorizontalBlank;
                }
                break;
            case PixelProcessingUnitMode::HorizontalBlank:
                step_horizontal_blank_single_dot();
                break;
            case PixelProcessingUnitMode::VerticalBlank:
                step_vertical_blank_single_dot();
                break;
        }
    }
    if (!were_stat_interrupts_handled_early)
        trigger_stat_interrupts();

    if (did_spurious_stat_interrupt_occur)
    {
        lcd_status_stat = stat_value_after_spurious_interrupt;
        did_spurious_stat_interrupt_occur = false;
        stat_value_after_spurious_interrupt = 0;
    }
}

void PixelProcessingUnit::step_object_attribute_memory_scan_single_dot()
{
    if (is_in_first_dot_of_current_step)
    {
        is_in_first_dot_of_current_step = false;
        return;
    }

    if (scanline_selected_objects.size() < MAX_OBJECTS_PER_LINE)
    {
        const uint8_t object_start_local_address = (2 * current_scanline_dot_number) - 4;
        ObjectAttributes current_object
        {
            object_start_local_address,
            read_byte_object_attribute_memory_internally(object_start_local_address),
            read_byte_object_attribute_memory_internally(object_start_local_address + 1),
            read_byte_object_attribute_memory_internally(object_start_local_address + 2),
            read_byte_object_attribute_memory_internally(object_start_local_address + 3),
        };

        const uint8_t object_height = is_bit_set(lcd_control_lcdc, 2) ? 16 : 8;
        const bool does_object_intersect_with_scanline = lcd_y_coordinate_ly >= (current_object.y_position - 16) &&
                                                         lcd_y_coordinate_ly < (current_object.y_position - 16) + object_height;
        if (does_object_intersect_with_scanline)
        {
            scanline_selected_objects.push_back(current_object);
        }
    }
    is_in_first_dot_of_current_step = true;

    if (current_scanline_dot_number == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS)
    {
        std::stable_sort(scanline_selected_objects.begin(), scanline_selected_objects.end(), 
            [](const ObjectAttributes &a, const ObjectAttributes &b)
            {
                return a.x_position < b.x_position; 
            });
        switch_to_mode(PixelProcessingUnitMode::PixelTransfer);
    }
}

void PixelProcessingUnit::step_pixel_transfer_single_dot()
{
    const uint8_t dot_number_for_dummy_push = (is_in_first_scanline_after_lcd_enable
        ? FIRST_HORIZONTAL_BLANK_AFTER_LCD_ENABLE_DURATION_DOTS
        : OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS) + 5;

    if (current_scanline_dot_number < dot_number_for_dummy_push)
    {
        return;
    }
    else if (current_scanline_dot_number == dot_number_for_dummy_push)
    {
        background_pixel_shift_register.load_new_tile_row(background_fetcher.tile_row);
    }

    if (background_fetcher.is_enabled &&
        background_fetcher.fetcher_mode == FetcherMode::BackgroundMode &&
        is_window_enabled_for_scanline && was_wy_condition_triggered_this_frame &&
        static_cast<int8_t>(internal_lcd_x_coordinate_plus_8_lx) - 1 == window_x_position_plus_7_wx)
    {
        background_pixel_shift_register.clear();
        background_fetcher.reset_state();
        background_fetcher.fetcher_mode = FetcherMode::WindowMode;
    }
    if (background_fetcher.is_enabled &&
        background_fetcher.fetcher_mode == FetcherMode::WindowMode &&
        background_fetcher.fetcher_x <= 7)
    {
        background_fetcher.fetcher_x++;
    }

    step_fetchers_single_dot(); 

    const bool should_draw_or_discard_pixels = !object_fetcher.is_enabled && !background_pixel_shift_register.is_empty();
    if (should_draw_or_discard_pixels)
    {
        const BackgroundPixel next_background_pixel = background_pixel_shift_register.shift_out();
        const ObjectPixel next_object_pixel = object_pixel_shift_register.shift_out();

        if (scanline_pixels_to_discard_from_dummy_fetch_count > 0)
        {
            internal_lcd_x_coordinate_plus_8_lx++;
            scanline_pixels_to_discard_from_dummy_fetch_count--;
            return;
        }
        if (scanline_pixels_to_discard_from_scrolling_count == -1)
        {
            scanline_pixels_to_discard_from_scrolling_count = (background_fetcher.fetcher_mode == FetcherMode::BackgroundMode || window_x_position_plus_7_wx == 0)
                ? viewport_x_position_scx % 8
                : 0;
        }
        if (scanline_pixels_to_discard_from_scrolling_count > 0)
        {
            scanline_pixels_to_discard_from_scrolling_count--;
            return;
        }

        const bool are_background_and_window_enabled = is_bit_set(lcd_control_lcdc, 0);
        uint8_t pixel_with_palette_applied = 0;

        if (are_background_and_window_enabled &&
            (!is_object_display_enabled() ||
             (next_object_pixel.is_priority_bit_set && next_background_pixel.colour_index != 0) ||
             next_object_pixel.colour_index == 0))
        {
            const uint8_t palette_colour_position = next_background_pixel.colour_index << 1;
            pixel_with_palette_applied = (background_palette_bgp & (0b11 << palette_colour_position)) >> palette_colour_position;
        }
        else
        {
            const uint8_t palette_colour_position = next_object_pixel.colour_index << 1;
            const uint8_t palette = next_object_pixel.is_palette_bit_set
                ? object_palette_1_obp1
                : object_palette_0_obp0;
            pixel_with_palette_applied = (palette & (0b11 << palette_colour_position)) >> palette_colour_position;
        }
        const uint16_t pixel_address = static_cast<uint16_t>(DISPLAY_WIDTH_PIXELS * lcd_y_coordinate_ly) + (internal_lcd_x_coordinate_plus_8_lx - 8);
        pixel_frame_buffers[in_progress_frame_index][pixel_address] = pixel_with_palette_applied;

        background_fetcher.fetcher_x++;
        if (++internal_lcd_x_coordinate_plus_8_lx == 168)
        {
            switch_to_mode(PixelProcessingUnitMode::HorizontalBlank);
        }
    }
}

void PixelProcessingUnit::step_horizontal_blank_single_dot()
{
    if (current_scanline_dot_number < SCANLINE_DURATION_DOTS &&
        !(is_in_first_scanline_after_lcd_enable && current_scanline_dot_number == FIRST_SCANLINE_AFTER_LCD_ENABLE_DURATION_DOTS))
    {
        if (is_in_first_scanline_after_lcd_enable && 
            current_scanline_dot_number == FIRST_HORIZONTAL_BLANK_AFTER_LCD_ENABLE_DURATION_DOTS)
        {
            switch_to_mode(PixelProcessingUnitMode::PixelTransfer);
        }
        return;
    }

    if (background_fetcher.fetcher_mode == FetcherMode::WindowMode)
    {
        internal_window_line_counter_wlc++;
    }
    lcd_y_coordinate_ly++;
    current_scanline_dot_number = 0;
    did_scan_line_end_during_this_machine_cycle = true;

    if (lcd_y_coordinate_ly == FIRST_SCANLINE_OF_VERTICAL_BLANK)
    {
        switch_to_mode(PixelProcessingUnitMode::VerticalBlank);
    }
    else
    {
        is_in_first_scanline_after_lcd_enable = false;
        switch_to_mode(PixelProcessingUnitMode::ObjectAttributeMemoryScan);
    }
}

void PixelProcessingUnit::step_vertical_blank_single_dot()
{
    if (lcd_y_coordinate_ly == FINAL_SCANLINE_OF_FRAME && current_scanline_dot_number == FINAL_SCANLINE_EARLY_LY_RESET_DOT_NUMBER)
    {
        lcd_y_coordinate_ly = 0;
        internal_window_line_counter_wlc = 0;
        did_scan_line_end_during_this_machine_cycle = true;
    }
    if (current_scanline_dot_number < SCANLINE_DURATION_DOTS)
        return;

    if (lcd_y_coordinate_ly == 0)
    {
        was_wy_condition_triggered_this_frame = false;
        switch_to_mode(PixelProcessingUnitMode::ObjectAttributeMemoryScan);
    }
    else
    {
        lcd_y_coordinate_ly++;
        did_scan_line_end_during_this_machine_cycle = true;
    }
    current_scanline_dot_number = 0;
}

void PixelProcessingUnit::trigger_stat_interrupts()
{
    bool should_stat_interrupt_trigger = false;

    if (did_scan_line_end_during_this_machine_cycle)
    {
        set_bit(lcd_status_stat, 2, false);
        did_scan_line_end_during_this_machine_cycle = false;
    }
    else
    {
        const bool is_lyc_interrupt_select_enabled = is_bit_set(lcd_status_stat, 6);
        const bool is_ly_equal_to_lyc = (lcd_y_coordinate_ly == lcd_y_coordinate_compare_lyc);
        set_bit(lcd_status_stat, 2, is_ly_equal_to_lyc);
        should_stat_interrupt_trigger = (is_ly_equal_to_lyc && is_lyc_interrupt_select_enabled);
    }
    const bool is_object_attribute_memory_scan_interrupt_select_enabled = is_bit_set(lcd_status_stat, 5);
    const bool is_vertical_blank_interrupt_select_enabled = is_bit_set(lcd_status_stat, 4);
    const bool is_horizontal_blank_interrupt_select_enabled = is_bit_set(lcd_status_stat, 3);

    switch (previous_mode)
    {
        case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
            should_stat_interrupt_trigger |= is_object_attribute_memory_scan_interrupt_select_enabled;
            break;
        case PixelProcessingUnitMode::HorizontalBlank:
            should_stat_interrupt_trigger |= is_horizontal_blank_interrupt_select_enabled;
            break;
        case PixelProcessingUnitMode::VerticalBlank:
            should_stat_interrupt_trigger |= is_vertical_blank_interrupt_select_enabled;
            break;
    }
    should_stat_interrupt_trigger |= (is_object_attribute_memory_scan_interrupt_select_enabled &&
                                      lcd_y_coordinate_ly == FIRST_SCANLINE_OF_VERTICAL_BLANK);

    if (should_stat_interrupt_trigger)
    {
        if (!are_stat_interrupts_blocked)
        {
            are_stat_interrupts_blocked = true;
            request_interrupt_callback(INTERRUPT_FLAG_STAT_MASK);
        }
    }
    else
        are_stat_interrupts_blocked = false;
}

void PixelProcessingUnit::switch_to_mode(PixelProcessingUnitMode new_mode)
{
    switch (new_mode)
    {
        case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
            scanline_selected_objects.clear();
            current_object_index = 0;
            break;
        case PixelProcessingUnitMode::PixelTransfer:
            if (!was_wy_condition_triggered_this_frame)
            {
                was_wy_condition_triggered_this_frame = (window_y_position_wy == lcd_y_coordinate_ly);
            }
            is_window_enabled_for_scanline = is_bit_set(lcd_control_lcdc, 5);

            scanline_pixels_to_discard_from_dummy_fetch_count = 8;
            scanline_pixels_to_discard_from_scrolling_count = -1;

            internal_lcd_x_coordinate_plus_8_lx = 0;
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

void PixelProcessingUnit::step_fetchers_single_dot()
{
    if (background_fetcher.is_enabled)
        step_background_fetcher_single_dot();
    else
        step_object_fetcher_single_dot();

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
}

void PixelProcessingUnit::step_background_fetcher_single_dot()
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
            tile_map_area_bit = is_bit_set(lcd_control_lcdc, 3) ? (1 << 10) : 0;
            tile_id_address |= (static_cast<uint8_t>(lcd_y_coordinate_ly + viewport_y_position_scy) >> 3) << 5;
            tile_id_address |= static_cast<uint8_t>(internal_lcd_x_coordinate_plus_8_lx + viewport_x_position_scx) >> 3;
            break;
        case FetcherMode::WindowMode:
            tile_map_area_bit = is_bit_set(lcd_control_lcdc, 6) ? (1 << 10) : 0;
            tile_id_address |= (internal_window_line_counter_wlc >> 3) << 5;
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

    if (!is_bit_set(lcd_control_lcdc, 4) && !is_bit_set(background_fetcher.tile_index, 7))
    {
        tile_row_address |= (1 << 12);
    }
    tile_row_address |= ((background_fetcher.fetcher_mode == FetcherMode::BackgroundMode
        ? (lcd_y_coordinate_ly + viewport_y_position_scy)
        : internal_window_line_counter_wlc) << 1) & 0b1110;

    const uint16_t local_address = tile_row_address - VIDEO_RAM_START;
    return video_ram[local_address];
}

void PixelProcessingUnit::step_object_fetcher_single_dot()
{
    switch (object_fetcher.current_step)
    {
        case PixelSliceFetcherStep::GetTileId:
            if (!object_fetcher.is_in_first_dot_of_current_step)
            {
                const uint8_t object_starting_index = get_current_object().object_attribute_memory_starting_index;
                get_current_object().tile_index = read_byte_object_attribute_memory_internally(object_starting_index + 2);
                get_current_object().flags = read_byte_object_attribute_memory_internally(object_starting_index + 3);

                const bool is_object_double_height = is_bit_set(lcd_control_lcdc, 2);
                if (is_object_double_height)
                {
                    const bool is_flipped_vertically = is_bit_set(get_current_object().flags, 6);

                    set_bit(get_current_object().tile_index, 0, 
                            lcd_y_coordinate_ly < get_current_object().y_position - 8 == is_flipped_vertically);
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
    const uint8_t tile_row_address_bits_1_to_3 = lcd_y_coordinate_ly - get_current_object().y_position;

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

uint8_t PixelProcessingUnit::read_byte_object_attribute_memory_internally(uint16_t local_address) const
{
    if (is_oam_dma_in_progress)
    {
        return 0xff;
    }
    return object_attribute_memory[local_address];
}

void PixelProcessingUnit::publish_new_frame()
{
    published_frame_index.store(in_progress_frame_index, std::memory_order_release);
    in_progress_frame_index = 1 - in_progress_frame_index;
}

bool PixelProcessingUnit::is_object_display_enabled() const
{
    return is_bit_set(lcd_control_lcdc, 1);
}

bool PixelProcessingUnit::is_next_object_hit() const
{
    return current_object_index < scanline_selected_objects.size() &&
           scanline_selected_objects[current_object_index].x_position == internal_lcd_x_coordinate_plus_8_lx;
}

ObjectAttributes &PixelProcessingUnit::get_current_object()
{
    return scanline_selected_objects[current_object_index];
}

uint8_t PixelProcessingUnit::get_pixel_colour_id(PixelSliceFetcher pixel_slice_fetcher, uint8_t bit_position) const
{
    const uint8_t low_bit = (pixel_slice_fetcher.tile_row_low >> bit_position) & 1;
    const uint8_t high_bit = (pixel_slice_fetcher.tile_row_high >> bit_position) & 1;
    return (high_bit << 1) | low_bit;
}

} // namespace GameBoyCore
