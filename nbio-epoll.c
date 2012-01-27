/*
 * This file is part of Firestorm NIDS
 * Copyright (c) 2004 Gianni Tedesco
 * Released under the terms of the GNU GPL version 2
 *
 * Linux epoll based eventloop
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <list.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <stdio.h>

#include <os.h>
#include <nbio.h>

static int epoll_init(struct iothread *t)
{
	t->priv.epoll = epoll_create(1);
	if ( t->priv.epoll < 0 )
		return 0;
	return 1;
}

static void epoll_fini(struct iothread *t)
{
	while ( close(t->priv.epoll) && (errno == EINTR) )
		/* do nothing */;
}

static void epoll_active(struct iothread *t, struct nbio *n)
{
	if ( n->ev_priv.poll == 0 )
		return;
	if ( n->fd < 0 )
		return;

	n->ev_priv.poll = 0;
	epoll_ctl(t->priv.epoll, EPOLL_CTL_DEL, n->fd, NULL);
}

static void epoll_pump(struct iothread *t, int mto)
{
	struct epoll_event ev[8];
	struct nbio *n;
	int nfd, i;

again:
	nfd = epoll_wait(t->priv.epoll, ev, sizeof(ev)/sizeof(*ev), mto);
	if ( nfd < 0 ) {
		if ( errno == EINTR )
			goto again;
		fprintf(stderr, "epoll_wait: %s\n", strerror(errno));
		return;
	}

	for(i=0; i < nfd; i++) {
		n = ev[i].data.ptr;
		n->flags = 0;
		if ( ev[i].events & (EPOLLIN|EPOLLHUP) )
			n->flags |= NBIO_READ;
		if ( ev[i].events & EPOLLOUT )
			n->flags |= NBIO_WRITE;
		if ( ev[i].events & EPOLLERR )
			n->flags |= NBIO_ERROR;

		list_move_tail(&n->list, &t->active);
	}
}

static void epoll_inactive(struct iothread *t, struct nbio *n)
{
	struct epoll_event ev;

	if ( n->ev_priv.poll == 1 )
		return;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLERR|EPOLLHUP|EPOLLET;
	ev.data.ptr = n;

	if ( n->mask & NBIO_READ )
		ev.events |= EPOLLIN;
	if ( n->mask & NBIO_WRITE )
		ev.events |= EPOLLOUT;

	/* Eeek */
	if ( epoll_ctl(t->priv.epoll, EPOLL_CTL_ADD, n->fd, &ev) )
		return;

	n->ev_priv.poll = 1;

}

static struct eventloop eventloop_epoll = {
	.name = "epoll",
	.init = epoll_init,
	.fini = epoll_fini,
	.inactive = epoll_inactive,
	.active = epoll_active,
	.pump = epoll_pump,
};

void _eventloop_epoll_ctor(void)
{
	eventloop_add(&eventloop_epoll);
}
