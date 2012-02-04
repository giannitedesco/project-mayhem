/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/

#ifndef _WMDUMP_MAYHEM_H
#define _WMDUMP_MAYHEM_H

typedef struct _mayhem *mayhem_t;

#define MAYHEM_FLAGS_ADDUSER		(1<<0)
#define MAYHEM_FLAGS_NOBASIC		(1<<1)
#define MAYHEM_FLAGS_ANONYMOUS		(1<<2)
#define MAYHEM_FLAGS_BASICUSER		(1<<3)
#define MAYHEM_FLAGS_SYSMSG		(1<<4)
#define MAYHEM_FLAGS_IGNOREBASIC	(1<<5)
#define MAYHEM_FLAGS_PARTYCHAT		(1<<6)
#define MAYHEM_FLAGS_GOLDSYS		(1<<7)
#define MAYHEM_FLAGS_EXTSS		(1<<8)
struct naiad_room {
	unsigned int flags;
	const char *topic;
};

#define MAYHEM_GOLDSHOW_START		2
#define MAYHEM_GOLDSHOW_CANCEL		3
#define MAYHEM_GOLDSHOW_END		4
#define MAYHEM_GS_ERROR_AUTH		1
#define MAYHEM_GS_ERROR_LIMIT		2
#define MAYHEM_GS_ERROR_UNAVAILABLE	3
struct naiad_goldshow {
	unsigned int duration;
	unsigned int id;
	unsigned int maxwait;
	unsigned int minbuyin;
	unsigned int pledged;
	unsigned int pledgedamt;
	unsigned int requestedamt;
	const char *showtopic;
	double timetostart;
	unsigned int total;
};

struct mayhem_ops {
	void (*NaiadAuthorize)(void *priv, int code,
				const char *nick,
				const char *bitch,
				unsigned int sid,
				struct naiad_room *room);
	void (*NaiadFreeze)(void *priv, int code, void *u1,
				int u2, const char *desc);
	void (*NaiadPreGoldShow)(void *priv, struct naiad_goldshow *gs);
	void (*NaiadAddChat)(void *priv, const char *nick, const char *chat);

	void (*stream_error)(void *priv, const char *code, const char *desc);
	void (*stream_play)(void *priv);
	void (*stream_reset)(void *priv);
	void (*stream_stop)(void *priv);
	void (*stream_packet)(void *priv, struct rtmp_pkt *pkt,
				const uint8_t *buf, size_t sz);
};

mayhem_t mayhem_connect(struct iothread *t, wmvars_t vars,
			const struct mayhem_ops *ops, void *priv);
void mayhem_abort(mayhem_t m);
void mayhem_close(mayhem_t m);

#endif /* _WMDUMP_MAYHEM_H */
