/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/

#ifndef _WMDUMP_MAYHEM_H
#define _WMDUMP_MAYHEM_H

typedef struct _mayhem *mayhem_t;

struct naiad_room {
	unsigned int flags;
	const char *topic;
};

struct mayhem_ops {
	void (*NaiadAuthorize)(void *priv, int code,
				const char *nick,
				const char *bitch,
				unsigned int sid,
				struct naiad_room *room);
	void (*NaiadFreeze)(void *priv, int code, void *u1,
				int u2, const char *desc);

	void (*stream_play)(void *priv);
	void (*stream_reset)(void *priv);
	void (*stream_stop)(void *priv);
	void (*stream_packet)(void *priv, struct rtmp_pkt *pkt,
				const uint8_t *buf, size_t sz);
};

mayhem_t mayhem_connect(wmvars_t vars,
			const struct mayhem_ops *ops, void *priv);
int mayhem_pump(mayhem_t m);
void mayhem_abort(mayhem_t m);
void mayhem_close(mayhem_t m);

#endif /* _WMDUMP_MAYHEM_H */
