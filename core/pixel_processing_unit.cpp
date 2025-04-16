#include <algorithm>
#include <cstdint>
#include <memory>
#include <type_traits>
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

void ObjectFetcher::reset_state()
{
	PixelSliceFetcher::reset_state();
	current_scanline_selected_objects.clear();
	current_object_index = 0;
	is_enabled = false;
}

void BackgroundFetcher::reset_state()
{
	PixelSliceFetcher::reset_state();
	fetcher_mode = FetcherMode::BackgroundMode;
	tile_id_address = 0;
	tile_row.fill(BackgroundPixel{});
	is_enabled = true;
}

ObjectAttributes ObjectFetcher::get_current_object()
{
	return current_scanline_selected_objects[current_object_index];
}

PixelProcessingUnit::PixelProcessingUnit(std::function<void(uint8_t)> request_interrupt_callback)
	: request_interrupt{request_interrupt_callback}
{
	video_ram = std::make_unique<uint8_t[]>(VIDEO_RAM_SIZE);
	std::fill_n(video_ram.get(), VIDEO_RAM_SIZE, 0);

	object_attribute_memory = std::make_unique<uint8_t[]>(OBJECT_ATTRIBUTE_MEMORY_SIZE);
	std::fill_n(video_ram.get(), OBJECT_ATTRIBUTE_MEMORY_SIZE, 0);

	object_fetcher.reset_state();
	background_fetcher.reset_state();
}

uint8_t PixelProcessingUnit::read_byte_video_ram(uint16_t memory_address) const
{
	if (current_mode == PixelProcessingUnitMode::PixelTransfer)
		return 0xff;

	const uint16_t local_address = memory_address - VIDEO_RAM_START;
	return video_ram[local_address];
}

void PixelProcessingUnit::write_byte_video_ram(uint16_t memory_address, uint8_t value)
{
	if (current_mode == PixelProcessingUnitMode::PixelTransfer)
		return;

	const uint16_t local_address = memory_address - VIDEO_RAM_START;
	video_ram[local_address] = value;
}

uint8_t PixelProcessingUnit::read_byte_object_attribute_memory(uint16_t memory_address) const
{
	if (current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan || current_mode == PixelProcessingUnitMode::PixelTransfer)
		return 0xff;

	const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
	return object_attribute_memory[local_address];
}

void PixelProcessingUnit::write_byte_object_attribute_memory(uint16_t memory_address, uint8_t value)
{
	if (current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan || current_mode == PixelProcessingUnitMode::PixelTransfer)
		return;

	const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
	object_attribute_memory[local_address] = value;
}

uint8_t PixelProcessingUnit::read_lcd_control_lcdc() const
{
	return read_lcd_control_lcdc();
}

void PixelProcessingUnit::write_lcd_control_lcdc(uint8_t value)
{
	bool was_pixel_processing_unit_enable_reset = !is_bit_set_uint8(7, lcd_control_lcdc);

	if (was_pixel_processing_unit_enable_reset && is_bit_set_uint8(7, value))
	{
		lcd_status_stat |= 0b00000001;
		enable_status = PixelProcessingUnitEnableStatus::Enabling;
	}

	lcd_control_lcdc = value;
}

uint8_t PixelProcessingUnit::read_lcd_status_stat() const
{
	return lcd_status_stat;
}

void PixelProcessingUnit::write_lcd_status_stat(uint8_t value)
{
	const bool is_lcd_status_stat_interrupt_condition_met_before = is_lcd_status_stat_interrupt_condition_met();

	lcd_status_stat = (value & 0b11111000) | (lcd_status_stat & 0b00000111);

	if (!is_lcd_status_stat_interrupt_condition_met_before &&
		is_lcd_status_stat_interrupt_condition_met() &&
		enable_status == PixelProcessingUnitEnableStatus::Enabled)
	{
		request_interrupt(INTERRUPT_FLAG_LCD_STAT_MASK);
	}
}

uint8_t PixelProcessingUnit::read_lcd_y_coordinate_ly() const
{
	return lcd_y_coordinate_ly;
}

void PixelProcessingUnit::step_single_machine_cycle()
{
	if (enable_status == PixelProcessingUnitEnableStatus::Disabled) // TODO could just check lcdc bit and make diff var for first portion
		return;

	for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE; _++)
	{
		dots_elapsed_while_disable_pending = is_bit_set_uint8(7, lcd_control_lcdc)
			? dots_elapsed_while_disable_pending + 1
			: 0;
		if (dots_elapsed_while_disable_pending == 4560)
		{
			disable();
			return;
		}

		/*const bool is_lcd_status_stat_interrupt_condition_met_before = is_lcd_status_stat_interrupt_condition_met();
		const uint8_t updated_lcd_status_stat = (lcd_y_coordinate_ly == lcd_y_coordinate_compare_lyc)
			? lcd_status_stat | 0b00000010
			: lcd_status_stat & 0b11111101;
		if (!is_lcd_status_stat_interrupt_condition_met_before &&
			is_lcd_status_stat_interrupt_condition_met() &&
			enable_status == PixelProcessingUnitEnableStatus::Enabled)
		{
			request_interrupt(INTERRUPT_FLAG_LCD_STAT_MASK);
		}*/

		current_scanline_elapsed_dots_count++;

		switch (current_mode)
		{
			case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
				step_object_attribute_memory_scan_single_dot();

				if (current_scanline_elapsed_dots_count == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS)
				{
					std::stable_sort(object_fetcher.current_scanline_selected_objects.begin(), object_fetcher.current_scanline_selected_objects.end(),
						[](const ObjectAttributes &a, const ObjectAttributes &b) { return a.x_position < b.x_position; });

					current_mode = PixelProcessingUnitMode::PixelTransfer;
				}
				break;
			case PixelProcessingUnitMode::PixelTransfer:
				step_pixel_transfer_single_dot();

				if (lcd_internal_x_coordinate_lx == 160)
				{
					if (background_fetcher.fetcher_mode == FetcherMode::WindowMode)
					{
						background_fetcher.fetcher_mode = FetcherMode::BackgroundMode;
						window_internal_line_counter_wly++;
					}
					current_mode = PixelProcessingUnitMode::HorizontalBlank;
				}
				break;
			case PixelProcessingUnitMode::HorizontalBlank:
				if (enable_status == PixelProcessingUnitEnableStatus::Enabling && current_scanline_elapsed_dots_count == 76)
				{
					current_mode = PixelProcessingUnitMode::PixelTransfer;
					break;
				}

				if (current_scanline_elapsed_dots_count < SCAN_LINE_DURATION_DOTS)
					break;

				if (lcd_y_coordinate_ly == FINAL_DRAWING_SCAN_LINE)
				{
					current_mode = PixelProcessingUnitMode::VerticalBlank;
					request_interrupt(INTERRUPT_FLAG_VBLANK_MASK);
				}
				else
					current_mode = PixelProcessingUnitMode::ObjectAttributeMemoryScan;

				background_pixel_queue.clear();
				object_pixel_queue.clear();
				lcd_y_coordinate_ly++;
				current_scanline_elapsed_dots_count = 0;
				current_scanline_discarded_pixels_count = 0;
				break;
			case PixelProcessingUnitMode::VerticalBlank:
				if (current_scanline_elapsed_dots_count < SCAN_LINE_DURATION_DOTS)
					break;

				if (lcd_y_coordinate_ly == FINAL_FRAME_SCAN_LINE)
				{
					current_mode = PixelProcessingUnitMode::ObjectAttributeMemoryScan;

					if (enable_status == PixelProcessingUnitEnableStatus::Enabling)
						enable_status = PixelProcessingUnitEnableStatus::Enabled;
				}

				lcd_y_coordinate_ly = (lcd_y_coordinate_ly + 1) % (FINAL_FRAME_SCAN_LINE + 1);
				current_scanline_elapsed_dots_count = 0;
				break;
		}
	}
}

void PixelProcessingUnit::disable()
{
	lcd_status_stat &= 0x11111110;
	lcd_y_coordinate_ly = 0;
	lcd_internal_x_coordinate_lx = 0;
	window_internal_line_counter_wly = 0;

	enable_status = PixelProcessingUnitEnableStatus::Disabled;
	current_mode = PixelProcessingUnitMode::HorizontalBlank;
	dots_elapsed_while_disable_pending = 0;
	current_scanline_elapsed_dots_count = 0;
	current_scanline_discarded_pixels_count = 0;
	is_in_first_dot_of_current_step = true;
	is_pixel_output_enabled = true;

	background_pixel_queue.clear();
	object_pixel_queue.clear();

	object_fetcher.reset_state();
	background_fetcher.reset_state();
}

void PixelProcessingUnit::step_object_attribute_memory_scan_single_dot()
{
	if (current_scanline_elapsed_dots_count == 0)
		object_fetcher.current_scanline_selected_objects.clear();

	if (object_fetcher.current_scanline_selected_objects.size() == MAX_OBJECTS_PER_LINE)
		return;

	if (is_in_first_dot_of_current_step)
	{
		is_in_first_dot_of_current_step = false;
		return;
	}

	const uint8_t object_height = is_bit_set_uint8(2, lcd_control_lcdc) ? 16 : 8;
	const uint8_t starting_index = current_scanline_elapsed_dots_count / 2;
	ObjectAttributes current_object
	{
		object_attribute_memory[starting_index],
		object_attribute_memory[starting_index + 1],
		object_attribute_memory[starting_index + 2],
		object_attribute_memory[starting_index + 3],
	};

	const bool object_intersects_with_scanline = lcd_y_coordinate_ly >= current_object.y_position &&
		lcd_y_coordinate_ly < current_object.y_position + object_height;
	if (object_intersects_with_scanline)
	{
		if (object_height == 16)
		{
			const bool is_flipped_vertically = is_bit_set_uint8(6, current_object.flags);

			if (lcd_y_coordinate_ly < current_object.y_position + 8 == is_flipped_vertically)
				current_object.tile_index &= 0b11111110;
			else
				current_object.tile_index |= 0xb00000001;
		}
		object_fetcher.current_scanline_selected_objects.push_back(current_object);
	}
	is_in_first_dot_of_current_step = true;
}

void PixelProcessingUnit::step_pixel_transfer_single_dot()
{
	const bool is_object_display_enabled = is_bit_set_uint8(1, lcd_control_lcdc);

	if (is_object_display_enabled && !object_fetcher.is_enabled)
	{
		while (object_fetcher.current_object_index + 1 < object_fetcher.current_scanline_selected_objects.size() &&
			   object_fetcher.current_scanline_selected_objects[object_fetcher.current_object_index + 1].x_position <= lcd_internal_x_coordinate_lx + 8)
		{
			object_fetcher.current_object_index++;

			if (object_fetcher.get_current_object().x_position == lcd_internal_x_coordinate_lx + 8)
			{
				is_pixel_output_enabled = false;
				object_fetcher.is_enabled = true;
				break;
			}
		}
	}

	if (object_fetcher.is_enabled &&
		background_fetcher.current_step == PixelSliceFetcherStep::PushPixels &&
		!background_pixel_queue.is_empty())
	{
		if (is_object_display_enabled)
			background_fetcher.is_enabled = false;
		else
		{
			is_pixel_output_enabled = true;
			object_fetcher.is_enabled = false;
		}
	}

	if (background_fetcher.is_enabled)
		step_background_fetcher_single_dot();
	else
		step_object_fetcher_single_dot();

	if (is_pixel_output_enabled && !background_pixel_queue.is_empty())
	{
		BackgroundPixel next_background_pixel = background_pixel_queue.shift_out();
		ObjectPixel next_object_pixel = object_pixel_queue.shift_out();

		if (current_scanline_discarded_pixels_count < 8 + viewport_x_position_scx % 8)
		{
			current_scanline_discarded_pixels_count++;
			return;
		}

		const bool are_background_and_window_enabled = is_bit_set_uint8(0, lcd_control_lcdc);

		if (!are_background_and_window_enabled)
			auto x = next_object_pixel;

		const bool are_objects_enabled = is_bit_set_uint8(1, lcd_control_lcdc);

		if (!are_objects_enabled)
			auto x = next_background_pixel;

		if (next_object_pixel.is_priority_bit_set && next_background_pixel.colour_index != 0)
			auto x = next_background_pixel;

		if (next_object_pixel.colour_index == 0b00)
			auto x = next_background_pixel;
		else
			auto x = next_object_pixel;

		lcd_internal_x_coordinate_lx++;

		if (lcd_internal_x_coordinate_lx == window_x_position_plus_7_wx - 7)
		{
			background_fetcher.fetcher_mode = FetcherMode::WindowMode;
			background_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
			background_fetcher.is_in_first_dot_of_current_step = true;
			background_pixel_queue.clear();
		}
	}
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
				background_fetcher.tile_id = read_byte_video_ram(background_fetcher.tile_id_address);
				background_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
			}
			break;
		case PixelSliceFetcherStep::GetTileRowLow:
			if (background_fetcher.is_in_first_dot_of_current_step)
				set_background_fetcher_tile_row_address(0);
			else
			{
				background_fetcher.tile_row_low = read_byte_video_ram(background_fetcher.tile_row_address);
				background_fetcher.current_step = PixelSliceFetcherStep::GetTileRowHigh;
			}
			break;
		case PixelSliceFetcherStep::GetTileRowHigh:
			if (background_fetcher.is_in_first_dot_of_current_step)
				set_background_fetcher_tile_row_address(1);
			else
			{
				background_fetcher.tile_row_high = read_byte_video_ram(background_fetcher.tile_row_address);

				for (int i = 0; i < PIXELS_PER_TILE_ROW; i++)
					background_fetcher.tile_row[i] = BackgroundPixel{get_pixel_colour_id(background_fetcher, i)};

				background_fetcher.current_step = PixelSliceFetcherStep::PushPixels;

				if (object_fetcher.is_enabled)
					background_fetcher.is_enabled = false;
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
			tile_map_area_bit = is_bit_set_uint8(3, lcd_control_lcdc) ? (1 << 10) : 0;
			background_fetcher.tile_id_address |= tile_map_area_bit;
			background_fetcher.tile_id_address |= static_cast<uint8_t>((lcd_y_coordinate_ly + viewport_y_position_scy) / 8) << 5;
			background_fetcher.tile_id_address |= static_cast<uint8_t>((lcd_internal_x_coordinate_lx + viewport_x_position_scx) / 8);
			break;
		case FetcherMode::WindowMode:
			tile_map_area_bit = is_bit_set_uint8(6, lcd_control_lcdc) ? (1 << 10) : 0;
			background_fetcher.tile_id_address |= tile_map_area_bit;
			background_fetcher.tile_id_address |= static_cast<uint8_t>(window_internal_line_counter_wly / 8) << 5;
			background_fetcher.tile_id_address |= static_cast<uint8_t>(lcd_internal_x_coordinate_lx / 8);
			break;
	}
}

void PixelProcessingUnit::set_background_fetcher_tile_row_address(uint8_t offset) {
	background_fetcher.tile_row_address = static_cast<uint16_t>((1 << 15) | (background_fetcher.tile_id << 4) | offset);

	if (!is_bit_set_uint8(4, lcd_control_lcdc) && !is_bit_set_uint8(7, background_fetcher.tile_id))
	{
		background_fetcher.tile_row_address |= (1 << 12);
	}
	background_fetcher.tile_row_address |= (background_fetcher.fetcher_mode == FetcherMode::BackgroundMode)
		? ((lcd_y_coordinate_ly + viewport_y_position_scy) % 8) << 1
		: (window_internal_line_counter_wly % 8) << 1;
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
				object_pixel_queue[i].is_priority_bit_set = is_bit_set_uint8(7, lcd_control_lcdc);
				object_pixel_queue[i].is_palette_bit_set = is_bit_set_uint8(4, lcd_control_lcdc);
			}
			object_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
			is_pixel_output_enabled = true;
			object_fetcher.is_enabled = false;
			break;
	}

	if (object_fetcher.current_step != PixelSliceFetcherStep::PushPixels)
		object_fetcher.is_in_first_dot_of_current_step = !object_fetcher.is_in_first_dot_of_current_step;
}

void PixelProcessingUnit::set_object_fetcher_tile_row_address(uint8_t offset) {
	const bool is_flipped_vertically = is_bit_set_uint8(6, object_fetcher.get_current_object().flags);
	const uint8_t tile_row_address_bits_1_to_3 = (lcd_y_coordinate_ly - object_fetcher.get_current_object().y_position) % 8;

	object_fetcher.tile_row_address = static_cast<uint16_t>((1 << 15) | (object_fetcher.tile_id << 4) | offset);
	object_fetcher.tile_row_address |= is_flipped_vertically
		? ~tile_row_address_bits_1_to_3 << 1
		: tile_row_address_bits_1_to_3 << 1;
}

uint8_t PixelProcessingUnit::get_object_fetcher_tile_row() {
	const uint8_t tile_row_byte = read_byte_video_ram(object_fetcher.tile_row_address);

	return is_bit_set_uint8(5, object_fetcher.get_current_object().flags)
		? get_byte_horizontally_flipped(tile_row_byte)
		: tile_row_byte;
}

void PixelProcessingUnit::update_interrupts()
{

}

bool PixelProcessingUnit::is_lcd_status_stat_interrupt_condition_met()
{
	const bool is_mode_lyc_interrupt_select_set = is_bit_set_uint8(6, lcd_status_stat);
	const bool is_mode_2_interrupt_select_set = is_bit_set_uint8(5, lcd_status_stat);
	const bool is_mode_1_interrupt_select_set = is_bit_set_uint8(4, lcd_status_stat);
	const bool is_mode_0_interrupt_select_set = is_bit_set_uint8(3, lcd_status_stat);
	const bool does_lyc_equal_ly = is_bit_set_uint8(2, lcd_status_stat);
	const uint8_t pixel_processing_unit_status = lcd_status_stat & 0b00000011;

	return (is_mode_lyc_interrupt_select_set && does_lyc_equal_ly) ||
		   (is_mode_2_interrupt_select_set && pixel_processing_unit_status == 2) ||
		   (is_mode_1_interrupt_select_set && pixel_processing_unit_status == 1) ||
		   (is_mode_0_interrupt_select_set && pixel_processing_unit_status == 0);
}

uint8_t PixelProcessingUnit::get_byte_horizontally_flipped(uint8_t byte) {
	byte = ((byte & 0xf0) >> 4) | ((byte & 0x0f) << 4);
	byte = ((byte & 0xcc) >> 2) | ((byte & 0x33) << 2);
	byte = ((byte & 0xaa) >> 1) | ((byte & 0x55) << 1);
	return byte;
}

bool PixelProcessingUnit::is_bit_set_uint8(const uint8_t &bit_position_to_test, const uint8_t &uint8) const
{
	return (uint8 & (1 << bit_position_to_test)) != 0;
}

uint8_t PixelProcessingUnit::get_pixel_colour_id(PixelSliceFetcher pixel_slice_fetcher, uint8_t bit_position) const
{
	return ((pixel_slice_fetcher.tile_row_high >> (bit_position - 1)) & 0b10) | 
		   ((pixel_slice_fetcher.tile_row_low >> bit_position) & 0b01);
}

} // namespace GameBoy
