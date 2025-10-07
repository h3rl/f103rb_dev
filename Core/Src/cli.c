/*
 * cli.c - Portable CLI Core Engine
 *
 *  Created on: Oct 7, 2025
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
 * =============================================================================
 */

#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define CLI_BUFFER_SIZE 128
#define CLI_HISTORY_SIZE 10
#define CLI_MAX_ARGS 8

// =============================================================================
// CLI State
// =============================================================================

static char cli_buffer[CLI_BUFFER_SIZE];
static uint16_t cli_index = 0;

// Command history
static char cli_history[CLI_HISTORY_SIZE][CLI_BUFFER_SIZE];
static int history_count = 0;
static int current_history_pos = -1;

// Arrow key state machine
static enum {
    NORMAL,
    ESC_RECEIVED,
    BRACKET_RECEIVED
} arrow_state = NORMAL;

// =============================================================================
// Configuration (set during cli_init)
// =============================================================================

static const cli_command_t *cli_commands = NULL;
static uint8_t cli_num_commands = 0;
static void (*cli_putchar_fn)(char) = NULL;
static int (*cli_getchar_fn)(void) = NULL;

// =============================================================================
// Internal Helper Functions
// =============================================================================

/**
 * @brief Internal function to read next character
 * @return Character if available, -1 if no new data
 */
static int cli_getchar_internal(void)
{
    if (cli_getchar_fn != NULL)
    {
        return cli_getchar_fn();
    }
    return -1; // No getchar function provided
}

static void cli_puts(const char *s)
{
    if (cli_putchar_fn == NULL) return;
    while (*s)
    {
        cli_putchar_fn(*s++);
    }
}

static void cli_prompt(void)
{
    cli_puts("> ");
}

static void cli_add_to_history(const char *cmd)
{
    if (strlen(cmd) == 0) return;
    
    // Check if this is the same as the last command (avoid duplicates)
    if (history_count > 0 && strcmp(cli_history[history_count - 1], cmd) == 0)
    {
        return;
    }
    
    // Shift history down if full
    if (history_count >= CLI_HISTORY_SIZE)
    {
        for (int i = 0; i < CLI_HISTORY_SIZE - 1; i++)
        {
            strcpy(cli_history[i], cli_history[i + 1]);
        }
        history_count = CLI_HISTORY_SIZE - 1;
    }
    
    // Add new command to end
    strcpy(cli_history[history_count], cmd);
    history_count++;
    current_history_pos = -1;
}

static void cli_clear_line(void)
{
    if (cli_putchar_fn == NULL) return;
    
    // Move to beginning of line and clear it
    cli_putchar_fn('\r');
    cli_puts("> ");
    for (int i = 0; i < CLI_BUFFER_SIZE; i++)
    {
        cli_putchar_fn(' ');
    }
    cli_putchar_fn('\r');
    cli_puts("> ");
}

static void cli_load_history(int direction)
{
    if (history_count == 0) return;
    
    if (direction > 0) // Up arrow
    {
        if (current_history_pos == -1)
            current_history_pos = history_count - 1;
        else if (current_history_pos > 0)
            current_history_pos--;
    }
    else // Down arrow
    {
        if (current_history_pos == -1) return;
        
        current_history_pos++;
        if (current_history_pos >= history_count)
        {
            current_history_pos = -1;
            cli_clear_line();
            cli_index = 0;
            memset(cli_buffer, 0, sizeof(cli_buffer));
            return;
        }
    }
    
    // Load command from history
    cli_clear_line();
    strcpy(cli_buffer, cli_history[current_history_pos]);
    cli_index = strlen(cli_buffer);
    cli_puts(cli_buffer);
}

static int cli_find_matches(const char* prefix, char matches[][CLI_BUFFER_SIZE], int max_matches)
{
    int match_count = 0;
    int prefix_len = strlen(prefix);
    
    if (prefix_len == 0) return 0;
    
    // Check commands
    for (int i = 0; i < cli_num_commands && match_count < max_matches; i++)
    {
        if (strncmp(prefix, cli_commands[i].name, prefix_len) == 0)
        {
            strcpy(matches[match_count], cli_commands[i].name);
            match_count++;
        }
    }
    
    return match_count;
}

static void cli_tab_complete(void)
{
    if (cli_index == 0) return;
    
    // Find the start of the current word
    int word_start = cli_index;
    while (word_start > 0 && cli_buffer[word_start - 1] != ' ')
    {
        word_start--;
    }
    
    // Extract the current word
    char current_word[CLI_BUFFER_SIZE];
    int word_len = cli_index - word_start;
    strncpy(current_word, &cli_buffer[word_start], word_len);
    current_word[word_len] = '\0';
    
    // Find matches (only for commands at the beginning of the line)
    if (word_start > 0)
    {
        // Not at the beginning - don't autocomplete
        return;
    }
    
    char matches[10][CLI_BUFFER_SIZE];
    int match_count = cli_find_matches(current_word, matches, 10);
    
    if (match_count == 0)
    {
        return;
    }
    else if (match_count == 1)
    {
        // Check if already complete
        if (strcmp(current_word, matches[0]) == 0)
        {
            return;
        }
        
        // Single match - complete it
        while (cli_index > word_start)
        {
            cli_index--;
            cli_puts("\b \b");
        }
        
        strcpy(&cli_buffer[word_start], matches[0]);
        cli_index = word_start + strlen(matches[0]);
        cli_puts(matches[0]);
        
        // Add space after completion
        if (cli_index == strlen(cli_buffer))
        {
            cli_buffer[cli_index++] = ' ';
            if (cli_putchar_fn != NULL)
                cli_putchar_fn(' ');
        }
    }
    else
    {
        // Multiple matches - show them
        if (cli_putchar_fn != NULL)
        {
            cli_putchar_fn('\r');
            cli_putchar_fn('\n');
        }
        
        for (int i = 0; i < match_count; i++)
        {
            cli_puts(matches[i]);
            if (i < match_count - 1)
                cli_puts("  ");
        }
        
        if (cli_putchar_fn != NULL)
        {
            cli_putchar_fn('\r');
            cli_putchar_fn('\n');
        }
        
        // Find common prefix
        int common_len = strlen(matches[0]);
        for (int i = 1; i < match_count; i++)
        {
            int j = 0;
            while (j < common_len && j < (int)strlen(matches[i]) && 
                   matches[0][j] == matches[i][j])
            {
                j++;
            }
            common_len = j;
        }
        
        // Complete up to common prefix
        if (common_len > word_len)
        {
            while (cli_index > word_start)
            {
                cli_index--;
                cli_buffer[cli_index] = '\0';
            }
            
            strncpy(&cli_buffer[word_start], matches[0], common_len);
            cli_buffer[word_start + common_len] = '\0';
            cli_index = word_start + common_len;
        }
        
        // Redraw prompt and buffer
        cli_prompt();
        cli_puts(cli_buffer);
    }
}

static void cli_parse_and_execute(const char *cmd)
{
    if (strlen(cmd) == 0) return;
    
    // Tokenize the command
    char cmd_copy[CLI_BUFFER_SIZE];
    strncpy(cmd_copy, cmd, CLI_BUFFER_SIZE - 1);
    cmd_copy[CLI_BUFFER_SIZE - 1] = '\0';
    
    char *argv[CLI_MAX_ARGS];
    int argc = 0;
    
    char *token = strtok(cmd_copy, " ");
    while (token != NULL && argc < CLI_MAX_ARGS)
    {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    if (argc == 0) return;
    
    // Find and execute command
    for (int i = 0; i < cli_num_commands; i++)
    {
        if (strcmp(argv[0], cli_commands[i].name) == 0)
        {
            if (cli_commands[i].handler != NULL)
            {
                cli_commands[i].handler(argc, argv);
            }
            return;
        }
    }
    
    // Command not found
    cli_puts("Unknown command: ");
    cli_puts(argv[0]);
    cli_puts("\r\nType 'help' for available commands.\r\n");
}

// =============================================================================
// Public API Functions
// =============================================================================

void cli_init(const cli_config_t *config)
{
    // Validate configuration
    if (config == NULL || config->putchar_fn == NULL || config->getchar_fn == NULL)
    {
        return;
    }
    
    // Store configuration
    cli_commands = config->commands;
    cli_num_commands = config->num_commands;
    cli_putchar_fn = config->putchar_fn;
    cli_getchar_fn = config->getchar_fn;
    
    // Initialize state
    cli_index = 0;
    memset(cli_buffer, 0, sizeof(cli_buffer));
    
    history_count = 0;
    current_history_pos = -1;
    arrow_state = NORMAL;
    
    // Print welcome message
    cli_puts("\r\n");
    cli_puts("========================================\r\n");
    cli_puts("  STM32 CLI Debug System v2.0\r\n");
    cli_puts("========================================\r\n");
    cli_puts("Type 'help' for commands\r\n");
    cli_puts("Arrow keys: history | Tab: completion\r\n");
    cli_prompt();
}

void cli_update(void)
{
    while (true)
    {
        int c = cli_getchar_internal();
        if (c < 0)
            return; // No new data
        
        char ch = (char)c;
        
        // Handle Enter/Return
        if (ch == '\r' || ch == '\n')
        {
            if (cli_putchar_fn != NULL)
            {
                cli_putchar_fn('\r');
                cli_putchar_fn('\n');
            }
            
            cli_buffer[cli_index] = '\0';
            cli_add_to_history(cli_buffer);
            cli_parse_and_execute(cli_buffer);
            
            cli_index = 0;
            memset(cli_buffer, 0, sizeof(cli_buffer));
            arrow_state = NORMAL;
            cli_prompt();
        }
        // Handle Backspace
        else if (ch == '\b' || ch == 127)
        {
            if (cli_index > 0)
            {
                cli_index--;
                cli_puts("\b \b");
            }
            arrow_state = NORMAL;
        }
        // Handle ESC sequence start
        else if (ch == 27)
        {
            arrow_state = ESC_RECEIVED;
        }
        // Handle ESC [ sequence
        else if (arrow_state == ESC_RECEIVED && ch == '[')
        {
            arrow_state = BRACKET_RECEIVED;
        }
        // Handle arrow keys
        else if (arrow_state == BRACKET_RECEIVED)
        {
            if (ch == 'A') // Up arrow
            {
                cli_load_history(1);
            }
            else if (ch == 'B') // Down arrow
            {
                cli_load_history(-1);
            }
            arrow_state = NORMAL;
        }
        // Handle Tab
        else if (ch == '\t')
        {
            cli_tab_complete();
            arrow_state = NORMAL;
            // Don't echo the tab character or add it to buffer
        }
        // Handle normal characters
        else if (arrow_state == NORMAL && cli_index < CLI_BUFFER_SIZE - 1)
        {
            cli_buffer[cli_index++] = ch;
            if (cli_putchar_fn != NULL)
                cli_putchar_fn(ch);
        }
        else
        {
            arrow_state = NORMAL;
        }
    }
}
