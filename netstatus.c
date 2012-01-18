/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
 *
 * Implements the NetStatusEvent class as documented in:
 * http://help.adobe.com/en_US/FlashPlatform/reference/actionscript/3/flash/events/NetStatusEvent.html
 * Specifically the NetStream and NetConnection interfaces
*/
#include <wmdump.h>
#include <wmvars.h>
#include <mayhem.h>
#include <os.h>

#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <rtmp/netstatus.h>

struct _netstatus {
	rtmp_t rtmp;
	unsigned int state;
	unsigned int stream_id;
	int chan;
	uint32_t dest;
};

struct netstatus_event {
	const char *level;
	const char *code;
	const char *desc;
	/* details, clientid */
};

static int rip_status(amf_t o_stat, struct netstatus_event *st)
{
	amf_t o_str;

	memset(st, 0, sizeof(*st));

	o_str = amf_object_get(o_stat, "level");
	if ( NULL == o_str || amf_type(o_str) != AMF_STRING ) {
		printf("netstatus: bad 'level' in _result\n");
		return 0;
	}
	st->level = amf_get_string(o_str);

	o_str = amf_object_get(o_stat, "code");
	if ( NULL == o_str || amf_type(o_str) != AMF_STRING ) {
		printf("netstatus: bad 'code' in _result\n");
		return 0;
	}
	st->code = amf_get_string(o_str);

	o_str = amf_object_get(o_stat, "description");
	if ( o_str && amf_type(o_str) == AMF_STRING ) {
		st->desc = amf_get_string(o_str);
	}

	return 1;
}

static invoke_t createstream(double num)
{
	invoke_t inv;

	inv = amf_invoke_new(3);
	if ( NULL == inv )
		return NULL;

	if ( !amf_invoke_append(inv, amf_string("createStream")) )
		goto err;
	if ( !amf_invoke_append(inv, amf_number(num)) )
		goto err;
	if ( !amf_invoke_append(inv, amf_null()) )
		goto err;
	return inv;
err:
	amf_invoke_free(inv);
	return NULL;
}

int netstatus_createstream(netstatus_t ns, double num)
{
	invoke_t inv;
	int ret;

	inv = createstream(num);
	if ( NULL == inv )
		return 0;

	ret = rtmp_flex_invoke(ns->rtmp, ns->chan, ns->dest, inv);
	if ( ret ) {
		ns->state = NETSTATUS_STATE_CREATE_SENT;
	}
	amf_invoke_free(inv);
	return ret;
}

int netstatus_play(netstatus_t ns, amf_t obj)
{
	invoke_t inv;
	int ret = 0;

	inv = amf_invoke_new(4);
	if ( NULL == inv ) {
		amf_free(obj);
		return 0;
	}

	if ( !amf_invoke_set(inv, 0, amf_string("play")) )
		goto out;
	if ( !amf_invoke_set(inv, 1, amf_number(0.0)) ) /* seq ? */
		goto out;
	if ( !amf_invoke_set(inv, 2, amf_null()) )
		goto out;
	if ( !amf_invoke_set(inv, 3, obj) )
		goto out;

	ret = rtmp_flex_invoke(ns->rtmp, 8, ns->stream_id, inv);
	if ( ret ) {
		ns->state = NETSTATUS_STATE_PLAY_SENT;
	}
out:
	amf_invoke_free(inv);
	return ret;
}

void netstatus_set_state(netstatus_t ns, unsigned int state)
{
	ns->state = state;
}

unsigned int netstatus_state(netstatus_t ns)
{
	return ns->state;
}

netstatus_t netstatus_new(rtmp_t rtmp, int chan, uint32_t dest)
{
	struct _netstatus *ns;

	ns = calloc(1, sizeof(*ns));
	if ( NULL == ns )
		goto out;

	ns->rtmp = rtmp;
	ns->chan = chan;
	ns->dest = dest;
	/* success */
out:
	return ns;
}

static int conn_success(struct _netstatus *ns, struct netstatus_event *ev)
{
	ns->state = NETSTATUS_STATE_CONNECTED;
	return 1;
}

static int play_start(struct _netstatus *ns, struct netstatus_event *ev)
{
	ns->state = NETSTATUS_STATE_PLAYING;
	return 1;
}

static int std_result(netstatus_t ns, invoke_t inv)
{
	static const struct {
		const char *code;
		int (*fn)(struct _netstatus *ns, struct netstatus_event *ev);
	}disp[] = {
		{.code = "NetConnection.Connect.Success", .fn = conn_success },
		{.code = "NetConnection.Connect.AppShutdown", },
		{.code = "NetConnection.Connect.Closed",},
		{.code = "NetConnection.Connect.Failed",},
		{.code = "NetConnection.Connect.IdleTimeout",},
		{.code = "NetConnection.Connect.InvalidApp",},
		{.code = "NetConnection.Connect.NetworkChange",},
		{.code = "NetConnection.Connect.Rejected",},
		{.code = "NetStream.Play.Start", .fn = play_start },
		{.code = "NetStream.Play.Reset",},
		{.code = "NetStream.Play.Stop",},
		{.code = "NetStream.Play.UnpublishNotify",},
		{.code = "NetStream.Play.PublishNotify",},
		{.code = "NetStream.Play.StreamNotFound",},
		{.code = "NetStream.Play.FileStructureInvalid",},
		{.code = "NetStream.Play.NoSupportedTrackFound",},
		{.code = "NetStream.Play.Transition",},
		{.code = "NetStream.Play.InsufficientBW",},
	};
	struct netstatus_event st;
	unsigned int rc, i;
	amf_t o_rc, o_stat;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("netstatus: too few args in result\n");
		return 0;
	}

	o_rc = amf_invoke_get(inv, 1);
	/* arg[2] is optional server description */
	o_stat = amf_invoke_get(inv, 3);

	if ( amf_type(o_rc) != AMF_NUMBER ) {
		printf("netstatus: wrong type for result code\n");
		return 0;
	}
	if ( amf_type(o_stat) != AMF_OBJECT ) {
		printf("netstatus: wrong type for result object\n");
		return 0;
	}

	rc = amf_get_number(o_rc);

	if ( !rip_status(o_stat, &st) )
		return 0;

	printf("NetStatusEvent: result:\n");
	printf(" rc = %d\n", rc);
	printf(" level = %s\n", st.level);
	printf(" code = %s\n", st.code);
	printf(" desc = %s\n", st.desc);

	for(i = 0; i < ARRAY_SIZE(disp); i++) {
		if ( strcmp(disp[i].code, st.code) )
			continue;
		if ( disp[i].fn )
			return (*disp[i].fn)(ns, &st);
		return 1;
	}

	printf(" UNHANDLED CODE\n");
	return 1;
}

static int create_result(netstatus_t ns, invoke_t inv)
{
	amf_t o_rc, o_id;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("netstatus: too few args in result\n");
		return 0;
	}
	o_rc = amf_invoke_get(inv, 1);
	/* arg[2] is null? */
	o_id = amf_invoke_get(inv, 3);

	if ( amf_type(o_rc) != AMF_NUMBER || amf_type(o_id) != AMF_NUMBER ) {
		printf("netstatus: create stream result: type mismatch\n");
		return 0;
	}

	ns->stream_id = amf_get_number(o_id);
	printf("netstatus: Stream created (%f) with id: %d\n",
		amf_get_number(o_rc), ns->stream_id);
	ns->state = NETSTATUS_STATE_CREATED;
	return 1;
}

static int n_result(netstatus_t ns, invoke_t inv)
{
	switch(ns->state) {
	case NETSTATUS_STATE_CONNECT_SENT:
		return std_result(ns, inv);
	case NETSTATUS_STATE_CREATE_SENT:
		return create_result(ns, inv);
	default:
		return 0;
	}
}

static int conn_close(netstatus_t ns, invoke_t inv)
{
	printf("netstatus: close\n");
	return 1;
}

static int dispatch(netstatus_t ns, invoke_t inv, const char *method)
{
	static const struct {
		const char *method;
		int (*call)(netstatus_t ns, invoke_t inv);
	}tbl[] = {
		{.method = "_result", .call = n_result},
		{.method = "_error", .call = std_result},
		{.method = "onStatus", .call = std_result},
		{.method = "close", .call = conn_close},
	};
	unsigned int i;

	for(i = 0; i < ARRAY_SIZE(tbl); i++) {
		if ( strcmp(tbl[i].method, method) )
			continue;
		if ( !(*tbl[i].call)(ns, inv) )
			return -1;
		return 1;
	}
	return 0;
}

int netstatus_invoke(netstatus_t ns, invoke_t inv)
{
	unsigned int nargs;
	amf_t method;

	nargs = amf_invoke_nargs(inv);
	if ( nargs < 1 )
		return -1;

	method = amf_invoke_get(inv, 0);
	if ( NULL == method || amf_type(method) != AMF_STRING )
		return -1;

	return dispatch(ns, inv, amf_get_string(method));
}

void netstatus_free(netstatus_t ns)
{
	if ( ns ) {
		free(ns);
	}
}
