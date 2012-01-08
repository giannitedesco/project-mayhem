/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMP_NETCONN_H
#define _RTMP_NETCONN_H

#define NETCONN_STATE_INITIAL		0
#define NETCONN_STATE_CONNECT_SENT	1
#define NETCONN_STATE_CONNECTED		2
#define NETCONN_STATE_CREATE_SENT	3
#define NETCONN_STATE_CREATED		4
#define NETCONN_STATE_PLAY_SENT		5
#define NETCONN_STATE_PLAYING		6

typedef struct _netconn *netconn_t;

netconn_t netconn_new(rtmp_t rtmp, int chan, uint32_t dest);
int netconn_invoke(netconn_t nc, invoke_t inv);
int netconn_createstream(netconn_t nc, double num);
int netconn_play(netconn_t nc, amf_t obj);

/* to let netconn know when you've sent messages on its behalf:
 * ie. custom connect AMF
*/
void netconn_set_state(netconn_t nc, unsigned int state);
unsigned int netconn_state(netconn_t nc);

void netconn_free(netconn_t nc);

#endif /* _RTMP_NETCONN_H */
