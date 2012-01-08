/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMP__H
#define _RTMP__H

#define RTMP_SCHEME			"rtmp://"
#define RTMP_DEFAULT_PORT		1935

typedef struct _rtmp *rtmp_t;

rtmp_t rtmp_connect(const char *tcUrl);
int rtmp_invoke(rtmp_t r, int chan, uint32_t dest, invoke_t inv);
void rtmp_set_invoke_handler(rtmp_t r, int(*cb)(void *priv, invoke_t inv),
				void *priv);
int rtmp_pump(rtmp_t r);
void rtmp_close(rtmp_t r);

#endif /* _RTMP__H */
