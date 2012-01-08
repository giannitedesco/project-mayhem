/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <wmvars.h>
#include <mayhem.h>
#include <os.h>

#include <rtmp/rtmp.h>
#include <rtmp/amf.h>

#include "cvars.h"
#include "amfbuf.h"

#define I_NUMBER		0
#define I_BOOLEAN		1
#define I_STRING		2
#define I_OBJECT		3
#define I_NULL			5
#define I_UNDEFINED		6
#define I_WEIRD_OBJECT		8
#define I_OBJECT_END		9
#define I_ARRAY			10

static size_t number(const uint8_t *ptr, size_t sz)
{
	union {
		uint64_t integral;
		double fp;
	}u;

	u.integral = ((uint64_t)ptr[0] << 56) |
			((uint64_t)ptr[1] << 48) |
			((uint64_t)ptr[2] << 40) |
			((uint64_t)ptr[3] << 32) |
			((uint64_t)ptr[4] << 24) |
			((uint64_t)ptr[5] << 16) |
			((uint64_t)ptr[6] << 8) |
			(uint64_t)ptr[7];
	printf("%f\n", u.fp);
	return 8;
}

static size_t boolean(const uint8_t *ptr, size_t sz)
{
	printf("%s\n", (*ptr) ? "true" : "false");
	return 1;
}

static size_t string(const uint8_t *ptr, size_t sz)
{
	uint16_t len;

	len = (ptr[0] << 8) | ptr[1];
	printf("'%.*s'\n", len, ptr + sizeof(len));
	return sizeof(len) + len;
}

static size_t null(const uint8_t *ptr, size_t sz)
{
	printf("null\n");
	return 0;
}

static size_t undef(const uint8_t *ptr, size_t sz)
{
	printf("*undefined*\n");
	return 0;
}

static size_t attr_name(const uint8_t *ptr, size_t sz)
{
	uint16_t len;

	len = (ptr[0] << 8) | ptr[1];
	if ( len ) {
		printf(" .%.*s = ", len, ptr + sizeof(len));
	}
	return sizeof(len) + len;
}

static size_t array(const uint8_t *ptr, size_t sz, unsigned int *elems)
{
	uint32_t num;

	num = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
	*elems = num;
	printf("Array(%u)\n", *elems);
	return sizeof(num);
}

typedef size_t (*tfn_t)(const uint8_t *ptr, size_t sz);
static void invoke(const uint8_t *buf, size_t sz)
{
	static const tfn_t tfn[] = {
		[I_NUMBER] number,
		[I_BOOLEAN] boolean,
		[I_STRING] string,
		[I_NULL] null,
		[I_UNDEFINED] undef,
	};
	unsigned int d = 0, i = 0;
	const uint8_t *ptr;
	const uint8_t *end;
	unsigned int elems;

	for(ptr = buf, end = buf + sz; ptr < end; i++) {
		uint8_t type;

		if ( d && !(i % 2) ) {
			printf("%*c", d + 1, ' ');
			ptr += attr_name(ptr, end - ptr);
			continue;
		}else{
			type = *ptr;
			ptr++;
		}

		switch(type) {
		case I_WEIRD_OBJECT:
			/* is it an array of objects? */
			ptr += 4;
			/* fall through */
		case I_OBJECT:
			printf("  {\n");
			d++;
			i = 1;
			break;
		case I_OBJECT_END:
			d--;
			i = 1;
			printf("}\n");
			break;
		case I_ARRAY:
			ptr += array(ptr, end - ptr, &elems);
			d++;
			break;
		default:
			if ( type >= sizeof(tfn)/sizeof(tfn[0]) ||
					NULL == tfn[type] ) {
				printf("unknown type: %d (0x%x)\n",
					type, type);
				hex_dump(ptr, end - ptr, 16);
				return;
			}
			printf("%*c", d + 1, ' ');
			ptr += (*tfn[type])(ptr, end - ptr);
			break;
		}
	}
}

struct _mayhem {
	rtmp_t rtmp;
};

static int i_connect(struct _mayhem *m, struct _wmvars *v)
{
	invoke_t inv;
	amf_t obj;

	inv = amf_invoke_new(6);
	if ( NULL == inv )
		return 0;

	if ( !amf_invoke_append(inv, amf_string("connect")) )
		goto err;
	if ( !amf_invoke_append(inv, amf_number(1.0)) )
		goto err;

	obj = amf_object();
	/* app */
	if ( !amf_object_set(obj, "flashVer", amf_string("LNX 11,1,102,55")) )
		goto err_obj;
	if ( !amf_object_set(obj, "tcUrl", amf_string(v->tcUrl)) )
		goto err_obj;
	/* swfUrl */
	if ( !amf_object_set(obj, "fpad", amf_bool(0)) )
		goto err_obj;
	if ( !amf_object_set(obj, "capabilities", amf_number(239.0)) )
		goto err_obj;
	if ( !amf_object_set(obj, "audioCodecs", amf_number(3575.0)) )
		goto err_obj;
	if ( !amf_object_set(obj, "videoCodecs", amf_number(252.0)) )
		goto err_obj;
	if ( !amf_object_set(obj, "videoFunction", amf_number(1.0)) )
		goto err_obj;
	if ( !amf_object_set(obj, "pageUrl", amf_string(v->pageurl)) )
		goto err_obj;
	if ( !amf_object_set(obj, "objectEncoding", amf_number(3.0)) )
		goto err_obj;
	if ( !amf_invoke_append(inv, obj) )
		goto err;

	if ( !amf_invoke_append(inv, amf_string("XXX: pid")) )
		goto err;

	if ( !amf_invoke_append(inv, amf_string("0")) )
		goto err;

	if ( !amf_invoke_append(inv, amf_string("nickname?")) )
		goto err;

	/* obj { */
	/*  .sid */
	/*  .ldmov */
	/*  .pid */
	/*  .sessionType */
	/*  .signupargs */
	/*  .g */
	/*  .ft */
	/*  .hd */
	/*  .sk */
	/*  .nickname */
	/*  .sakey */
	/*  .lang */
	/*  .version */
	/*  .srv */
	/* } */
	do {
		uint8_t *buf;
		size_t sz;

		sz = amf_invoke_buf_size(inv);
		printf("%zu bytes\n", sz);
		buf = malloc(sz);
		amf_invoke_to_buf(inv, buf);
		invoke(buf, sz);
	}while(0);

	return 1;
err_obj:
	amf_free(obj);
err:
	amf_invoke_free(inv);
	return 0;
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

#if 0
	m->rtmp = rtmp_connect(vars->tcUrl);
	if ( NULL == m->rtmp )
		goto out_free;
#endif
	if ( !i_connect(m, vars) )
		goto out_free;

	/* success */
	goto out;
out_free:
	free(m);
	m = NULL;
out:
	return m;
}
