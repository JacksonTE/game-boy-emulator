#include "memory_management_unit.h"
#include "timer.h"

namespace GameBoy {

Timer::Timer(std::function<void(uint8_t)> request_interrupt_callback)
    : request_interrupt{request_interrupt_callback} {
}

void Timer::step_single_machine_cycle() {
    system_counter += 4;

    if (did_timer_tima_overflow_occur) {
        request_interrupt(INTERRUPT_FLAG_TIMER_MASK);
        timer_tima = timer_modulo_tma;
    }
    is_timer_tima_overflow_handled = did_timer_tima_overflow_occur;
    did_timer_tima_overflow_occur = update_timer_tima_and_get_overflow_state();
}

uint8_t Timer::read_divider_div() const {
    return static_cast<uint8_t>(system_counter >> 8);
}

uint8_t Timer::read_timer_tima() const {
    return timer_tima;
}

uint8_t Timer::read_timer_modulo_tma() const {
    return timer_modulo_tma;
}

uint8_t Timer::read_timer_control_tac() const {
    return timer_control_tac;
}

void Timer::write_divider_div(uint8_t value) {
    system_counter = 0x0000;
    update_timer_tima_early();
}

void Timer::write_timer_tima(uint8_t value) {
    if(is_timer_tima_overflow_handled) {
        return;
    }
    timer_tima = value;
    did_timer_tima_overflow_occur = false;
}

void Timer::write_timer_modulo_tma(uint8_t value) {
    timer_modulo_tma = value;

    if (is_timer_tima_overflow_handled) {
        timer_tima = timer_modulo_tma;
    }
}

void Timer::write_timer_control_tac(uint8_t value) {
    timer_control_tac = value;
    update_timer_tima_early();
}

void Timer::update_timer_tima_early() {
    if (update_timer_tima_and_get_overflow_state()) {
        request_interrupt(INTERRUPT_FLAG_TIMER_MASK);
        timer_tima = timer_modulo_tma;
    }
}

bool Timer::update_timer_tima_and_get_overflow_state() {
    const bool is_timer_tima_enabled = (timer_control_tac & 0b00000100) != 0;

    const uint8_t clock_select = timer_control_tac & 0b00000011;
    const uint8_t clock_select_to_selected_system_counter_bit[4] = {9, 3, 5, 7};
    const uint8_t selected_system_counter_bit = clock_select_to_selected_system_counter_bit[clock_select];
    const bool is_selected_system_counter_bit_set =
        is_timer_tima_enabled && (system_counter & (1 << selected_system_counter_bit)) != 0;

    const bool did_overflow_occur = !is_selected_system_counter_bit_set &&
        is_previously_selected_system_counter_bit_set &&
        (++timer_tima == 0);

    is_previously_selected_system_counter_bit_set = is_selected_system_counter_bit_set;
    return did_overflow_occur;
}

} // namespace Gameboy
