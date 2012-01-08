/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _WMDUMP_H
#define _WMDUMP_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#if WMDUMP_DEBUG
#define dprintf printf
#else
#define dprintf(x...) do {} while(0)
#endif

void hex_dumpf(FILE *f, const uint8_t *tmp, size_t len, size_t llen);
void hex_dump(const uint8_t *ptr, size_t len, size_t llen);

#endif /* _WMDUMP_H */
