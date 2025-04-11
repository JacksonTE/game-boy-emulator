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
			object_attribute_memory_scan_mode_2();
			if (current_scanline_elapsed_dots_count == OAM_SCAN_DURATION_DOTS) {
				std::stable_sort(current_scanline_selected_objects.begin(), current_scanline_selected_objects.end(),
					[](const ObjectAttribute &a, const ObjectAttribute &b) {
						return a.x_position < b.x_position;
					});
				current_mode = PixelProcessingUnitMode::DrawingPixels;
			}
			break;
		case PixelProcessingUnitMode::DrawingPixels:
			draw_pixels_mode_3();
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
				clear_queue(background_pixels_queue);
				clear_queue(object_pixels_queue);
				lcd_y_coordinate_ly++;
				current_scanline_elapsed_dots_count = 0;
			}
			break;
		case PixelProcessingUnitMode::VerticalBlank:
			if (current_scanline_elapsed_dots_count == SCAN_LINE_DURATION_DOTS) {
				if (lcd_y_coordinate_ly == FINAL_FRAME_SCAN_LINE) {
					current_mode = PixelProcessingUnitMode::ObjectAttributeMemoryScan;
				}
				lcd_y_coordinate_ly = (lcd_y_coordinate_ly + 1) % (FINAL_FRAME_SCAN_LINE + 1);		
				current_scanline_elapsed_dots_count = 0;
			}
			break;
	}
}

void PixelProcessingUnit::object_attribute_memory_scan_mode_2() {
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

void PixelProcessingUnit::draw_pixels_mode_3() {
	for (uint8_t _ = 0; _ < DOTS_PER_MACHINE_CYCLE; _++) {
		switch (current_pixel_slice_fetcher_step) {
			case PixelSliceFetcherStep::GetTileId:
				if (!background_pixels_queue.empty()) { // and trying to push pixels
					if (!is_bit_set_uint8(1, lcd_control_lcdc)) {
						break;
					}

					if (is_in_first_dot_of_current_step) {
						// iterate the 10 scanned sprites - first with a matching x-value (shifted 8 or something?) is fetched sprite
						// sounds like array should be sorted by x secondarily?
						for (ObjectAttribute object : current_scanline_selected_objects) {
							if (object.x_position == lcd_internal_x_coordinate_lx + 8) {

							}
						}
					}
					else {

					}
				}
				else {
					if (is_in_first_dot_of_current_step) {
						set_background_fetcher_address();
					}
					else {
						fetched_tile_id = memory_interface.read_byte(background_fetcher_address);
						current_pixel_slice_fetcher_step = PixelSliceFetcherStep::GetTileRowLow;
					}
				}
				is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
				break;
			case PixelSliceFetcherStep::GetTileRowLow:
				//handle obj
				if (is_in_first_dot_of_current_step) {
					set_tile_row_address(0);
				}
				else {
					tile_row_low_byte = memory_interface.read_byte(tile_row_address);
					current_pixel_slice_fetcher_step = PixelSliceFetcherStep::GetTileRowHigh;
				}
				is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
				break;
			case PixelSliceFetcherStep::GetTileRowHigh:
				//handle obj
				if (is_in_first_dot_of_current_step) {
					set_tile_row_address(1);
				}
				else {
					tile_row_high_byte = memory_interface.read_byte(tile_row_address);
					current_pixel_slice_fetcher_step = PixelSliceFetcherStep::PushPixels;
				}
				is_in_first_dot_of_current_step = !is_in_first_dot_of_current_step;
				break;
			case PixelSliceFetcherStep::PushPixels:
				switch (current_fetcher_mode) {
					case FetcherMode::BackgroundMode:
					case FetcherMode::WindowMode:
						if (background_pixels_queue.empty()) {
							for (int i = 7; i >= 0; i--) {
								background_pixels_queue.emplace(get_pixel_colour_id(tile_row_low_byte, tile_row_high_byte, i));
							}
							current_pixel_slice_fetcher_step = PixelSliceFetcherStep::GetTileId;
						}
						break;
					case FetcherMode::ObjectMode:
						// add logic
						break;
				}
				break;
		}
	
		//During each cycle, after clocking the Pixel Fetchers, the PPU attempts to push a pixel to the LCD, 
		// which can only be done if there are pixels in the background FIFO. If the Sprite FIFO contains any 
		// pixels, a sprite pixel is shifted out and “merged” with the background pixel.

		// SCX mod 8 pixels are discarded at the start of each scanline rather than being pushed to the LCD, 
		// which is also the cause of PPU Mode 3 being extended by SCX mod 8 cycles.


	}
}

void PixelProcessingUnit::set_background_fetcher_address() {
	background_fetcher_address = static_cast<uint16_t>(0b10011 << 11);

	switch (current_fetcher_mode) {
		case FetcherMode::BackgroundMode:
			if (is_bit_set_uint8(3, lcd_control_lcdc)) {
				background_fetcher_address |= (1 << 10);
			}
			background_fetcher_address |= static_cast<uint8_t>((lcd_y_coordinate_ly + viewport_y_position_scy) / 8) << 5;
			background_fetcher_address |= static_cast<uint8_t>((lcd_internal_x_coordinate_lx + viewport_x_position_scx) / 8);
			break;
		case FetcherMode::WindowMode:
			if (is_bit_set_uint8(6, lcd_control_lcdc)) {
				background_fetcher_address |= (1 << 10);
			}
			background_fetcher_address |= static_cast<uint8_t>(window_internal_line_counter_wly / 8) << 5;
			background_fetcher_address |= static_cast<uint8_t>(lcd_internal_x_coordinate_lx / 8);
			break;
	}
}

void PixelProcessingUnit::set_tile_row_address(uint8_t offset) {
	tile_row_address = static_cast<uint16_t>((0b100 << 13) | (fetched_tile_id << 4));

	switch (current_fetcher_mode) {
		case FetcherMode::BackgroundMode:
			if (!is_bit_set_uint8(4, lcd_control_lcdc) && !is_bit_set_uint8(7, fetched_tile_id)) {
				tile_row_address |= (1 << 12);
			}
			tile_row_address |= ((lcd_y_coordinate_ly + viewport_y_position_scy) % 8) << 1;
			break;
		case FetcherMode::WindowMode:
			if (!is_bit_set_uint8(4, lcd_control_lcdc) && !is_bit_set_uint8(7, fetched_tile_id)) {
				tile_row_address |= (1 << 12);
			}
			tile_row_address |= (window_internal_line_counter_wly % 8) << 1;
			break;
		case FetcherMode::ObjectMode:
			// fill in
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

template <typename T>
void PixelProcessingUnit::clear_queue(std::queue<T> &queue) {
	while (!queue.empty()) {
		queue.pop();
	}
}

} // namespace GameBoy
