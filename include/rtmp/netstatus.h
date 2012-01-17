/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _RTMP_NETSTATUS_H
#define _RTMP_NETSTATUS_H

#define NETSTATUS_STATE_INITIAL		0
#define NETSTATUS_STATE_CONNECT_SENT	1
#define NETSTATUS_STATE_CONNECTED		2
#define NETSTATUS_STATE_CREATE_SENT	3
#define NETSTATUS_STATE_CREATED		4
#define NETSTATUS_STATE_PLAY_SENT		5
#define NETSTATUS_STATE_PLAYING		6

typedef struct _netstatus *netstatus_t;

netstatus_t netstatus_new(rtmp_t rtmp, int chan, uint32_t dest);
int netstatus_invoke(netstatus_t nc, invoke_t inv);
int netstatus_createstream(netstatus_t nc, double num);
int netstatus_play(netstatus_t nc, amf_t obj);

/* to let netstatus know when you've sent messages on its behalf:
 * ie. custom connect AMF
*/
void netstatus_set_state(netstatus_t nc, unsigned int state);
unsigned int netstatus_state(netstatus_t nc);

void netstatus_free(netstatus_t nc);

#endif /* _RTMP_NETSTATUS_H */
