from urlparse import urlparse
from collections import deque
import httplib
import threading

from webparser import WebParser
from formripper import FormRipper
from camripper import CamRipper

class WorkQueue(threading.Thread):
	def __init__(self):
		threading.Thread.__init__(self)
		self.daemon = True
		self.queue = deque()
		self.lock = threading.Lock()
		self.wait = threading.Event(1)
	def push(self, thing):
		self.lock.acquire()
		self.queue.append(thing)
		self.lock.release()
		self.wait.set()
	def run(self):
		while True:
			self.wait.wait()

			self.lock.acquire()
			work = self.queue.popleft()
			if not len(self.queue):
				self.wait.clear()
			self.lock.release()

			if work is not None:
				self.do_work(work)

class Task:
	def __init__(self, cb):
		self.cb = cb
	def __repr__(self):
		return self.__class__.__name__
class LoginTask(Task):
	def __init__(self, login, pwd, cb):
		Task.__init__(self, cb)
		self.login = login
		self.pwd = pwd
class GetGirlTask(Task):
	def __init__(self, url, cb):
		Task.__init__(self, cb)
		self.url = urlparse(url)
class GetImageTask(Task):
	def __init__(self, url, cb):
		Task.__init__(self, cb)
		self.url = urlparse(url)
class GetGirlListTask(Task):
	pass

class WebThread(WorkQueue):
	USER_AGENT = 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:14.0) Gecko/20100101 Firefox/14.0.1'
	ACCEPT_LANG = 'en-us,en;q=0.5'

	def __init__(self, url, login = None, pwd = None):
		WorkQueue.__init__(self)
		self.daemon = True
		self.url = urlparse(url)
		self.login = login
		self.pwd = pwd
		self.iconn = {}

		if self.url.scheme != 'http':
			raise Exception, 'HTTP URLs only'

		self.connect()

	def convert(self, d, url):
		v = mayhem.vars()
		v.sid = int(d['p_sid'])
		v.pid = int(d['p_pid'])
		v.hd = int(d.get('p_hd', 0))
		v.ft = int(d['p_ft'])
		v.srv = int(d['p_srv'])
		v.tcUrl = d['turbo'].rsplit(';', 1)[0]
		v.signupargs = d.get('p_signupargs')
		v.sessiontype = d.get('sessionType')
		v.nickname = d.get('p_nickname')
		v.sakey = d.get('p_sakey')
		v.g = d.get('p_g')
		v.ldmov = d.get('p_ldmov')
		v.sk = d.get('p_sk')
		v.pageurl = url.geturl()
		return v

	def connect(self):
		u = self.url
		self.conn = httplib.HTTPConnection(u.hostname, u.port)

	def do_get_girl_list(self, cb):
		self.conn.request('GET', self.url.path)
		r = self.conn.getresponse()
		html = r.read()

		h = CamRipper()
		h.feed(html)
		for cam in h.cams:
			cb(cam.name, cam.url, cam.thumb)

	def do_get_image(self, u, cb):
		conn = self.iconn.get((u.hostname, u.port), None)
		if conn is None:
			print 'connect', u.hostname, u.port
			conn = httplib.HTTPConnection(u.hostname, u.port)

		conn.request('GET', u.path)
		r = conn.getresponse()
		self.iconn[(u.hostname, u.port)] = conn
		cb(r.read())

	def do_get_girl(self, url, cb):
		self.conn.request('GET', url.path)
		r = self.conn.getresponse()
		html = r.read()

		h = WebParser()
		h.feed(html)
		v = self.convert(h.result, url)
		print v.tcUrl
		cb(v)

	def get_girl_list(self, cb):
		t = GetGirlListTask(cb)
		self.push(t)

	def get_girl(self, url, cb):
		t = GetGirlTask(url, cb)
		self.push(t)

	def get_image(self, url, cb):
		t = GetImageTask(url, cb)
		self.push(t)

	def do_work(self, task):
		#print 'do task:', task
		if isinstance(task, GetGirlListTask):
			self.do_get_girl_list(task.cb)
		elif isinstance(task, GetImageTask):
			self.do_get_image(task.url, task.cb)
		elif isinstance(task, GetGirlTask):
			self.do_get_girl(task.url, task.cb)
