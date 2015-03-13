/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMPD__H
#define _RTMPD__H

typedef struct _rtmpd *rtmpd_t;
struct rtmpd_ops {
	int (*ctor)(rtmpd_t r, void *listener_priv);
	void (*connected)(rtmpd_t r);
	void (*pkt)(rtmpd_t r, struct rtmp_pkt *pkt,
			const uint8_t *data, size_t len);
	void (*conn_reset)(rtmpd_t r, const char *str);
	void (*dtor)(rtmpd_t r);
};

typedef struct _rtmp_listener *rtmp_listener_t;
rtmp_listener_t rtmp_listen(struct iothread *t, const char *addr, uint16_t port,
				const struct rtmpd_ops *ops, void *priv);

int rtmpd_send(struct _rtmpd *r, int chan, uint32_t dest, uint32_t ts,
			uint8_t type, const uint8_t *pkt, size_t len);
#ifdef USE_TPROXY
int rtmpd_original_dst(rtmpd_t r, uint32_t *addr, uint16_t *port);
#endif
void rtmpd_close(rtmpd_t r);
void rtmpd_set_handlers(rtmpd_t r, const struct rtmpd_ops *ops, void *priv);
void rtmpd_set_priv(rtmpd_t r, void *priv);
void *rtmpd_get_priv(rtmpd_t r);

#endif /* _RTMPD__H */
