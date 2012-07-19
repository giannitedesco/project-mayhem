from HTMLParser import HTMLParser
from urllib import urlencode

class Girl:
	def __init__(self, id):
		self.id = id
		self.name = ''
		self.href = None
		self.thumb = None

class CamRipper(HTMLParser):
	def __init__(self):
		HTMLParser.__init__(self)
		self.stk = []
		self.girlstk = []
		self.cams = []
		self.namedata = False

	def handle_starttag(self, tag, attrs):
		girl = False
		goturl = False
		gotpic = False

		if tag.lower() == 'div':
			for (k,v) in attrs:
				if k.lower() == 'id' and v[:3] == 'sr-':
					self.girlstk.append(Girl(v))
					girl = True
		elif tag.lower() == 'a':
			for (k,v) in attrs:
				if k.lower() == 'class':
					if v == 'secondaryBioLink':
						self.namedata = True
					elif v == 'primaryBioLink':
						goturl = True
				elif k.lower() == 'href':
					href = v
		elif tag.lower() == 'img':
			for (k,v) in attrs:
				if k.lower() == 'class' and v == 'thumb':
					gotpic = True
				elif k.lower() == 'src':
					src = v

		if goturl or gotpic:
			g = self.girlstk.pop()
			if goturl:
				g.url = href
			if gotpic:
				g.thumb = src
			self.girlstk.append(g)

		if tag.lower() in ['div', 'a']:
			self.stk.append(girl)

	def handle_endtag(self, tag):
		if tag.lower() not in ['div', 'a']:
			return
		self.namedata = False
		girl = self.stk.pop()
		if girl:
			obj = self.girlstk.pop()
			self.cams.append(obj)

	def handle_data(self, data):
		if self.namedata:
			g = self.girlstk.pop()
			g.name = g.name + data
			self.girlstk.append(g)
		return

