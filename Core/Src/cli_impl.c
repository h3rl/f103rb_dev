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
    int min_val;  // For int types (range validation)
    int max_val;  // For int types
} cli_var_t;

// =============================================================================
// Variables
// =============================================================================

// Example variables - feel free to add your own!
static bool debug_enabled = false;
static bool verbose_mode = false;
static bool test_mode = false;
static bool imu_calibrate = false;

// Exported variables (accessible from other files)
bool led_enabled = true;           // LED control state
bool cli_logging_enabled = false;  // Logging state

static int32_t sample_rate = 100;
static int32_t log_level = 2;
static int32_t filter_order = 3;
static int32_t buffer_size = 256;

static float temperature = 25.5f;
static float voltage = 3.3f;
static float threshold = 0.5f;

// Variable registry - add new variables here!
static cli_var_t cli_vars[] = {
    // Boolean variables
    {"debug",      "Enable debug output",              VAR_BOOL,  &debug_enabled,      0, 0},
    {"verbose",    "Enable verbose logging",           VAR_BOOL,  &verbose_mode,       0, 0},
    {"test",       "Enable test mode",                 VAR_BOOL,  &test_mode,          0, 0},
    {"led",        "Control LED state",                VAR_BOOL,  &led_enabled,        0, 0},
    {"imu_cal",    "Trigger IMU calibration",          VAR_BOOL,  &imu_calibrate,      0, 0},
    {"log",        "Enable continuous data logging",   VAR_BOOL,  &cli_logging_enabled, 0, 0},
    
    // Integer variables
    {"rate",       "Sample rate in Hz",                VAR_INT,   &sample_rate,    1, 1000},
    {"loglevel",   "Log level (0=none, 3=all)",        VAR_INT,   &log_level,      0, 3},
    {"filter",     "Filter order",                     VAR_INT,   &filter_order,   1, 10},
    {"bufsize",    "Buffer size",                      VAR_INT,   &buffer_size,    64, 1024},
    
    // Float variables
    {"temp",       "Temperature in Celsius",           VAR_FLOAT, &temperature,    0, 0},
    {"vdd",        "Supply voltage",                   VAR_FLOAT, &voltage,        0, 0},
    {"thresh",     "Detection threshold",              VAR_FLOAT, &threshold,      0, 0},
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
    
    cli_puts("=== CLI Help ===\r\n");
    cli_puts("Commands:\r\n");
    cli_puts("  help              - Show this help message\r\n");
    cli_puts("  list / vars       - List all variables with descriptions\r\n");
    cli_puts("  get <var>         - Get variable value\r\n");
    cli_puts("  set <var> <val>   - Set variable value\r\n");
    cli_puts("  status            - Show system status summary\r\n");
    cli_puts("  reset             - Reset all variables to defaults\r\n");
    cli_puts("  info              - Show firmware information\r\n");
    cli_puts("\r\nNavigation:\r\n");
    cli_puts("  Up/Down arrows    - Navigate command history\r\n");
    cli_puts("  Tab               - Auto-complete commands\r\n");
    cli_puts("  Backspace         - Delete character\r\n");
    cli_puts("\r\nData Logging:\r\n");
    cli_puts("  set log true      - Enable continuous data logging\r\n");
    cli_puts("  Press Enter       - Stop logging and return to CLI\r\n");
    cli_puts("\r\nExamples:\r\n");
    cli_puts("  > list            - Show all variables\r\n");
    cli_puts("  > get debug       - Get debug variable\r\n");
    cli_puts("  > set debug true  - Enable debug mode\r\n");
    cli_puts("  > set rate 200    - Set sample rate to 200 Hz\r\n");
    cli_puts("  > set log true    - Start logging IMU/sensor data\r\n");
    cli_puts("  > status          - Show system status\r\n");
}

void cli_cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    cli_puts("=== System Information ===\r\n");
    cli_puts("Firmware:     STM32F103 CLI Debug System\r\n");
    cli_puts("Version:      2.0.0 (Modular)\r\n");
    cli_puts("Build Date:   " __DATE__ " " __TIME__ "\r\n");
    char buffer[16];
    cli_puts("Variables:    ");
    snprintf(buffer, sizeof(buffer), "%d\r\n", (int)NUM_VARS);
    cli_puts(buffer);
    cli_puts("Commands:     help, list, get, set, status, reset, info\r\n");
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
                    if (intval < cli_vars[i].min_val) intval = cli_vars[i].min_val;
                    if (intval > cli_vars[i].max_val) intval = cli_vars[i].max_val;
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

void cli_cmd_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    cli_puts("=== System Status ===\r\n");
    cli_puts("Debug Mode:   ");
    cli_puts(debug_enabled ? "ENABLED" : "DISABLED");
    cli_puts("\r\nVerbose Mode: ");
    cli_puts(verbose_mode ? "ENABLED" : "DISABLED");
    cli_puts("\r\nTest Mode:    ");
    cli_puts(test_mode ? "ENABLED" : "DISABLED");
    cli_puts("\r\nLED State:    ");
    cli_puts(led_enabled ? "ON" : "OFF");
    cli_puts("\r\nLogging:      ");
    cli_puts(cli_logging_enabled ? "ACTIVE" : "STOPPED");
    
    char buffer[32];
    cli_puts("\r\n\r\nSample Rate:  ");
    snprintf(buffer, sizeof(buffer), "%d Hz\r\n", (int)sample_rate);
    cli_puts(buffer);
    
    cli_puts("Log Level:    ");
    snprintf(buffer, sizeof(buffer), "%d\r\n", (int)log_level);
    cli_puts(buffer);
    
    cli_puts("Temperature:  ");
    snprintf(buffer, sizeof(buffer), "%.1f C\r\n", temperature);
    cli_puts(buffer);
    
    cli_puts("Voltage:      ");
    snprintf(buffer, sizeof(buffer), "%.2f V\r\n", voltage);
    cli_puts(buffer);
}

void cli_cmd_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    debug_enabled = false;
    verbose_mode = false;
    test_mode = false;
    led_enabled = true;
    imu_calibrate = false;
    cli_logging_enabled = false;
    sample_rate = 100;
    log_level = 2;
    filter_order = 3;
    buffer_size = 256;
    temperature = 25.5f;
    voltage = 3.3f;
    threshold = 0.5f;
    
    cli_puts("All variables reset to defaults\r\n");
}

void cli_cmd_vars(int argc, char *argv[])
{
    // Alias for 'list' command
    cli_cmd_list(argc, argv);
}

// =============================================================================
// Command Registry
// =============================================================================

static const cli_command_t app_commands[] = {
    {"help",   cli_cmd_help},
    {"info",   cli_cmd_info},
    {"list",   cli_cmd_list},
    {"vars",   cli_cmd_vars},
    {"get",    cli_cmd_get},
    {"set",    cli_cmd_set},
    {"status", cli_cmd_status},
    {"reset",  cli_cmd_reset},
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
