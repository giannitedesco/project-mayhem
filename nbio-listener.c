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
#include <fcntl.h>
#include <errno.h>

#include <list.h>
#include <nbio.h>
#include <nbio-listener.h>
#include <os.h>

struct listener {
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
	struct listener *l = (struct listener *)io;
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
	sock_close(io->fd);
	free(io);
}

static struct nbio_ops listener_ops = {
	.read = listener_read,
	.dtor = listener_dtor,
};

int listener_inet(struct iothread *t, int type, int proto,
				uint32_t addr, uint16_t port,
				listener_cbfn_t cb, void *priv,
				listener_oom_t oom)
{
	struct sockaddr_in sa;
	struct listener *l;

	l = calloc(1, sizeof(*l));
	if ( l == NULL )
		return 0;

	INIT_LIST_HEAD(&l->io.list);

	l->cbfn = cb;
	l->oom = oom;
	l->priv = priv;

	l->io.fd = socket(PF_INET, type, proto);
	if ( l->io.fd < 0 )
		goto err_free;

#if 1
	do{
		int val = 1;
		setsockopt(l->io.fd, SOL_SOCKET, SO_REUSEADDR,
				&val, sizeof(val));
	}while(0);
#endif

	if ( !sock_blocking(l->io.fd, 0) )
		goto err_close;

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(addr);
	sa.sin_port = htons(port);

	if ( bind(l->io.fd, (struct sockaddr *)&sa, sizeof(sa)) )
		goto err_close;

	if ( listen(l->io.fd, 64) )
		goto err_close;

	l->io.ops = &listener_ops;

	nbio_add(t, &l->io, NBIO_READ);
	return 1;

err_close:
	sock_close(l->io.fd);
err_free:
	free(l);
	return 0;
}
