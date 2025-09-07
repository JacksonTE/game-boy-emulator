#include "memory_management_unit.h"
#include "internal_timer.h"

namespace GameBoyEmulator
{

InternalTimer::InternalTimer(std::function<void(uint8_t)> request_interrupt)
    : request_interrupt_callback{request_interrupt}
{
}

void InternalTimer::reset_state()
{
    system_counter = 0;
    tima_timer = 0;
    tma_timer_modulo = 0;
    tac_timer_control = 0b11111000;
    is_previously_selected_system_counter_bit_set = false;
    did_tima_overflow_occur = false;
    is_tima_overflow_handled = false;
}

void InternalTimer::set_post_boot_state()
{
    reset_state();
    system_counter = 0xABC8;
}

void InternalTimer::step_forward_one_machine_cycle()
{
    system_counter += 4;

    if (did_tima_overflow_occur)
    {
        request_interrupt_callback(TIMER_INTERRUPT_FLAG_MASK);
        tima_timer = tma_timer_modulo;
    }
    is_tima_overflow_handled = did_tima_overflow_occur;
    did_tima_overflow_occur = increment_tima_and_does_it_overflow();
}

uint8_t InternalTimer::read_div() const
{
    return static_cast<uint8_t>(system_counter >> 8);
}

uint8_t InternalTimer::read_tima() const
{
    return tima_timer;
}

uint8_t InternalTimer::read_tma() const
{
    return tma_timer_modulo;
}

uint8_t InternalTimer::read_tac() const
{
    return 0b11111000 | tac_timer_control;
}

void InternalTimer::write_div(uint8_t value)
{
    system_counter = 0x0000;
    increment_tima_early();
}

void InternalTimer::write_tima(uint8_t value)
{
    if (is_tima_overflow_handled)
    {
        return;
    }
    tima_timer = value;
    did_tima_overflow_occur = false;
}

void InternalTimer::write_tma(uint8_t value)
{
    if (is_tima_overflow_handled)
    {
        tima_timer = value;
    }
    tma_timer_modulo = value;
}

void InternalTimer::write_tac(uint8_t value)
{
    tac_timer_control = 0b11111000 | value;
    increment_tima_early();
}

void InternalTimer::increment_tima_early()
{
    if (increment_tima_and_does_it_overflow())
    {
        request_interrupt_callback(TIMER_INTERRUPT_FLAG_MASK);
        tima_timer = tma_timer_modulo;
    }
}

bool InternalTimer::increment_tima_and_does_it_overflow()
{
    const bool is_tima_enabled = (tac_timer_control & 0b00000100) != 0;

    const uint8_t clock_select = tac_timer_control & 0b00000011;
    const uint8_t clock_select_to_selected_system_counter_bit[4] = {9, 3, 5, 7};
    const uint8_t selected_system_counter_bit = clock_select_to_selected_system_counter_bit[clock_select];
    const bool is_selected_system_counter_bit_set = is_tima_enabled && (system_counter & (1 << selected_system_counter_bit)) != 0;

    const bool did_overflow_occur = !is_selected_system_counter_bit_set &&
                                    is_previously_selected_system_counter_bit_set &&
                                    (++tima_timer == 0);

    is_previously_selected_system_counter_bit_set = is_selected_system_counter_bit_set;
    return did_overflow_occur;
}

} // namespace GameBoyEmulator
