/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>
#include <list.h>
#include <nbio.h>
#include <hgang.h>
#include <nbio-listener.h>
#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <rtmp/proto.h>
#include <rtmp/rtmpd.h>

#include "amfbuf.h"

#define STATE_INIT		0
#define STATE_ABORT		1
#define STATE_CONN_RESET	2
#define STATE_HANDSHAKE_1	3
#define STATE_HANDSHAKE_2	4
#define STATE_READY		5

struct rtmp_chan {
	uint8_t *p_buf;
	uint8_t *p_cur;
	size_t p_left;

	uint32_t cur_ts;
	uint32_t ts;
	uint32_t sz;
	uint32_t dest;
	uint8_t type;
};

struct _rtmp {
	struct nbio io;
	struct iothread *t;

	uint8_t *r_buf;
	uint8_t *r_cur;
	const struct rtmp_ops *ev_ops;
	void *ev_priv;
	size_t chunk_sz;
	size_t r_space;
	uint8_t state;
	uint8_t killed;
	uint8_t conn_reset;
	uint32_t server_bw;
	uint32_t client_bw;
	uint8_t client_bw2;
	uint32_t nbytes;
	uint32_t nbytes_update;

	struct rtmp_chan chan[RTMP_MAX_CHANNELS];
};

static int transition(struct _rtmp *r, unsigned int state);

static int rtmp_send_raw(struct _rtmp *r, const uint8_t *buf, size_t len)
{
	const uint8_t *end;
	ssize_t ret;

	for(end = buf + len; buf < end; buf++) {
		ret = sock_send(r->io.fd, buf, end - buf);
		if ( ret < 0 ) {
			if ( errno == EAGAIN || errno == ENOTCONN ) {
				dprintf("rtmpd: Sleep on write\n");
				nbio_inactive(r->t, &r->io, NBIO_WRITE);
				return 0;
			}
			printf("rtmpd: sock_send: %s\n", sock_err());
			return transition(r, STATE_ABORT);
		}
		if ( ret == 0 )
			return transition(r, STATE_CONN_RESET);

		buf += ret;
	}

	return 1;
}

static size_t current_buf_sz(struct _rtmp *r)
{
	if ( NULL == r->r_buf )
		return 0;
	return (r->r_cur + r->r_space) - r->r_buf;
}

/* do not use... only for mainloop to adjust when no other code is
 * using the data in the buffer
*/
static int rbuf_update_size(struct _rtmp *r)
{
	uint8_t *new;
	size_t ofs;

	dprintf("rtmpd: Change chunk size: %zu\n", r->chunk_sz);
	new = realloc(r->r_buf, r->chunk_sz + RTMP_HDR_MAX_SZ);
	if ( NULL == new )
		return transition(r, STATE_ABORT);

	ofs = r->r_cur - r->r_buf;

	r->r_buf = new;
	r->r_cur = new + ofs;
	r->r_space = (r->chunk_sz + RTMP_HDR_MAX_SZ) - ofs;
	return 1;
}

/* to be used within the generic rtmp code, this prevents the buffer being
 * re-sized while the recv/dispatch main program loop still has unprocessed
 * packet data in there
*/
static int rbuf_request_size(struct _rtmp *r, size_t sz)
{
	/* paranoia */
	if ( sz == 0 )
		return transition(r, STATE_ABORT);

	r->chunk_sz = sz;

	if ( sz <= current_buf_sz(r) ) {
		/* shrink the buffer later, after we're done with it */
		dprintf("rtmpd: Request bufsz %zu: deferred\n", sz);
		return 1;
	}

	/* should be safe to upsize it right away */
	return rbuf_update_size(r);
}

static int init(struct _rtmp *r)
{
	/* make space for the client handshake */
	if ( !rbuf_request_size(r, RTMP_HANDSHAKE_LEN + 1) )
		return 0;
	return 1;
}

static int handshake1(struct _rtmp *r)
{
	uint8_t buf[RTMP_HANDSHAKE_LEN * 2 + 1];
	unsigned int i;

	buf[0] = 3;
	os_reseed_rand();
	for(i = 1; i < sizeof(buf); i++)
		buf[i] = rand();

	/* XXX: We rely on handhsake1 response still being sat in buffer */
	memcpy(buf + 1 + RTMP_HANDSHAKE_LEN,
		r->r_buf + 1, RTMP_HANDSHAKE_LEN);

	if ( !rtmp_send_raw(r, buf, sizeof(buf)) )
		return 0;

	dprintf("rtmpd: sent handshake 1 reply\n");
	nbio_set_wait(r->t, &r->io, NBIO_READ);
	return 1;
}

static int transition(struct _rtmp *r, unsigned int state)
{
	const char *str;
	int ret;

	switch(state) {
	case STATE_ABORT:
	case STATE_CONN_RESET:
	case STATE_INIT:
		if ( state == STATE_ABORT ) {
			str = "aborted";
		}else{
			str = "connection reset by peer";
		}
		r->state = state;
		if ( r->ev_ops )
			(*r->ev_ops->conn_reset)(r->ev_priv, str);
		r->conn_reset = 1;
		nbio_del(r->t, &r->io);
		return 0;
	case STATE_HANDSHAKE_1:
		ret = init(r);
		break;
	case STATE_HANDSHAKE_2:
		ret = handshake1(r);
		break;
	case STATE_READY:
		//ret = ready(r);
		ret = 1;
		break;
	default:
		printf("Bad state %u\n", state);
		abort();
		break;
	}

	if ( ret )
		r->state = state;

	return ret;
}

static ssize_t handshake1_req(struct _rtmp *r, const uint8_t *buf, size_t len)
{
	if ( len < RTMP_HANDSHAKE_LEN + 1)
		return -1;

	dprintf("rtmpd: client requests version: 0x%x\n", buf[0]);

	if ( !transition(r, STATE_HANDSHAKE_2) )
		return 0;

	return RTMP_HANDSHAKE_LEN + 1;
}

static ssize_t handshake2_req(struct _rtmp *r, const uint8_t *buf, size_t len)
{
	if ( len < RTMP_HANDSHAKE_LEN )
		return -1;

	if ( !rbuf_request_size(r, RTMP_DEFAULT_CHUNK_SZ) )
		return 0;

	if ( !transition(r, STATE_READY) )
		return 0;

	return RTMP_HANDSHAKE_LEN;
}

static ssize_t rtmp_drain_buf(struct _rtmp *r)
{
	const uint8_t *buf = r->r_buf;
	size_t sz = r->r_cur - r->r_buf;
	ssize_t ret;

	if ( !sz )
		return -1;

	switch(r->state) {
	case STATE_ABORT:
	case STATE_CONN_RESET:
		abort();
		break;
	case STATE_HANDSHAKE_1:
		ret = handshake1_req(r, buf, sz);
		break;
	case STATE_HANDSHAKE_2:
		ret = handshake2_req(r, buf, sz);
		break;
	case STATE_READY:
		//ret = decode_rtmp(r, buf, sz);
		hex_dump(buf, sz, 0);
		ret = sz;
		break;
	default:
		printf("bad state %d\n", r->state);
		abort();
	}

	return ret;
}

static int fill_rcv_buf(struct _rtmp *r)
{
	ssize_t ret;
	assert(r->r_space);
	ret = sock_recv(r->io.fd, r->r_cur, r->r_space);
	if ( ret < 0 ) {
		if ( errno == EAGAIN ) {
			dprintf("rtmpd: Sleep on read\n");
			nbio_inactive(r->t, &r->io, NBIO_READ);
			return 0;
		}
		printf("rtmpd: sock_recv: %s\n", sock_err());
		return transition(r, STATE_ABORT);
	}
	if ( ret == 0 )
		return transition(r, STATE_CONN_RESET);

	dprintf("rtmpd: received %zu bytes\n", ret);
	//hex_dump(r->r_cur, ret, 16);
	r->r_cur += ret;
	r->r_space -= ret;

	return 1;
}

static void rtmp_pump(rtmp_t r)
{
	ssize_t taken;

again:
	taken = rtmp_drain_buf(r);
	if ( !taken )
		return;

	/* need more data? */
	if ( taken < 0 ) {
		if ( !fill_rcv_buf(r) )
			return;
		goto again;
	}

	assert(taken <= (r->r_cur - r->r_buf));

	/* shuffle the buffer along for the data that was processed */
	memmove(r->r_buf, r->r_buf + taken, (r->r_cur - r->r_buf) - taken);
	r->r_cur -= taken;
	r->r_space += taken;

	/* try to match buffer size to requested chunk size,
	 * provided we won't chop off any outstanding data that is
	*/
//	printf("req = %zu, cur = %zu, used = %zu, space = %zu\n",
//		r->chunk_sz, current_buf_sz(r),
//		(size_t)(r->r_cur - r->r_buf),
//		r->r_space);
	if ( r->chunk_sz != (current_buf_sz(r) - RTMP_HDR_MAX_SZ) &&
		r->chunk_sz >= (size_t)(r->r_cur - r->r_buf) ) {
		if ( !rbuf_update_size(r) )
			return;
	}

	return;
}

static void iop_read(struct iothread *t, struct nbio *io)
{
	struct _rtmp *r = (struct _rtmp *)io;
	rtmp_pump(r);
}

static void iop_write(struct iothread *t, struct nbio *io)
{
	struct _rtmp *r = (struct _rtmp *)io;
	transition(r, r->state);
}

static void rtmp_dtor(struct _rtmp *r)
{
	unsigned int i;
	for(i = 0; i < RTMP_MAX_CHANNELS; i++) {
		free(r->chan[i].p_buf);
	}
	free(r->r_buf);
	sock_close(r->io.fd);
	r->io.fd = BAD_SOCKET;
	free(r);
}

static void iop_dtor(struct iothread *t, struct nbio *io)
{
	struct _rtmp *r = (struct _rtmp *)io;
	r->conn_reset = 1;
	if ( r->killed )
		rtmp_dtor(r);
}

static const struct nbio_ops iops = {
	.read = iop_read,
	.write = iop_write,
	.dtor = iop_dtor,
};


/* --- listener infrastructure --- */
static hgang_t conns;

static int rtmpd_init(void)
{
	if ( NULL == conns ) {
		conns = hgang_new(sizeof(struct _rtmp), 0);
		if ( NULL == conns )
			return 0;
	}
	return 1;
}

struct _rtmp_listener {
	listener_t listener;
	void *priv;
};

static void listen_cb(struct iothread *t, int s, void *priv)
{
	struct _rtmp_listener *l = priv;
	struct _rtmp *r;

	(void)l->priv;

	r = hgang_alloc0(conns);
	if ( NULL == r ) {
		fprintf(stderr, "hgang_alloc: %s\n", os_err());
		sock_close(s);
		return;
	}

	r->io.fd = s;
	r->io.ops = &iops;
	r->t = t;
	nbio_add(t, &r->io, NBIO_READ);
	transition(r, STATE_HANDSHAKE_1);
	dprintf("rtmpd: Got connection\n");
}

static void listen_oom(struct iothread *t, struct nbio *io)
{
	/* TODO: do the waitq shuffle */
}

rtmp_listener_t rtmp_listen(struct iothread *t, const char *addr, uint16_t port,
				void *priv)
{
	struct _rtmp_listener *l = NULL;

	if ( !rtmpd_init() )
		goto out;

	l = calloc(1, sizeof(*l));
	if ( NULL == l )
		goto out;

	l->priv = priv;
	l->listener = listener_tcp(t, addr, port, listen_cb, l, listen_oom);
	if ( NULL == l->listener )
		goto out_free;

	/* success */
	goto out;

out_free:
	free(l);
	l = NULL;
out:
	return l;
}

