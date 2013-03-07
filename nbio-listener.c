/*
 * This is the listener object it manages listening TCP sockets, for
 * each new connection that comes in we spawn off a new proxy object.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define __USE_GNU
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <list.h>
#include <os.h>
#include <nbio.h>
#include <nbio-listener.h>

struct _listener {
	struct nbio io;
	listener_cbfn_t cbfn;
	listener_oom_t oom;
	void *priv;
};

void listener_wake(struct iothread *t, struct nbio *io)
{
	nbio_wake(t, io, NBIO_READ);
}

static void listener_read(struct iothread *t, struct nbio *io)
{
	struct _listener *l = (struct _listener *)io;
	struct sockaddr_in sa;
	socklen_t salen = sizeof(sa);
	int fd;

#if HAVE_ACCEPT4
	fd = accept4(l->io.fd, (struct sockaddr *)&sa, &salen,
			SOCK_NONBLOCK|SOCK_CLOEXEC);
#else
	fd = accept(l->io.fd, (struct sockaddr *)&sa, &salen);
#endif
	if ( fd < 0 ) {
		switch(errno) {
		case ENFILE:
		case EMFILE:
		case ENOMEM:
		case ENOBUFS:
			(*l->oom)(t, io);
			break;
		case EAGAIN:
			nbio_inactive(t, &l->io, NBIO_READ);
			break;
		}
		return;
	}

#if !HAVE_ACCEPT4
	if ( !sock_blocking(fd, 0) )
		return;
#endif

	//printf("Accepted connection from %s:%u\n",
	//	inet_ntoa(sa.sin_addr),
	//	htons(sa.sin_port));

	(*l->cbfn)(t, fd, l->priv);

	/* Make sure to service just-accepted connection before
	 * acccepting new ones Probably it's petty, sure it's a bit
	 * hacky, it relies on knowing that nbio_set_wait puts us at
	 * the end of the active queue
	 */
	nbio_set_wait(t, io, NBIO_READ);
}

static void listener_dtor(struct iothread *t, struct nbio *io)
{
	close(io->fd);
	free(io);
}

static struct nbio_ops listener_ops = {
	.read = listener_read,
	.dtor = listener_dtor,
};

listener_t listener_tcp(struct iothread *t, const char *addr, uint16_t port,
				listener_cbfn_t cb, void *priv,
				listener_oom_t oom)
{
	struct _listener *l = NULL;
	struct hostent *h = NULL;
	struct sockaddr_in sa;

	if ( addr && (NULL == (h = gethostbyname(addr))) ) {
		fprintf(stderr, "gethostbyname: %s\n", strerror(h_errno));
		goto out;
	}


	l = calloc(1, sizeof(*l));
	if ( l == NULL )
		goto out;

	INIT_LIST_HEAD(&l->io.list);

	l->cbfn = cb;
	l->oom = oom;
	l->priv = priv;

	l->io.fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( l->io.fd < 0 )
		goto out_free;

#if 1
	do{
		int val = 1;
		setsockopt(l->io.fd, SOL_SOCKET, SO_REUSEADDR,
				&val, sizeof(val));
	}while(0);
#endif
#ifdef TCP_FASTOPEN
	do{
		int q = 64;
		setsockopt(l->io.fd, SOL_TCP, TCP_FASTOPEN,
				&q, sizeof(q));
	}while(0);
#endif

	if ( !sock_blocking(l->io.fd, 0) )
		goto out_close;

	sa.sin_family = AF_INET;
	if ( h )
		memcpy(&sa.sin_addr, h->h_addr, sizeof(sa.sin_addr));
	else
		sa.sin_addr.s_addr = 0;
	sa.sin_port = htons(port);

	if ( bind(l->io.fd, (struct sockaddr *)&sa, sizeof(sa)) )
		goto out_close;

	if ( listen(l->io.fd, 64) )
		goto out_close;

	l->io.ops = &listener_ops;

	nbio_add(t, &l->io, NBIO_READ);

	/* success */
	goto out;

out_close:
	close(l->io.fd);
out_free:
	free(l);
	l = NULL;
out:
	return l;
}
