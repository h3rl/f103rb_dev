/*
 * util.c
 *
 *  Created on: Jan 28, 2025
 *      Author: halvard
 * 
 *  Description: utility functions
 */

#include "util.h"
#include "config.h"

#include <stdarg.h>
#include <string.h>

void zeromem(void* pdata, size_t size)
{
    memset(pdata, 0, size);
}

void print(const char* format, ...)
{
#ifndef ENABLE_LOGGING
    (void)format;
#else
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#endif
}