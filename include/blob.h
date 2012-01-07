/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _WMDUMP_BLOB_H
#define _WMDUMP_BLOB_H

void blob_free(uint8_t *blob, size_t sz);
uint8_t *blob_from_file(const char *fn, size_t *sz);

#endif /* _WMDUMP_BLOB_H */
