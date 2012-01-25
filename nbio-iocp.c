/*
 * This file is part of project-mayhem
 * Copyright (c) 2004 Gianni Tedesco
 * Released under the terms of the GNU GPL version 3
 *
 * Windows I/O completion port based eventloop
 */

#include <nbio.h>

static int iocp_init(struct iothread *t)
{
	t->priv.iocp = CreateIoCompletionPort(BAD_FD, NULL, 0UL, 0);
	if ( t->priv.iocp < 0 ) {
		msg(M_ERR, "Unable to create IOCP: %s", err_str());
		return 0;
	}
	return 1;
}

static void iocp_fini(struct iothread *t)
{
	fd_close(t->priv.iocp);
}

static void iocp_active(struct iothread *t, struct nbio *n)
{
}

static void iocp_pump(struct iothread *t, int mto)
{
	struct nbio *n;
	ULONG_PTR key;
	LPOVERLAPPED ov;
	DWORD sz;
	DWORD mt;
	BOOL ret;

	mt = ( mto < 0 ) ? INFINITE : mto;

	list_for_each_entry(n, &t->active, list)
		iocp_active(t, n);

	msg(M_DEBUG, "Sleeping on it");
	do {
		ret = GetQueuedCompletionStatus(t->priv.iocp,
						&sz, &key, &ov, mt);
		if ( ret == FALSE )
			return;
		n = (struct nbio *)key;
		msg(M_DEBUG, "We got one!! %u", n->fd);
		n->flags = n->mask;
		list_move_tail(&n->list, &t->active);
	}while(0);
	msg(M_DEBUG, "iocp: %s", err_str());
}

static void iocp_inactive(struct iothread *t, struct nbio *n)
{
	if ( n->ev_priv.poll == 1 )
		return;

	n->ev_priv.poll = 1;

	msg(M_DEBUG, "associate with IOCP");
	if ( CreateIoCompletionPort((HANDLE)n->fd, t->priv.iocp,
					(ULONG_PTR)n, 0) == NULL )
		msg(M_ERR, "iocp_inactive: %s", err_str());
}

static struct eventloop eventloop_iocp = {
	.name = "iocp",
	.init = iocp_init,
	.fini = iocp_fini,
	.inactive = iocp_inactive,
	.active = iocp_active,
	.pump = iocp_pump,
};

void _eventloop_iocp_ctor(void)
{
	eventloop_add(&eventloop_iocp);
}
