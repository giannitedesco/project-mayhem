/*
 * This file is part of project mayhem
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include "pypm.h"

#include <wmdump.h>
#include <wmvars.h>
#include <rtmp/amf.h>
#include <os.h>
#include <list.h>
#include <nbio.h>
#include <rtmp/rtmp.h>
#include <mayhem.h>
#include <flv.h>
#include "cvars.h"

#include "pyvars.h"

struct pymayhem {
	PyObject_HEAD;
	struct iothread t;
	mayhem_t mayhem;
};

static void NaiadAuthorize(void *priv, int code,
				const char *nick,
				const char *bitch,
				unsigned int sid,
				struct naiad_room *room)
{
	PyObject *self = priv;
	PyObject *ret, *obj;

	obj = Py_None;
	Py_INCREF(obj);

	ret = PyObject_CallMethod(self, "NaiadAuthorize",
					"issiO",
					code,
					nick,
					bitch,
					sid,
					obj);
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

static void rip(void *priv, struct rtmp_pkt *pkt,
			const uint8_t *buf, size_t sz)
{
}

static void play(void *priv)
{
	PyObject *ret, *self = priv;
	ret = PyObject_CallMethod(self, "stream_play", "");
	if ( NULL == ret )
		return;
	Py_DECREF(ret);
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
	if ( NULL == ret )
		return;
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
	struct nbio *n;
	int fd, ev;

	if ( !PyArg_ParseTuple(args, "ii", &fd, &ev) )
		return NULL;

	list_for_each_entry(n, &self->t.inactive, list) {
		if ( n->fd != fd )
			continue;
		n->flags = ev;
		list_move_tail(&n->list, &self->t.active);
		Py_INCREF(Py_None);
		return Py_None;
	}

	PyErr_SetString(PyExc_ValueError, "fd not found");
	return NULL;
}

static PyObject *pymayhem_pump(struct pymayhem *self, PyObject *args,
				PyObject *kwds)
{
	nbio_pump(&self->t, -1);
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
	struct pypm_vars *vars;
	static const struct mayhem_ops ops = {
		.NaiadAuthorize = NaiadAuthorize,
		.NaiadFreeze = NaiadFreeze,

		.stream_play = play,
		.stream_reset = reset,
		.stream_stop = stop,
		.stream_error = error,
		.stream_packet = rip,
	};

	if ( !PyArg_ParseTuple(args, "O", &obj) )
		return -1;
	if ( !pypm_vars_Check(obj) ) {
		PyErr_SetString(PyExc_TypeError, "Expected pypm.vars");
		return -1;
	}

	nbio_init(&self->t, &eventloop_app);
	self->t.priv.ptr = self;

	vars = (struct pypm_vars *)obj;
	self->mayhem = mayhem_connect(&self->t, &vars->vars, &ops, self);
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
	self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef pymayhem_methods[] = {
	{"nbio_set_active",(PyCFunction)pymayhem_set_active, METH_VARARGS,
		"mayhem.nbio_set_active(fd, flags) - Make an fd active"},
	{"nbio_pump",(PyCFunction)pymayhem_pump, METH_VARARGS,
		"mayhem.nbio_pump() - Run an iteration of the event loop"},
#if 0
	{"abort",(PyCFunction)pymayhem_abort, METH_VARARGS,
		"mayhem.abort() - Abort"},
#endif
	{NULL, }
};

static PyGetSetDef pymayhem_attribs[] = {
#if 0
	{"machine", (getter)pymayhem_machine_get, NULL, "Machine"},
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

