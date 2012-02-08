/*
 * This file is part of project mayhem
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include "pypm.h"

#include <wmdump.h>
#include <wmvars.h>
#include <rtmp/amf.h>
#include <rtmp/proto.h>
#include <os.h>
#include <list.h>
#include <nbio.h>
#include <rtmp/rtmp.h>
#include <mayhem.h>
#include <flv.h>
#include "cvars.h"

#include "pyvars.h"
#include "pyrtmp_pkt.h"
#include "pynaiad_goldshow.h"
#include "pynaiad_room.h"
#include "pynaiad_user.h"

struct pymayhem {
	PyObject_HEAD;
	struct iothread t;
	struct pypm_vars *vars;
	mayhem_t mayhem;
	int fuck;
};

static void NaiadAuthorize(void *priv, int code,
				const char *nick,
				const char *bitch,
				unsigned int sid,
				struct naiad_room *room)
{
	PyObject *ret, *self = priv;
	struct pypm_naiad_room *r;


	r = (struct pypm_naiad_room *)pypm_naiad_room_New(room);
	if ( NULL == r ) {
		return;
	}

	r->naiad_room.topic = (r->naiad_room.topic) ?
					strdup(r->naiad_room.topic) :
					NULL;
	ret = PyObject_CallMethod(self, "NaiadAuthorize",
					"issiO",
					code,
					nick,
					bitch,
					sid,
					r);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void NaiadFreeze(void *priv, int code, void *u1,
				int u2, const char *desc)
{
	PyObject *self = priv;
	PyObject *ret;
	ret = PyObject_CallMethod(self, "NaiadFreeze",
					"isis",
					code,
					NULL,
					u2,
					desc);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void NaiadUserList(void *priv, unsigned int ac,
				struct naiad_user *usr, unsigned int nusr)
{
	PyObject *ret, *l, *self = priv;
	unsigned int i;

	l = PyList_New(nusr);
	if ( NULL == l )
		return;

	for(i = 0; i < nusr; i++) {
		struct pypm_naiad_user *u;

		u = (struct pypm_naiad_user *)pypm_naiad_user_New(&usr[i]);
		if ( NULL == u ) {
			/* FIXME: leak */
			return;
		}

		u->naiad_user.id = (u->naiad_user.id) ?
					strdup(u->naiad_user.id) :
					NULL;
		u->naiad_user.name = (u->naiad_user.name) ?
					strdup(u->naiad_user.name) :
					NULL;

		PyList_SetItem(l, i, (PyObject *)u);
	}

	ret = PyObject_CallMethod(self, "NaiadUserList", "iO", i, l);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void NaiadPreGoldShow(void *priv, struct naiad_goldshow *gs)
{
	PyObject *ret, *self = priv;
	struct pypm_naiad_goldshow *g;

	g = (struct pypm_naiad_goldshow *)pypm_naiad_goldshow_New(gs);
	if ( NULL == g ) {
		return;
	}

	g->naiad_goldshow.showtopic = (g->naiad_goldshow.showtopic) ?
					strdup(g->naiad_goldshow.showtopic) :
					NULL;

	ret = PyObject_CallMethod(self, "NaiadPreGoldShow", "O", g);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void NaiadAddChat(void *priv, const char *nick, const char *chat)
{
	PyObject *self = priv;
	PyObject *ret;
	ret = PyObject_CallMethod(self, "NaiadAddChat",
					"ss",
					nick,
					chat);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void push_buffer(PyObject *self, const uint8_t *buf, size_t sz)
{
	PyObject *ret;

	ret = PyObject_CallMethod(self, "push_flv", "s#", buf, sz);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

struct flv_tag {
	uint8_t type;
	uint8_t len[3];
	uint8_t ts[3];
	uint8_t ts_high;
	uint8_t id[3];
}__attribute__((packed));

static void rip(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
	PyObject *self = priv;
	uint8_t *fbuf;
	uint8_t prev_tag_sz[4];
	struct flv_tag tag;
	size_t fsz, tsz;

	if ( !sz )
		return;

	switch(pkt->type) {
	case RTMP_MSG_AUDIO:
		if ( sz <= 2 )
			return;
		break;
	case RTMP_MSG_VIDEO:
		if ( sz <= 6 )
			return;
		break;
	default:
		break;
	}

	fsz = sizeof(tag) + sz + sizeof(prev_tag_sz);
	fbuf = malloc(fsz);
	if ( NULL == fbuf )
		return;

	memset(&tag, 0, sizeof(tag));
	tag.type = pkt->type;
	tag.len[0] = (sz >> 16) & 0xff;
	tag.len[1] = (sz >> 8) & 0xff;
	tag.len[2] = (sz) & 0xff;
	tag.ts[0] = (pkt->ts >> 16) & 0xff;
	tag.ts[1] = (pkt->ts >> 8) & 0xff;
	tag.ts[2] = (pkt->ts) & 0xff;
	tag.ts_high = (pkt->ts >> 24) & 0xff;

	tsz = sz + sizeof(tag);
	prev_tag_sz[0] = (tsz >> 24) & 0xff;
	prev_tag_sz[1] = (tsz >> 16) & 0xff;
	prev_tag_sz[2] = (tsz >> 8) & 0xff;
	prev_tag_sz[3] = (tsz) & 0xff;

	memcpy(fbuf, &tag, sizeof(tag));
	memcpy(fbuf + sizeof(tag), buf, sz);
	memcpy(fbuf + sizeof(tag) + sz, prev_tag_sz, sizeof(prev_tag_sz));
	push_buffer(self, fbuf, fsz);

	free(fbuf);
}

static void play(void *priv)
{
	PyObject *ret, *self = priv;
	static const uint8_t hdr[] = {'F', 'L', 'V', 1, 5, 0, 0, 0, 9,
					0, 0, 0, 0};
	ret = PyObject_CallMethod(self, "stream_play", "");
	if ( NULL == ret )
		return;
	Py_DECREF(ret);

	push_buffer(self, hdr, sizeof(hdr));
}

static void stop(void *priv)
{
	PyObject *ret, *self = priv;
	ret = PyObject_CallMethod(self, "stream_stop", "");
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void error(void *priv, const char *code, const char *desc)
{
	PyObject *ret, *self = priv;
	ret = PyObject_CallMethod(self, "stream_error", "ss", code, desc);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void conn_error(void *priv, const char *code, const char *desc)
{
	PyObject *ret, *self = priv;
	ret = PyObject_CallMethod(self, "conn_error", "ss", code, desc);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static void reset(void *priv)
{
	PyObject *ret, *self = priv;
	ret = PyObject_CallMethod(self, "stream_reset", "");
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static int app_init(struct iothread *t)
{
	return 1;
}

static void app_fini(struct iothread *t)
{
}

static void app_active(struct iothread *t, struct nbio *n)
{
	PyObject *self;
	PyObject *ret;

	self = t->priv.ptr;

	if ( !n->ev_priv.poll )
		return;
	if ( n->fd < 0 )
		return;
	n->ev_priv.poll = 0;
	ret = PyObject_CallMethod(self, "nbio_active",
					"i", n->fd);
	if ( NULL == ret ) {
		struct pymayhem *this = (struct pymayhem *)self;
		this->fuck = 1;
		return;
	}
	Py_DECREF(ret);
}

static void app_pump(struct iothread *t, int mto)
{
	/* nothing to do, it's all done in set_active() */
}

static void app_inactive(struct iothread *t, struct nbio *n)
{
	PyObject *self;
	PyObject *ret;

	self = t->priv.ptr;

	if ( n->ev_priv.poll )
		return;
	if ( n->fd < 0 )
		return;

	n->ev_priv.poll = 1;
	ret = PyObject_CallMethod(self, "nbio_inactive",
					"ii", n->fd, n->mask);
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
}

static PyObject *pymayhem_set_active(struct pymayhem *self, PyObject *args,
				PyObject *kwds)
{
	struct nbio *n, *tmp;
	int fd, ev;

	self->fuck = 0;

	if ( !PyArg_ParseTuple(args, "ii", &fd, &ev) )
		return NULL;

	list_for_each_entry_safe(n, tmp, &self->t.inactive, list) {
		if ( n->fd != fd )
			continue;
		n->flags = ev;
		list_move_tail(&n->list, &self->t.active);
		self->t.plugin->active(&self->t, n);
		if ( self->fuck )
			return NULL;
		Py_INCREF(Py_None);
		return Py_None;
	}

	PyErr_SetString(PyExc_ValueError, "fd not found");
	return NULL;
}

static PyObject *pymayhem_pump(struct pymayhem *self)
{
	PyObject *ret;

	nbio_pump(&self->t, -1);

	ret = list_empty(&self->t.active) ? Py_False : Py_True;
	Py_INCREF(ret);
	return ret;
}

static PyObject *pymayhem_abort(struct pymayhem *self)
{
	mayhem_abort(self->mayhem);
	Py_INCREF(Py_None);
	return Py_None;
}

static struct eventloop eventloop_app = {
	.name = "app",
	.init = app_init,
	.fini = app_fini,
	.inactive = app_inactive,
	.active = app_active,
	.pump = app_pump,
};

static int pymayhem_init(struct pymayhem *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;
	static const struct mayhem_ops ops = {
		.NaiadAuthorize = NaiadAuthorize,
		.NaiadFreeze = NaiadFreeze,
		.NaiadPreGoldShow = NaiadPreGoldShow,
		.NaiadAddChat = NaiadAddChat,
		.NaiadUserList = NaiadUserList,

		.stream_play = play,
		.stream_reset = reset,
		.stream_stop = stop,
		.stream_error = error,
		.stream_packet = rip,

		.connect_error = conn_error,
	};

	if ( !PyArg_ParseTuple(args, "O", &obj) )
		return -1;
	if ( !pypm_vars_Check(obj) ) {
		PyErr_SetString(PyExc_TypeError, "Expected pypm.vars");
		return -1;
	}

	if ( !sock_init(0) ) {
		PyErr_SetString(PyExc_OSError, "sock_init() failed");
		return -1;
	}
	nbio_init(&self->t, &eventloop_app);
	self->t.priv.ptr = self;

	Py_INCREF(obj);
	self->vars = (struct pypm_vars *)obj;
	self->mayhem = mayhem_connect(&self->t, &self->vars->vars, &ops, self);
	if ( NULL == self->mayhem ) {
		pymayhem_error("mayhem_connect failed");
		return -1;
	}

	do{
		nbio_pump(&self->t, -1);
	}while( !list_empty(&self->t.active) );

	return 0;
}

static void pymayhem_dealloc(struct pymayhem *self)
{
	mayhem_close(self->mayhem);
	Py_DECREF(self->vars);
	nbio_fini(&self->t);
	sock_fini();
	self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef pymayhem_methods[] = {
	{"nbio_set_active",(PyCFunction)pymayhem_set_active, METH_VARARGS,
		"mayhem.nbio_set_active(fd, flags) - Make an fd active"},
	{"nbio_pump",(PyCFunction)pymayhem_pump, METH_NOARGS,
		"mayhem.nbio_pump() - Run an iteration of the event loop"},
	{"abort",(PyCFunction)pymayhem_abort, METH_NOARGS,
		"mayhem.abort() - Abort"},
	{NULL, }
};

static PyGetSetDef pymayhem_attribs[] = {
#if 0
	{"aborted", (getter)pymayhem_aborted_get, NULL, "Is aborted?"},
#endif
	{NULL, }
};

PyTypeObject mayhem_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = PACKAGE ".mayhem",
	.tp_basicsize = sizeof(struct pymayhem),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)pymayhem_init,
	.tp_dealloc = (destructor)pymayhem_dealloc,
	.tp_methods = pymayhem_methods,
	.tp_getset = pymayhem_attribs,
	.tp_doc = "ELF file",
};

