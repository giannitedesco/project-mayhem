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
void rtmp_close(rtmp_t r);

#endif /* _RTMP__H */
