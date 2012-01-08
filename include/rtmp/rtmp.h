/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMP__H
#define _RTMP__H

#define RTMP_SCHEME			"rtmp://"
#define RTMP_DEFAULT_PORT		1935

typedef struct _rtmp *rtmp_t;

struct rtmp_pkt {
	int chan;
	uint32_t dest;
	uint32_t ts;
	uint8_t type;
};

struct rtmp_ops {
	int(*invoke)(void *priv, invoke_t inv);
	int(*notify)(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t len);
	int(*audio)(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t len);
	int(*video)(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t len);
	int(*stream_start)(void *priv);
};

rtmp_t rtmp_connect(const char *tcUrl);
int rtmp_invoke(rtmp_t r, int chan, uint32_t dest, invoke_t inv);
int rtmp_flex_invoke(rtmp_t r, int chan, uint32_t dest, invoke_t inv);
void rtmp_set_handlers(rtmp_t r, const struct rtmp_ops *ops, void *priv);
int rtmp_pump(rtmp_t r);
void rtmp_close(rtmp_t r);

#endif /* _RTMP__H */
