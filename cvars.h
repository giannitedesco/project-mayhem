/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _CVARS_INTERNAL_H
#define _CVARS_INTERNAL_H

struct _wmvars {
	char *pageurl;
	char *tcUrl;
	char *signupargs;
	char *sessiontype;
	char *nickname;
	char *sakey;
	char *g;
	char *ldmov;
	char *lang;
	char *sk;
	unsigned int sid;
	unsigned int srv;
	unsigned int pid;
	unsigned int ft;
	unsigned int hd;
};

#endif /* _CVARS_INTERNAL_H */
