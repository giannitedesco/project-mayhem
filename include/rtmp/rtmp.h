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
	void(*read_report_sent)(void *priv, uint32_t ts);
	void(*connected)(void *priv);
	void(*conn_reset)(void *priv, const char *reason);
};

rtmp_t rtmp_connect(struct iothread *t, const char *tcUrl,
			const struct rtmp_ops *ops, void *priv);
int rtmp_invoke(rtmp_t r, int chan, uint32_t dest, invoke_t inv);
int rtmp_flex_invoke(rtmp_t r, int chan, uint32_t dest, invoke_t inv);
void rtmp_set_handlers(rtmp_t r, const struct rtmp_ops *ops, void *priv);
void rtmp_close(rtmp_t r);

/* die die die */
int rtmp_send(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			uint8_t type, const uint8_t *pkt, size_t len);

#endif /* _RTMP__H */
