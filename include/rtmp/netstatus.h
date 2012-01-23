/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMP_NETSTATUS_H
#define _RTMP_NETSTATUS_H

typedef struct _netstatus *netstatus_t;

struct netstream_ops {
	void(*start)(netstatus_t ns, void *priv);
	void(*stop)(netstatus_t ns, void *priv);
	void(*reset)(netstatus_t ns, void *priv);
	void(*error)(netstatus_t ns, void *priv,
			const char *code, const char *desc);
};

struct netconn_ops {
	void (*connected)(netstatus_t ns, void *priv);
	void (*stream_created)(netstatus_t ns, void *priv,
				unsigned int stream_id);
	void (*error)(netstatus_t ns, void *priv,
			const char *code, const char *desc);
};

netstatus_t netstatus_new(rtmp_t rtmp, int chan, uint32_t dest);
void netstatus_stream_ops(netstatus_t ns,
				const struct netstream_ops *ops, void *priv);
void netstatus_connect_ops(netstatus_t ns,
				const struct netconn_ops *ops, void *priv);
int netstatus_connect_custom(netstatus_t ns, invoke_t inv);
int netstatus_invoke(netstatus_t nc, invoke_t inv);
int netstatus_createstream(netstatus_t nc, double num);
int netstatus_play(netstatus_t nc, amf_t obj, unsigned int stream_id);

void netstatus_free(netstatus_t nc);

#endif /* _RTMP_NETSTATUS_H */
