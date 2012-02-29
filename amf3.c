/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>
#include <rtmp/amf.h>

#include "amfbuf.h"

#include <stdarg.h>

static size_t amf3_integer(const uint8_t *buf, size_t sz, int32_t *val)
{
	const uint8_t *ptr = buf, *end = buf + sz;
	size_t i;

	*val = 0;

	for(i = 0; i < 3; i++, ptr++) {
		if ( ptr >= end )
			return 0;
		if ( *ptr & 0x80 ) {
			*val <<= 7;
			*val |= *ptr & 0x7f;
		}else{
			break;
		}
	}

	if ( ptr >= end )
		return 0;
	*val <<= (i == 3) ? 8 : 7;
	*val |= *ptr;
	ptr++;

	return ptr - buf;
}

static size_t amf3_string(const uint8_t *buf, size_t sz,
				const char **str, size_t *slen)
{
	const uint8_t *ptr = buf, *end = buf + sz;
	int32_t s_len;
	size_t len;

	len = amf3_integer(buf, sz, &s_len);
	if ( 0 == len )
		return 0;

	ptr += len;

	if ( 0 == (s_len & 1) ) {
		/* string reference */
		*str = NULL;
		*slen = 0;
		return len;
	}

	s_len >>= 1;

	if ( ptr + s_len > end )
		return 0;

	*str = (char *)ptr;
	*slen = s_len;

	ptr += s_len;
	return ptr - buf;
}

static size_t amf3_prop(const uint8_t *buf, size_t sz,
			char **nstr, struct _amf **amf)
{
	const uint8_t *ptr = buf, *end = buf + sz;
	const char *n, *v;
	size_t nlen, ret, vlen;
	uint8_t type;
	int32_t ival;

	ret = amf3_string(buf, sz, &n, &nlen);
	if ( !ret )
		return 0;

	/* end of dynamic list */
	if ( 0 == nlen ) {
		*nstr = NULL;
		*amf = NULL;
		return ret;
	}

	ptr += ret;

	if ( ptr >= end )
		return 0;

	type = *ptr;
	ptr++;

	*amf = NULL;

	switch(type) {
	case AMF3_UNDEFINED:
		*amf = amf_undefined();
		break;
	case AMF3_NULL:
		*amf = amf_null();
		break;
	case AMF3_FALSE:
		*amf = amf_bool(0);
		break;
	case AMF3_TRUE:
		*amf = amf_bool(1);
		break;
	case AMF3_INTEGER:
		ret = amf3_integer(ptr, end - ptr, &ival);
		if ( ret )
			*amf = amf_number(ival);
		break;
	case AMF3_DOUBLE:
		break;
	case AMF3_STRING:
	case AMF3_XML_DOC:
	case AMF3_XML:
		ret = amf3_string(buf, sz, &v, &vlen);
		if ( ret )
			*amf = amf_stringf("%.*s\n", (int)vlen, v);
		break;
	case AMF3_DATE:
	case AMF3_OBJECT:
	case AMF3_ARRAY:
	case AMF3_BYTEARRAY:
	default:
		printf("amf3: Unhandled: %d (0x%x)\n", type, type);
		hex_dump(buf, sz, 16);
		ret = ptr - end;
		*amf = amf_undefined();
		break;
	}

	if ( NULL == *amf ) {
		if ( *amf )
			amf_free(*amf);
		return 0;
	}

	ptr += ret;

	*nstr = malloc(nlen + 1);
	if ( NULL == nstr ) {
		amf_free(*amf);
		return 0;
	}

	memcpy(*nstr, n, nlen);
	(*nstr)[nlen] = '\0';

	return ptr - buf;
}

struct _amf *amf3_parse(const uint8_t *buf, size_t sz, size_t *taken)
{
	const uint8_t *ptr = buf, *end = buf + sz;
	int32_t ref, externalizable, dynamic, num;
	struct _amf *ret = NULL, *elem;
	size_t len, slen;
	const char *str;
	char *n;

	if ( ptr >= end )
		goto out;

	if ( *ptr != AMF3_OBJECT ) {
		printf("amf3: object doesn't begin with object tag\n");
		printf("%x %x\n", *ptr, AMF3_OBJECT);
		*taken = sz;
		return amf_null();
		goto out;
	}

	ptr++;

	len = amf3_integer(ptr, end - ptr, &ref);
	if ( 0 == len )
		goto out;

	ptr += len;

	/* check for an object reference */
	if ( 0 == (ref & 1) ) {
		printf("amf3: objref: %d (0x%x)\n", ref >> 1, ref >> 1);
		ret = amf_undefined();
		goto out;
	}
	ref >>= 1;

	/* check for class reference */
	if ( 0 == (ref & 1) ) {
		printf("amf3: classref: %d (0x%x)\n", ref >> 1, ref >> 1);
		ret = amf_undefined();
		goto out;
	}
	ref >>= 1;

	externalizable = !!(ref & 1);
	ref >>= 1;

	dynamic = !!(ref & 1);
	ref >>= 1;

	num = ref;

	len = amf3_string(ptr, end - ptr, &str, &slen);
	if ( 0 == len )
		goto out;
	ptr += len;

	if ( num || externalizable ||!dynamic ) {
		printf("amf3: name=%.*s, extern = %d, "
			"dynamic = %d num_members = %d\n",
			(int)slen, str, externalizable, dynamic, num);

		printf("amf3: Unsupported object type\n");
		goto out;
	}

	ret = amf_object();
	if ( NULL == ret )
		goto out;

	for(; ptr < end; ptr += len) {
		int rc;

		len = amf3_prop(ptr, end - ptr, &n, &elem);
		if ( 0 == len )
			goto out_free;
		if ( NULL == n ) {
			ptr += len;
			break;
		}

		rc = amf_object_set(ret, n, elem);
		free(n);
		if ( !rc )
			goto out_free;
	}

	goto out;

out_free:
	amf_free(ret);
	ret = NULL;
out:
	*taken = ptr - buf;
	return ret;
}

