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

static size_t encode_chan(uint8_t *buf, uint8_t htype, int chan)
{
	assert(chan >= 0);
	assert(htype < 4);

	/* and jesus wept */
	if ( chan < 64 ) {
		buf[0] = (htype << 6) | chan;
		return 1;
	}else if ( chan < 0xff + 64 ) {
		buf[0] = (htype << 6);
		buf[1] = chan - 64;
		return 2;
	}else if ( chan < 0xffff + 64 ) {
		buf[0] = (htype << 6) | 1;
		buf[1] = (chan - 64) & 0xff;
		buf[2] = ((chan - 64) >> 8) & 0xff;
		return 3;
	}else{
		abort();
	}
}

static size_t hdr_full(uint8_t *buf, int chan, uint32_t dest, uint32_t ts,
			uint8_t type, size_t len)
{
	uint8_t *ptr = buf;

	ptr += encode_chan(ptr, 0, chan);

	if ( ts >= (1 << 24) ) {
		encode_int24(ptr, 0xffffff), ptr += 3;
	}else{
		encode_int24(ptr, ts), ptr += 3;
	}

	encode_int24(ptr, len), ptr += 3;

	*ptr = type;
	ptr++;

	encode_int32le(ptr, dest), ptr += 4;

	if ( ts >= (1 << 24) ) {
		encode_int32(buf, ts), ptr += 4;
	}

	return ptr - buf;
}

static size_t hdr_small(uint8_t *buf, int chan, uint32_t dest, uint32_t ts,
			uint8_t type, size_t len)
{
	return encode_chan(buf, 3, chan);
}

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

int rtmp_send(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			uint8_t type, const uint8_t *pkt, size_t len)
{
	uint8_t buf[RTMP_HDR_MAX_SZ + r->chunk_sz];
	unsigned int i;

	for(i = 0; len; i++) {
		size_t this_sz;
		size_t hdr_sz;

		if ( i )
			hdr_sz = hdr_small(buf, chan, dest, ts, type, len);
		else
			hdr_sz = hdr_full(buf, chan, dest, ts, type, len);

		this_sz = (len <= r->chunk_sz) ? len : r->chunk_sz;

		memcpy(buf + hdr_sz, pkt, this_sz);

		if ( !rtmp_send_raw(r, buf, hdr_sz + this_sz) )
			return 0;

		pkt += this_sz;
		len -= this_sz;
	}

	return 1;
}

static int send_server_bw(struct _rtmp *r)
{
	uint8_t buf[4];
	encode_int32(buf, r->server_bw);
	return rtmp_send(r, 2, 0, 0xfe227, RTMP_MSG_SERVER_BW, buf, sizeof(buf));
}

static int send_ctl(struct _rtmp *r, uint16_t type, uint32_t val, uint32_t ts)
{
	uint8_t buf[6];
	uint8_t *ptr = buf;

	encode_int16(ptr, type), ptr += 2;
	encode_int32(ptr, val), ptr += 4;
	return rtmp_send(r, 2, 0, ts, RTMP_MSG_CTL, buf, ptr - buf);
}

static int r_invoke(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	invoke_t inv;
	int ret = 0;

	inv = amf_invoke_from_buf(buf, sz);
	if ( NULL == inv )
		goto out;

	dprintf("rtmp: invoke: chan=0x%x dest=0x%x\n", pkt->chan, pkt->dest);
	amf_invoke_pretty_print(inv);
	if ( r->ev_ops && r->ev_ops->invoke ) {
		ret = (*r->ev_ops->invoke)(r->ev_priv, inv);
		if ( !ret ) {
			printf("rtmp: received: bad INVOKE\n");
			amf_invoke_pretty_print(inv);
		}
	}else{
		printf("rtmp: received: unhandled INVOKE\n");
		amf_invoke_pretty_print(inv);
		ret = 1;
	}
	amf_invoke_free(inv);
out:
	return 1;
}

static int r_ctl(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	uint16_t type;
	uint32_t echo;

	if ( sz < sizeof(type) )
		return 0;
	type = decode_int16(buf);
	buf += sizeof(type);
	sz -= sizeof(type);

	switch(type) {
	case RTMP_CTL_STREAM_BEGIN:
		dprintf("rtmp: Stream begin\n");
		if ( r->ev_ops && r->ev_ops->stream_start ) {
			return (*r->ev_ops->stream_start)(r->ev_priv);
		}
		break;
	case RTMP_CTL_PING:
		dprintf("rtmp: PING\n");
		if ( sz < sizeof(echo) )
			return 0;
		echo = decode_int32(buf);
		return send_ctl(r, RTMP_CTL_PONG, echo, 0xfe227);
	case RTMP_CTL_PONG:
		break;
	default:
		printf("rtmp: CTL of unknown type %d (0x%x)\n", type, type);
		hex_dump(buf, sz, 16);
		break;
	}

	return 1;
}

static int r_server_bw(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	dprintf("rtmp: received: SERVER_BW\n");
	if ( sz < sizeof(uint32_t) )
		return 0;

	r->server_bw = decode_int32(buf);
	return send_server_bw(r);
}

static int r_client_bw(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	dprintf("rtmp: received: CLIENT_BW\n");
	if ( sz < sizeof(uint32_t) )
		return 0;

	r->client_bw = decode_int32(buf);
	if ( sz > sizeof(uint32_t) )
		r->client_bw2 = buf[4];

	return 1;
}

static int r_chunksz(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	if ( sz < sizeof(uint32_t) )
		return 0;
	rbuf_request_size(r, decode_int32(buf));
	return 1;
}

static int r_notify(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	dprintf("rtmp: notify: chan=0x%x dest=0x%x\n", pkt->chan, pkt->dest);
	if ( r->ev_ops && r->ev_ops->notify ) {
		return (*r->ev_ops->notify)(r->ev_priv, pkt, buf, sz);
	}
	return 1;
}

static int r_audio(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	r->nbytes += sz;
	if ( r->ev_ops && r->ev_ops->audio ) {
		return (*r->ev_ops->audio)(r->ev_priv, pkt, buf, sz);
	}
	return 1;
}

static int r_video(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz)
{
	r->nbytes += sz;
	if ( r->ev_ops && r->ev_ops->video ) {
		return (*r->ev_ops->video)(r->ev_priv, pkt, buf, sz);
	}
	return 1;
}

typedef int (*rmsg_t)(struct _rtmp *r, struct rtmp_pkt *pkt,
			 const uint8_t *buf, size_t sz);

static int rtmp_dispatch(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			 uint8_t type, const uint8_t *buf, size_t sz)
{
	static const rmsg_t tbl[] = {
		[RTMP_MSG_CHUNK_SZ] r_chunksz,
		[RTMP_MSG_CTL] r_ctl,
		[RTMP_MSG_SERVER_BW] r_server_bw,
		[RTMP_MSG_CLIENT_BW] r_client_bw,
		[RTMP_MSG_AUDIO] r_audio,
		[RTMP_MSG_VIDEO] r_video,
		[RTMP_MSG_NOTIFY] r_notify,
		[RTMP_MSG_INVOKE] r_invoke,
	};
	struct rtmp_pkt pkt;
	int ret;

	pkt.chan = chan;
	pkt.dest = dest;
	pkt.ts = ts;
	pkt.type = type;

	if ( type >= ARRAY_SIZE(tbl) || NULL == tbl[type] ) {
		printf(".id = %d (0x%x)\n", chan, chan);
		printf(".dest = %d (0x%x)\n", dest, dest);
		printf(".ts = %d (0x%x)\n", ts, ts);
		printf(".sz = %zu\n", sz);
		printf(".type = %d (0x%x)\n", type, type);
		hex_dump(buf, sz, 16);
		return 1;
	}

	ret = (*tbl[type])(r, &pkt, buf, sz);
	return ret;
}

/* sigh.. */
static ssize_t decode_rtmp(struct _rtmp *r, const uint8_t *buf, size_t sz)
{
	static const size_t sizes[] = {12, 8, 4, 1};
	struct rtmp_chan *pkt;
	const uint8_t *ptr = buf, *end = buf + sz;
	uint8_t type;
	size_t nsz, chunk_sz;
	int chan;
	size_t cur_ts;

	type = (*ptr & 0xc0) >> 6;
	chan = (*ptr & 0x3f);
	if ( (++ptr) > end )
		return -1;

	switch(chan) {
	case 0:
		chan = *ptr + 64;
		ptr++;
		break;
	case 1:
		chan = (ptr[1] << 8) + ptr[0] + 64;
		ptr += 2;
		break;
	default:
		break;
	}

	pkt = r->chan + chan;
	nsz = sizes[type];

	if ( nsz >= 4 ) {
		cur_ts = pkt->cur_ts = decode_int24(ptr);
		ptr += 3;
	}else{
		cur_ts = pkt->cur_ts;
	}

	if ( nsz >= 8 ) {
		pkt->sz = decode_int24(ptr);
		ptr += 3;

		pkt->type = ptr[0];
		ptr++;
	}

	if ( nsz >= 12 ) {
		pkt->dest = decode_int32le(ptr);
		ptr += 4;
	}

	if ( cur_ts == 0xffffff ) {
		cur_ts = pkt->cur_ts = decode_int32(ptr);
		ptr += 4;
	}

	if ( NULL == pkt->p_buf ) {
		pkt->p_cur = pkt->p_buf = malloc(pkt->sz);
		pkt->p_left = pkt->sz;
	}

	chunk_sz = (pkt->p_left < r->chunk_sz) ? 
		pkt->p_left : r->chunk_sz;

	if ( ptr + chunk_sz > end ) {
		if ( pkt->p_cur == pkt->p_buf ) {
			free(pkt->p_buf);
			pkt->p_buf = NULL;
		}

		return -1;
	}

	memcpy(pkt->p_cur, ptr, chunk_sz);
	pkt->p_cur += chunk_sz;
	pkt->p_left -= chunk_sz;

	dprintf("rtmp: recv: %zu/%zu @ %zu\n", chunk_sz,
		(pkt->p_cur + pkt->p_left) - pkt->p_buf,
		(pkt->p_cur - pkt->p_buf));

	if ( !pkt->p_left ) {
		pkt->ts += cur_ts;

		rtmp_dispatch(r, chan, pkt->dest, pkt->ts, pkt->type,
				pkt->p_buf, pkt->p_cur - pkt->p_buf);

		free(pkt->p_buf);
		pkt->p_buf = NULL;
	}else{
		dprintf(" Fragment %zu left to go:\n", pkt->p_left);
		//hex_dump(ptr, chunk_sz, 16);
	}

	ptr += chunk_sz;
	return (ptr - buf);
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
		/* can't fail */
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
		ret = decode_rtmp(r, buf, sz);
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

