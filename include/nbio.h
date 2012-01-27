/*
 * This file is part of Firestorm NIDS
 * Copyright (c) 2004 Gianni Tedesco
 * Released under the terms of the GNU GPL version 2
*/
#ifndef _NBIO_HEADER_INCLUDED_
#define _NBIO_HEADER_INCLUDED_

typedef uint8_t nbio_flags_t;

/* Represents a given fd */
struct nbio {
	os_sock_t fd;
#define NBIO_READ	(1<<0)
#define NBIO_WRITE	(1<<1)
#define NBIO_ERROR	(1<<2)
#define NBIO_WAIT	(NBIO_READ|NBIO_WRITE|NBIO_ERROR)
	nbio_flags_t mask;
	nbio_flags_t flags;
	const struct nbio_ops *ops;
	struct list_head list;
	union {
		int poll;
		void *ptr;
	}ev_priv;
};

/* Represents all the I/Os for a given thread */
struct iothread {
	struct list_head inactive;
	struct list_head active;
	struct eventloop *plugin;
	union {
		int epoll;
		void *ptr;
	}priv;
	struct list_head deleted;
};

struct nbio_ops {
	void (*read)(struct iothread *t, struct nbio *n);
	void (*write)(struct iothread *t, struct nbio *n);
	void (*dtor)(struct iothread *t, struct nbio *n);
};

/* nbio API */
void nbio_add(struct iothread *, struct nbio *, nbio_flags_t);
void nbio_del(struct iothread *, struct nbio *);
void nbio_pump(struct iothread *, int mto);
void nbio_fini(struct iothread *);
int nbio_init(struct iothread *, const char *plugin);
void nbio_inactive(struct iothread *, struct nbio *, nbio_flags_t);
void nbio_set_wait(struct iothread *, struct nbio *, nbio_flags_t);
nbio_flags_t nbio_get_wait(struct nbio *io);
void nbio_to_waitq(struct iothread *, struct nbio *,
				struct list_head *q);
void nbio_wake(struct iothread *, struct nbio *, nbio_flags_t);
void nbio_wait_on(struct iothread *t, struct nbio *n, nbio_flags_t);

/* eventloop plugin API */
struct eventloop {
	const char *name;
	int (*init)(struct iothread *);
	void (*fini)(struct iothread *);
	void (*pump)(struct iothread *, int);
	void (*inactive)(struct iothread *, struct nbio *);
	void (*active)(struct iothread *, struct nbio *);
	struct eventloop *next;
};

void eventloop_add(struct eventloop *e);
struct eventloop *eventloop_find(const char *name);

/* Do not call */
void _eventloop_poll_ctor(void);
void _eventloop_epoll_ctor(void);
void _eventloop_iocp_ctor(void);

#endif /* _NBIO_HEADER_INCLUDED_ */
