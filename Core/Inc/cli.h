/*
 * cli.h - Portable CLI Core Engine
 *
 *  Created on: Oct 6, 2025
 *      Author: halva
 *
 * =============================================================================
 * PORTABLE COMMAND LINE INTERFACE ENGINE
 * =============================================================================
 *
 * This is a portable, reusable CLI core that handles:
 * - Command parsing and tokenization
 * - Command history with up/down arrow navigation
 * - Tab completion for commands
 * - Line editing (backspace, cursor control)
 * - Circular buffer management for UART RX
 *
 * To use this CLI in your project:
 * 1. Implement your commands in cli_impl.c
 * 2. Register commands via cli_config_t
 * 3. Provide a putchar function for output
 * 4. Call cli_init() with configuration
 * 5. Call cli_update() in your main loop
 *
 * =============================================================================
 */

#ifndef INC_CLI_H_
#define INC_CLI_H_

#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// Command Handler Callback Type
// =============================================================================

/**
 * @brief Command handler callback function
 * @param argc Number of arguments (including command name)
 * @param argv Array of argument strings
 *
 * Example: For input "set led true"
 *   argc = 3
 *   argv[0] = "set"
 *   argv[1] = "led"
 *   argv[2] = "true"
 */
typedef void (*cli_command_handler_t)(int argc, char *argv[]);

/**
 * @brief Command definition structure
 */
typedef struct {
  const char *name;              // Command name (e.g., "help", "set")
  cli_command_handler_t handler; // Function to call when command is invoked
} cli_command_t;

// =============================================================================
// CLI Configuration Structure
// =============================================================================

/**
 * @brief CLI initialization configuration
 */
typedef struct {
  // Command configuration
  const cli_command_t *commands; // Array of available commands
  uint8_t num_commands;          // Number of commands in array

  // I/O functions
  void (*putchar_fn)(char c); // Function to output a character
  int (*getchar_fn)(
      void); // Function to get a character (returns -1 if none available)
} cli_config_t;

// =============================================================================
// Core CLI Functions
// =============================================================================

/**
 * @brief Initialize the CLI engine
 * @param config Configuration structure with commands and I/O functions
 *
 * The config must provide:
 * - commands: Array of command definitions
 * - num_commands: Number of commands
 * - putchar_fn: Function to output characters
 * - getchar_fn: Function to read characters (returns -1 if none available)
 */
void cli_init(const cli_config_t *config);

/**
 * @brief Update CLI - processes incoming characters and executes commands
 *
 * Call this frequently in your main loop. It will process all available
 * characters by calling the getchar_fn and handle command execution.
 */
void cli_update(void);

#endif /* INC_CLI_H_ */
