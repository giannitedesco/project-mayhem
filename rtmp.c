/* This file is part of wmdump
 * Copyright (c) 2011 Gianni Tedesco
 * Released under the terms of GPLv3
*/
#include <wmdump.h>
#include <os.h>
#include <rtmp/rtmp.h>
#include <rtmp/proto.h>

#include "handshake.h"

#define STATE_INIT		0
#define STATE_ABORT		1
#define STATE_CONN_RESET	2
#define STATE_HANDSHAKE_1	3
#define STATE_HANDSHAKE_2	4
#define STATE_CONNECTED		5

struct _rtmp {
	unsigned int state;
	size_t chunk_sz;
	uint8_t *r_buf;
	uint8_t *r_cur;
	size_t r_space;
	os_sock_t sock;
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

	printf("Change bufsz %zu\n", r->chunk_sz);
	new = realloc(r->r_buf, r->chunk_sz);
	if ( NULL == new )
		return transition(r, STATE_ABORT);

	r->r_buf = r->r_cur = new;
	r->r_space = r->chunk_sz;
	return 1;
}

/* to be used within the generic rtmp code, this prevents the buffer being
 * re-sized while the recv/dispatch main program loop still has unprocessed
 * packet data in there
*/
static int rbuf_request_size(struct _rtmp *r, size_t sz)
{
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

	/* make space for the response */
	if ( !rbuf_request_size(r, RTMP_HANDSHAKE_LEN * 2 + 1) )
		return 0;

	buf[0] = 3;
	memcpy(buf + 1, rtmp_handshake, RTMP_HANDSHAKE_LEN);

	if ( !rtmp_send_raw(r, buf, sizeof(buf)) )
		return 0;

	printf("rtmp: sent handshake 1\n");
	return 1;
}

static int handshake1_resp(struct _rtmp *r, const uint8_t *buf, size_t len)
{
	if ( len < RTMP_HANDSHAKE_LEN * 2 + 1 )
		return -1;

	if ( !transition(r, STATE_HANDSHAKE_2) )
		return 0;

	return RTMP_HANDSHAKE_LEN * 2 + 1;
}

static int handshake2(struct _rtmp *r)
{
	/* XXX: Rely on handhsake1 response still being sat in buffer */
	if ( !rtmp_send_raw(r, r->r_buf + RTMP_HANDSHAKE_LEN + 1,
				RTMP_HANDSHAKE_LEN) )
		return 0;

	/* now ready for RTMP chunks */
	if ( !rbuf_request_size(r, RTMP_DEFAULT_CHUNK_SZ) )
		return 0;

	printf("rtmp: sent handshake 2\n");
	return 1;
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
	printf("rtmp: received %zu bytes\n", ret);

	return 1;
}

static int rtmp_pump(struct _rtmp *r)
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
		r->chunk_sz >= (r->r_cur - r->r_buf) ) {
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
	free(name);
	if ( BAD_SOCKET == r->sock )
		goto out_free;

	if ( !transition(r, STATE_HANDSHAKE_1) )
		goto out_close;

	rtmp_pump(r);

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
