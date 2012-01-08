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

static int dispatch(void *priv, invoke_t inv)
{
	struct _mayhem *m = priv;
	int ret;

	/* first try netconn */
	ret = netconn_invoke(m->nc, inv);
	if ( ret < 0 )
		return 0;
	if ( ret )
		return 1;
	/* ret == 0, means unhandled, lets try Naiad app */
	printf("mayhem: INVOKE\n");
	amf_invoke_pretty_print(inv);
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
