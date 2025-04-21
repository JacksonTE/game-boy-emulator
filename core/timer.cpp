#include "memory_management_unit.h"
#include "timer.h"

namespace GameBoy
{

Timer::Timer(std::function<void(uint8_t)> request_interrupt)
    : request_interrupt_callback{request_interrupt}
{
}

void Timer::reset_state()
{
    uint16_t system_counter = 0;
    uint8_t timer_tima = 0;
    uint8_t timer_modulo_tma = 0;
    uint8_t timer_control_tac = 0b11111000;
    bool is_previously_selected_system_counter_bit_set = false;
    bool did_tima_overflow_occur = false;
    bool is_tima_overflow_handled = false;
}

void Timer::set_post_boot_state()
{
    reset_state();
    system_counter = 0xabc4; // TODO verify that the 0xc4 portion is correct
}

void Timer::step_single_machine_cycle()
{
    system_counter += 4;

    if (did_tima_overflow_occur)
    {
        request_interrupt_callback(INTERRUPT_FLAG_TIMER_MASK);
        timer_tima = timer_modulo_tma;
    }
    is_tima_overflow_handled = did_tima_overflow_occur;
    did_tima_overflow_occur = update_tima_and_get_overflow_state();
}

uint8_t Timer::read_div() const
{
    return static_cast<uint8_t>(system_counter >> 8);
}

uint8_t Timer::read_tima() const
{
    return timer_tima;
}

uint8_t Timer::read_tma() const
{
    return timer_modulo_tma;
}

uint8_t Timer::read_tac() const
{
    return 0b11111000 | timer_control_tac;
}

void Timer::write_div(uint8_t value)
{
    system_counter = 0x0000;
    update_tima_early();
}

void Timer::write_tima(uint8_t value)
{
    if (is_tima_overflow_handled)
        return;

    timer_tima = value;
    did_tima_overflow_occur = false;
}

void Timer::write_tma(uint8_t value)
{
    timer_modulo_tma = value;

    if (is_tima_overflow_handled)
    {
        timer_tima = timer_modulo_tma;
    }
}

void Timer::write_tac(uint8_t value)
{
    timer_control_tac = 0b11111000 | value;
    update_tima_early();
}

void Timer::update_tima_early()
{
    if (update_tima_and_get_overflow_state())
    {
        request_interrupt_callback(INTERRUPT_FLAG_TIMER_MASK);
        timer_tima = timer_modulo_tma;
    }
}

bool Timer::update_tima_and_get_overflow_state()
{
    const bool is_tima_enabled = (timer_control_tac & 0b00000100) != 0;

    const uint8_t clock_select = timer_control_tac & 0b00000011;
    const uint8_t clock_select_to_selected_system_counter_bit[4] = {9, 3, 5, 7};
    const uint8_t selected_system_counter_bit = clock_select_to_selected_system_counter_bit[clock_select];
    const bool is_selected_system_counter_bit_set = is_tima_enabled && (system_counter & (1 << selected_system_counter_bit)) != 0;

    const bool did_overflow_occur = !is_selected_system_counter_bit_set &&
        is_previously_selected_system_counter_bit_set &&
        (++timer_tima == 0);

    is_previously_selected_system_counter_bit_set = is_selected_system_counter_bit_set;
    return did_overflow_occur;
}

} // namespace Gameboy
