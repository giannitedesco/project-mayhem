from HTMLParser import HTMLParser
from urllib import urlencode

class FormInput:
	def __init__(self, attrs):
		self.__inputs = []
		self.type = None
		self.name = None
		self.value = None
		self.id = None
		for (k,v) in attrs:
			if k.lower() not in ['type', 'name', 'value', 'id']:
				continue
			setattr(self, k.lower().replace('-', '_'), v)
	def __repr__(self):
		if self.name is not None:
			return 'FormInput(%s,%s)'%(self.type, self.name)
		else:
			return 'FormInput(%s)'%(self.type)

class Form:
	def __init__(self, attrs):
		self.__inputs = {}
		self.name = None
		self.action = None
		self.method = 'post'
		for (k,v) in attrs:
			if k.lower() not in ['name', 'action', 'method']:
				continue
			setattr(self, k.lower().replace('-', '_'), v)
	def get_form_data(self):
		d = {}
		for x in self.__inputs.values():
			d[x.name] = x.value
		return urlencode(d)
	def __iter__(self):
		return iter(self.__inputs.values())
	def keys(self):
		return self.__inputs.keys()
	def append(self, item):
		if isinstance(item, FormInput):
			if item.name is not None:
				self.__inputs[item.name] = item
	def __repr__(self):
		if self.name is not None:
			return 'Form(%s,%s,%s)'%(self.name,
						self.method, self.action)
		else:
			return 'Form(%s,%s)'%(self.method, self.action)
	def __getitem__(self, k):
		return self.__inputs[k].value
	def __setitem__(self, k, v):
		self.__inputs[k].value = v
	def __str__(self):
		return repr(self)

class FormRipper(HTMLParser):
	def __init__(self):
		HTMLParser.__init__(self)
		self.stk = []
		self.forms = []
		self.result = {}

	def handle_starttag(self, tag, attrs):
		if tag.lower() == 'form':
			self.stk.append(Form(attrs))
			return
		if not len(self.stk):
			return
		cur = self.stk[-1]
		if tag.lower() == 'input':
			cur.append(FormInput(attrs))

	def handle_endtag(self, tag):
		if tag.lower() == 'form':
			self.forms.append(self.stk.pop())
	def handle_data(self, data):
		return

