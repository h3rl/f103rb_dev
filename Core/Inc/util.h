/*
 * util.h
 *
 *  Created on: Jan 28, 2025
 *      Author: halvard
 * 
 *  Description: utility functions
 */

#ifndef __UTIL_H
#define __UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include <stdio.h>

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define clamp(value, minimum, maximum) (((value) < (minimum)) ? (minimum) : (((value) > (maximum)) ? (maximum) : (value)))

void zeromem(void* pdata, size_t size);

void print(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_H */
