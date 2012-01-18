/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _AMF_BUF_H
#define _AMF_BUF_H

#define AMF3_UNDEFINED		0
#define AMF3_NULL		1
#define AMF3_FALSE		2
#define AMF3_TRUE		3
#define AMF3_INTEGER		4
#define AMF3_DOUBLE		5
#define AMF3_STRING		6
#define AMF3_XML_DOC		7
#define AMF3_DATE		8
#define AMF3_ARRAY		9
#define AMF3_OBJECT		10
#define AMF3_XML		11
#define AMF3_BYTEARRAY		12

size_t amf_invoke_buf_size(invoke_t inv);
void amf_invoke_to_buf(invoke_t inv, uint8_t *buf);

struct _amf *amf3_parse(const uint8_t *buf, size_t sz, size_t *taken);

#endif /* _AMF_BUF_H */
