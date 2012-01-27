#ifndef _PY_PM_H
#define _PY_PM_H

#include <Python.h>

#define PACKAGE "mayhem"

void pymayhem_error(const char *str);

/* types */
extern PyTypeObject mayhem_pytype;

#endif /* _PY_PM_H */
