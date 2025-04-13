#include <algorithm>
#include <cstdint>
#include <type_traits>
#include "pixel_processing_unit.h"

namespace GameBoy
{

PixelProcessingUnit::PixelProcessingUnit(std::function<void(uint8_t)> request_interrupt_callback)
	: request_interrupt{request_interrupt_callback}
{
	video_ram = std::make_unique<uint8_t[]>(VIDEO_RAM_SIZE);
	std::fill_n(video_ram.get(), VIDEO_RAM_SIZE, 0);

	object_attribute_memory = std::make_unique<uint8_t[]>(OBJECT_ATTRIBUTE_MEMORY_SIZE);
	std::fill_n(video_ram.get(), OBJECT_ATTRIBUTE_MEMORY_SIZE, 0);
}

void PixelProcessingUnit::step_single_machine_cycle()
{
	current_scanline_elapsed_dots_count += DOTS_PER_MACHINE_CYCLE;

	switch (current_mode)
	{
		case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
			step_object_attribute_memory_scan_single_machine_cycle();

			if (current_scanline_elapsed_dots_count == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS)
			{
				std::stable_sort(current_scanline_selected_objects.begin(), current_scanline_selected_objects.end(),
					[](const ObjectAttributes &a, const ObjectAttributes &b) { return a.x_position < b.x_position; });
				current_mode = PixelProcessingUnitMode::DrawingPixels;
			}
			break;
		case PixelProcessingUnitMode::DrawingPixels:
			step_drawing_pixels_single_machine_cycle();
			if (true)
				current_mode = PixelProcessingUnitMode::HorizontalBlank;
			break;
		case PixelProcessingUnitMode::HorizontalBlank:
			if (current_scanline_elapsed_dots_count == SCAN_LINE_DURATION_DOTS)
			{
				current_mode = (lcd_y_coordinate_ly == FINAL_DRAWING_SCAN_LINE)
					? PixelProcessingUnitMode::VerticalBlank
					: PixelProcessingUnitMode::ObjectAttributeMemoryScan;
				background_pixel_queue.clear();
				object_pixel_queue.clear();
				lcd_y_coordinate_ly++;
				current_scanline_elapsed_dots_count = 0;
			}
			break;
		case PixelProcessingUnitMode::VerticalBlank:
			if (current_scanline_elapsed_dots_count == SCAN_LINE_DURATION_DOTS)
			{
				if (lcd_y_coordinate_ly == FINAL_FRAME_SCAN_LINE)
				{
					current_mode = PixelProcessingUnitMode::ObjectAttributeMemoryScan;
				}
				lcd_y_coordinate_ly = (lcd_y_coordinate_ly + 1) % (FINAL_FRAME_SCAN_LINE + 1);		
				current_scanline_elapsed_dots_count = 0;
			}
			break;
	}
}

void PixelProcessingUnit::step_object_attribute_memory_scan_single_machine_cycle()
{
	if (current_scanline_elapsed_dots_count == 0)
		current_scanline_selected_objects.clear();

	for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE && current_scanline_selected_objects.size() < MAX_OBJECTS_PER_LINE; _++)
	{
		if (is_in_first_dot_of_current_step)
			continue;

		uint8_t starting_index = current_scanline_elapsed_dots_count / 2;
		ObjectAttributes current_object
		{
			object_attribute_memory[starting_index],
			object_attribute_memory[starting_index + 1],
			object_attribute_memory[starting_index + 2],
			object_attribute_memory[starting_index + 3],
		};

		const uint8_t object_height = is_bit_set_uint8(2, lcd_control_lcdc) ? 16 : 8;

		if (lcd_y_coordinate_ly >= current_object.y_position && lcd_y_coordinate_ly < current_object.y_position + object_height)
		{
			if (object_height == 16)
			{
				const bool is_flipped_vertically = is_bit_set_uint8(6, current_object.flags);

				if (lcd_y_coordinate_ly < current_object.y_position + 8 == is_flipped_vertically)
					current_object.tile_index &= 0xfe;
				else
					current_object.tile_index |= 0x01;
			}
			current_scanline_selected_objects.push_back(current_object);
		}
		is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
	}
}

void PixelProcessingUnit::step_drawing_pixels_single_machine_cycle()
{
	for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE; _++)
	{
		const bool is_object_display_enabled = is_bit_set_uint8(1, lcd_control_lcdc);

		if (is_object_display_enabled && !object_fetcher.is_enabled) 
		{
			for (ObjectAttributes object = current_scanline_selected_objects[object_fetcher.current_object_index];
				object_fetcher.current_object_index < MAX_OBJECTS_PER_LINE && object.x_position <= lcd_internal_x_coordinate_lx + 8;
				object_fetcher.current_object_index++)
			{
				if (object.x_position == lcd_internal_x_coordinate_lx + 8)
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

		if (is_pixel_output_enabled)
		{

		}

		//When rendering the window the background FIFO is cleared and the fetcher is reset to step 1. 
		// When WX is 0 and the SCX & 7 > 0 mode 3 is shortened by 1 dot.

		//During each cycle, after clocking the Pixel Fetchers, the PPU attempts to push a pixel to the LCD, 
		// which can only be done if there are pixels in the background FIFO. If the Sprite FIFO contains any 
		// pixels, a sprite pixel is shifted out and “merged” with the background pixel.

		// SCX mod 8 pixels are discarded at the start of each scanline rather than being pushed to the LCD, 
		// which is also the cause of PPU Mode 3 being extended by SCX mod 8 cycles.

		// The 12 extra dots of penalty come from two tile fetches at the beginning of Mode 3. 
		// One is the first tile in the scanline (the one that gets shifted by SCX % 8 pixels), the other is simply discarded.
	}
}

void PixelProcessingUnit::step_background_fetcher_single_dot()
{
	switch (background_fetcher.current_step)
	{
		case PixelSliceFetcherStep::GetTileId:
			if (is_in_first_dot_of_current_step)
				set_background_fetcher_tile_id_address();
			else
			{
				background_fetcher.tile_id = read_byte_video_ram(background_fetcher.tile_id_address);
				background_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
			}
			break;
		case PixelSliceFetcherStep::GetTileRowLow:
			if (is_in_first_dot_of_current_step)
				set_background_fetcher_tile_row_address(0);
			else
				background_fetcher.tile_row_low = read_byte_video_ram(background_fetcher.tile_row_address);			
			break;
		case PixelSliceFetcherStep::GetTileRowHigh:
			if (is_in_first_dot_of_current_step)
				set_background_fetcher_tile_row_address(1);
			else
			{
				background_fetcher.tile_row_high = read_byte_video_ram(background_fetcher.tile_row_address);

				for (int i = 0; i < PIXELS_PER_TILE_ROW; i++)
					background_fetcher.tile_row[i] = BackgroundPixel{get_pixel_colour_id(background_fetcher, i)};

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
		is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
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
			if (is_in_first_dot_of_current_step)
				break;
			ObjectAttributes current_object = current_scanline_selected_objects[object_fetcher.current_object_index];
			object_fetcher.tile_id = current_object.tile_index;
			object_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
			break;
		case PixelSliceFetcherStep::GetTileRowLow:
			if (is_in_first_dot_of_current_step)
				set_object_fetcher_tile_row_address(0);
			else
			{
				object_fetcher.tile_row_low = get_object_fetcher_tile_row();
				object_fetcher.current_step = PixelSliceFetcherStep::GetTileRowHigh;
			}
			break;
		case PixelSliceFetcherStep::GetTileRowHigh:
			if (is_in_first_dot_of_current_step)
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
			for (uint8_t i = 0; i < PIXELS_PER_TILE_ROW; i++) {
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
		is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
}

void PixelProcessingUnit::set_object_fetcher_tile_row_address(uint8_t offset) {
	ObjectAttributes current_object = current_scanline_selected_objects[object_fetcher.current_object_index];
	
	const bool is_flipped_vertically = is_bit_set_uint8(6, current_object.flags);
	const uint8_t tile_row_address_bits_1_to_3 = (lcd_y_coordinate_ly - current_object.y_position) % 8;

	object_fetcher.tile_row_address = static_cast<uint16_t>((1 << 15) | (object_fetcher.tile_id << 4) | offset);
	object_fetcher.tile_row_address |= is_flipped_vertically
		? ~tile_row_address_bits_1_to_3 << 1
		: tile_row_address_bits_1_to_3 << 1;
}

uint8_t PixelProcessingUnit::get_object_fetcher_tile_row() {
	ObjectAttributes current_object = current_scanline_selected_objects[object_fetcher.current_object_index];
	const uint8_t tile_row_byte = read_byte_video_ram(background_fetcher.tile_row_address);

	return is_bit_set_uint8(5, current_object.flags)
		? get_byte_horizontally_flipped(tile_row_byte)
		: tile_row_byte;
}

uint8_t PixelProcessingUnit::read_byte_video_ram(uint16_t memory_address) const
{
	if (current_mode == PixelProcessingUnitMode::DrawingPixels)
		return 0xff;
	const uint16_t local_address = memory_address - VIDEO_RAM_START;
	return video_ram[local_address];
}

void PixelProcessingUnit::write_byte_video_ram(uint16_t memory_address, uint8_t value)
{
	if (current_mode == PixelProcessingUnitMode::DrawingPixels)
		return;
	const uint16_t local_address = memory_address - VIDEO_RAM_START;
	video_ram[local_address] = value;
}

uint8_t PixelProcessingUnit::read_byte_object_attribute_memory(uint16_t memory_address) const {
	if (current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan ||
		current_mode == PixelProcessingUnitMode::DrawingPixels)
	{
		return 0xff;
	}
	const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
	return object_attribute_memory[local_address];
}

void PixelProcessingUnit::write_byte_object_attribute_memory(uint16_t memory_address, uint8_t value)
{
	if (current_mode == PixelProcessingUnitMode::ObjectAttributeMemoryScan ||
		current_mode == PixelProcessingUnitMode::DrawingPixels)
	{
		return;
	}
	const uint16_t local_address = memory_address - OBJECT_ATTRIBUTE_MEMORY_START;
	object_attribute_memory[local_address] = value;
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
