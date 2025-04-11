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

enum class PixelProcessingUnitMode {
	HorizontalBlank,
	VerticalBlank,
	ObjectAttributeMemoryScan,
	DrawingPixels
};

enum class FetcherMode {
	BackgroundMode,
	WindowMode,
	ObjectMode
};

enum class PixelSliceFetcherStep {
	GetTileId,
	GetTileRowLow,
	GetTileRowHigh,
	PushPixels
};

struct ObjectAttribute {
	uint8_t y_position{};
	uint8_t x_position{};
	uint8_t tile_index{};
	uint8_t attributes{};
};

struct BackgroundPixelQueueEntry {
	uint8_t colour_index{};

	BackgroundPixelQueueEntry() {
	}

	BackgroundPixelQueueEntry(uint8_t colour)
		: colour_index{colour} {
	}
};

struct ObjectPixelQueueEntry {
	uint8_t colour_index{};
	bool is_palette_bit_set{};
	bool is_priority_bit_set{};

	ObjectPixelQueueEntry() {
	}
	
	ObjectPixelQueueEntry(uint8_t colour, bool palette, bool priority)
		: colour_index{colour},
		  is_palette_bit_set{palette},
		  is_priority_bit_set{priority} {
	}
};

template <typename EntryType>
class PixelQueue {
public:
	FetcherMode fetcher_mode{}; // TODO maybe separate this into different states eventually
	PixelSliceFetcherStep fetcher_step{PixelSliceFetcherStep::GetTileId};
	uint16_t tile_id_fetch_address{}; // TODO separate this into different states eventually
	uint8_t fetched_tile_id{};
	uint16_t tile_row_fetch_address{};
	uint8_t fetched_tile_row_low_byte{};
	uint8_t fetched_tile_row_high_byte{};
	bool is_fetcher_enabled{};
	
	PixelQueue(bool fetcher_enabled, bool pixel_output_enabled, FetcherMode mode)
		: is_fetcher_enabled{fetcher_enabled},
		  is_pixel_output_enabled{pixel_output_enabled},
		  fetcher_mode{mode} {
	}

	inline void clear() {
		if (fetcher_mode == FetcherMode::ObjectMode) {
			for (EntryType entry : entries) {
				entry = EntryType{};
			}
		}
		else {
			front = 0;
			back = 0;
			size = 0;
		}
	}

	inline void push_back(EntryType entry) {
		entries[back] = entry;
		back = (back + 1) % 8;
		size++;
	}

	inline EntryType pop_front() {
		EntryType entry = entries[entry];
		front = (front + 1) % 8;
		size--;
		return entry;
	}

	inline EntryType &operator[](uint8_t index) {
		return entries[(front + index) % 8];
	}

	inline bool is_empty() const {
		return size == 0;
	}

	inline bool is_full() const {
		return size == 8;
	}

private:
	std::array<EntryType, 8> entries{};
	uint8_t front{0};
	uint8_t back{7};
	uint8_t size{8};
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

	PixelQueue<BackgroundPixelQueueEntry> background_pixels_queue{true, true, FetcherMode::BackgroundMode};
	PixelQueue<ObjectPixelQueueEntry> object_pixels_queue{false, false, FetcherMode::ObjectMode};

	uint16_t current_scanline_elapsed_dots_count{};
	PixelProcessingUnitMode current_mode{PixelProcessingUnitMode::ObjectAttributeMemoryScan};
	bool is_in_first_dot_of_current_step{true};
	bool is_pixel_output_enabled{true};

	std::vector<ObjectAttribute> current_scanline_selected_objects;
	uint8_t current_scanline_next_object_index{};

	void step_single_machine_cycle();
	void step_mode_2_single_machine_cycle();
	void step_mode_3_single_machine_cycle();

	void set_tile_id_fetch_address(PixelQueue<BackgroundPixelQueueEntry> &pixel_queue);

	template <typename EntryType>
	void set_tile_row_fetch_address(PixelQueue<EntryType> &pixel_queue, uint8_t offset);

	template <typename EntryType>
	void step_pixel_fetcher_single_dot(PixelQueue<EntryType> &pixel_queue);

	uint8_t read_object_attribute_memory_byte(uint16_t address_offset);
	bool is_bit_set_uint8(const uint8_t &bit_position_to_test, const uint8_t &uint8) const;
	uint8_t get_pixel_colour_id(uint8_t tile_low_byte, uint8_t tile_high_byte, uint8_t bit_position) const;
};

} // namespace GameBoy
