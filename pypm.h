#ifndef _PY_PM_H
#define _PY_PM_H

#include <Python.h>

#define PACKAGE "mayhem"

void pymayhem_error(const char *str);

int idl__set_ulong(unsigned long *ref, PyObject *val);
int idl__set_long(long *ref, PyObject *val);
int idl__set_uint(unsigned int *ref, PyObject *val);
int idl__set_u32(uint32_t *ref, PyObject *val);
int idl__set_u8(uint8_t *ref, PyObject *val);
int idl__set_int(int *ref, PyObject *val);
int idl__set_str(char **ref, PyObject *val);

/* types */
extern PyTypeObject mayhem_pytype;

#endif /* _PY_PM_H */
