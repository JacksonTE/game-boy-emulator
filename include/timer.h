#pragma once

#include <cstdint>
#include <functional>

namespace GameBoy {

class Timer {
public:
	Timer(std::function<void(uint8_t)> request_interrupt_callback);

	void step_single_machine_cycle();

	uint8_t read_divider_div() const;
	uint8_t read_timer_tima() const;
	uint8_t read_timer_modulo_tma() const;
	uint8_t read_timer_control_tac() const;

	void write_divider_div(uint8_t value);
	void write_timer_tima(uint8_t value);
	void write_timer_modulo_tma(uint8_t value);
	void write_timer_control_tac(uint8_t value);

private:
	std::function<void(uint8_t)> request_interrupt;
	uint16_t system_counter{};
	uint8_t timer_tima{};
	uint8_t timer_modulo_tma{};
	uint8_t timer_control_tac{};
	bool is_previously_selected_system_counter_bit_set{};
	bool did_timer_tima_overflow{};
	bool is_timer_tima_overflow_handled{};

	bool conditionally_increment_timer_tima_and_get_overflow_status();
};

} // namespace GameBoy
