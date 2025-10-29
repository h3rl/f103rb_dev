/*
 * cli_impl.c - CLI Implementation for STM32F103
 *
 *  Created on: Oct 7, 2025
 *      Author: halva
 *
 * =============================================================================
 * CLI APPLICATION LAYER
 * =============================================================================
 *
 * This file contains all application-specific CLI code:
 * - Variable definitions and registry
 * - Command handlers (help, list, get, set, status, reset, info)
 * - UART output function
 * - User initialization
 *
 * TO ADD NEW VARIABLES:
 * 1. Declare the variable in the "Variables" section below
 * 2. Add entry to cli_vars[] array
 * 3. Use VAR_BOOL, VAR_INT, VAR_FLOAT, or VAR_STRING type
 *
 * TO ADD NEW COMMANDS:
 * 1. Implement command handler function: void cli_cmd_xxx(int argc, char *argv[])
 * 2. Add entry to app_commands[] array
 *
 * =============================================================================
 */

#include "cli_impl.h"
#include "cli.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// Variable Types
// =============================================================================

typedef enum {
    VAR_BOOL,
    VAR_INT,
    VAR_FLOAT,
    VAR_STRING
} var_type_t;

typedef struct {
    const char *name;
    const char *description;
    var_type_t type;
    void *ptr;
} cli_var_t;

// =============================================================================
// Variables
// =============================================================================

// Example variables - feel free to add your own!
//static bool debug_enabled = false;
//static bool verbose_mode = false;
//static bool test_mode = false;
//static bool imu_calibrate = false;

// Exported variables (accessible from other files)
int led_mode = 1;           // LED control state
float led_blink_rate = 1.0f; // LED blink rate in Hz
bool imu_logging_enabled = false;  // IMU logging state

// Variable registry - add new variables here!
static cli_var_t cli_vars[] = {
    // Led settings
    {"ledmode",   "LED mode (0=off,1=on,2=blink)",     VAR_INT,   &led_mode},
    {"ledrate",  "LED blink rate in Hz",               VAR_FLOAT, &led_blink_rate},

    // imu logging to console
    {"imulog",        "Enable imu logging to console",   VAR_BOOL,  &imu_logging_enabled},
};

#define NUM_VARS (sizeof(cli_vars) / sizeof(cli_vars[0]))

// =============================================================================
// DMA Buffer State (for getchar implementation)
// =============================================================================

static uint8_t *cli_dma_rx_buffer = NULL;
static uint16_t cli_dma_buffer_size = 0;
static volatile uint32_t *cli_dma_counter = NULL;
static uint16_t cli_last_processed = 0;

// =============================================================================
// UART I/O Functions
// =============================================================================

static void cli_uart_putchar_impl(char c)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)&c, 1, HAL_MAX_DELAY);
}

static int cli_uart_getchar_impl(void)
{
    if (cli_dma_counter == NULL || cli_dma_rx_buffer == NULL)
    {
        return -1; // Not initialized
    }
    
    // Calculate current RX position from DMA counter
    uint16_t current_pos = cli_dma_buffer_size - *cli_dma_counter;
    
    // Check if there's new data
    if (cli_last_processed != current_pos)
    {
        char ch = (char)cli_dma_rx_buffer[cli_last_processed];
        cli_last_processed = (cli_last_processed + 1) % cli_dma_buffer_size;
        return (int)ch;
    }
    
    return -1; // No new character
}

static void cli_puts(const char *s)
{
    while (*s)
    {
        cli_uart_putchar_impl(*s++);
    }
}

// =============================================================================
// Forward Declarations
// =============================================================================

void cli_cmd_help(int argc, char *argv[]);
void cli_cmd_info(int argc, char *argv[]);
void cli_cmd_list(int argc, char *argv[]);
void cli_cmd_get(int argc, char *argv[]);
void cli_cmd_set(int argc, char *argv[]);
void cli_cmd_status(int argc, char *argv[]);
void cli_cmd_reset(int argc, char *argv[]);
void cli_cmd_vars(int argc, char *argv[]);

// =============================================================================
// Helper Functions
// =============================================================================

static void cli_print_var_value(int var_index)
{
    char buffer[32];
    int len = 0;
    
    switch (cli_vars[var_index].type)
    {
        case VAR_BOOL:
            if (*(bool*)cli_vars[var_index].ptr)
            {
                cli_puts("true");
                len = 4;
            }
            else
            {
                cli_puts("false");
                len = 5;
            }
            break;
        case VAR_INT:
            len = snprintf(buffer, sizeof(buffer), "%ld", *(int32_t*)cli_vars[var_index].ptr);
            cli_puts(buffer);
            break;
        case VAR_FLOAT:
            len = snprintf(buffer, sizeof(buffer), "%.3f", *(float*)cli_vars[var_index].ptr);
            cli_puts(buffer);
            break;
        case VAR_STRING:
            len = strlen((char*)cli_vars[var_index].ptr);
            cli_puts((char*)cli_vars[var_index].ptr);
            break;
    }
    
    // Pad to 12 characters for alignment
    for (int i = len; i < 12; i++)
    {
        cli_uart_putchar_impl(' ');
    }
}

// =============================================================================
// Command Handlers
// =============================================================================

void cli_cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    cli_puts("=== Help ===\r\n");
    cli_puts("Commands:\r\n");
    cli_puts("  help              - Show this help message\r\n");
    cli_puts("  status            - Show system status summary\r\n");
    cli_puts("  list              - List all variables with descriptions\r\n");
    cli_puts("  get <var>         - Get variable value\r\n");
    cli_puts("  set <var> <val>   - Set variable value\r\n");
    cli_puts("\r\nNavigation:\r\n");
    cli_puts("  Up/Down arrows    - Navigate command history\r\n");
    cli_puts("  Tab               - Auto-complete commands\r\n");
    cli_puts("  Backspace         - Delete character\r\n");
}

void cli_cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    cli_puts("=== System Information ===\r\n");
    cli_puts("Firmware:     STM32F103 CLI Debug System\r\n");
    cli_puts("Version:      2.0.0 (Modular)\r\n");
    char buffer[16];
    cli_puts("Variables:    ");
    snprintf(buffer, sizeof(buffer), "%d\r\n", (int)NUM_VARS);
    cli_puts(buffer);
    cli_puts("Commands:     help, list, get, set, status, reset, info\r\n");
}

void cli_cmd_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    char buffer[32];
    
    cli_puts("Firmware build date: " __DATE__ " " __TIME__ "\r\n");
    cli_puts("=== System Status ===\r\n");
    cli_puts("\r\nLED State:    ");
    cli_puts(led_mode == 0 ? "OFF" : (led_mode == 1 ? "ON" : "BLINKING"));
    cli_puts("\r\nLED Blink Rate: ");
    snprintf(buffer, sizeof(buffer), "%.1f Hz\r\n", led_blink_rate);
    cli_puts(buffer);
    cli_puts("\r\nIMU Logging:      ");
    cli_puts(imu_logging_enabled ? "ACTIVE" : "STOPPED");
}

void cli_cmd_list(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    cli_puts("Variable Name    Type    Value       Description\r\n");
    cli_puts("==============================================================\r\n");
    
    for (int i = 0; i < NUM_VARS; i++)
    {
        cli_puts(cli_vars[i].name);
        
        // Padding
        for (int j = strlen(cli_vars[i].name); j < 17; j++)
        {
            cli_uart_putchar_impl(' ');
        }
        
        // Type
        switch (cli_vars[i].type)
        {
            case VAR_BOOL:   cli_puts("bool    "); break;
            case VAR_INT:    cli_puts("int     "); break;
            case VAR_FLOAT:  cli_puts("float   "); break;
            case VAR_STRING: cli_puts("string  "); break;
        }
        
        // Value (with padding included)
        cli_print_var_value(i);
        
        // Description
        cli_puts(cli_vars[i].description);
        cli_puts("\r\n");
    }
}

void cli_cmd_get(int argc, char *argv[])
{
    if (argc < 2)
    {
        cli_puts("Usage: get <var>\r\n");
        return;
    }
    
    for (int i = 0; i < NUM_VARS; i++)
    {
        if (strcmp(argv[1], cli_vars[i].name) == 0)
        {
            cli_puts(argv[1]);
            cli_puts(" = ");
            cli_print_var_value(i);
            cli_puts("\r\n");
            return;
        }
    }
    
    cli_puts("Unknown variable: ");
    cli_puts(argv[1]);
    cli_puts("\r\n");
}

void cli_cmd_set(int argc, char *argv[])
{
    if (argc < 3)
    {
        cli_puts("Usage: set <var> <value>\r\n");
        return;
    }
    
    for (int i = 0; i < NUM_VARS; i++)
    {
        if (strcmp(argv[1], cli_vars[i].name) == 0)
        {
            // Set based on type
            switch (cli_vars[i].type)
            {
                case VAR_BOOL:
                    bool value = 0;
                    value |= (strcmp(argv[2], "true") == 0);
                    value |= (strcmp(argv[2], "on") == 0);
                    value |= (strcmp(argv[2], "1") == 0);
                    *(bool*)cli_vars[i].ptr = value;
                    break;
                    
                case VAR_INT:
                {
                    int32_t intval = atoi(argv[2]);
                    *(int32_t*)cli_vars[i].ptr = intval;
                    break;
                }
                
                case VAR_FLOAT:
                    *(float*)cli_vars[i].ptr = atof(argv[2]);
                    break;
                    
                case VAR_STRING:
                    strncpy((char*)cli_vars[i].ptr, argv[2], 31);
                    break;
            }
            
            cli_puts(argv[1]);
            cli_puts(" = ");
            cli_print_var_value(i);
            cli_puts("\r\n");
            return;
        }
    }
    
    cli_puts("Unknown variable: ");
    cli_puts(argv[1]);
    cli_puts("\r\n");
}

void cli_cmd_cfg(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (argc != 3)
    {
        cli_puts("Usage: cfg <load|save> [filename]\r\n");
        return;
    }

    switch (argv[1][0]) {
        case 'l': // load
            //cfg_load(argv[2]);
            cli_puts("Configuration loaded from ");
            cli_puts(argv[2]);
            cli_puts(" (not really, placeholder)\r\n");
            break;
        case 's': // save
            //cfg_save(argv[2]);
            // TODO: Implement actual save/load functionality
            // check for overwrite, file existence, etc.
            cli_puts("Configuration saved to ");
            cli_puts(argv[2]);
            cli_puts(" (not really, placeholder)\r\n");
            break;
        default:
            cli_puts("Unknown subcommand for 'cfg': ");
            cli_puts(argv[1]);
            cli_puts("\r\n");
            return;
    }
}

void cli_cmd_calibrate(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (argc < 2)
    {
        cli_puts("Usage: calibrate <gyro|mag|accel>\r\n");
        return;
    }

    switch (argv[1][0]) {
        case 'g': // gyro
            cli_puts("Calibrating gyro... (Not really, this is a placeholder)\r\n");
            break;
        case 'm': // mag
            cli_puts("Calibrating magnetometer... (Not really, this is a placeholder)\r\n");
            break;
        case 'a': // accel
            cli_puts("Calibrating accelerometer... (Not really, this is a placeholder)\r\n");
            break;
        default:
            cli_puts("Unknown sensor type: ");
            cli_puts(argv[1]);
            cli_puts("\r\n");
            return;
    }
}

void cli_cmd_filedump(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (argc != 2)
    {
        cli_puts("Usage: filedump <filename>\r\n");
        return;
    }

    // Placeholder for file dump functionality
    cli_puts("Dumping file: ");
    // dump line by line directly to UART
    cli_puts(argv[1]);
    cli_puts(" (Not really, this is a placeholder)\r\n");
}

void cli_cmd_flashdump(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (argc != 2)
    {
        cli_puts("Usage: flashdump <address>\r\n");
        return;
    }

    // Placeholder for flash dump functionality
    cli_puts("Dumping flash memory at address: ");
    cli_puts(argv[1]);
    cli_puts(" (Not really, this is a placeholder)\r\n");
}

// =============================================================================
// Command Registry
// =============================================================================

static const cli_command_t app_commands[] = {
    {"help",   cli_cmd_help},
    {"status", cli_cmd_status},

    {"list",   cli_cmd_list},
    {"get",    cli_cmd_get},
    {"set",    cli_cmd_set},

    {"cfg",   cli_cmd_cfg},

    {"calibrate", cli_cmd_calibrate},
    {"filedump", cli_cmd_filedump},
    {"flashdump", cli_cmd_flashdump},
};

#define NUM_COMMANDS (sizeof(app_commands) / sizeof(app_commands[0]))

// =============================================================================
// User Initialization
// =============================================================================

void cli_user_init(uint8_t *rx_buffer, uint16_t buffer_size, volatile uint32_t *dma_counter)
{
    // Store DMA buffer configuration for getchar implementation
    cli_dma_rx_buffer = rx_buffer;
    cli_dma_buffer_size = buffer_size;
    cli_dma_counter = dma_counter;
    cli_last_processed = 0;
    
    // Configure CLI with commands and I/O functions
    cli_config_t config = {
        .commands = app_commands,
        .num_commands = NUM_COMMANDS,
        .putchar_fn = cli_uart_putchar_impl,
        .getchar_fn = cli_uart_getchar_impl
    };
    
    cli_init(&config);
}
