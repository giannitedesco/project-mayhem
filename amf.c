/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>
#include <rtmp/amf.h>

#include "amfbuf.h"

struct amf_elem {
	char *key;
	struct _amf *val;
};

struct amf_obj {
	unsigned int nmemb;
	struct amf_elem *elem;
};

struct _amf {
	uint8_t type;
	union {
		double num;
		uint8_t b;
		char *str;
		struct amf_obj obj;
	}u;
};

struct _invoke {
	unsigned int nmemb;
	unsigned int cur_elem;
	struct _amf *elem[0];
};

static struct _amf *amf_alloc(unsigned int type)
{
	struct _amf *a;
	a = calloc(1, sizeof(*a));
	if ( NULL == a )
		return NULL;
	a->type = type;
	return a;
}

amf_t amf_number(double num)
{
	struct _amf *a;
	a = amf_alloc(AMF_NUMBER);
	if ( NULL == a )
		return NULL;
	a->u.num = num;
	return a;
}

amf_t amf_bool(uint8_t val)
{
	struct _amf *a;
	a = amf_alloc(AMF_BOOLEAN);
	if ( NULL == a )
		return NULL;
	a->u.b = !!val;
	return a;
}

amf_t amf_string(const char *str)
{
	struct _amf *a;

	a = amf_alloc(AMF_STRING);
	if ( NULL == a )
		return NULL;

	if ( NULL == str )
		return a;

	a->u.str = strdup(str);
	if ( NULL == a->u.str ) {
		amf_free(a);
		return NULL;
	}

	return a;
}

amf_t amf_null(void)
{
	struct _amf *a;
	a = amf_alloc(AMF_NULL);
	if ( NULL == a )
		return NULL;
	return a;
}

amf_t amf_undefined(void)
{
	struct _amf *a;
	a = amf_alloc(AMF_UNDEFINED);
	if ( NULL == a )
		return NULL;
	return a;
}

amf_t amf_object(void)
{
	struct _amf *a;
	a = amf_alloc(AMF_OBJECT);
	if ( NULL == a )
		return NULL;
	return a;
}

static int object_index(amf_t a, const char *name)
{
	unsigned int i;
	assert(a->type == AMF_OBJECT);
	for(i = 0; i < a->u.obj.nmemb; i++) {
		if ( !strcmp(name, a->u.obj.elem[i].key) )
			return i;
	}
	return -1;
}

int amf_object_set(amf_t a, const char *name, amf_t obj)
{
	int idx;

	assert(a->type == AMF_OBJECT);

	if ( NULL == obj )
		return 0;

	idx = object_index(a, name);
	if ( idx < 0 ) {
		char *n;
		struct amf_elem *new;

		n = strdup(name);
		if ( NULL == n )
			return 0;

		new = realloc(a->u.obj.elem,
				(a->u.obj.nmemb + 1) * sizeof(*new));
		if ( new == NULL ) {
			free(n);
			return 0;
		}

		a->u.obj.elem = new;

		a->u.obj.elem[a->u.obj.nmemb].key = n;
		a->u.obj.elem[a->u.obj.nmemb].val = obj;
		a->u.obj.nmemb++;
	}else{
		amf_free(a->u.obj.elem[idx].val);
		a->u.obj.elem[idx].val = obj;
	}
	return 1;
}

amf_t amf_object_get(amf_t a, const char *name)
{
	assert(a->type == AMF_OBJECT);
	return NULL;
}

void amf_free(amf_t a)
{
	if ( a ) {
		unsigned int i;
		switch(a->type) {
		case AMF_STRING:
			free(a->u.str);
			break;
		case AMF_OBJECT:
			for(i = 0; i < a->u.obj.nmemb; i++) {
				free(a->u.obj.elem[i].key);
				amf_free(a->u.obj.elem[i].val);
			}
		default:
			break;
		}
		free(a);
	}
}

unsigned int amf_type(amf_t a)
{
	return a->type;
}

double amf_get_number(amf_t a)
{
	assert(a->type == AMF_NUMBER);
	return a->u.num;
}

int amf_get_bool(amf_t a)
{
	assert(a->type == AMF_BOOLEAN);
	return a->u.b;
}

const char *amf_get_string(amf_t a)
{
	assert(a->type == AMF_STRING);
	return a->u.str;
}

invoke_t amf_invoke_new(unsigned int nmemb)
{
	struct _invoke *inv;

	inv = calloc(1, sizeof(*inv) + sizeof(inv->elem[0]) * nmemb);
	if ( inv )
		inv->nmemb = nmemb;
	return inv;
}

int amf_invoke_set(invoke_t inv, unsigned int elem, amf_t obj)
{
	assert(elem < inv->nmemb);
	if ( NULL == obj )
		return 0;
	amf_free(inv->elem[elem]);
	inv->elem[elem] = obj;
	return 1;
}

int amf_invoke_append(invoke_t inv, amf_t obj)
{
	return amf_invoke_set(inv, inv->cur_elem++, obj);
}

void amf_invoke_free(invoke_t inv)
{
	unsigned int i;
	if ( inv ) {
		for(i = 0; i < inv->nmemb; i++)
			amf_free(inv->elem[i]);
		free(inv);
	}
}

static size_t amf_strlen(const char *str)
{
	if ( NULL == str )
		return 0;
	return strlen(str);
}

static size_t amf_size(struct _amf *a)
{
	unsigned int i;
	size_t ret;

	switch(a->type) {
	case AMF_NUMBER:
		return 1 + sizeof(double);
	case AMF_BOOLEAN:
		return 1 + sizeof(uint8_t);
	case AMF_STRING:
		return 1 + sizeof(uint16_t) + amf_strlen(a->u.str);
	case AMF_OBJECT:
		for(ret = 4, i = 0; i < a->u.obj.nmemb; i++) {
			ret += 2 + amf_strlen(a->u.obj.elem[i].key);
			ret += amf_size(a->u.obj.elem[i].val);
		}
		return ret;
	case AMF_NULL:
	case AMF_UNDEFINED:
		return 1;
	default:
		abort();
	}
}

size_t amf_invoke_buf_size(invoke_t inv)
{
	unsigned int i;
	size_t ret;
	for(ret = 0, i = 0; i < inv->nmemb; i++) {
		if ( inv->elem[i] ) {
			ret += amf_size(inv->elem[i]);
		}else{
			ret += 1;
		}
	}
	return ret;
}

static size_t amf_to_buf(struct _amf *a, uint8_t *buf)
{
	uint8_t *ptr = buf;
	unsigned int i;
	int slen;

	*ptr = a->type, ptr++;

	switch(a->type) {
	case AMF_NUMBER:
		memset(ptr, 0, sizeof(double));
		ptr += sizeof(double);
		break;
	case AMF_BOOLEAN:
		*ptr = a->u.b, ptr++;
		break;
	case AMF_STRING:
		slen = amf_strlen(a->u.str);
		assert(slen <= 0xffff);
		ptr[0] = (slen >> 8) & 0xff;
		ptr[1] = slen & 0xff;
		memcpy(ptr + 2, a->u.str, slen);
		ptr += slen + 2;
		break;
	case AMF_OBJECT:
		for(i = 0; i < a->u.obj.nmemb; i++) {
			slen = amf_strlen(a->u.obj.elem[i].key);
			ptr[0] = (slen >> 8) & 0xff;
			ptr[1] = slen & 0xff;
			memcpy(ptr + 2, a->u.obj.elem[i].key, slen);
			ptr += 2 + slen;
			ptr += amf_to_buf(a->u.obj.elem[i].val, ptr);
		}
		ptr[0] = 0;
		ptr[1] = 0;
		ptr += 2;
		*ptr = AMF_OBJECT_END;
		ptr++;
		break;
	case AMF_NULL:
	case AMF_UNDEFINED:
		break;
	default:
		abort();
	}

	return (ptr - buf);
}

void amf_invoke_to_buf(invoke_t inv, uint8_t *buf)
{
	unsigned int i;
	for(i = 0; i < inv->nmemb; i++) {
		if ( inv->elem[i] ) {
			buf += amf_to_buf(inv->elem[i], buf);
		}else{
			buf[0] = AMF_NULL;
			buf++;
		}
	}
}
