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

static inline void encode_int16(uint8_t *buf, uint16_t val)
{
	buf[0] = (val >> 8) & 0xff;
	buf[1] = val & 0xff;
}

static inline void encode_int24(uint8_t *buf, uint32_t val)
{
	buf[0] = (val >> 16) & 0xff;
	buf[1] = (val >> 8) & 0xff;
	buf[2] = val & 0xff;
}

static inline void encode_int32(uint8_t *buf, uint32_t val)
{
	buf[0] = (val >> 24) & 0xff;
	buf[1] = (val >> 16) & 0xff;
	buf[2] = (val >> 8) & 0xff;
	buf[3] = val & 0xff;
}

static inline void encode_int32le(uint8_t *buf, uint32_t val)
{
	buf[3] = (val >> 24) & 0xff;
	buf[2] = (val >> 16) & 0xff;
	buf[1] = (val >> 8) & 0xff;
	buf[0] = val & 0xff;
}

static inline uint16_t decode_int16(const uint8_t *ptr)
{
	return (ptr[0] << 8) | ptr[1];
}

static inline uint32_t decode_int24(const uint8_t *ptr)
{
	return (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
}

static inline uint32_t decode_int32(const uint8_t *ptr)
{
	return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
}

static inline uint32_t decode_int32le(const uint8_t *ptr)
{
	return (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];
}

#endif /* _WMDUMP_H */
