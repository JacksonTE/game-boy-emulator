#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

#include "timer.h"

namespace GameBoy
{

constexpr uint8_t INTERRUPT_FLAG_STAT_MASK = 1 << 1;
constexpr uint8_t INTERRUPT_FLAG_VERTICAL_BLANK_MASK = 1 << 0;

constexpr uint16_t VIDEO_RAM_SIZE = 0x2000;
constexpr uint16_t OBJECT_ATTRIBUTE_MEMORY_SIZE = 0x00a0;

constexpr uint16_t VIDEO_RAM_START = 0x8000;
constexpr uint16_t OBJECT_ATTRIBUTE_MEMORY_START = 0xfe00;

constexpr uint8_t DOTS_PER_MACHINE_CYCLE = 4;
constexpr uint8_t PIXELS_PER_TILE_ROW = 8;
constexpr uint8_t MAX_OBJECTS_PER_LINE = 10;

constexpr uint8_t OBJECT_ATTRIBUTE_MEMORY_SCAN_DURATION_DOTS = 80;
constexpr uint16_t SCAN_LINE_DURATION_DOTS = 456;
constexpr uint16_t FIRST_VERTICAL_BLANK_LINE = 144;
constexpr uint16_t FINAL_FRAME_SCAN_LINE = 153;

enum class PixelProcessingUnitEnableStatus
{
    Disabled,
    Enabling,
    Enabled
};

enum class PixelProcessingUnitMode
{
    HorizontalBlank,
    VerticalBlank,
    ObjectAttributeMemoryScan,
    PixelTransfer
};

enum class FetcherMode
{
    BackgroundMode,
    WindowMode
};

enum class PixelSliceFetcherStep
{
    GetTileId,
    GetTileRowLow,
    GetTileRowHigh,
    PushPixels
};

struct ObjectAttributes
{
    int16_t y_position{};
    uint16_t x_position{};
    uint8_t tile_index{};
    uint8_t flags{};
};

struct BackgroundPixel
{
    uint8_t colour_index{};
};

struct ObjectPixel
{
    uint8_t colour_index{};
    bool is_palette_bit_set{};
    bool is_priority_bit_set{};
};

struct PixelSliceFetcher
{
    PixelSliceFetcherStep current_step{PixelSliceFetcherStep::GetTileId};
    uint8_t tile_id{};
    uint8_t tile_row_low{};
    uint8_t tile_row_high{};
    bool is_in_first_dot_of_current_step{true};
    bool is_enabled{};

    virtual void reset_state();
};

struct BackgroundFetcher : PixelSliceFetcher
{
    FetcherMode fetcher_mode{FetcherMode::BackgroundMode};
    std::array<BackgroundPixel, PIXELS_PER_TILE_ROW> tile_row{};

    void reset_state() override;
};

template <typename T>
class PixelPisoShiftRegisters
{
public:
    PixelPisoShiftRegisters(bool should_track_size)
        : is_tracking_size{should_track_size}
    {
    }

    void load_new_tile_row(std::array<T, PIXELS_PER_TILE_ROW> new_entries)
    {
        if (is_tracking_size)
            current_size = PIXELS_PER_TILE_ROW;

        entries = new_entries;
    }

    T shift_out()
    {
        if (is_tracking_size)
        {
            if (current_size == 0)
                std::cout << "Warning: attempted to shift out of an empty PISO shift register while tracking its size.\n";
            else
                current_size--;
        }

        T head = entries[0];
        for (uint8_t i = 0; i < PIXELS_PER_TILE_ROW - 1; i++)
        {
            entries[i] = entries[i + 1];
        }
        entries[PIXELS_PER_TILE_ROW - 1] = T{};
        return head;
    }

    void clear() {
        if (is_tracking_size)
            current_size = 0;

        entries.fill(T{});
    }

    T &operator[](uint8_t index)
    {
        return entries[index];
    }

    bool is_empty()
    {
        return is_tracking_size && current_size == 0;
    }

private:
    bool is_tracking_size{};
    uint8_t current_size{};
    std::array<T, PIXELS_PER_TILE_ROW> entries{};
};

class PixelProcessingUnit
{
public:
    uint8_t viewport_y_position_scy{};
    uint8_t viewport_x_position_scx{};
    uint8_t lcd_y_coordinate_compare_lyc{};
    uint8_t object_attribute_memory_direct_memory_access_dma{};
    uint8_t background_palette_bgp{};
    uint8_t object_palette_0_obp0{};
    uint8_t object_palette_1_obp1{};
    uint8_t window_y_position_wy{};
    uint8_t window_x_position_plus_7_wx{};

    bool is_oam_dma_in_progress{};

    PixelProcessingUnit(std::function<void(uint8_t)> request_interrupt);

    void reset_state();
    void set_post_boot_state();

    uint8_t read_lcd_control_lcdc() const;
    void write_lcd_control_lcdc(uint8_t value);

    uint8_t read_lcd_status_stat() const;
    void write_lcd_status_stat(uint8_t value);

    uint8_t read_lcd_y_coordinate_ly() const;

    uint8_t read_byte_video_ram(uint16_t memory_address, bool is_access_for_oam_dma) const;
    void write_byte_video_ram(uint16_t memory_address, uint8_t value);

    uint8_t read_byte_object_attribute_memory(uint16_t memory_address) const;
    void write_byte_object_attribute_memory(uint16_t memory_address, uint8_t value);

    void step_single_machine_cycle();

private:
    std::function<void(uint8_t)> request_interrupt_callback;

    std::unique_ptr<uint8_t[]> video_ram;
    std::unique_ptr<uint8_t[]> object_attribute_memory;

    uint8_t lcd_control_lcdc{};
    uint8_t lcd_status_stat{0b10000000};
    uint8_t lcd_y_coordinate_ly{};
    uint8_t lcd_internal_x_coordinate_lx{};
    uint8_t window_internal_line_counter_wly{};

    PixelProcessingUnitMode previous_mode{PixelProcessingUnitMode::HorizontalBlank};
    PixelProcessingUnitMode current_mode{PixelProcessingUnitMode::HorizontalBlank};
    uint16_t current_scanline_dot_number{};
    bool is_in_first_scanline_after_enable{};
    bool is_in_first_dot_of_current_step{true};
    bool is_window_enabled_for_scanline{};

    uint8_t stat_value_after_spurious_interrupt{};
    bool did_spurious_stat_interrupt_occur{};
    bool were_stat_interrupts_handled_early{};
    bool are_stat_interrupts_blocked{};
    bool did_line_end_during_this_machine_cycle{};

    std::vector<ObjectAttributes> scanline_selected_objects;
    uint8_t current_object_index{};
    
    uint8_t scanline_pixels_to_discard_from_dummy_fetch_count{8};
    int scanline_pixels_to_discard_from_scrolling_count{-1};

    BackgroundFetcher background_fetcher{};
    PixelSliceFetcher object_fetcher{};
    
    PixelPisoShiftRegisters<BackgroundPixel> background_pixel_shift_registers{true};
    PixelPisoShiftRegisters<ObjectPixel> object_pixel_shift_registers{false};

    void step_object_attribute_memory_scan_single_dot();
    void step_pixel_transfer_single_dot();
    void step_horizontal_blank_single_dot();
    void step_vertical_blank_single_dot();
    void trigger_stat_interrupts();

    void initialize_pixel_transfer();
    void step_fetchers_single_dot();

    void step_background_fetcher_single_dot();
    uint8_t get_background_fetcher_tile_id();
    uint8_t get_background_fetcher_tile_row_byte(uint8_t offset);

    void step_object_fetcher_single_dot();
    uint8_t get_object_fetcher_tile_row_byte(uint8_t offset);

    uint8_t read_byte_object_attribute_memory_internally(uint16_t memory_address) const;

    ObjectAttributes get_current_object() const;
    uint8_t get_pixel_colour_id(PixelSliceFetcher pixel_slice_fetcher, uint8_t bit_position) const;
};

} // namespace GameBoy
