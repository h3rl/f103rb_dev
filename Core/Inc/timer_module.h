/*
 * timer_module.h
 *
 *  Created on: Sep 22, 2025
 *      Author: Halvard
 */

#ifndef INC_TIMER_MODULE_H_
#define INC_TIMER_MODULE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initializes the timer module for high-resolution timing.
 *
 * Enables the DWT cycle counter, which is used for microsecond and millisecond
 * timing functions. Call this once at startup before using any timer functions.
 */
void timer_module_init(void);

/**
 * @brief Returns the number of microseconds since the DWT counter was
 * initialized.
 *
 * @return Microseconds elapsed as a 64-bit unsigned integer.
 */
uint64_t micros(void);

/**
 * @brief Checks if the specified microsecond interval has elapsed since the
 * last timer update.
 *
 * @param timer Pointer to a variable storing the last timer value
 * (microseconds).
 * @param interval_us Interval in microseconds to check.
 * @return true if the interval has elapsed, false otherwise.
 */
bool has_interval_elapsed_us(uint64_t *timer, uint64_t interval_us);

/**
 * @brief Checks if the specified millisecond interval has elapsed since the
 * last timer update.
 *
 * @param timer Pointer to a variable storing the last timer value
 * (milliseconds).
 * @param interval_ms Interval in milliseconds to check.
 * @return true if the interval has elapsed, false otherwise.
 */
bool has_interval_elapsed_ms(uint64_t *timer, uint64_t interval_ms);

#endif /* INC_TIMER_MODULE_H_ */
