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

struct pymayhem {
	PyObject_HEAD;
	mayhem_t mayhem;
};

static int pymayhem_init(struct pymayhem *self, PyObject *args, PyObject *kwds)
{
#if 0
	PyObject *ret;
	ret = PyObject_CallMethod((PyObject *)self, "boo", "s", "Hello world");
	if ( NULL == ret )
		return -1;
#endif
	return 0;
}

static void pymayhem_dealloc(struct pymayhem *self)
{
	self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef pymayhem_methods[] = {
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

