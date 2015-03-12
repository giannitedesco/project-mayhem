#ifndef _NBIO_LISTENER_H
#define _NBIO_LISTENER_H

typedef struct _listener *listener_t;

typedef void(*listener_cbfn_t)(struct iothread *t, os_sock_t s, void *priv);
typedef void(*listener_oom_t)(struct iothread *t, struct nbio *io);

listener_t listener_tcp(struct iothread *t, const char *addr, uint16_t port,
					listener_cbfn_t cb, void *priv,
					listener_oom_t oom);
void listener_wake(struct iothread *t, struct nbio *io);
int listener_original_dst(struct nbio *io, uint32_t *addr, uint16_t *port);

#endif /* _NBIO_LISTENER_H */
