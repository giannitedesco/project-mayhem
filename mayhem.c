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

#include "cvars.h"
#include "mayhem-amf.h"

struct _mayhem {
	rtmp_t rtmp;
	netconn_t nc;
};

static int NaiadFreeze(mayhem_t m, int code, void *u1, int u2, const char *desc)
{
	printf("NaiadFreeze: %d: %s\n", code, desc);
	return 1;
}

static int i_freeze(mayhem_t m, invoke_t inv)
{
	amf_t f_rc, f_desc;
	if ( amf_invoke_nargs(inv) < 5 ) {
		printf("mayhem: too few args in NaiadFreeze\n");
		return 0;
	}

	f_rc = amf_invoke_get(inv, 1);
	f_desc = amf_invoke_get(inv, 4);

	if ( amf_type(f_rc) != AMF_NUMBER || amf_type(f_desc) != AMF_STRING ) {
		printf("mayhem: type mismatch in NaiadFreeze args\n");
		return 0;
	}

	return NaiadFreeze(m, amf_get_number(f_rc),
				NULL, -1, amf_get_string(f_desc));
}

static int naiad_dispatch(mayhem_t m, invoke_t inv, const char *method)
{
	static const struct {
		const char *method;
		int (*call)(mayhem_t m, invoke_t inv);
	}tbl[] = {
		{.method = "NaiadFreeze", .call = i_freeze},
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
	if ( ret )
		return 1;

	/* unhandled */
	return 0;
}

static int invoke_connect(struct _mayhem *m, struct _wmvars *v)
{
	invoke_t inv;
	int ret = 0;
	inv = mayhem_amf_invoke(v);
	if ( NULL == inv )
		goto out;
	ret = rtmp_invoke(m->rtmp, 3, 0, inv);
	if ( ret )
		netconn_set_state(m->nc, NETCONN_STATE_CONNECT_SENT);
	amf_invoke_free(inv);
out:
	return ret;
}

void mayhem_close(mayhem_t m)
{
	if ( m ) {
		rtmp_close(m->rtmp);
		free(m);
	}
}

mayhem_t mayhem_connect(wmvars_t vars)
{
	struct _mayhem *m;

	m = calloc(1, sizeof(*m));
	if ( NULL == m )
		goto out;

	m->rtmp = rtmp_connect(vars->tcUrl);
	if ( NULL == m->rtmp )
		goto out_free;

	m->nc = netconn_new(3, 0);
	if ( NULL == m->nc )
		goto out_free;

	rtmp_set_invoke_handler(m->rtmp, dispatch, m);

	if ( !invoke_connect(m, vars) )
		goto out_free_netconn;

	while ( rtmp_pump(m->rtmp) )
		/* do nothing */;

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
