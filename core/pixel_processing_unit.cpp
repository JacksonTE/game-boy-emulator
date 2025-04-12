#include <algorithm>
#include <cstdint>
#include <type_traits>
#include "pixel_processing_unit.h"

namespace GameBoy {

PixelProcessingUnit::PixelProcessingUnit(std::function<void(uint8_t)> request_interrupt_callback)
	: request_interrupt{request_interrupt_callback}
{
	video_ram = std::make_unique<uint8_t[]>(VIDEO_RAM_SIZE);
	std::fill_n(video_ram.get(), VIDEO_RAM_SIZE, 0);
	
	object_attribute_memory = std::make_unique<ObjectAttributes[]>(OBJECT_ATTRIBUTE_MEMORY_OBJECT_COUNT);
	std::fill_n(video_ram.get(), OBJECT_ATTRIBUTE_MEMORY_OBJECT_COUNT, 0);
}

void PixelProcessingUnit::step_single_machine_cycle() {
	current_scanline_elapsed_dots_count += DOTS_PER_MACHINE_CYCLE;

	switch (current_mode) {
		case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
			step_mode_2_single_machine_cycle();
			if (current_scanline_elapsed_dots_count == OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS) {
				std::stable_sort(current_scanline_selected_objects.begin(), current_scanline_selected_objects.end(),
					[](const ObjectAttributes &a, const ObjectAttributes &b) {
						return a.x_position < b.x_position;
					});
				current_mode = PixelProcessingUnitMode::DrawingPixels;
			}
			break;
		case PixelProcessingUnitMode::DrawingPixels:
			step_mode_3_single_machine_cycle();
			// if ending early in mode 3 might have to add the remaining cycles to hblank?
			// maybe not though since hblank has a minimum length
			if (true) {
				current_mode = PixelProcessingUnitMode::HorizontalBlank;
				// release vram bus either here or next mcycle
			}
			break;
		case PixelProcessingUnitMode::HorizontalBlank:
			// TODO Make OAM and VRAM accessible again here or before switch
			if (current_scanline_elapsed_dots_count == SCAN_LINE_DURATION_DOTS) {
				current_mode = (lcd_y_coordinate_ly == FINAL_DRAWING_SCAN_LINE)
					? PixelProcessingUnitMode::VerticalBlank
					: PixelProcessingUnitMode::ObjectAttributeMemoryScan;
				background_pixels_queue.clear();
				object_pixels_queue.clear();
				lcd_y_coordinate_ly++;
				current_scanline_elapsed_dots_count = 0;
			}
			break;
		case PixelProcessingUnitMode::VerticalBlank:
			if (current_scanline_elapsed_dots_count == SCAN_LINE_DURATION_DOTS)
			{
				if (lcd_y_coordinate_ly == FINAL_FRAME_SCAN_LINE)
					current_mode = PixelProcessingUnitMode::ObjectAttributeMemoryScan;
				lcd_y_coordinate_ly = (lcd_y_coordinate_ly + 1) % (FINAL_FRAME_SCAN_LINE + 1);		
				current_scanline_elapsed_dots_count = 0;
			}
			break;
	}
}

void PixelProcessingUnit::step_mode_2_single_machine_cycle() {
	if (current_scanline_elapsed_dots_count == 0) {
		current_scanline_selected_objects.clear();
	}

	uint16_t y_position_object_attribute_memory_offset = 0;
	for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE && current_scanline_selected_objects.size() < MAX_OBJECTS_PER_LINE; _++) {
		if (is_in_first_dot_of_current_step) {
			// TODO I THINK LOGIC IS WRONG
			y_position_object_attribute_memory_offset = (current_scanline_elapsed_dots_count - DOTS_PER_MACHINE_CYCLE);
		}
		else {
			const uint8_t object_y_position = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset);
			const uint8_t object_height = is_bit_set_uint8(2, lcd_control_lcdc)
				? 16
				: 8;
			if (lcd_y_coordinate_ly >= object_y_position && lcd_y_coordinate_ly <= object_y_position + object_height) {
				ObjectAttributes object_attribute{};

				//TODO REFACTOR WITH READING OAM
				object_attribute.y_position = object_y_position;
				object_attribute.x_position = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset + 1);
				object_attribute.tile_index = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset + 2);
				object_attribute.flags = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset + 3);
				current_scanline_selected_objects.push_back(object_attribute);
			}
		}
		is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
	}
}

void PixelProcessingUnit::step_mode_3_single_machine_cycle()
{
	for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE; _++)
	{
		const bool is_object_display_enabled = is_bit_set_uint8(1, lcd_control_lcdc);

		if (is_object_display_enabled && !object_pixel_slice_fetcher.is_enabled) 
		{
			for (ObjectAttributes object = current_scanline_selected_objects[object_pixel_slice_fetcher.next_object_index];
				object_pixel_slice_fetcher.next_object_index < MAX_OBJECTS_PER_LINE && object.x_position <= lcd_internal_x_coordinate_lx + 8;
				object_pixel_slice_fetcher.next_object_index++)
			{
				if (object.x_position == lcd_internal_x_coordinate_lx + 8)
				{
					is_pixel_output_enabled = false;
					object_pixel_slice_fetcher.is_enabled = true;
					break;
				}
			}
		}

		if (is_object_display_enabled &&
			object_pixel_slice_fetcher.is_enabled &&
			background_pixel_slice_fetcher.current_step == PixelSliceFetcherStep::PushPixels &&
			!background_pixels_queue.is_empty())
		{
			background_pixel_slice_fetcher.is_enabled = false;
		}

		step_active_pixel_fetcher_single_dot();

		if (is_pixel_output_enabled) {

		}

		//During each cycle, after clocking the Pixel Fetchers, the PPU attempts to push a pixel to the LCD, 
		// which can only be done if there are pixels in the background FIFO. If the Sprite FIFO contains any 
		// pixels, a sprite pixel is shifted out and “merged” with the background pixel.

		// SCX mod 8 pixels are discarded at the start of each scanline rather than being pushed to the LCD, 
		// which is also the cause of PPU Mode 3 being extended by SCX mod 8 cycles.


	}
}

void PixelProcessingUnit::step_active_pixel_fetcher_single_dot() {
	const PixelSliceFetcherStep fetcher_step = background_pixel_slice_fetcher.is_enabled
		? background_pixel_slice_fetcher.current_step
		: object_pixel_slice_fetcher.current_step;

	switch (fetcher_step) {
		case PixelSliceFetcherStep::GetTileId:
			if (background_pixel_slice_fetcher.is_enabled) {
				if (is_in_first_dot_of_current_step) {
					set_background_pixel_fetcher_tile_id_fetch_address();
				}
				else {
					background_pixel_slice_fetcher.tile_id = memory_interface.read_byte(background_pixel_slice_fetcher.tile_id_fetch_address);
					background_pixel_slice_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
				}
			}
			else {
				if (!is_in_first_dot_of_current_step) {
					ObjectAttributes current_object = current_scanline_selected_objects[object_pixel_slice_fetcher.next_object_index];
					object_pixel_slice_fetcher.tile_id = current_object.tile_index;
					object_pixel_slice_fetcher.current_step = PixelSliceFetcherStep::GetTileRowLow;
				}
			}
			is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
			break;
		case PixelSliceFetcherStep::GetTileRowLow:
			if (is_in_first_dot_of_current_step) {
				set_active_pixel_fetcher_tile_row_fetch_address(0);
			}
			else {
				if (background_pixel_slice_fetcher.is_enabled) {
					background_pixel_slice_fetcher.tile_row_low_byte = memory_interface.read_byte(background_pixel_slice_fetcher.tile_row_fetch_address);
				}
				else {
					object_pixel_slice_fetcher.tile_row_low_byte = memory_interface.read_byte(object_pixel_slice_fetcher.tile_row_fetch_address);
				}
				object_pixel_slice_fetcher.current_step = PixelSliceFetcherStep::GetTileRowHigh;
			}
			is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
			break;
		case PixelSliceFetcherStep::GetTileRowHigh:
			if (is_in_first_dot_of_current_step) {
				set_active_pixel_fetcher_tile_row_fetch_address(1);
			}
			else {
				if (background_pixel_slice_fetcher.is_enabled) {
					background_pixel_slice_fetcher.tile_row_high_byte = memory_interface.read_byte(background_pixel_slice_fetcher.tile_row_fetch_address);
				}
				else {
					object_pixel_slice_fetcher.tile_row_high_byte = memory_interface.read_byte(background_pixel_slice_fetcher.tile_row_fetch_address);
				}
				object_pixel_slice_fetcher.current_step = PixelSliceFetcherStep::PushPixels;

				if (object_pixel_slice_fetcher.is_enabled && background_pixel_slice_fetcher.is_enabled) {
					background_pixel_slice_fetcher.is_enabled = false;
				}
			}
			is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
			break;
		case PixelSliceFetcherStep::PushPixels:
			if (background_pixel_slice_fetcher.is_enabled) { // TODO ADD CONSTRUCTOR
				if (background_pixels_queue.is_empty()) {
					std::array<BackgroundPixel, 8> pixels{};
					for (int i = 0; i < 7; i++) {
						pixels[i] = BackgroundPixel{get_pixel_colour_id(background_pixel_slice_fetcher.tile_row_low_byte, background_pixel_slice_fetcher.tile_row_high_byte, i)};
					}
					background_pixels_queue.load(pixels);
					background_pixel_slice_fetcher.current_step = PixelSliceFetcherStep::GetTileId;
				}
			}
			else {
				for (int i = 0; i < 8; i++) { // TODO ADD AN ARRAY GETTER, PERFORM OPERATIONS OUTSIDE, THEN PUSH BACK IN
					if (object_pixels_queue[i].colour_index == 0) {
						object_pixels_queue[i].colour_index = get_pixel_colour_id(pixel_queue.fetched_tile_row_low_byte, pixel_queue.fetched_tile_row_high_byte, 7 - i);
						object_pixels_queue[i].is_priority_bit_set = is_bit_set_uint8(7, lcd_control_lcdc);
						object_pixels_queue[i].is_palette_bit_set = is_bit_set_uint8(4, lcd_control_lcdc);
					}
				}
				pixel_queue.fetcher_step = PixelSliceFetcherStep::GetTileId;
				is_pixel_output_enabled = true;
				object_pixels_queue.is_fetcher_enabled = false;
			}			
			break;
		}
}

void PixelProcessingUnit::set_background_pixel_fetcher_tile_id_fetch_address() {
	uint16_t tile_map_area_bit = 0x0000;
	switch (background_pixel_slice_fetcher.fetcher_mode) {
		case FetcherMode::BackgroundMode:
			background_pixel_slice_fetcher.tile_id_fetch_address = static_cast<uint16_t>(0b10011 << 11);
			tile_map_area_bit = is_bit_set_uint8(3, lcd_control_lcdc) ? (1 << 10) : 0;
			background_pixel_slice_fetcher.tile_id_fetch_address |= tile_map_area_bit;
			background_pixel_slice_fetcher.tile_id_fetch_address |= static_cast<uint8_t>((lcd_y_coordinate_ly + viewport_y_position_scy) / 8) << 5;
			background_pixel_slice_fetcher.tile_id_fetch_address |= static_cast<uint8_t>((lcd_internal_x_coordinate_lx + viewport_x_position_scx) / 8);
			break;
		case FetcherMode::WindowMode:
			background_pixel_slice_fetcher.tile_id_fetch_address = static_cast<uint16_t>(0b10011 << 11);
			tile_map_area_bit = is_bit_set_uint8(6, lcd_control_lcdc) ? (1 << 10) : 0;
			background_pixel_slice_fetcher.tile_id_fetch_address |= tile_map_area_bit;
			background_pixel_slice_fetcher.tile_id_fetch_address |= static_cast<uint8_t>(window_internal_line_counter_wly / 8) << 5;
			background_pixel_slice_fetcher.tile_id_fetch_address |= static_cast<uint8_t>(lcd_internal_x_coordinate_lx / 8);
			break;
	}
}

void PixelProcessingUnit::set_active_pixel_fetcher_tile_row_fetch_address(uint8_t offset) {
	pixel_queue.tile_row_fetch_address = static_cast<uint16_t>((0b100 << 13) | (pixel_queue.fetched_tile_id << 4)) | offset;

	switch (pixel_queue.fetcher_mode) {
		case FetcherMode::BackgroundMode:
			if (!is_bit_set_uint8(4, lcd_control_lcdc) && !is_bit_set_uint8(7, pixel_queue.fetched_tile_id)) {
				pixel_queue.tile_row_fetch_address |= (1 << 12);
			}
			pixel_queue.tile_row_fetch_address |= ((lcd_y_coordinate_ly + viewport_y_position_scy) % 8) << 1;
			break;
		case FetcherMode::WindowMode:
			if (!is_bit_set_uint8(4, lcd_control_lcdc) && !is_bit_set_uint8(7, pixel_queue.fetched_tile_id)) {
				pixel_queue.tile_row_fetch_address |= (1 << 12);
			}
			pixel_queue.tile_row_fetch_address |= (window_internal_line_counter_wly % 8) << 1;
			break;
		case FetcherMode::ObjectMode:
			ObjectAttributes object = current_scanline_selected_objects[next_object_index];
			pixel_queue.tile_row_fetch_address |= ((lcd_y_coordinate_ly - object.object_y_position) % 8) << 1;
			break;
	}
}

uint8_t PixelProcessingUnit::read_byte_video_ram(uint16_t local_address) const {
	// TODO ADD LOCKING
	return video_ram[local_address];
}

void PixelProcessingUnit::write_byte_video_ram(uint16_t local_address, uint8_t value) {
	// TODO ADD LOCKING
	video_ram[local_address] = value;
}

uint8_t PixelProcessingUnit::read_byte_object_attribute_memory(uint16_t local_address) const {
	// TODO ADD LOCKING
	const ObjectAttributes object = object_attribute_memory[local_address / 4];
	switch (local_address % 4) {
		case 0:
			return object.y_position;
		case 1:
			return object.x_position;
		case 2:
			return object.tile_index;
		case 3:
			return object.flags;
	}
}

void PixelProcessingUnit::write_byte_object_attribute_memory(uint16_t local_address, uint8_t value) {
	// TODO ADD LOCKING
	ObjectAttributes object = object_attribute_memory[local_address / 4];
	switch (local_address % 4) {
		case 0:
			object.y_position = value;
		case 1:
			object.x_position = value;
		case 2:
			object.tile_index = value;
		case 3:
			object.flags = value;
	}

}

bool PixelProcessingUnit::is_bit_set_uint8(const uint8_t &bit_position_to_test, const uint8_t &uint8) const {
	return (uint8 & (1 << bit_position_to_test)) != 0;
}

uint8_t PixelProcessingUnit::get_pixel_colour_id(uint8_t tile_low_byte, uint8_t tile_high_byte, uint8_t bit_position) const {
	return ((tile_low_byte >> (bit_position - 1)) & 0b10) | ((tile_low_byte >> bit_position) & 0b01);
}

} // namespace GameBoy
