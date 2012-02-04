from sys import argv

class Field:
	def __init__(self, name, **kw):
		self.name = name
		self.doc = kw.get('doc', self.name)

class Pointer(Field):
	def __init__(self, name, **kw):
		Field.__init__(self, name, **kw)
class Scalar(Field):
	def __init__(self, name, **kw):
		Field.__init__(self, name, **kw)

class Double(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyFloat_FromDouble(%s)'%ref
	def set(self, ref, val):
		return 'idl__set_double(%s, %s)'%(ref, val)

class Long(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyLong_FromLongLong(%s)'%ref
	def set(self, ref, val):
		return 'idl__set_long(%s, %s)'%(ref, val)

class ULong(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyLong_FromUnsignedLongLong(%s)'%ref
	def set(self, ref, val):
		return 'idl__set_ulong(%s, %s)'%(ref, val)

class Int(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyInt_FromLong(%s)'%ref
	def set(self, ref, val):
		return 'idl__set_int(%s, %s)'%(ref, val)

class UInt(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyInt_FromLong(%s)'%ref
	def set(self, ref, val):
		return 'idl__set_uint(%s, %s)'%(ref, val)

class UInt32(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyInt_FromLong(%s)'%ref
	def set(self, ref, val):
		return 'idl__set_u32(%s, %s)'%(ref, val)

class UInt8(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyInt_FromLong(%s)'%ref
	def set(self, ref, val):
		return 'idl__set_u8(%s, %s)'%(ref, val)

class FixedBuffer(Scalar):
	def __init__(self, name, sz, **kw):
		Scalar.__init__(self, name, **kw)
		self.sz = sz
	def get(self, ref):
		return 'PyByteArray_FromStringAndSize((const char *)%s, %d)'%(ref, self.sz)

class String(Pointer):
	def __init__(self, name, **kw):
		Pointer.__init__(self, name, **kw)
	def get(self, ref):
		return 'PyString_FromString((const char *)%s)'%(ref)
	def set(self, ref, val):
		return 'idl__set_str((char **)%s, %s)'%(ref, val)

class Synthetic(Scalar):
	def __init__(self, name, **kw):
		Scalar.__init__(self, name, **kw)

class Struct:
	def __init__(self, ctype, name, modname = None, fields = [], doc = None, byref = False):
		self.ctype = ctype
		self.name = name
		self.fields = fields
		self.modname = modname
		self.doc = doc
		self.byref = byref

	def __get_fullname(self):
		if self.modname is None:
			return self.name
		else:
			return '%s_%s'%(self.modname, self.name)

	def __get_doc(self):
		if self.doc is None:
			return self.name
		else:
			return self.doc

	def __get_deref(self):
		return self.name + (self.byref and '->' or '.')

	def __getattr__(self, attr):
		d = {'fullname': self.__get_fullname, 
			'deref': self.__get_deref,
			'doc': self.__get_doc}
		if d.has_key(attr):
			return d[attr]()
		raise AttributeError

	def __getset(self, f):
		return '{"%s", (getter)%s__%s_get, (setter)%s__%s_set, "%s"},'%(
							f.name,
							self.fullname, f.name,
							self.fullname, f.name,
							f.doc)

	def __getset_s(self, f):
		return '{"%s", (getter)%s_%s_get, NULL, "%s"},'%(f.name,
							f.name,
							self.fullname, f.name,
							self.fullname, f.name,
							f.doc)

	def __getter(self, f):
		l = []
		l.append('static PyObject *%s__%s_get(struct %s *self)'%(
							self.fullname,
							f.name, self.fullname))
		l.append('{')
		if isinstance(f, Pointer):
			l.append('\tif ( NULL == self->%s%s ) {'%(self.deref,
								f.name))
			l.append('\t\tPy_INCREF(Py_None);')
			l.append('\t\treturn Py_None;')
			l.append('\t}')
		l.append('\treturn ' + f.get('self->%s%s'%(self.deref, f.name)) + ';')
		l.append('}')
		return '\n'.join(l) + '\n'

	def __setter(self, f):
		l = []
		l.append('static int %s__%s_set(struct %s *self, PyObject *value, void *closure)'%(
							self.fullname,
							f.name, self.fullname))
		l.append('{')
		l.append('\treturn ' + f.set('&self->%s%s'%(self.deref, f.name),
						'value') + ';')
		l.append('}')
		return '\n'.join(l) + '\n'

	def decls(self):
		l = []
		l.append('struct %s {'%self.fullname)
		l.append('\tPyObject_HEAD;')
		l.append('\t%s %s;'%(self.ctype, self.name))
		l.append('};')
		l.append('extern PyTypeObject %s_pytype;'%(self.fullname))
		l.append('PyObject *%s_New(%s *%s);'%(self.fullname, self.ctype, self.name))
		l.append('int %s_Check(PyObject *obj);'%(self.fullname))
		return '\n'.join(l) + '\n'

	def defns(self):
		l = []
		l.append('static PyMethodDef %s_methods[] = {'%self.fullname)
		l.append('\t{NULL, }')
		l.append('};')
		l.append('')
		for f in filter(lambda f:not isinstance(f, Synthetic), self.fields):
			l.append(self.__getter(f))
			l.append(self.__setter(f))
		l.append('static PyGetSetDef %s_attribs[] = {'%self.fullname)
		for f in self.fields:
			if isinstance(f, Synthetic):
				l.append('\t' + self.__getset_s(f))
			else:
				l.append('\t' + self.__getset(f))
		l.append('\t{NULL, }')
		l.append('};')
		l.append('')
		l.append('static void %s_dealloc(struct %s *self)'%(self.fullname, self.fullname))
		l.append('{')
		for f in self.fields:
			if isinstance(f, Pointer):
				l.append('\tfree((void *)self->%s%s);'%(self.deref,
								f.name))
		l.append('\tself->ob_type->tp_free((PyObject*)self);')
		l.append('}')
		l.append('')
		l.append('PyTypeObject %s_pytype = {'%self.fullname)
		l.append('\tPyObject_HEAD_INIT(NULL)')
		l.append('\t.tp_name = PACKAGE ".%s",'%self.name)
		l.append('\t.tp_basicsize = sizeof(struct %s),'%self.fullname)
		l.append('\t.tp_flags = Py_TPFLAGS_DEFAULT,')
		l.append('\t.tp_new = PyType_GenericNew,')
		l.append('\t.tp_dealloc = (destructor)%s_dealloc,'%self.fullname)
		l.append('\t.tp_methods = %s_methods,'%self.fullname)
		l.append('\t.tp_getset = %s_attribs,'%self.fullname)
		l.append('\t.tp_doc = "%s",'%self.doc)
		l.append('};')
		l.append('')
		l.append('int %s_Check(PyObject *obj)'%(self.fullname))
		l.append('{')
		l.append('\treturn obj->ob_type == &%s_pytype;'%self.fullname)
		l.append('}')
		l.append('')
		l.append('PyObject *%s_New(%s *%s)'%(self.fullname, self.ctype, self.name))
		l.append('{')
		l.append('\tstruct %s *self;'%self.fullname)
		l.append('\tself = (struct %s *)'%self.fullname)
		l.append('\t\t%s_pytype.tp_alloc(&%s_pytype, 0);'%(self.fullname, self.fullname))
		l.append('\tif ( NULL == self )')
		l.append('\t\treturn NULL;')
		l.append('\tmemcpy(&self->%s, %s, sizeof(self->%s));'%(self.name, self.name, self.name))
		l.append('\treturn (PyObject *)self;')
		l.append('}')
		return '\n'.join(l)

class CFiles:
	def __init__(self, name, structs):
		self.name = name
		self.__structs = list(structs)
		self.__sysincl = ['Python.h']
		self.__incl = []

	def include(self, path, system = True):
		if system:
			self.__sysincl.append(str(path))
		else:
			self.__incl.append(str(path))

	def __getattr__(self, attr):
		if attr == 'include_guard':
			return '_%s_H'%self.name.upper()
		raise AttributeError

	def __write_hdr(self, f):
		print ' [IDL]', f.name
		f.write('/* Autogenerated by %s: DO NOT EDIT */\n'%argv[0])
		f.write('#ifndef %s\n'%self.include_guard)
		f.write('#define %s\n'%self.include_guard)
		f.write('\n')
		for s in self.__structs:
			f.write(s.decls() + '\n')
		f.write('#endif /* %s */\n'%self.include_guard)

	def __write_c(self, f):
		print ' [IDL]', f.name
		f.write('/* Autogenerated by %s: DO NOT EDIT */\n'%argv[0])
		for i in self.__sysincl:
			f.write('#include <%s>\n'%i)
		for i in self.__incl:
			f.write('#include "%s"\n'%i)
		if len(self.__sysincl) or len(self.__incl):
			f.write('\n')
		f.write('#include "%s.h"\n\n'%self.name)
		for s in self.__structs:
			f.write(s.defns() + '\n')

	def write(self):
		self.__write_hdr(open('%s.h'%self.name, 'w'))
		self.__write_c(open('%s.c'%self.name, 'w'))
