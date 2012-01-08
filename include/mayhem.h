/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _WMDUMP_MAYHEM_H
#define _WMDUMP_MAYHEM_H

typedef struct _mayhem *mayhem_t;

mayhem_t mayhem_connect(wmvars_t vars);
void mayhem_close(mayhem_t m);

#endif /* _WMDUMP_MAYHEM_H */
