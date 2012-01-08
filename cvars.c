/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <blob.h>
#include <wmvars.h>

#include <ctype.h>

#include "cvars.h"

static int p_sid(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->sid = atoi((char *)val);
	return 1;
}

static int p_srv(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->srv = atoi((char *)val);
	return 1;
}

static int p_pid(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->pid = atoi((char *)val);
	return 1;
}

static int p_ft(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->ft = atoi((char *)val);
	return 1;
}

static int p_hd(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->hd = atoi((char *)val);
	return 1;
}

static int turbo(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	char *blah = strrchr((char *)val, ';');
	if ( blah )
		*blah = '\0';
	v->tcUrl = strdup((char *)val);
	if ( NULL == v->tcUrl )
		return 0;
	return 1;
}

static int signupargs(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->signupargs = strdup((char *)val);
	if ( NULL == v->signupargs )
		return 0;
	return 1;
}

static int sessiontype(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->sessiontype = strdup((char *)val);
	if ( NULL == v->sessiontype )
		return 0;
	return 1;
}

static int p_nickname(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	if ( val ) {
		v->nickname = strdup((char *)val);
		if ( NULL == v->nickname )
			return 0;
	}
	return 1;
}

static int p_sakey(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	if ( val ) {
		v->sakey = strdup((char *)val);
		if ( NULL == v->sakey )
			return 0;
	}
	return 1;
}

static int p_g(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->g = strdup((char *)val);
	if ( NULL == v->g)
		return 0;
	return 1;
}

static int p_lang(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->lang = strdup((char *)val);
	if ( NULL == v->lang )
		return 0;
	return 1;
}

static int p_ldmov(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->ldmov = strdup((char *)val);
	if ( NULL == v->ldmov )
		return 0;
	return 1;
}

static int p_sk(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->sk = strdup((char *)val);
	if ( NULL == v->sk )
		return 0;
	return 1;
}

static int pageurl(struct _wmvars *v, const uint8_t *key, uint8_t *val)
{
	v->pageurl = strdup((char *)val);
	if ( NULL == v->pageurl )
		return 0;
	return 1;
}

static int do_line(struct _wmvars *v, uint8_t *str)
{
	uint8_t *key, *val = NULL, *ptr;
	unsigned int state;
	unsigned int i;
	static const struct {
		const char *name;
		int (*handler)(struct _wmvars *v,
				const uint8_t *key, uint8_t *val);
	}vals[] = {
		/* ints */
		{.name = "p_sid", .handler = p_sid},
		{.name = "p_pid", .handler = p_pid},
		{.name = "p_hd", .handler = p_hd},
		{.name = "p_ft", .handler = p_ft},
		{.name = "p_srv", .handler = p_srv},

		/* strings */
		{.name = "turbo", .handler = turbo},
		{.name = "p_signupargs", .handler = signupargs},
		{.name = "sessionType", .handler = sessiontype},
		{.name = "p_nickname", .handler = p_nickname},
		{.name = "p_sakey", .handler = p_sakey},
		{.name = "p_g", .handler = p_g},
		{.name = "p_ldmov", .handler = p_ldmov},
		{.name = "p_lang", .handler = p_lang},
		{.name = "p_sk", .handler = p_sk},

		{.name = "$PageUrl", .handler = pageurl},
	};


	if ( str[0] == '\0' || str[0] == '#' )
		return 1;

	for(state = 0, key = ptr = str; *ptr && state != 2; ptr++) {
		switch(state) {
		case 0:
			if ( *ptr == ':' ) {
				*ptr = '\0';
				state = 1;
			}
			break;
		case 1:
			if ( !isspace(*ptr) ) {
				val = ptr;
				state = 2;
			}
			break;
		default:
			break;
		}
	}

	if ( state < 1 )
		return 0;

	for(i = 0; i < ARRAY_SIZE(vals); i++) {
		if ( strcmp(vals[i].name, (char *)key) )
			continue;
		if ( vals[i].handler && !vals[i].handler(v, key, val) )
			return 0;
		return 1;

	}
	//printf("Unknown var: %s\n", key);
	return 1;
}

static int do_parse(struct _wmvars *v, uint8_t *buf, size_t len)
{
	uint8_t *ptr, *line;

	for(ptr = line = buf; ptr < (buf + len); ptr++) {
		switch(*ptr) {
		case '\r':
			*ptr = '\0';
			break;
		case '\n':
			*ptr = '\0';
			if ( !do_line(v, line) )
				return 0;
			line = ptr + 1;
			break;
		default:
			break;
		}
	}

	return 1;
}

void wmvars_free(wmvars_t v)
{
	if ( v ) {
		free(v->tcUrl);
		free(v->signupargs);
		free(v->sessiontype);
		free(v->nickname);
		free(v->sakey);
		free(v->g);
		free(v->ldmov);
		free(v->lang);
		free(v->sk);
		free(v->pageurl);
		free(v);
	}
}

wmvars_t wmvars_parse(const char *fn)
{
	struct _wmvars *v;
	uint8_t *dat;
	size_t sz;

	v = calloc(1, sizeof(*v));
	if ( NULL == v )
		return NULL;

	dat = blob_from_file(fn, &sz);
	if ( NULL == dat )
		goto err;

	if ( !do_parse(v, dat, sz) )
		goto err_blob;

	blob_free(dat, sz);
	return v;
err_blob:
	blob_free(dat, sz);
err:
	wmvars_free(v);
	return NULL;
}
