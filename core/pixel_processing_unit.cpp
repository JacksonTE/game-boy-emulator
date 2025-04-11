#include <algorithm>
#include <cstdint>
#include "pixel_processing_unit.h"

namespace GameBoy {

PixelProcessingUnit::PixelProcessingUnit(MemoryManagementUnit &memory_management_unit)
	: memory_interface{memory_management_unit} {
}

void PixelProcessingUnit::step_single_machine_cycle() {
	current_scanline_elapsed_dots_count += DOTS_PER_MACHINE_CYCLE;

	switch (current_mode) {
		case PixelProcessingUnitMode::ObjectAttributeMemoryScan:
			step_mode_2_single_machine_cycle();
			if (current_scanline_elapsed_dots_count == OAM_SCAN_DURATION_DOTS) {
				std::stable_sort(current_scanline_selected_objects.begin(), current_scanline_selected_objects.end(),
					[](const ObjectAttribute &a, const ObjectAttribute &b) {
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
	for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE && current_scanline_selected_objects.size() < 10; _++) {
		if (is_in_first_dot_of_current_step) {
			y_position_object_attribute_memory_offset = (current_scanline_elapsed_dots_count - DOTS_PER_MACHINE_CYCLE);
		}
		else {
			const uint8_t y_position = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset);
			const uint8_t object_height = is_bit_set_uint8(2, lcd_control_lcdc)
				? 16
				: 8;
			if (lcd_y_coordinate_ly >= y_position && lcd_y_coordinate_ly <= y_position + object_height) {
				ObjectAttribute object_attribute{};
				object_attribute.y_position = y_position;
				object_attribute.x_position = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset + 1);
				object_attribute.tile_index = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset + 2);
				object_attribute.attributes = read_object_attribute_memory_byte(y_position_object_attribute_memory_offset + 3);
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

		if (is_object_display_enabled && !object_pixels_queue.is_fetcher_enabled) 
		{
			for (ObjectAttribute object = current_scanline_selected_objects[current_scanline_next_object_index];
				 current_scanline_next_object_index < 10 && object.x_position <= lcd_internal_x_coordinate_lx + 8;
				 current_scanline_next_object_index++)
			{
				if (object.x_position == lcd_internal_x_coordinate_lx + 8)
				{
					is_pixel_output_enabled = false;
					object_pixels_queue.is_fetcher_enabled = true;
					break;
				}
			}
		}

		if (is_object_display_enabled &&
			object_pixels_queue.is_fetcher_enabled &&
			background_pixels_queue.fetcher_step == PixelSliceFetcherStep::PushPixels &&
			!background_pixels_queue.is_empty())
		{
			background_pixels_queue.is_fetcher_enabled = false;
		}

		if (background_pixels_queue.is_fetcher_enabled)
			step_pixel_fetcher_single_dot(background_pixels_queue);
		else
			step_pixel_fetcher_single_dot(object_pixels_queue);

		if (is_pixel_output_enabled) {

		}

		//During each cycle, after clocking the Pixel Fetchers, the PPU attempts to push a pixel to the LCD, 
		// which can only be done if there are pixels in the background FIFO. If the Sprite FIFO contains any 
		// pixels, a sprite pixel is shifted out and “merged” with the background pixel.

		// SCX mod 8 pixels are discarded at the start of each scanline rather than being pushed to the LCD, 
		// which is also the cause of PPU Mode 3 being extended by SCX mod 8 cycles.


	}
}

template <typename EntryType>
void PixelProcessingUnit::step_pixel_fetcher_single_dot(PixelQueue<EntryType> &pixel_queue) {
	switch (pixel_queue.fetcher_step) {
		case PixelSliceFetcherStep::GetTileId:
			switch (pixel_queue.fetcher_mode) {
				case FetcherMode::BackgroundMode:
				case FetcherMode::WindowMode:
					if (is_in_first_dot_of_current_step) {
						set_tile_id_fetch_address(pixel_queue);
					}
					else {
						pixel_queue.fetched_tile_id = memory_interface.read_byte(pixel_queue.tile_id_fetch_address);
						pixel_queue.fetcher_step = PixelSliceFetcherStep::GetTileRowLow;
					}
					break;
				case FetcherMode::ObjectMode:
					if (!is_in_first_dot_of_current_step) {
						ObjectAttribute object = current_scanline_selected_objects[current_scanline_next_object_index];
						pixel_queue.fetched_tile_id = object.tile_index;
						pixel_queue.fetcher_step = PixelSliceFetcherStep::GetTileRowLow;
					}
					break;
			}
			is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
			break;
		case PixelSliceFetcherStep::GetTileRowLow:
			if (is_in_first_dot_of_current_step) {
				set_tile_row_fetch_address(pixel_queue, 0);
			}
			else {
				pixel_queue.fetched_tile_row_low_byte = memory_interface.read_byte(pixel_queue.tile_row_fetch_address);
				pixel_queue.fetcher_step = PixelSliceFetcherStep::GetTileRowHigh;
			}
			is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
			break;
		case PixelSliceFetcherStep::GetTileRowHigh:
			if (is_in_first_dot_of_current_step) {
				set_tile_row_fetch_address(pixel_queue, 1);
			}
			else {
				pixel_queue.fetched_tile_row_high_byte = memory_interface.read_byte(pixel_queue.tile_row_fetch_address);
				pixel_queue.fetcher_step = PixelSliceFetcherStep::PushPixels;
				if (object_pixels_queue.is_fetcher_enabled && pixel_queue.fetcher_mode != FetcherMode::ObjectMode) {
					pixel_queue.is_fetcher_enabled = false;
				}
			}
			is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
			break;
		case PixelSliceFetcherStep::PushPixels:
			switch (pixel_queue.fetcher_mode) {
				case FetcherMode::BackgroundMode:
				case FetcherMode::WindowMode:
					if (background_pixels_queue.is_empty()) {
						for (int i = 7; i >= 0; i--) {
							uint8_t colour_id = get_pixel_colour_id(pixel_queue.fetched_tile_row_low_byte, pixel_queue.fetched_tile_row_high_byte, i);
							background_pixels_queue.push_back(BackgroundPixelQueueEntry{colour_id});
						}
						pixel_queue.fetcher_step = PixelSliceFetcherStep::GetTileId;
					}
					break;
				case FetcherMode::ObjectMode:
					for (int i = 0; i < 8; i++) {
						if (object_pixels_queue[i].colour_index == 0) {
							object_pixels_queue[i].colour_index = get_pixel_colour_id(pixel_queue.fetched_tile_row_low_byte, pixel_queue.fetched_tile_row_high_byte, 7 - i);
							object_pixels_queue[i].is_priority_bit_set = is_bit_set_uint8(7, lcd_control_lcdc);
							object_pixels_queue[i].is_palette_bit_set = is_bit_set_uint8(4, lcd_control_lcdc);
						}
					}
					pixel_queue.fetcher_step = PixelSliceFetcherStep::GetTileId;
					is_pixel_output_enabled = true;
					object_pixels_queue.is_fetcher_enabled = false;
					break;
			}
			break;
		}
}

void PixelProcessingUnit::set_tile_id_fetch_address(PixelQueue<BackgroundPixelQueueEntry> &pixel_queue) {
	uint16_t tile_map_area_bit = 0x0000;
	switch (pixel_queue.fetcher_mode) {
		case FetcherMode::BackgroundMode:
			pixel_queue.tile_id_fetch_address = static_cast<uint16_t>(0b10011 << 11);
			tile_map_area_bit = is_bit_set_uint8(3, lcd_control_lcdc) ? (1 << 10) : 0;
			pixel_queue.tile_id_fetch_address |= tile_map_area_bit;
			pixel_queue.tile_id_fetch_address |= static_cast<uint8_t>((lcd_y_coordinate_ly + viewport_y_position_scy) / 8) << 5;
			pixel_queue.tile_id_fetch_address |= static_cast<uint8_t>((lcd_internal_x_coordinate_lx + viewport_x_position_scx) / 8);
			break;
		case FetcherMode::WindowMode:
			pixel_queue.tile_id_fetch_address = static_cast<uint16_t>(0b10011 << 11);
			tile_map_area_bit = is_bit_set_uint8(6, lcd_control_lcdc) ? (1 << 10) : 0;
			pixel_queue.tile_id_fetch_address |= tile_map_area_bit;
			pixel_queue.tile_id_fetch_address |= static_cast<uint8_t>(window_internal_line_counter_wly / 8) << 5;
			pixel_queue.tile_id_fetch_address |= static_cast<uint8_t>(lcd_internal_x_coordinate_lx / 8);
			break;
	}
}

template <typename T>
void PixelProcessingUnit::set_tile_row_fetch_address(PixelQueue<T> &pixel_queue, uint8_t offset) {
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
			ObjectAttribute object = current_scanline_selected_objects[current_scanline_next_object_index];
			pixel_queue.tile_row_fetch_address |= ((lcd_y_coordinate_ly - object.y_position) % 8) << 1;
			break;
	}
}

uint8_t PixelProcessingUnit::read_object_attribute_memory_byte(uint16_t address_offset) {
	return memory_interface.read_byte(OBJECT_ATTRIBUTE_MEMORY_START + address_offset);
}

bool PixelProcessingUnit::is_bit_set_uint8(const uint8_t &bit_position_to_test, const uint8_t &uint8) const {
	return (uint8 & (1 << bit_position_to_test)) != 0;
}

uint8_t PixelProcessingUnit::get_pixel_colour_id(uint8_t tile_low_byte, uint8_t tile_high_byte, uint8_t bit_position) const {
	return ((tile_low_byte >> (bit_position - 1)) & 0b10) | ((tile_low_byte >> bit_position) & 0b01);
}

} // namespace GameBoy
