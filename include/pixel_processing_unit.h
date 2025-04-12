#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <queue>
#include <vector>
#include "memory_management_unit.h"

namespace GameBoy {

constexpr uint16_t VIDEO_RAM_SIZE = 0x2000;
constexpr uint8_t OBJECT_ATTRIBUTE_MEMORY_SIZE = 160;
constexpr uint8_t OBJECT_ATTRIBUTE_MEMORY_OBJECT_COUNT = 40;

constexpr uint8_t DOTS_PER_MACHINE_CYCLE = 4;
constexpr uint8_t PIXELS_PER_TILE_ROW = 8;
constexpr uint8_t MAX_OBJECTS_PER_LINE = 10;

constexpr uint8_t OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS = 80;
constexpr uint16_t SCAN_LINE_DURATION_DOTS = 456;
constexpr uint16_t FINAL_DRAWING_SCAN_LINE = 143;
constexpr uint16_t FINAL_FRAME_SCAN_LINE = 153;

enum class PixelProcessingUnitMode {
	HorizontalBlank,
	VerticalBlank,
	ObjectAttributeMemoryScan,
	DrawingPixels
};

enum class FetcherMode {
	BackgroundMode,
	WindowMode
};

enum class PixelSliceFetcherStep {
	GetTileId,
	GetTileRowLow,
	GetTileRowHigh,
	PushPixels
};

struct ObjectAttributes {
	uint8_t y_position{};
	uint8_t x_position{};
	uint8_t tile_index{};
	uint8_t flags{};
};

struct PixelSliceFetcher {
	PixelSliceFetcherStep current_step{PixelSliceFetcherStep::GetTileId};
	uint8_t tile_id{};
	uint16_t tile_row_fetch_address{};
	uint8_t tile_row_low_byte{};
	uint8_t tile_row_high_byte{};
	bool is_enabled{};
};

struct ObjectPixelSliceFetcher : PixelSliceFetcher {
	uint8_t next_object_index{};
};

struct BackgroundPixelSliceFetcher : PixelSliceFetcher {
	FetcherMode fetcher_mode{FetcherMode::BackgroundMode};
	uint16_t tile_id_fetch_address{};
};

struct BackgroundPixel {
	uint8_t colour_index_low_bit;
	uint8_t colour_index_high_bit;
};

struct ObjectPixel {
	uint8_t colour_index_low_bit;
	uint8_t colour_index_high_bit;
	uint8_t palette;
	uint8_t priority;
};

template <typename T>
class PixelPisoShiftRegisters {
public:
	PixelPisoShiftRegisters(bool should_track_size)
		: is_tracking_size{should_track_size};

	void load(std::array<T, PIXELS_PER_TILE_ROW> new_entries) {
		if (is_tracking_size) {
			current_size = PIXELS_PER_TILE_ROW;
		}
		entries = new_entries;
	}

	T shift_out() {
		if (is_tracking_size) {
			if (current_size == 0) {
				std::cout << "Warning: attempted to shift out of an empty PISO shift register while tracking its size."
			}
			else {
				current_size--;
			}
		}
		T head = entries[0];
		for (uint8_t i = 0; i < PIXELS_PER_TILE_ROW - 1; ++i) {
			entries[i] = entries[i + 1];
		}
		entries[PIXELS_PER_TILE_ROW - 1] = 0;
		return head;
	}

	void clear() {
		if (is_tracking_size) {
			current_size = 0;
		}
		entries.fill(T{});
	}

	T &operator[](uint8_t index) {
		return entries[index];
	}

	bool is_empty() {
		return is_tracking_size && current_size == 0;
	}

private:
	bool is_tracking_size{};
	uint8_t current_size{};
	std::array<T, PIXELS_PER_TILE_ROW> entries{};
};

class PixelProcessingUnit {
public:
	PixelProcessingUnit(std::function<void(uint8_t)> request_interrupt_callback);

	uint8_t read_byte_video_ram(uint16_t local_address) const;
	void write_byte_video_ram(uint16_t local_address, uint8_t value);

	uint8_t read_byte_object_attribute_memory(uint16_t local_address) const;
	void write_byte_object_attribute_memory(uint16_t local_address, uint8_t value);

private:
	std::function<void(uint8_t)> request_interrupt;

	std::unique_ptr<uint8_t[]> video_ram;
	std::unique_ptr<ObjectAttributes[]> object_attribute_memory;

	uint8_t lcd_control_lcdc;
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

	PixelPisoShiftRegisters<BackgroundPixel> background_pixels_queue{true};
	PixelPisoShiftRegisters<ObjectPixel> object_pixels_queue{false};
	ObjectPixelSliceFetcher object_pixel_slice_fetcher{};
	BackgroundPixelSliceFetcher background_pixel_slice_fetcher{};

	uint16_t current_scanline_elapsed_dots_count{};
	PixelProcessingUnitMode current_mode{PixelProcessingUnitMode::ObjectAttributeMemoryScan};
	bool is_in_first_dot_of_current_step{true};
	bool is_pixel_output_enabled{true};

	std::vector<ObjectAttributes> current_scanline_selected_objects;
	
	void step_single_machine_cycle();
	void step_mode_2_single_machine_cycle();
	void step_mode_3_single_machine_cycle();

	void step_active_pixel_fetcher_single_dot();
	void set_background_pixel_fetcher_tile_id_fetch_address();
	void set_active_pixel_fetcher_tile_row_fetch_address(uint8_t offset);

	bool is_bit_set_uint8(const uint8_t &bit_position_to_test, const uint8_t &uint8) const;
	uint8_t get_pixel_colour_id(uint8_t tile_low_byte, uint8_t tile_high_byte, uint8_t bit_position) const;
};

} // namespace GameBoy
