/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <wmvars.h>
#include <mayhem.h>

#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <rtmp/netconn.h>
#include <flv.h>

#include "cvars.h"
#include "mayhem-amf.h"

#include <time.h>

#define MAYHEM_STATE_ABORT		0
#define MAYHEM_STATE_CONNECTING		1
#define MAYHEM_STATE_FROZEN		2
#define MAYHEM_STATE_AUTHORIZED		3
#define MAYHEM_STATE_GOT_STREAM		4
#define MAYHEM_STATE_PLAYING		5
#define MAYHEM_STATE_PAUSED		6

struct _mayhem {
	char *bitch;
	rtmp_t rtmp;
	netconn_t nc;
	FILE *flv;
	unsigned int state;
	unsigned int sid;
};

struct naiad_room {
	unsigned int flags;
	const char *topic;
};

static void mayhem_abort(struct _mayhem *m)
{
	m->state = MAYHEM_STATE_ABORT;
}

static int update_bitch(struct _mayhem *m, const char *bitch)
{
	char *b = strdup(bitch);

	if ( NULL == b )
		return 0;

	m->bitch = b;
	return 1;
}

static int NaiadAuthorize(mayhem_t m, int code,
				const char *nick,
				const char *bitch,
				unsigned int sid,
				struct naiad_room *room)
{
	printf("NaiadAuthorize: code = %u\n", code);
	printf(" your nickname: %s\n", nick);
	printf(" performer: %s\n", bitch);
	printf(" room flags: %d (0x%x)\n", room->flags, room->flags);
	printf(" topic is: %s\n", (room->topic) ? room->topic : "");
	update_bitch(m, bitch);
	m->state = MAYHEM_STATE_AUTHORIZED;
	return 1;
}

static int NaiadFreeze(mayhem_t m, int code, void *u1, int u2, const char *desc)
{
	printf("NaiadFreeze: %d: %s\n", code, desc);
	//m->state = MAYHEM_STATE_FROZEN;
	m->state = MAYHEM_STATE_AUTHORIZED;
	return 1;
}

static int i_auth(mayhem_t m, invoke_t inv)
{
	amf_t o_rc, o_nick, o_bitch, o_sid, o_room;
	amf_t o_topic, o_flags;
	struct naiad_room room;

	if ( amf_invoke_nargs(inv) < 26 ) {
		printf("mayhem: too few args in NaiadAuthorize\n");
		return 0;
	}

	o_rc = amf_invoke_get(inv, 1);
	o_nick = amf_invoke_get(inv, 6);
	o_bitch = amf_invoke_get(inv, 7);
	o_sid = amf_invoke_get(inv, 12);
	o_room = amf_invoke_get(inv, 25);

	if ( amf_type(o_rc) != AMF_NUMBER ||
			amf_type(o_nick) != AMF_STRING ||
			amf_type(o_bitch) != AMF_STRING ||
			amf_type(o_sid) != AMF_STRING ||
			amf_type(o_room) != AMF_OBJECT ) {
		printf("mayhem: type mismatch in NaiadAuthorize args\n");
		return 0;
	}
	
	o_flags = amf_object_get(o_room, "flags");
	o_topic = amf_object_get(o_room, "roomtopic");
	if ( NULL == o_flags ||
			NULL == o_topic ||
			amf_type(o_flags) != AMF_NUMBER ) {
		printf("mayhem: Attribs not found in NaiadAuthorize room\n");
		return 0;
	}

	room.flags = amf_get_number(o_flags);
	switch(amf_type(o_topic)) {
	case AMF_STRING:
		room.topic = amf_get_string(o_topic);
		break;
	case AMF_NULL:
		room.topic = NULL;
		break;
	default:
		printf("mayhem: bad type for roomtopic\n");
		return 0;
	}

	return NaiadAuthorize(m,
				amf_get_number(o_rc),
				amf_get_string(o_nick),
				amf_get_string(o_bitch),
				atoi(amf_get_string(o_sid)),
				&room);
}


static int i_freeze(mayhem_t m, invoke_t inv)
{
	amf_t o_rc, o_desc;
	if ( amf_invoke_nargs(inv) < 5 ) {
		printf("mayhem: too few args in NaiadFreeze\n");
		return 0;
	}

	o_rc = amf_invoke_get(inv, 1);
	o_desc = amf_invoke_get(inv, 4);

	if ( amf_type(o_rc) != AMF_NUMBER || amf_type(o_desc) != AMF_STRING ) {
		printf("mayhem: type mismatch in NaiadFreeze args\n");
		return 0;
	}

	return NaiadFreeze(m, amf_get_number(o_rc),
				NULL, -1, amf_get_string(o_desc));
}

static int naiad_dispatch(mayhem_t m, invoke_t inv, const char *method)
{
	static const struct {
		const char *method;
		int (*call)(mayhem_t m, invoke_t inv);
	}tbl[] = {
		{.method = "NaiadFreeze", .call = i_freeze},
		{.method = "NaiadAuthorized", .call = i_auth},
	};
	unsigned int i;

	for(i = 0; i < ARRAY_SIZE(tbl); i++) {
		if ( strcmp(tbl[i].method, method) )
			continue;
		if ( !(*tbl[i].call)(m, inv) )
			return -1;
		return 1;
	}
	return 0;
}

static int dispatch(void *priv, invoke_t inv)
{
	struct _mayhem *m = priv;
	amf_t method;
	int ret;

	if ( amf_invoke_nargs(inv) < 1 ) {
		printf("mayhem: missing method name in invoke\n");
		return 0;
	}

	method = amf_invoke_get(inv, 0);
	if ( NULL == method || amf_type(method) != AMF_STRING ) {
		printf("mayhem: bad method name in invoke\n");
		return 0;
	}

	/* try dispatch the method call */
	ret = naiad_dispatch(m, inv, amf_get_string(method));
	if ( ret < 0 )
		return 0;
	if ( ret )
		return 1;

	/* ret == 0, means unhandled, lets try netconn */
	ret = netconn_invoke(m->nc, inv);
	if ( ret < 0 )
		return 0;
	if ( !ret )
		return 0; /* unhandled */

	/* TODO: We may need to handle netconn status and reflect it
	 * in overall application state
	*/
	switch(netconn_state(m->nc)) {
	case NETCONN_STATE_CREATED:
		m->state = MAYHEM_STATE_GOT_STREAM;
		break;
	case NETCONN_STATE_PLAYING:
		printf("mayhem: playing...\n");
		m->state = MAYHEM_STATE_PLAYING;
		break;
	default:
		break;
	}

	return 1;
}

static int invoke_connect(struct _mayhem *m, struct _wmvars *v)
{
	invoke_t inv;
	int ret = 0;
	inv = mayhem_amf_invoke(v);
	if ( NULL == inv )
		goto out;
	ret = rtmp_invoke(m->rtmp, 3, 0, inv);
	if ( ret ) {
		m->state = MAYHEM_STATE_CONNECTING;
		netconn_set_state(m->nc, NETCONN_STATE_CONNECT_SENT);
	}
	amf_invoke_free(inv);
out:
	return ret;
}

static int create_stream(struct _mayhem *m, double num)
{
	if (netconn_state(m->nc) == NETCONN_STATE_CONNECTED) {
		return netconn_createstream(m->nc, 2.0);
	}
	return 1;
}

static int play(struct _mayhem *m)
{
	if (netconn_state(m->nc) == NETCONN_STATE_CREATED ) {
		return netconn_play(m->nc, amf_stringf("%d", m->sid));
	}
	return 1;
}

void mayhem_close(mayhem_t m)
{
	if ( m ) {
		flv_close(m->flv);
		free(m->bitch);
		netconn_free(m->nc);
		rtmp_close(m->rtmp);
		free(m);
	}
}

static int notify(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	struct _mayhem *m = priv;
	invoke_t inv;

	inv = amf_invoke_from_buf(buf, sz);
	if ( inv ) {
		amf_t a;

		if ( amf_invoke_nargs(inv) < 1 )
			return 1;
		a = amf_invoke_get(inv, 0);
		if ( NULL == a || amf_type(a) != AMF_STRING )
			return 1;
		if ( strcmp(amf_get_string(a), "onMetaData") )
			return 1;
		printf("Video metadata:\n");
		amf_invoke_pretty_print(inv);
		amf_invoke_free(inv);
	}

	flv_rip(m->flv, pkt, buf, sz);
	return 1;
}

static int rip(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	struct _mayhem *m = priv;
	flv_rip(m->flv, pkt, buf, sz);
	return 1;
}

static int stream_start(void *priv)
{
	struct _mayhem *m = priv;
	char buf[((m->bitch) ? strlen(m->bitch) : 64) + 128];
	char tmbuf[128];
	struct tm *tm;
	time_t t;

	t = time(NULL);

	tm = localtime(&t);
	strftime(tmbuf, sizeof(tmbuf), "%F-%H-%M-%S", tm);
	snprintf(buf, sizeof(buf), "%s-%s.flv",
		(m->bitch) ? m->bitch : "UNKNOWN", tmbuf);

	if ( m->flv )
		flv_close(m->flv);
	m->flv = flv_creat(buf);

	printf("mayhem: create FLV: %s\n", buf);
	return 1;
}

mayhem_t mayhem_connect(wmvars_t vars)
{
	struct _mayhem *m;
	static const struct rtmp_ops ops = {
		.invoke = dispatch,
		.notify = notify,
		.audio = rip,
		.video = rip,
		.stream_start = stream_start,
	};

	m = calloc(1, sizeof(*m));
	if ( NULL == m )
		goto out;

	m->sid = vars->sid;

	m->rtmp = rtmp_connect(vars->tcUrl);
	if ( NULL == m->rtmp )
		goto out_free;

	m->nc = netconn_new(m->rtmp, 3, 0);
	if ( NULL == m->nc )
		goto out_free;

	rtmp_set_handlers(m->rtmp, &ops, m);

	if ( !invoke_connect(m, vars) )
		goto out_free_netconn;

	while ( m->state != MAYHEM_STATE_ABORT && rtmp_pump(m->rtmp) ) {
		switch(m->state) {
		case MAYHEM_STATE_CONNECTING:
			/* I don't sleep, I wait */
			break;
		case MAYHEM_STATE_FROZEN:
			printf("mayhem: fuck this shit!\n");
			mayhem_abort(m);
			break;
		case MAYHEM_STATE_AUTHORIZED:
			create_stream(m, 2.0);
			break;
		case MAYHEM_STATE_GOT_STREAM:
			play(m);
			break;
		case MAYHEM_STATE_PLAYING:
			/* let the good times roll */
			break;
		default:
			printf("ugh? %d\n", m->state);
			abort();
		}
	}

	/* success */
	goto out;
out_free_netconn:
	netconn_free(m->nc);
out_free:
	free(m);
	m = NULL;
out:
	return m;
}
