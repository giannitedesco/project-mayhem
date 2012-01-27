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

/* Exception hierarchy */
static PyObject *pymayhem_err_base;

/* Generic functions */
void pymayhem_error(const char *str)
{
	PyErr_SetString(PyExc_SystemError, str);
}

/* methods of base class */
static PyMethodDef methods[] = {
	{NULL, }
};

#define PYMAYHEM_INT_CONST(m, c) PyModule_AddIntConstant(m, #c, c)
PyMODINIT_FUNC initmayhem(void);
PyMODINIT_FUNC initmayhem(void)
{
	PyObject *m;

	if ( PyType_Ready(&mayhem_pytype) < 0 )
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

	PyModule_AddObject(m, "Error", pymayhem_err_base);
	PyModule_AddObject(m, "mayhem", (PyObject *)&mayhem_pytype);
}
