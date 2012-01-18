/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _AMF__H
#define _AMF__H

typedef struct _invoke *invoke_t;
typedef struct _amf *amf_t;

#define AMF_NUMBER		0
#define AMF_BOOLEAN		1
#define AMF_STRING		2
#define AMF_OBJECT		3
#define AMF_NULL		5
#define AMF_UNDEFINED		6
#define AMF_ECMA_ARRAY		8
#define AMF_OBJECT_END		9
#define AMF_AVMPLUS		17

amf_t amf_number(double num);
amf_t amf_bool(uint8_t val);
amf_t amf_string(const char *str);
__attribute__((format(printf,1,2))) amf_t amf_stringf(const char *fmt, ...);
amf_t amf_null(void);
amf_t amf_undefined(void);

amf_t amf_object(void);
int amf_object_set(amf_t a, const char *name, amf_t obj);
amf_t amf_object_get(amf_t a, const char *name);

unsigned int amf_type(amf_t a);
double amf_get_number(amf_t a);
int amf_get_bool(amf_t a);
const char *amf_get_string(amf_t a);

void amf_free(amf_t a);

invoke_t amf_invoke_new(unsigned int nmemb);
invoke_t amf_invoke_from_buf(const uint8_t *buf, size_t sz);
int amf_invoke_set(invoke_t inv, unsigned int elem, amf_t obj);
int amf_invoke_append(invoke_t inv, amf_t obj);
unsigned int amf_invoke_nargs(invoke_t inv);
amf_t amf_invoke_get(invoke_t inv, unsigned int i);
void amf_invoke_free(invoke_t inv);

void amf_invoke_pretty_print(invoke_t inv);
void amf_pretty_print(amf_t a);

#endif /* _AMF__H */
