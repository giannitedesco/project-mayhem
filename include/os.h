/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#ifndef _WMDUMP_OS_H
#define _WMDUMP_OS_H

#ifdef WIN32
typedef SOCKET os_sock_t;
#else
typedef int os_sock_t;
#endif

#define BAD_SOCKET -1

os_sock_t sock_connect(const char *ip, uint16_t port);
ssize_t sock_send(os_sock_t sock, const uint8_t *buf, size_t len);
ssize_t sock_recv(os_sock_t sock, uint8_t *buf, size_t len);
void sock_close(os_sock_t sock);
int sock_blocking(os_sock_t s, int b);
const char *sock_err(void);

/* if globalstate is 0 then don't miss with eg. signal handlers.
 * should be called by python binding to let python process do what
 * is required.
*/
int sock_init(int globalstate);
void sock_fini(void);

void os_reseed_rand(void);

#endif /* _WMDUMP_OS_H */
