#ifndef _NBIO_CONECTER_H
#define _NBIO_CONECTER_H

typedef void(*connect_cbfn_t)(struct iothread *t, os_sock_t s, void *priv);

/* return zero indicates no-resources, nothing to do with whether connection
 * was established or not
 */
int connecter(struct iothread *t, const char *addr, uint16_t port,
				connect_cbfn_t cb, void *priv);

#endif /* _NBIO_CONECTER_H */
