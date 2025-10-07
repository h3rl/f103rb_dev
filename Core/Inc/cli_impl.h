/*
 * cli_impl.h - CLI Implementation for STM32F103
 *
 *  Created on: Oct 7, 2025
 *      Author: halva
 *
 * =============================================================================
 * CLI APPLICATION LAYER
 * =============================================================================
 *
 * This file contains the application-specific CLI interface:
 * - Variable exports (led_enabled, cli_logging_enabled)
 * - User initialization function
 * - Application-specific types and definitions
 *
 * =============================================================================
 */

#ifndef INC_CLI_IMPL_H_
#define INC_CLI_IMPL_H_

#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// Exported Variables
// =============================================================================

// LED control state - set this variable to control LED
// When true, LED is on; when false, LED is off
extern bool led_enabled;

// Logging control state - set this variable to enable continuous data logging
// When true, data logging is active; when false, logging is disabled
// Any keypress automatically disables logging
extern bool cli_logging_enabled;

// =============================================================================
// User Initialization Function
// =============================================================================

/**
 * @brief Initialize the CLI with application-specific configuration
 *
 * This function sets up the CLI with all commands, variables, and
 * hardware-specific configuration. Call this instead of cli_init() directly.
 *
 * @param rx_buffer Pointer to UART RX circular buffer
 * @param buffer_size Size of the RX buffer
 * @param dma_counter Pointer to DMA counter register
 */
void cli_user_init(uint8_t *rx_buffer, uint16_t buffer_size,
                   volatile uint32_t *dma_counter);

#endif /* INC_CLI_IMPL_H_ */
