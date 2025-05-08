#pragma once

#include <cstdint>
#include <functional>

namespace GameBoyCore
{

class InternalTimer
{
public:
    InternalTimer(std::function<void(uint8_t)> request_interrupt);

    void reset_state();
    void set_post_boot_state();

    void step_single_machine_cycle();

    uint8_t read_div() const;
    uint8_t read_tima() const;
    uint8_t read_tma() const;
    uint8_t read_tac() const;

    void write_div(uint8_t value);
    void write_tima(uint8_t value);
    void write_tma(uint8_t value);
    void write_tac(uint8_t value);

private:
    std::function<void(uint8_t)> request_interrupt_callback;
    uint16_t system_counter{};
    uint8_t timer_tima{};
    uint8_t timer_modulo_tma{};
    uint8_t timer_control_tac{0b11111000};
    bool is_previously_selected_system_counter_bit_set{};
    bool did_tima_overflow_occur{};
    bool is_tima_overflow_handled{};

    void update_tima_early();
    bool update_tima_and_get_overflow_state();
};

} // namespace GameBoyCore
