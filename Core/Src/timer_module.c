/*
 * timer_module.c
 *
 *  Created on: Sep 22, 2025
 *      Author: Halvard
 */
#include "timer_module.h"

// Replace with your specific STM32 series header if needed
#include "stm32f1xx_hal.h"

static uint32_t last_seen = 0;
static uint32_t local_overflow = 0;

uint64_t micros(void) {
    __disable_irq();
    uint32_t current = DWT->CYCCNT;
    if (current < last_seen) {
        local_overflow++;  // overflow happened during this call
    }
    last_seen = current;
    __enable_irq();

    uint64_t total_cycles = ((uint64_t)local_overflow << 32) | current;
    return total_cycles / 72;
}

void timer_module_init(void) {
    // Enable DWT cycle counter for timing
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

bool has_interval_elapsed_us(uint64_t *timer, uint64_t interval_us) {
    uint64_t currentTime = micros();
    if (currentTime - *timer >= interval_us) {
        *timer = currentTime;
        return true; // Timer has elapsed
    }
    return false; // Timer has not elapsed
}

bool has_interval_elapsed_ms(uint64_t *timer, uint64_t interval_ms) {
    uint64_t currentTime = micros() / 1000; // Convert microseconds to milliseconds
    if ((currentTime - *timer) >= interval_ms) {
        *timer = currentTime;
        return true; // Timer has elapsed
    }
    return false; // Timer has not elapsed
}
