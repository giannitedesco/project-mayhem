/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _WMDUMP_FMV_H
#define _WMDUMP_FMV_H

FILE *flv_creat(const char *fn);
void flv_rip(FILE *f, struct rtmp_pkt *pkt, const uint8_t *buf, size_t sz);
void flv_close(FILE *f);

#endif /* _WMDUMP_FMV_H */
