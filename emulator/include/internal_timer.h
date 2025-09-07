#pragma once

#include <cstdint>
#include <functional>

namespace GameBoyEmulator
{

class InternalTimer
{
public:
    InternalTimer(std::function<void(uint8_t)> request_interrupt);

    void reset_state();
    void set_post_boot_state();

    void step_forward_one_machine_cycle();

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
    uint8_t tima_timer{};
    uint8_t tma_timer_modulo{};
    uint8_t tac_timer_control{0b11111000};
    bool is_previously_selected_system_counter_bit_set{};
    bool did_tima_overflow_occur{};
    bool is_tima_overflow_handled{};

    void increment_tima_early();
    bool increment_tima_and_does_it_overflow();
};

} // namespace GameBoyEmulator
