/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> /* inet_aton(3) */
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

const char *sock_err(void)
{
	return strerror(errno);
}

ssize_t sock_send(os_sock_t sock, const uint8_t *buf, size_t len)
{
	return send(sock, buf, len, MSG_NOSIGNAL);
}

ssize_t sock_recv(os_sock_t sock, uint8_t *buf, size_t len)
{
	return recv(sock, buf, len, MSG_NOSIGNAL);
}

os_sock_t sock_connect(const char *ip, uint16_t port)
{
	struct sockaddr_in sa;
	struct hostent *h;
	int s;

	h = gethostbyname(ip);
	if ( NULL == h ) {
		fprintf(stderr, "gethostbyname: %s\n", strerror(h_errno));
		goto out;
	}

	sa.sin_family = AF_INET;
	memcpy(&sa.sin_addr, h->h_addr, sizeof(sa.sin_addr));
	sa.sin_port = htons(port);

	s = socket(PF_INET, SOCK_STREAM, 0);
	if ( s < 0 ) {
		fprintf(stderr, "socket: %s\n", strerror(errno));
		goto out;
	}

	if ( connect(s, (struct sockaddr *)&sa, sizeof(sa)) ) {
		fprintf(stderr, "connect: %s\n", strerror(errno));
		goto out_close;
	}

	return s;

out_close:
	sock_close(s);
out:
	return BAD_SOCKET;
}

void sock_close(os_sock_t s)
{
	close(s);
}
