/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h> /* inet_aton(3) */
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

void os_reseed_rand(void)
{
	srand(time(NULL) ^ getpid());
}

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

	s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( s < 0 ) {
		fprintf(stderr, "socket: %s\n", strerror(errno));
		goto out;
	}

	if ( !sock_blocking(s, 0) ) {
		sock_close(s);
		return 0;
	}

	if ( connect(s, (struct sockaddr *)&sa, sizeof(sa)) &&
			errno != EINPROGRESS ) {
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

/** Configure blocking mode on a file descriptor.
 * @param fd FD to set blocking mode on
 * @param b Whether to enable or disable blocking mode
 *
 * Configures blocking mode on a file descriptor.
 *
 * @return 0 on error, 1 on success.
 */
int sock_blocking(os_sock_t s, int b)
{
	int fl;

	fl = fcntl(s, F_GETFL);
	if ( fl < 0 )
		return 0;

	if ( b )
		fl &= ~O_NONBLOCK;
	else
		fl |= O_NONBLOCK;

	fl = fcntl(s, F_SETFL, fl);
	if ( fl < 0 )
		return 0;

	return 1;
}
