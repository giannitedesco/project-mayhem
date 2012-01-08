/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>
#include <rtmp/amf.h>
#include <endian.h>

#include "amfbuf.h"

#include <stdarg.h>

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
	struct _amf **elem;
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

amf_t amf_stringf(const char *fmt, ...)
{
	struct _amf *a;
	va_list va;
	int len;
	char *buf;

	/* size the buffer */
	va_start(va, fmt);
	len = vsnprintf(NULL, 0, fmt, va);
	if ( len <= 0 )
		return amf_string(NULL);
	va_end(va);

	/* create the string */
	buf = malloc(len + 1);
	if ( buf == NULL )
		return NULL;
	va_start(va, fmt);
	vsnprintf(buf, len + 1, fmt, va);
	va_end(va);

	/* return the object */
	a = amf_alloc(AMF_STRING);
	if ( NULL == a )
		return NULL;

	a->u.str = buf;
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
			free(a->u.obj.elem);
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

	inv = calloc(1, sizeof(*inv));
	if ( NULL == inv )
		goto out;

	inv->nmemb = nmemb;
	if ( nmemb ) {
		inv->elem = calloc(sizeof(*inv->elem), nmemb);
		if ( NULL == inv->elem )
			goto out_free;
	}

	/* success */
	goto out;

out_free:
	free(inv);
	inv = NULL;
out:
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
	if ( inv->cur_elem >= inv->nmemb ) {
		struct _amf **new;

		new = realloc(inv->elem, (inv->nmemb + 1) * sizeof(*new));
		if ( NULL == new )
			return 0;

		inv->elem = new;
		inv->elem[inv->nmemb] = NULL;
		inv->nmemb++;
	}
	return amf_invoke_set(inv, inv->cur_elem++, obj);
}

void amf_invoke_free(invoke_t inv)
{
	unsigned int i;
	if ( inv ) {
		for(i = 0; i < inv->nmemb; i++)
			amf_free(inv->elem[i]);
		free(inv->elem);
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

static uint64_t double_to_be64(double fp)
{
	union {
		double fp;
		uint64_t integral;
	}u;

	u.fp = fp;
	return htobe64(u.integral);
}

static double be64_to_double(uint64_t integral)
{
	union {
		double fp;
		uint64_t integral;
	}u;
	u.fp = 0.0; /* shutup gcc, i know its skanky */
	u.integral = be64toh(u.integral);
	return u.fp;
}

static size_t amf_to_buf(struct _amf *a, uint8_t *buf)
{
	uint8_t *ptr = buf;
	unsigned int i;
	int slen;

	*ptr = a->type, ptr++;

	switch(a->type) {
	case AMF_NUMBER:
		*(uint64_t *)ptr = double_to_be64(a->u.num);
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

static struct _amf *parse_element(const uint8_t *buf, size_t sz, size_t *taken)
{
	const uint8_t *ptr = buf, *end = buf + sz;
	uint8_t type;
	uint16_t slen;
	struct _amf *ret;

	assert(sz);

	type = ptr[0], ptr++;
	switch(type) {
	case AMF_NUMBER:
		if ( ptr + sizeof(double) > end )
			return 0;
		ret = amf_number(be64_to_double(*(uint64_t *)ptr));
		ptr += sizeof(double);
		break;
	case AMF_BOOLEAN:
		if ( ptr >= end )
			return NULL;
		ret = amf_bool(ptr[0]);
		ptr++;
		break;
	case AMF_STRING:
		if ( ptr + 2 >= end )
			return NULL;

		slen = (ptr[0] << 8) | ptr[1];
		ptr += 2;

		if ( ptr + slen > end )
			return NULL;

		ret = amf_stringf("%.*s", slen, ptr);
		ptr += slen;
		break;
	case AMF_ECMA_ARRAY:
		if ( ptr + 4 > end )
			return NULL;
		ptr += 4;
	case AMF_OBJECT:
		ret = amf_object();
		do {
			struct _amf *elem;
			char *name;
			size_t t;

			if ( ptr + 2 >= end )
				return NULL;

			slen = (ptr[0] << 8) | ptr[1];
			ptr += 2;

			if ( ptr + slen > end ) {
				amf_free(ret);
				return NULL;
			}

			name = malloc(slen + 1);
			if ( NULL == name ) {
				free(name);
				amf_free(ret);
				return NULL;
			}

			memcpy(name, ptr, slen);
			name[slen] = '\0';
			ptr += slen;

			if ( *ptr == AMF_OBJECT_END ) {
				free(name);
				break;
			}

			elem = parse_element(ptr, end - ptr, &t);
			if ( NULL == elem ) {
				free(name);
				amf_free(ret);
				return NULL;
			}

			if ( !amf_object_set(ret, name, elem) ) {
				free(name);
				amf_free(ret);
				return NULL;
			}

			ptr += t;
		}while(*ptr != AMF_OBJECT_END);
		ptr++;
		break;
	case AMF_NULL:
		ret = amf_null();
		break;
	case AMF_UNDEFINED:
		ret = amf_undefined();
		break;
	default:
		printf("Unhandled: %d (0x%x)\n", type, type);
		hex_dump(buf, sz, 16);
		abort();
	}

	*taken = (ptr - buf);
	return ret;
}

static void do_pretty_print(amf_t a, unsigned int depth)
{
	unsigned int i;
	if ( NULL == a ) {
		printf("null\n"); 
		return;
	}

	switch(a->type) {
	case AMF_NUMBER:
		printf("%f\n", a->u.num);
		break;
	case AMF_BOOLEAN:
		printf("%s\n", (a->u.b) ? "true" : "false");
		break;
	case AMF_STRING:
		printf("'%s'\n", a->u.str ? a->u.str : "");
		break;
	case AMF_OBJECT:
		printf("{\n");
		for(i = 0; i < a->u.obj.nmemb; i++) {
			printf("%*c.%s = ", depth, ' ',
				a->u.obj.elem[i].key);
			do_pretty_print(a->u.obj.elem[i].val, depth + 4);
		}
		printf("%*c}\n", depth - 4, ' ');
		break;
	case AMF_NULL:
		printf("null\n");
		break;
	case AMF_UNDEFINED:
		printf("*undefined*\n");
		break;
	default:
		break;
	}
}

void amf_pretty_print(amf_t a)
{
	do_pretty_print(a, 4);
}

void amf_invoke_pretty_print(invoke_t inv)
{
	unsigned int i;
	printf("INVOKE(\n");
	for(i = 0; i < inv->nmemb; i++) {
		printf(" ");
		amf_pretty_print(inv->elem[i]);
	}
	printf(")\n\n");
}

invoke_t amf_invoke_from_buf(const uint8_t *buf, size_t sz)
{
	struct _invoke *inv;
	const uint8_t *end;
	size_t taken;

	//printf("==== AMF INVOKE ===\n");
	//hex_dump(buf, sz, 16);
	inv = amf_invoke_new(0);
	if ( NULL == inv )
		goto out;

	for(end = buf + sz; buf < end; buf += taken) {
		struct _amf *a;

		a = parse_element(buf, end - buf, &taken);
		if ( NULL == a )
			goto out_free;

		if ( !amf_invoke_append(inv, a) )
			goto out_free;
	}

	/* success */
	goto out;

out_free:
	amf_invoke_free(inv);
	inv = NULL;
out:
	return inv;
}
