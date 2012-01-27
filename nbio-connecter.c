#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <os.h>
#include <list.h>
#include <nbio.h>
#include <nbio-connecter.h>

struct connecter {
	struct nbio io;
	connect_cbfn_t cb;
	void *priv;
};

static void connect_write(struct iothread *t, struct nbio *io)
{
	struct connecter *c = (struct connecter *)io;
	int err;
	socklen_t len = sizeof(err);

	if ( getsockopt(c->io.fd, SOL_SOCKET, SO_ERROR, &err, &len) ) {
		fprintf(stderr, "connecter: getsockopt: %s\n", sock_err());
		goto barf;
	}

	if ( len != sizeof(err) ) {
		fprintf(stderr, "connecter: getsockopt: %u != %zu\n",
			len, sizeof(err));
		goto barf;
	}

	if ( err ) {
		errno = err;
		fprintf(stderr, "connecter: deferred connect: %s\n",
			sock_err());
		goto barf;
	}

	(*c->cb)(t, c->io.fd, c->priv);
	c->io.fd = -1;
	nbio_del(t, &c->io);
	return;
barf:
	sock_close(c->io.fd);
	c->io.fd = -1;
	nbio_del(t, &c->io);
}

static void connect_dtor(struct iothread *t, struct nbio *io)
{
	struct connecter *c = (struct connecter *)io;
	if ( c->io.fd >= 0 )
		sock_close(c->io.fd);
	free(c);
}

static const struct nbio_ops c_ops = {
	.write = connect_write,
	.dtor = connect_dtor,
};

int connecter(struct iothread *t, const char *addr, uint16_t port,
				connect_cbfn_t cb, void *priv)
{
	struct connecter *c;
	os_sock_t s;

	s = sock_connect(addr, port);
	if ( s != BAD_SOCKET ) {
		(*cb)(t, s, priv);
		return 1;
	}

	if ( errno != EINPROGRESS ) {
		fprintf(stderr, "connecter: connect: %s\n", sock_err());
		return 0;
	}

	c = calloc(1, sizeof(*c));
	if ( NULL == c )
		return 0;

	c->io.fd = s;
	c->io.ops = &c_ops;
	c->cb = cb;
	c->priv = priv;
	nbio_add(t, &c->io, NBIO_WRITE);
	nbio_inactive(t, &c->io, NBIO_WRITE);
	return 1;
}
