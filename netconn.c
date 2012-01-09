/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <wmvars.h>
#include <mayhem.h>
#include <os.h>

#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <rtmp/netconn.h>

struct _netconn {
	rtmp_t rtmp;
	unsigned int state;
	unsigned int stream_id;
	int chan;
	uint32_t dest;
};

struct netconn_status {
	const char *level;
	const char *code;
	const char *desc;
	/* details, clientid */
};

static int rip_status(amf_t o_stat, struct netconn_status *st)
{
	amf_t o_str;

	memset(st, 0, sizeof(*st));

	o_str = amf_object_get(o_stat, "level");
	if ( NULL == o_str || amf_type(o_str) != AMF_STRING ) {
		printf("netconn: bad 'level' in _result\n");
		return 0;
	}
	st->level = amf_get_string(o_str);

	o_str = amf_object_get(o_stat, "code");
	if ( NULL == o_str || amf_type(o_str) != AMF_STRING ) {
		printf("netconn: bad 'code' in _result\n");
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

int netconn_createstream(netconn_t nc, double num)
{
	invoke_t inv;
	int ret;

	inv = createstream(num);
	if ( NULL == inv )
		return 0;

	ret = rtmp_flex_invoke(nc->rtmp, nc->chan, nc->dest, inv);
	if ( ret ) {
		nc->state = NETCONN_STATE_CREATE_SENT;
	}
	amf_invoke_free(inv);
	return ret;
}

int netconn_play(netconn_t nc, amf_t obj)
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

	ret = rtmp_flex_invoke(nc->rtmp, 8, nc->stream_id, inv);
	if ( ret ) {
		nc->state = NETCONN_STATE_PLAY_SENT;
	}
out:
	amf_invoke_free(inv);
	return ret;
}

void netconn_set_state(netconn_t nc, unsigned int state)
{
	nc->state = state;
}

unsigned int netconn_state(netconn_t nc)
{
	return nc->state;
}

netconn_t netconn_new(rtmp_t rtmp, int chan, uint32_t dest)
{
	struct _netconn *nc;

	nc = calloc(1, sizeof(*nc));
	if ( NULL == nc )
		goto out;

	nc->rtmp = rtmp;
	nc->chan = chan;
	nc->dest = dest;
	/* success */
out:
	return nc;
}

static int std_result(netconn_t nc, invoke_t inv)
{
	struct netconn_status st;
	unsigned int rc;
	amf_t o_rc, o_stat;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("netconn: too few args in result\n");
		return 0;
	}

	o_rc = amf_invoke_get(inv, 1);
	/* arg[2] is optional server description */
	o_stat = amf_invoke_get(inv, 3);

	if ( amf_type(o_rc) != AMF_NUMBER ) {
		printf("netconn: wrong type for result code\n");
		return 0;
	}
	if ( amf_type(o_stat) != AMF_OBJECT ) {
		printf("netconn: wrong type for result object\n");
		return 0;
	}

	rc = amf_get_number(o_rc);

	if ( !rip_status(o_stat, &st) )
		return 0;

	printf("NetConnection: result:\n");
	printf(" rc = %d\n", rc);
	printf(" level = %s\n", st.level);
	printf(" code = %s\n", st.code);
	printf(" desc = %s\n", st.desc);

	if ( !strcmp(st.code, "NetConnection.Connect.Success") ) {
		nc->state = NETCONN_STATE_CONNECTED;
	}else if ( !strcmp(st.code, "NetStream.Play.Reset") ) {
		/* what to do? */
	}else if ( !strcmp(st.code, "NetStream.Play.Start") ) {
		nc->state = NETCONN_STATE_PLAYING;
	}else if ( !strcmp(st.code, "NetStream.Play.InsufficientBW") ) {
		/* why oh why? */
	}

	return 1;
}

static int create_result(netconn_t nc, invoke_t inv)
{
	amf_t o_rc, o_sid;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("netconn: too few args in result\n");
		return 0;
	}
	o_rc = amf_invoke_get(inv, 1);
	/* arg[2] is null? */
	o_sid = amf_invoke_get(inv, 3);

	if ( amf_type(o_rc) != AMF_NUMBER || amf_type(o_sid) != AMF_NUMBER ) {
		printf("netconn: create stream result: type mismatch\n");
		return 0;
	}

	nc->stream_id = amf_get_number(o_sid);
	printf("netconn: Stream created (%f) with id: %d\n",
		amf_get_number(o_rc), nc->stream_id);
	nc->state = NETCONN_STATE_CREATED;
	return 1;
}

static int n_result(netconn_t nc, invoke_t inv)
{
	switch(nc->state) {
	case NETCONN_STATE_CONNECT_SENT:
		return std_result(nc, inv);
	case NETCONN_STATE_CREATE_SENT:
		return create_result(nc, inv);
	default:
		return 0;
	}
}

static int n_error(netconn_t nc, invoke_t inv)
{
	printf("Got an error:\n");
	amf_invoke_pretty_print(inv);
	return 1;
}

static int dispatch(netconn_t nc, invoke_t inv, const char *method)
{
	static const struct {
		const char *method;
		int (*call)(netconn_t nc, invoke_t inv);
	}tbl[] = {
		{.method = "_result", .call = n_result},
		{.method = "_error", .call = n_error},
		{.method = "onStatus", .call = std_result},
	};
	unsigned int i;

	for(i = 0; i < ARRAY_SIZE(tbl); i++) {
		if ( strcmp(tbl[i].method, method) )
			continue;
		if ( !(*tbl[i].call)(nc, inv) )
			return -1;
		return 1;
	}
	return 0;
}

int netconn_invoke(netconn_t nc, invoke_t inv)
{
	unsigned int nargs;
	amf_t method;

	nargs = amf_invoke_nargs(inv);
	if ( nargs < 1 )
		return -1;

	method = amf_invoke_get(inv, 0);
	if ( NULL == method || amf_type(method) != AMF_STRING )
		return -1;

	return dispatch(nc, inv, amf_get_string(method));
}

void netconn_free(netconn_t nc)
{
	if ( nc ) {
		free(nc);
	}
}
