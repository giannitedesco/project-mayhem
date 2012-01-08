/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMP__H
#define _RTMP__H

#define RTMP_SCHEME			"rtmp://"
#define RTMP_DEFAULT_PORT		1935

#define RTMP_MSG_CHUNK_SZ		0x01
#define RTMP_MSG_INVOKE			0x14

typedef struct _rtmp *rtmp_t;

rtmp_t rtmp_connect(const char *tcUrl);
int rtmp_invoke(rtmp_t r, invoke_t inv);
int rtmp_pump(rtmp_t r);
void rtmp_close(rtmp_t r);

#endif /* _RTMP__H */
