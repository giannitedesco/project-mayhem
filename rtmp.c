/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>
#include <rtmp/amf.h>
#include <rtmp/rtmp.h>
#include <rtmp/proto.h>

#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "amfbuf.h"

#define STATE_INIT		0
#define STATE_ABORT		1
#define STATE_CONN_RESET	2
#define STATE_HANDSHAKE_1	3
#define STATE_HANDSHAKE_2	4
#define STATE_CONNECTED		4 /* equivalent to handshake_2 */

struct rtmp_chan {
	uint8_t *p_buf;
	uint8_t *p_cur;
	size_t p_left;

	uint32_t ts;
	uint32_t sz;
	uint32_t dest;
	uint8_t type;
};

struct _rtmp {
	size_t chunk_sz;
	size_t r_space;
	unsigned int state;
	uint32_t server_bw;
	uint8_t *r_buf;
	uint8_t *r_cur;
	os_sock_t sock;

	struct rtmp_chan chan[RTMP_MAX_CHANNELS];
};

static int transition(struct _rtmp *r, unsigned int state);

static int rtmp_send_raw(struct _rtmp *r, const uint8_t *buf, size_t len)
{
	const uint8_t *end;
	ssize_t ret;

	for(end = buf + len; buf < end; buf++) {
		ret = sock_send(r->sock, buf, end - buf);
		if ( ret < 0 )
			return transition(r, STATE_ABORT);
		if ( ret == 0 )
			return transition(r, STATE_CONN_RESET);

		buf += ret;
	}

	return 1;
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

static void encode_int24(uint8_t *buf, uint32_t val)
{
	buf[0] = (val >> 16) & 0xff;
	buf[1] = (val >> 8) & 0xff;
	buf[2] = val & 0xff;
}

static void encode_int32(uint8_t *buf, uint32_t val)
{
	buf[0] = (val >> 24) & 0xff;
	buf[1] = (val >> 16) & 0xff;
	buf[2] = (val >> 8) & 0xff;
	buf[3] = val & 0xff;
}

static uint32_t decode_int24(const uint8_t *ptr)
{
	return (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
}

static uint32_t decode_int32(const uint8_t *ptr)
{
	return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
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

	encode_int32(ptr, dest), ptr += 4;

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

static int rtmp_send(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
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

	printf("rtmp: Change chunk size: %zu\n", r->chunk_sz);
	new = realloc(r->r_buf, r->chunk_sz);
	if ( NULL == new )
		return transition(r, STATE_ABORT);

	ofs = r->r_cur - r->r_buf;

	r->r_buf = new;
	r->r_cur = new + ofs;
	r->r_space = r->chunk_sz;
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
		printf("Request bufsz %zu: deferred\n", sz);
		return 1;
	}

	/* should be safe to upsize it right away */
	return rbuf_update_size(r);
}

static int handshake1(struct _rtmp *r)
{
	uint8_t buf[RTMP_HANDSHAKE_LEN + 1];
	unsigned int i;

	/* make space for the response */
	if ( !rbuf_request_size(r, RTMP_HANDSHAKE_LEN * 2 + 1) )
		return 0;

	srand(time(NULL) ^ getpid());

	buf[0] = 3;

	for(i = 1; i < sizeof(buf); i++)
		buf[i] = rand();

	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x07;
	buf[7] = 0x02;

	if ( !rtmp_send_raw(r, buf, sizeof(buf)) )
		return 0;

	printf("rtmp: sent handshake 1\n");
	return 1;
}

static ssize_t handshake1_resp(struct _rtmp *r, const uint8_t *buf, size_t len)
{
	if ( len < RTMP_HANDSHAKE_LEN * 2 + 1 )
		return -1;

	if ( !transition(r, STATE_HANDSHAKE_2) )
		return 0;

	return RTMP_HANDSHAKE_LEN * 2 + 1;
}

static int handshake2(struct _rtmp *r)
{
	/* XXX: We rely on handhsake1 response still being sat in buffer */
	if ( !rtmp_send_raw(r, r->r_buf + 1,
				RTMP_HANDSHAKE_LEN) )
		return 0;

	/* now ready for RTMP chunks */
	if ( !rbuf_request_size(r, RTMP_DEFAULT_CHUNK_SZ) )
		return 0;

	printf("rtmp: sent handshake 2\n");
	return 1;
}

static int r_invoke(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			 const uint8_t *buf, size_t sz)
{
	invoke_t inv;

	printf("rtmp: received: INVOKE\n");
	inv = amf_invoke_from_buf(buf, sz);
	if ( NULL == inv )
		return 0;

	amf_invoke_pretty_print(inv);
	amf_invoke_free(inv);
	return 1;
}

static int r_server_bw(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			 const uint8_t *buf, size_t sz)
{
	printf("rtmp: received: SERVER_BW\n");
	if ( sz < sizeof(uint32_t) )
		return 0;

	r->server_bw = decode_int32(buf);
	return 1;
}

static int r_chunksz(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			 const uint8_t *buf, size_t sz)
{
	if ( sz < sizeof(uint32_t) )
		return 0;
	rbuf_request_size(r, decode_int32(buf));
	return 1;
}

typedef int (*rmsg_t)(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			 const uint8_t *buf, size_t sz);

static int rtmp_dispatch(struct _rtmp *r, int chan, uint32_t dest, uint32_t ts,
			 uint8_t type, const uint8_t *buf, size_t sz)
{
	rmsg_t tbl[] = {
		[RTMP_MSG_CHUNK_SZ] r_chunksz,
		[RTMP_MSG_SERVER_BW] r_server_bw,
		[RTMP_MSG_INVOKE] r_invoke,
	};

	if ( type >= ARRAY_SIZE(tbl) || NULL == tbl[type] ) {
		printf(".id = %d (0x%x)\n", chan, chan);
		printf(".dest = %d (0x%x)\n", dest, dest);
		printf(".ts = %d (0x%x)\n", ts, ts);
		printf(".sz = %zu\n", sz);
		printf(".type = %d (0x%x)\n", type, type);
		hex_dump(buf, sz, 16);
		return 1;
	}

	return (*tbl[type])(r, chan, dest, ts, buf, sz);
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
		return 0;

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
		cur_ts = decode_int24(ptr);
		ptr += 3;
	}else{
		cur_ts = 0;
	}

	if ( nsz >= 8 ) {
		pkt->sz = decode_int24(ptr);
		ptr += 3;

		pkt->type = ptr[0];
		ptr++;
	}

	if ( nsz >= 12 ) {
		pkt->dest = decode_int32(ptr);
		ptr += 4;
	}

	if ( cur_ts == 0xffffff ) {
		cur_ts = decode_int32(ptr);
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

		return 0;
	}

	memcpy(pkt->p_cur, ptr, chunk_sz);
	pkt->p_cur += chunk_sz;
	pkt->p_left -= chunk_sz;
//	printf("%zu/%zu @ %zu\n", chunk_sz,
//		(pkt->p_cur + pkt->p_left) - pkt->p_buf,
//		(pkt->p_cur - pkt->p_buf));

	if ( !pkt->p_left ) {
		if ( nsz < 12 ) {
			pkt->ts += cur_ts;
		}else{
			pkt->ts = cur_ts;
		}

		rtmp_dispatch(r, chan, pkt->dest, pkt->ts, pkt->type,
				pkt->p_buf, pkt->p_cur - pkt->p_buf);

		free(pkt->p_buf);
		pkt->p_buf = NULL;
	}else{
		//printf(" Fragment %zu left to go:\n", pkt->p_left);
		//hex_dump(ptr, chunk_sz, 16);
	}

	ptr += chunk_sz;
	return (ptr - buf);
}

int rtmp_invoke(rtmp_t r, invoke_t inv)
{
	uint8_t *buf;
	size_t sz;
	int ret;

	sz = amf_invoke_buf_size(inv);
	buf = malloc(sz);
	if ( NULL == buf )
		return 0;

	amf_invoke_to_buf(inv, buf);

	ret = rtmp_send(r, 3, 0, 1, RTMP_MSG_INVOKE, buf, sz);
	free(buf);
	return ret;
}

static char *urlparse(const char *url, uint16_t *port)
{
	const char *ptr = url, *tmp;
	char *buf, *prt;
	uint16_t pnum;
	int hsz;

	/* skip past scheme part of url */
	if ( strncmp(url, RTMP_SCHEME, strlen(RTMP_SCHEME)) ) {
		fprintf(stderr, "Bad scheme: %s\n", url);
		return 0;
	}
	ptr += strlen(RTMP_SCHEME);

	tmp = strchr(ptr, '/');
	if ( NULL == tmp )
		tmp = ptr + strlen(ptr);

	hsz = tmp - ptr;
	buf = malloc(hsz + 1);
	if ( NULL == buf )
		return 0;

	snprintf(buf, hsz + 1, "%.*s", hsz, ptr);

	prt = strchr(buf, ':');
	if ( prt ) {
		*prt = '\0';
		pnum = atoi(prt + 1);
	}else{
		pnum = RTMP_DEFAULT_PORT;
	}

	if ( !pnum || pnum == 0xffff ) {
		/* assume atoi went wrong */
		pnum = RTMP_DEFAULT_PORT;
	}

	*port = pnum;
	return buf;
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
		ret = handshake1_resp(r, buf, sz);
		break;
	case STATE_CONNECTED:
		ret = decode_rtmp(r, buf, sz);
		return sz;
	default:
		abort();
	}

	return ret;
}

static int fill_rcv_buf(struct _rtmp *r)
{
	ssize_t ret;
	ret = sock_recv(r->sock, r->r_cur, r->r_space);
	if ( ret < 0 )
		return transition(r, STATE_ABORT);
	if ( ret == 0 )
		return transition(r, STATE_CONN_RESET);

	r->r_cur += ret;
	r->r_space -= ret;
//	printf("rtmp: received %zu bytes\n", ret);

	return 1;
}

int rtmp_pump(rtmp_t r)
{
	ssize_t taken;

again:
	taken = rtmp_drain_buf(r);
	if ( !taken )
		return 0;

	/* need more data? */
	if ( taken < 0 ) {
		if ( !fill_rcv_buf(r) )
			return 0;
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
	if ( r->chunk_sz != current_buf_sz(r) &&
		r->chunk_sz >= (size_t)(r->r_cur - r->r_buf) ) {
		if ( !rbuf_update_size(r) )
			return 0;
	}

	return 1;
}

rtmp_t rtmp_connect(const char *tcUrl)
{
	struct _rtmp *r;
	char *name;
	uint16_t port;

	r = calloc(1, sizeof(*r));
	if ( NULL == r )
		goto out;

	r->sock = BAD_SOCKET;

	name = urlparse(tcUrl, &port);
	if ( NULL == name )
		goto out_free;

	printf("rtmp: Connecting to: %s:%d\n", name, port);
	r->sock = sock_connect(name, port);
	//r->sock = sock_connect("127.0.0.1", port);
	free(name);
	if ( BAD_SOCKET == r->sock )
		goto out_free;

	if ( !transition(r, STATE_HANDSHAKE_1) )
		goto out_close;

	while ( r->state != STATE_CONNECTED ) {
		if ( !rtmp_pump(r) ) {
			rtmp_close(r);
			return NULL;
		}
	}

	/* success */
	goto out;

out_close:
	free(r->r_buf);
	sock_close(r->sock);
out_free:
	free(r);
	r = NULL;
out:
	return r;
}

void rtmp_close(rtmp_t r)
{
	if ( r ) {
		unsigned int i;
		for(i = 0; i < RTMP_MAX_CHANNELS; i++) {
			free(r->chan[i].p_buf);
		}
		free(r->r_buf);
		sock_close(r->sock);
		free(r);
	}
}

static int transition(struct _rtmp *r, unsigned int state)
{
	int ret;

	switch(state) {
	case STATE_ABORT:
		printf("rtmp: aborted\n");
		r->state = state;
		return 0;
	case STATE_CONN_RESET:
		printf("rtmp: connection reset by peer\n");
		r->state = state;
		return 0;
	case STATE_HANDSHAKE_1:
		ret = handshake1(r);
		break;
	case STATE_HANDSHAKE_2:
		ret = handshake2(r);
		break;
	default:
		abort();
		break;
	}

	if ( ret )
		r->state = state;

	return ret;
}
