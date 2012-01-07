/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _WMDUMP_VARS_H
#define _WMDUMP_VARS_H

typedef struct _wmvars *wmvars_t;

wmvars_t wmvars_parse(const char *fn);
void wmvars_free(wmvars_t v);

#endif /* _WMDUMP_VARS_H */
