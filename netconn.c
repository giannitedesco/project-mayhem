/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <wmvars.h>
#include <mayhem.h>
#include <os.h>

#include <rtmp/amf.h>
#include <rtmp/netconn.h>

struct _netconn {
	unsigned int state;
	int chan;
	uint32_t dest;
};

struct nc_result {
	unsigned int rc;
	const char *level;
	const char *code;
	const char *desc;
};

void netconn_set_state(netconn_t nc, unsigned int state)
{
	nc->state = state;
}

unsigned int netconn_state(netconn_t nc)
{
	return nc->state;
}

netconn_t netconn_new(int chan, uint32_t dest)
{
	struct _netconn *nc;

	nc = calloc(1, sizeof(*nc));
	if ( NULL == nc )
		goto out;

	nc->chan = chan;
	nc->dest = dest;
	/* success */
out:
	return nc;
}

static int handle_result(netconn_t nc, struct nc_result *res)
{
	printf("NetConnection: result:\n");
	printf(" rc = %d\n", res->rc);
	printf(" level = %s\n", res->level);
	printf(" code = %s\n", res->code);
	printf(" desc = %s\n", res->desc);
	if ( !strcmp(res->code, "NetConnection.Connect.Success") ) {
		nc->state = NETCONN_STATE_CONNECTED;
	}
	printf("\n");
	return 1;
}

static int n_result(netconn_t nc, invoke_t inv)
{
	struct nc_result res = {0};
	amf_t o_rc, o_stat;
	amf_t o_str;

	if ( amf_invoke_nargs(inv) < 4 ) {
		printf("netconn: too few args in result\n");
		return -0;
	}

	o_rc = amf_invoke_get(inv, 1);
	/* arg[2] is optional server description */
	o_stat = amf_invoke_get(inv, 3);

	if ( amf_type(o_rc) != AMF_NUMBER ) {
		printf("netconn: wrong type for result code\n");
		return -1;
	}
	if ( amf_type(o_stat) != AMF_OBJECT ) {
		printf("netconn: wrong type for result object\n");
		return -1;
	}

	res.rc = amf_get_number(o_rc);

	o_str = amf_object_get(o_stat, "level");
	if ( NULL == o_str || amf_type(o_str) != AMF_STRING ) {
		printf("netconn: bad 'level' in _result\n");
		return -1;
	}
	res.level = amf_get_string(o_str);

	o_str = amf_object_get(o_stat, "code");
	if ( NULL == o_str || amf_type(o_str) != AMF_STRING ) {
		printf("netconn: bad 'code' in _result\n");
		return -1;
	}
	res.code = amf_get_string(o_str);

	o_str = amf_object_get(o_stat, "description");
	if ( NULL == o_str || amf_type(o_str) != AMF_STRING ) {
		printf("netconn: bad 'desc' in _result\n");
		return -1;
	}
	res.desc = amf_get_string(o_str);

	return handle_result(nc, &res);
}

static int dispatch(netconn_t nc, invoke_t inv, const char *method)
{
	static const struct {
		const char *method;
		int (*call)(netconn_t nc, invoke_t inv);
	}tbl[] = {
		{.method = "_result", .call = n_result},
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
