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

#include "cvars.h"
#include "mayhem-amf.h"

struct _mayhem {
	rtmp_t rtmp;
};

static int invoke_connect(struct _mayhem *m, struct _wmvars *v)
{
	invoke_t inv;
	int ret = 0;
	inv = mayhem_amf_invoke(v);
	if ( NULL == inv )
		goto out;
	ret = rtmp_invoke(m->rtmp, inv);
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

	if ( !invoke_connect(m, vars) )
		goto out_free;

	while ( rtmp_pump(m->rtmp) )
		/* do nothing */;

	/* success */
	goto out;
out_free:
	free(m);
	m = NULL;
out:
	return m;
}
