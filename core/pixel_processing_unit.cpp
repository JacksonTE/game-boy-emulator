#include <algorithm>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "bitwise_utilities.h"
#include "pixel_processing_unit.h"

namespace GameBoy
{

void PixelSliceFetcher::reset_state()
{
    current_step = PixelSliceFetcherStep::GetTileId;
    tile_id = 0;
    tile_row_address = 0;
    tile_row_low = 0;
    tile_row_high = 0;
    is_in_first_dot_of_current_step = true;
}

ObjectAttributes ObjectFetcher::get_current_object()
{
    return current_scanline_selected_objects[current_object_index];
}

void ObjectFetcher::reset_state()
{
    PixelSliceFetcher::reset_state();
    is_enabled = false;
    current_scanline_selected_objects.clear();
    current_object_index = 0;
}

void BackgroundFetcher::reset_state()
{
    PixelSliceFetcher::reset_state();
    is_enabled = true;
    fetcher_mode = FetcherMode::BackgroundMode;
    is_on_first_fetch_of_scanline = true;
    current_scanline_pixels_to_discard_count = -1;
    tile_id_address = 0;
    tile_row_scy_value = 0;
    tile_row.fill(BackgroundPixel{});
}

PixelProcessingUnit::PixelProcessingUnit(std::function<void(uint8_t)> request_interrupt)
    : request_interrupt_callback{request_interrupt}
{
    video_ram = std::make_unique<uint8_t[]>(VIDEO_RAM_SIZE);
    std::fill_n(video_ram.get(), VIDEO_RAM_SIZE, 0);

    object_attribute_memory = std::make_unique<uint8_t[]>(OBJECT_ATTRIBUTE_MEMORY_SIZE);
    std::fill_n(video_ram.get(), OBJECT_ATTRIBUTE_MEMORY_SIZE, 0);

    object_fetcher.reset_state();
    background_fetcher.reset_state();
}

uint8_t PixelProcessingUnit::read_lcd_control_lcdc() const
{
    return lcd_control_lcdc;
}

void PixelProcessingUnit::write_lcd_control_lcdc(uint8_t value)
{
    const bool is_lcd_enable_bit_previously_set = is_bit_set(lcd_control_lcdc, 7);
    const bool will_lcd_enable_bit_be_set = is_bit_set(value, 7);

    if (will_lcd_enable_bit_be_set && !is_lcd_enable_bit_previously_set)
    {
        is_in_first_scanline_after_enable = true;
        is_in_first_dot_of_current_step = true;
        current_scanline_dot_number = 0;
        trigger_lcd_status_stat_interrupts();
    }
    else if (!will_lcd_enable_bit_be_set && is_lcd_enable_bit_previously_set)
    {
        lcd_y_coordinate_ly = 0;
        window_internal_line_counter_wly = 0;
        object_fetcher.reset_state();
        current_mode = PixelProcessingUnitMode::HorizontalBlank;
    }
    lcd_control_lcdc = value;
}

uint8_t PixelProcessingUnit::read_lcd_status_stat() const
{
    return 0b10000000 | (lcd_status_stat & 0b11111100) | static_cast<uint8_t>(previous_mode);
}

void PixelProcessingUnit::write_lcd_status_stat(uint8_t value)
{
    uint8_t new_stat_value = (value & 0b01111000) | (lcd_status_stat & 0b10000111);

    if (current_mode == PixelProcessingUnitMode::VerticalBlank || lcd_y_coordinate_ly == lcd_y_coordinate_compare_lyc)
    {
        stat_value_after_spurious_interrupt = new_stat_value;
        did_spurious_stat_interrupt_occur = true;
        lcd_status_stat = 0xff;
    }
    else
        lcd_status_stat = new_stat_value;
}

void PixelProcessingUnit::reset_state()
{
    std::fill_n(video_ram.get(), VIDEO_RAM_SIZE, 0);
    std::fill_n(video_ram.get(), OBJECT_ATTRIBUTE_MEMORY_SIZE, 0);

    lcd_control_lcdc = 0;
    lcd_status_stat = 0b10000000;
    lcd_y_coordinate_ly = 0;
    lcd_internal_x_coordinate_lx = 0;
    window_internal_line_counter_wly = 0;

    previous_mode = PixelProcessingUnitMode::HorizontalBlank;
    current_mode = PixelProcessingUnitMode::HorizontalBlank;
    current_scanline_dot_number = 0;
    stat_value_after_spurious_interrupt = 0;
    is_in_first_scanline_after_enable = false;
    is_in_first_dot_of_current_step = true;
    is_window_enabled_for_scanline = false;
    did_horizontal_blank_end_last_machine_cycle = false;
    are_stat_interrupts_blocked = false;
    did_spurious_stat_interrupt_occur = false;

    background_pixel_queue.clear();
    object_pixel_queue.clear();
    object_fetcher.reset_state();
    background_fetcher.reset_state();
}

void PixelProcessingUnit::set_post_boot_state()
{
    reset_state(); // TODO confirm what mode it should be in
    lcd_control_lcdc = 0x91;
    lcd_status_stat = 0x85;
    object_attribute_memory_direct_memory_access_dma = 0xff;
    background_palette_bgp = 0xfc;
}

uint8_t PixelProcessingUnit::read_byte_video_ram(uint16_t memory_address) const
{
    if (previous_mode == PixelProcessingUnitMode::PixelTransfer ||
        (previous_mode != PixelProcessingUnitMode::HorizontalBlank && 
         current_mode == PixelProcessingUnitMode::PixelTransfer))
    {
        return 0xff;
    }
    const uint16_t local_address = memory_address - VIDEO_RAM_START;
    return video_ram[local_address];
}

void PixelProcessingUnit::write_byte_video_ram(uint16_t memory_address, uint8_t value)
{
    if (previous_mode == PixelProcessingUnitMode::PixelTransfer)
        return;

    const uint16_t local_address = memory_address - VIDEO_RAM_START;
    video_ram[local_address] = value;
}

uint8_t PixelProcessingUnit::read_byte_object_attribute_memory(uint16_t memory_address) const
{
    if ((previous_mode != PixelProcessingUnitMode::HorizontalBlank || 
         current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan) &&
        current_mode != PixelProcessingUnitMode::VerticalBlank)
    {
        return 0xff;
    }
    const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
    return object_attribute_memory[local_address];
}

void PixelProcessingUnit::write_byte_object_attribute_memory(uint16_t memory_address, uint8_t value)
{
    if (previous_mode == PixelProcessingUnitMode::PixelTransfer ||
        (previous_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan &&
         current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan))
    {
        return;
    }
    const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
    object_attribute_memory[local_address] = value;
}

uint8_t PixelProcessingUnit::read_lcd_y_coordinate_ly() const
{
    return lcd_y_coordinate_ly;
}

void PixelProcessingUnit::step_single_machine_cycle()
{
    previous_mode = current_mode;

    const bool is_lcd_enable_bit_set = is_bit_set(lcd_control_lcdc, 7);
    if (!is_lcd_enable_bit_set)
        return;

    for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE; _++)
    {
        current_scanline_dot_number++;

        switch (current_mode)
        {
            case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
                step_object_attribute_memory_scan_single_dot();
                break;
            case PixelProcessingUnitMode::PixelTransfer:
                step_pixel_transfer_single_dot();
                break;
            case PixelProcessingUnitMode::HorizontalBlank:
                step_horizontal_blank_single_dot();
                break;
            case PixelProcessingUnitMode::VerticalBlank:
                step_vertical_blank_single_dot();
                break;
        }
    }
    trigger_lcd_status_stat_interrupts();

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

    if (object_fetcher.current_scanline_selected_objects.size() < MAX_OBJECTS_PER_LINE)
    {
        const uint8_t object_start_index = (2 * current_scanline_dot_number) - 4;
        ObjectAttributes current_object
        {
            object_attribute_memory[object_start_index],
            object_attribute_memory[object_start_index + 1],
            object_attribute_memory[object_start_index + 2],
            object_attribute_memory[object_start_index + 3],
        };

        const uint8_t object_height = is_bit_set(lcd_control_lcdc, 2) ? 16 : 8;
        const bool does_object_intersect_with_scanline = lcd_y_coordinate_ly >= current_object.y_position &&
                                                         lcd_y_coordinate_ly < current_object.y_position + object_height;
        if (does_object_intersect_with_scanline)
        {
            if (object_height == 16)
            {
                const bool is_flipped_vertically = is_bit_set(current_object.flags, 6);
                set_bit(current_object.tile_index, 0, lcd_y_coordinate_ly < current_object.y_position + 8 != is_flipped_vertically);
            }
            object_fetcher.current_scanline_selected_objects.push_back(current_object);
        }
    }
    is_in_first_dot_of_current_step = true;

    if (current_scanline_dot_number == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS)
    {
        std::stable_sort(object_fetcher.current_scanline_selected_objects.begin(), object_fetcher.current_scanline_selected_objects.end(),
            [](const ObjectAttributes &a, const ObjectAttributes &b) { return a.x_position < b.x_position; });
        
        initialize_pixel_transfer();
        current_mode = PixelProcessingUnitMode::PixelTransfer;
    }
}

void PixelProcessingUnit::step_pixel_transfer_single_dot()
{
    const bool is_object_display_enabled = is_bit_set(lcd_control_lcdc, 1);

    if (is_object_display_enabled && !object_fetcher.is_enabled)
    {
        if (object_fetcher.current_object_index < object_fetcher.current_scanline_selected_objects.size() &&
            object_fetcher.get_current_object().x_position <= lcd_internal_x_coordinate_lx)
        {
            object_fetcher.is_enabled = true;
        }
    }

    if (!object_fetcher.is_enabled)
        step_background_fetcher_single_dot();
    else
        step_object_fetcher_single_dot();

    if (background_fetcher.fetcher_mode == FetcherMode::BackgroundMode && 
        is_window_enabled_for_scanline &&
        lcd_internal_x_coordinate_lx >= window_x_position_plus_7_wx - 7 &&
        window_x_position_plus_7_wx != 0)
    {
        background_pixel_queue.clear();
        background_fetcher.reset_state();
        background_fetcher.fetcher_mode = FetcherMode::WindowMode;
    }

    if (!object_fetcher.is_enabled && !background_pixel_queue.is_empty())
    {
        const BackgroundPixel next_background_pixel = background_pixel_queue.shift_out();

        if (background_fetcher.current_scanline_pixels_to_discard_count == -1)
        {
            if (background_fetcher.fetcher_mode == FetcherMode::BackgroundMode)
                background_fetcher.current_scanline_pixels_to_discard_count = viewport_x_position_scx % 8;
            else
            {
                background_fetcher.current_scanline_pixels_to_discard_count = 0;

                for (int i = 0; i < (window_x_position_plus_7_wx < 7 ? (7 - window_x_position_plus_7_wx - 1) : 0); i++)
                    background_pixel_queue.shift_out();
            }
        }
        if (background_fetcher.current_scanline_pixels_to_discard_count > 0)
        {
            background_fetcher.current_scanline_pixels_to_discard_count--;
            return;
        }

        const ObjectPixel next_object_pixel = object_pixel_queue.shift_out();

        //handle pixel merging/selection, etc

        if (++lcd_internal_x_coordinate_lx == 160)
        {
            current_mode = PixelProcessingUnitMode::HorizontalBlank;
        }
    }
}

void PixelProcessingUnit::step_horizontal_blank_single_dot()
{
    if (is_in_first_scanline_after_enable && current_scanline_dot_number == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS - 4)
    {
        initialize_pixel_transfer();
        current_mode = PixelProcessingUnitMode::PixelTransfer;
    }
    if (current_scanline_dot_number < SCAN_LINE_DURATION_DOTS &&
        !(is_in_first_scanline_after_enable && current_scanline_dot_number == SCAN_LINE_DURATION_DOTS - 4))
    {
        return;
    }

    if (background_fetcher.fetcher_mode == FetcherMode::WindowMode)
    {
        window_internal_line_counter_wly++;
    }
    lcd_y_coordinate_ly++;
    current_scanline_dot_number = 0;
    did_horizontal_blank_end_last_machine_cycle = true;

    if (lcd_y_coordinate_ly - 1 == FINAL_DRAWING_SCAN_LINE)
    {
        request_interrupt_callback(INTERRUPT_FLAG_VBLANK_MASK);
        current_mode = PixelProcessingUnitMode::VerticalBlank;
    }
    else
    {
        if (is_in_first_scanline_after_enable)
            is_in_first_scanline_after_enable = false;

        object_fetcher.reset_state();
        current_mode = PixelProcessingUnitMode::ObjectAttributeMemoryScan;
    }
}

void PixelProcessingUnit::step_vertical_blank_single_dot()
{
    if (lcd_y_coordinate_ly == FINAL_FRAME_SCAN_LINE && current_scanline_dot_number == 4)
    {
        lcd_y_coordinate_ly = 0;
        window_internal_line_counter_wly = 0;
    }

    if (current_scanline_dot_number < SCAN_LINE_DURATION_DOTS)
        return;

    lcd_y_coordinate_ly++;
    current_scanline_dot_number = 0;

    if (lcd_y_coordinate_ly == 1)
    {	
        lcd_y_coordinate_ly = 0;
        object_fetcher.reset_state();
        current_mode = PixelProcessingUnitMode::ObjectAttributeMemoryScan;
    }
}

void PixelProcessingUnit::trigger_lcd_status_stat_interrupts()
{
    bool should_lcd_status_stat_interrupt_trigger = false;

    if (did_horizontal_blank_end_last_machine_cycle)
    {
        set_bit(lcd_status_stat, 2, false);
        did_horizontal_blank_end_last_machine_cycle = false;
    }
    else
    {
        const bool is_lyc_interrupt_select_enabled = is_bit_set(lcd_status_stat, 6);
        const bool lyc_flag = (lcd_y_coordinate_ly == lcd_y_coordinate_compare_lyc);
        set_bit(lcd_status_stat, 2, lyc_flag);
        should_lcd_status_stat_interrupt_trigger = (lyc_flag && is_lyc_interrupt_select_enabled);
    }

    const bool is_object_attribute_memory_scan_interrupt_select_enabled = is_bit_set(lcd_status_stat, 5);
    const bool is_vertical_blank_interrupt_select_enabled = is_bit_set(lcd_status_stat, 4);
    const bool is_horizontal_blank_interrupt_select_enabled = is_bit_set(lcd_status_stat, 3);

    switch (current_mode)
    {
        case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
            should_lcd_status_stat_interrupt_trigger |= is_object_attribute_memory_scan_interrupt_select_enabled;
            break;
        case PixelProcessingUnitMode::HorizontalBlank:
            should_lcd_status_stat_interrupt_trigger |= is_horizontal_blank_interrupt_select_enabled;
            break;
        case PixelProcessingUnitMode::VerticalBlank:
            should_lcd_status_stat_interrupt_trigger |= (is_vertical_blank_interrupt_select_enabled ||
                (is_object_attribute_memory_scan_interrupt_select_enabled && lcd_y_coordinate_ly == FINAL_DRAWING_SCAN_LINE + 1));
            break;
    }

    if (should_lcd_status_stat_interrupt_trigger)
    {
        if (!are_stat_interrupts_blocked)
        {
            are_stat_interrupts_blocked = true;
            request_interrupt_callback(INTERRUPT_FLAG_LCD_STATUS_MASK);
        }
    }
    else
        are_stat_interrupts_blocked = false;
}

void PixelProcessingUnit::initialize_pixel_transfer()
{
    background_pixel_queue.clear();
    object_pixel_queue.clear();
    background_fetcher.reset_state();
    lcd_internal_x_coordinate_lx = 0;
    is_window_enabled_for_scanline = is_bit_set(lcd_control_lcdc, 5);
}

void PixelProcessingUnit::step_background_fetcher_single_dot()
{
    switch (background_fetcher.current_step)
    {
        case PixelSliceFetcherStep::GetTileId:
            if (background_fetcher.is_in_first_dot_of_current_step)
                set_background_fetcher_tile_id_address();
            else
            {
                background_fetcher.tile_id = video_ram[background_fetcher.tile_id_address];
                background_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
            }
            break;
        case PixelSliceFetcherStep::GetTileRowLow:
            if (background_fetcher.is_in_first_dot_of_current_step)
                set_background_fetcher_tile_row_byte_address(0);
            else
            {
                background_fetcher.tile_row_low = video_ram[background_fetcher.tile_row_address];
                background_fetcher.current_step = PixelSliceFetcherStep::GetTileRowHigh;
            }
            break;
        case PixelSliceFetcherStep::GetTileRowHigh:
            if (background_fetcher.is_in_first_dot_of_current_step)
                set_background_fetcher_tile_row_byte_address(1);
            else
            {
                background_fetcher.tile_row_high = video_ram[background_fetcher.tile_row_address];

                for (int i = 0; i < PIXELS_PER_TILE_ROW; i++)
                    background_fetcher.tile_row[i] = BackgroundPixel{get_pixel_colour_id(background_fetcher, i)};

                if (background_fetcher.is_on_first_fetch_of_scanline)
                {
                    background_fetcher.is_on_first_fetch_of_scanline = false;
                    background_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
                }
                else
                    background_fetcher.current_step = PixelSliceFetcherStep::PushPixels;
            }
            break;
        case PixelSliceFetcherStep::PushPixels:
            if (!background_pixel_queue.is_empty())
                break;

            background_pixel_queue.load_new_tile_row(background_fetcher.tile_row);
            background_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
            break;
    }

    if (background_fetcher.current_step != PixelSliceFetcherStep::PushPixels)
        background_fetcher.is_in_first_dot_of_current_step = !background_fetcher.is_in_first_dot_of_current_step;
}

void PixelProcessingUnit::set_background_fetcher_tile_id_address()
{
    background_fetcher.tile_id_address = static_cast<uint16_t>(0b10011 << 11);
    uint16_t tile_map_area_bit = 0x0000;

    switch (background_fetcher.fetcher_mode)
    {
        case FetcherMode::BackgroundMode:
            tile_map_area_bit = is_bit_set(lcd_control_lcdc, 3) ? (1 << 10) : 0;
            background_fetcher.tile_id_address |= tile_map_area_bit;
            background_fetcher.tile_id_address |= static_cast<uint8_t>((lcd_y_coordinate_ly + viewport_y_position_scy) / 8) << 5;
            background_fetcher.tile_id_address |= static_cast<uint8_t>(lcd_internal_x_coordinate_lx + (viewport_x_position_scx / 8)); // TODO FIX THIS FORMULA LATER
            break;
        case FetcherMode::WindowMode:
            tile_map_area_bit = is_bit_set(lcd_control_lcdc, 6) ? (1 << 10) : 0;
            background_fetcher.tile_id_address |= tile_map_area_bit;
            background_fetcher.tile_id_address |= static_cast<uint8_t>(window_internal_line_counter_wly / 8) << 5;
            background_fetcher.tile_id_address |= static_cast<uint8_t>(lcd_internal_x_coordinate_lx / 8);
            break;
    }
    background_fetcher.tile_id_address -= VIDEO_RAM_START;
}

void PixelProcessingUnit::set_background_fetcher_tile_row_byte_address(uint8_t offset) {
    if (offset == 0)
        background_fetcher.tile_row_scy_value = viewport_y_position_scy;

    background_fetcher.tile_row_address = static_cast<uint16_t>((1 << 15) | (background_fetcher.tile_id << 4) | offset);

    if (!is_bit_set(lcd_control_lcdc, 4) && !is_bit_set(background_fetcher.tile_id, 7))
    {
        background_fetcher.tile_row_address |= (1 << 12);
    }
    background_fetcher.tile_row_address |= (background_fetcher.fetcher_mode == FetcherMode::BackgroundMode)
        ? ((lcd_y_coordinate_ly + background_fetcher.tile_row_scy_value) % 8) << 1
        : (window_internal_line_counter_wly % 8) << 1;
    background_fetcher.tile_row_address -= VIDEO_RAM_START;
}

void PixelProcessingUnit::step_object_fetcher_single_dot() {
    switch (object_fetcher.current_step)
    {
        case PixelSliceFetcherStep::GetTileId:
        {
            if (object_fetcher.is_in_first_dot_of_current_step)
                break;

            object_fetcher.tile_id = object_fetcher.get_current_object().tile_index;
            object_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
            break;
        }
        case PixelSliceFetcherStep::GetTileRowLow:
            if (object_fetcher.is_in_first_dot_of_current_step)
                set_object_fetcher_tile_row_address(0);
            else
            {
                object_fetcher.tile_row_low = get_object_fetcher_tile_row();
                object_fetcher.current_step = PixelSliceFetcherStep::GetTileRowHigh;
            }
            break;
        case PixelSliceFetcherStep::GetTileRowHigh:
            if (object_fetcher.is_in_first_dot_of_current_step)
                set_object_fetcher_tile_row_address(1);
            else
            {
                object_fetcher.tile_row_high = get_object_fetcher_tile_row();
                object_fetcher.current_step = PixelSliceFetcherStep::PushPixels;

                if (object_fetcher.is_enabled && background_fetcher.is_enabled)
                    background_fetcher.is_enabled = false;
            }
            break;
        case PixelSliceFetcherStep::PushPixels:
            for (uint8_t i = 0; i < PIXELS_PER_TILE_ROW; i++)
            {
                if (object_pixel_queue[i].colour_index != 0b00)
                    break;

                object_pixel_queue[i].colour_index = get_pixel_colour_id(object_fetcher, PIXELS_PER_TILE_ROW - 1 - i);
                object_pixel_queue[i].is_priority_bit_set = is_bit_set(lcd_control_lcdc, 7);
                object_pixel_queue[i].is_palette_bit_set = is_bit_set(lcd_control_lcdc, 4);
            }
            object_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
            object_fetcher.is_enabled = false;
            break;
    }

    if (object_fetcher.current_step != PixelSliceFetcherStep::PushPixels)
        object_fetcher.is_in_first_dot_of_current_step = !object_fetcher.is_in_first_dot_of_current_step;
}

void PixelProcessingUnit::set_object_fetcher_tile_row_address(uint8_t offset) {
    const bool is_flipped_vertically = is_bit_set(object_fetcher.get_current_object().flags, 6);
    const uint8_t tile_row_address_bits_1_to_3 = (lcd_y_coordinate_ly - object_fetcher.get_current_object().y_position) % 8;

    object_fetcher.tile_row_address = static_cast<uint16_t>((1 << 15) | (object_fetcher.tile_id << 4) | offset);
    object_fetcher.tile_row_address |= is_flipped_vertically
        ? ~tile_row_address_bits_1_to_3 << 1
        : tile_row_address_bits_1_to_3 << 1;
}

uint8_t PixelProcessingUnit::get_object_fetcher_tile_row() {
    const uint8_t tile_row_byte = read_byte_video_ram(object_fetcher.tile_row_address);

    return is_bit_set(object_fetcher.get_current_object().flags, 5)
        ? get_byte_horizontally_flipped(tile_row_byte)
        : tile_row_byte;
}

uint8_t PixelProcessingUnit::get_byte_horizontally_flipped(uint8_t byte) {
    byte = ((byte & 0b11110000) >> 4) | ((byte & 0b00001111) << 4);
    byte = ((byte & 0b11001100) >> 2) | ((byte & 0b00110011) << 2);
    byte = ((byte & 0b10101010) >> 1) | ((byte & 0b01010101) << 1);
    return byte;
}

uint8_t PixelProcessingUnit::get_pixel_colour_id(PixelSliceFetcher pixel_slice_fetcher, uint8_t bit_position) const
{
    return ((pixel_slice_fetcher.tile_row_high >> (bit_position - 1)) & 0b10) | 
           ((pixel_slice_fetcher.tile_row_low >> bit_position) & 0b01);
}

} // namespace GameBoy
