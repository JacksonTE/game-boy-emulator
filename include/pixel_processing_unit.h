#pragma once

#include <array>
#include <cstdint>
#include <queue>
#include <vector>
#include "memory_management_unit.h"

namespace GameBoy {

constexpr uint8_t DOTS_PER_MACHINE_CYCLE = 4;

constexpr uint8_t OAM_SCAN_DURATION_DOTS = 80;
constexpr uint16_t SCAN_LINE_DURATION_DOTS = 456;
constexpr uint16_t FINAL_DRAWING_SCAN_LINE = 143;
constexpr uint16_t FINAL_FRAME_SCAN_LINE = 153;

struct ObjectAttribute {
	uint8_t y_position{};
	uint8_t x_position{};
	uint8_t tile_index{};
	uint8_t attributes{};
};

struct BackgroundPixelQueueEntry {
	uint8_t colour_index{};

	BackgroundPixelQueueEntry(uint8_t colour)
		: colour_index{colour} {
	}
};

struct ObjectPixelQueueEntry {
	uint8_t colour_index{};
	bool is_palette_bit_set{};
	bool is_priority_bit_set{};

	ObjectPixelQueueEntry(uint8_t colour, bool palette, bool priority)
		: colour_index{colour},
		  is_palette_bit_set{palette},
		  is_priority_bit_set{priority} {
	}
};



enum class PixelProcessingUnitMode {
	HorizontalBlank,
	VerticalBlank,
	ObjectAttributeMemoryScan,
	DrawingPixels
};

enum class PixelSliceFetcherStep {
	GetTileId,
	GetTileRowLow,
	GetTileRowHigh,
	PushPixels
};

enum class FetcherMode {
	BackgroundMode,
	WindowMode,
	ObjectMode
};

class PixelProcessingUnit {
public:
	PixelProcessingUnit(MemoryManagementUnit &memory_management_unit);

private:
	MemoryManagementUnit &memory_interface;

	uint8_t lcd_control_lcdc{};
	uint8_t lcd_status_stat{};
	uint8_t viewport_y_position_scy{};
	uint8_t viewport_x_position_scx{};
	uint8_t lcd_y_coordinate_ly{};
	uint8_t lcd_y_coordinate_compare_lyc{};
	uint8_t lcd_internal_x_coordinate_lx{};
	//dma
	uint8_t background_palette_bgp{};
	uint8_t object_palette_0_obp0{};
	uint8_t object_palette_1_obp1{};
	uint8_t window_y_position_wy{};
	uint8_t window_x_position_plus_7_wx{};
	uint8_t window_internal_line_counter_wly{};

	std::queue<BackgroundPixelQueueEntry> background_pixels_queue;
	std::queue<ObjectPixelQueueEntry> object_pixels_queue;

	uint16_t current_scanline_elapsed_dots_count{};
	PixelProcessingUnitMode current_mode{PixelProcessingUnitMode::ObjectAttributeMemoryScan};
	PixelSliceFetcherStep current_pixel_slice_fetcher_step{};
	FetcherMode current_fetcher_mode{};
	bool is_in_first_dot_of_current_step{true};

	std::vector<ObjectAttribute> current_scanline_selected_objects;
	uint16_t background_fetcher_address{};
	uint16_t tile_row_address{};
	uint8_t fetched_tile_id{};
	uint16_t tile_row_low_byte{};
	uint8_t tile_row_high_byte{};
	

	void step_single_machine_cycle();
	void object_attribute_memory_scan_mode_2();
	void draw_pixels_mode_3();
	void set_background_fetcher_address();
	void set_tile_row_address(uint8_t offset);
	uint8_t read_object_attribute_memory_byte(uint16_t address_offset);

	bool is_bit_set_uint8(const uint8_t &bit_position_to_test, const uint8_t &uint8) const;
	uint8_t get_pixel_colour_id(uint8_t tile_low_byte, uint8_t tile_high_byte, uint8_t bit_position) const;

	template <typename T>
	void clear_queue(std::queue<T> &queue);
};

} // namespace GameBoy
