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
#include <rtmp/proto.h>
#include <mayhem.h>
#include <flv.h>

#include "cvars.h"

/* IDL generated code */
#include "pyvars.h"
#include "pyrtmp_pkt.h"
#include "pynaiad_goldshow.h"
#include "pynaiad_room.h"
#include "pynaiad_user.h"

/* Exception hierarchy */
static PyObject *pymayhem_err_base;

/* Generic functions */
void pymayhem_error(const char *str)
{
	PyErr_SetString(pymayhem_err_base, str);
}

/* methods of base class */
static PyMethodDef methods[] = {
	{NULL, }
};

int idl__set_double(double *ref, PyObject *val)
{
	if ( NULL == val || val == Py_None ) {
		*ref = 0;
		return 0;
	}

	if ( PyFloat_Check(val) ) {
		*ref = PyFloat_AsDouble(val);
		return 0;
	}

	PyErr_SetString(PyExc_TypeError, "Type mismatch");
	return -1;
}

static int set_number(unsigned long *ref, PyObject *val)
{
	if ( NULL == val || val == Py_None ) {
		*ref = 0;
		return 0;
	}

	if ( PyLong_Check(val) ) {
		*ref = PyLong_AsLong(val);
		return 0;
	}else if ( PyInt_Check(val) ) {
		*ref = PyInt_AsLong(val);
		return 0;
	}

	PyErr_SetString(PyExc_TypeError, "Type mismatch");
	return -1;
}

int idl__set_ulong(unsigned long *ref, PyObject *val)
{
	unsigned long num;
	int ret;
	ret = set_number(&num, val);
	if ( !ret )
		*ref = num;
	return ret;
}

int idl__set_long(long *ref, PyObject *val)
{
	unsigned long num;
	int ret;
	ret = set_number(&num, val);
	if ( !ret )
		*ref = num;
	return ret;
}

int idl__set_uint(unsigned int *ref, PyObject *val)
{
	unsigned long num;
	int ret;
	ret = set_number(&num, val);
	if ( !ret ) {
		*ref = num;
	}
	return ret;
}

int idl__set_u32(uint32_t *ref, PyObject *val)
{
	unsigned long num;
	int ret;
	ret = set_number(&num, val);
	if ( !ret )
		*ref = num;
	return ret;
}

int idl__set_u8(uint8_t *ref, PyObject *val)
{
	unsigned long num;
	int ret;
	ret = set_number(&num, val);
	if ( !ret )
		*ref = num;
	return ret;
}

int idl__set_int(int *ref, PyObject *val)
{
	unsigned long num;
	int ret;
	ret = set_number(&num, val);
	if ( !ret )
		*ref = num;
	return ret;
}

int idl__set_str(char **ref, PyObject *val)
{
	if ( NULL == val || val == Py_None ) {
		free(*ref);
		*ref = NULL;
		return 0;
	}

	if ( PyString_Check(val) ) {
		char *str;
		str = strdup(PyString_AsString(val));
		if ( NULL == str ) {
			PyErr_SetString(PyExc_MemoryError, "Out of memory");
			return -1;
		}
		free(*ref);
		*ref = str;
		return 0;
	}

	PyErr_SetString(PyExc_TypeError, "Type mismatch");
	return -1;
}

#define PYMAYHEM_INT_CONST(m, c) PyModule_AddIntConstant(m, #c, c)
PyMODINIT_FUNC initmayhem(void);
PyMODINIT_FUNC initmayhem(void)
{
	PyObject *m;

	if ( PyType_Ready(&mayhem_pytype) < 0 )
		return;
	if ( PyType_Ready(&pypm_vars_pytype) < 0 )
		return;
	if ( PyType_Ready(&pypm_rtmp_pkt_pytype) < 0 )
		return;
	if ( PyType_Ready(&pypm_naiad_goldshow_pytype) < 0 )
		return;
	if ( PyType_Ready(&pypm_naiad_room_pytype) < 0 )
		return;
	if ( PyType_Ready(&pypm_naiad_user_pytype) < 0 )
		return;

	pymayhem_err_base = PyErr_NewException(PACKAGE ".Error",
						PyExc_RuntimeError, NULL);
	if ( NULL == pymayhem_err_base )
		return;

#if 0
	pymayhem_err_fmt = PyErr_NewException(PACKAGE ".FormatError",
						pymayhem_err_base, NULL);
	if ( NULL == pymayhem_err_fmt )
		return;
#endif

	m = Py_InitModule3(PACKAGE, methods, "Webcam Mayhem");
	if ( NULL == m )
		return;

	PYMAYHEM_INT_CONST(m, NBIO_READ);
	PYMAYHEM_INT_CONST(m, NBIO_WRITE);
	PYMAYHEM_INT_CONST(m, NBIO_ERROR);

	PYMAYHEM_INT_CONST(m, RTMP_MSG_AUDIO);
	PYMAYHEM_INT_CONST(m, RTMP_MSG_VIDEO);

	PyModule_AddObject(m, "Error",
				pymayhem_err_base);
	PyModule_AddObject(m, "mayhem",
				(PyObject *)&mayhem_pytype);
	PyModule_AddObject(m, "vars",
				(PyObject *)&pypm_vars_pytype);
	PyModule_AddObject(m, "rtmp_pkt",
				(PyObject *)&pypm_rtmp_pkt_pytype);
	PyModule_AddObject(m, "naiad_goldshow",
				(PyObject *)&pypm_naiad_goldshow_pytype);
	PyModule_AddObject(m, "naiad_room",
				(PyObject *)&pypm_naiad_room_pytype);
	PyModule_AddObject(m, "naiad_user",
				(PyObject *)&pypm_naiad_user_pytype);
}
