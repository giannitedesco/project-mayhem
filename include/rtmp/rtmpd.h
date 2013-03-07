/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMPD__H
#define _RTMPD__H

typedef struct _rtmp_listener *rtmp_listener_t;
rtmp_listener_t rtmp_listen(struct iothread *t, const char *addr, uint16_t port,
				void *priv);

#endif /* _RTMPD__H */
