/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _AMF_BUF_H
#define _AMF_BUF_H

size_t amf_invoke_buf_size(invoke_t inv);
void amf_invoke_to_buf(invoke_t inv, uint8_t *buf);

#endif /* _AMF_BUF_H */
