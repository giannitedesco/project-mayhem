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
		self.start()
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
				self._do_work(work)

class WebReq:
	def __init__(self, cb, url,
			method = 'GET', content = None, headers = {}):
		self.url = url
		self.method = method
		self.content = content
		self.headers = headers
		self.cb = cb

class WebConn(WorkQueue):
	USER_AGENT = 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:14.0) Gecko/20100101 Firefox/14.0.1'
	ACCEPT_LANG = 'en-us,en;q=0.5'

	def __init__(self, u):
		WorkQueue.__init__(self)
		self.hostname = u.hostname
		self.port = u.port
		print 'conn:', u.hostname, u.port
		self.conn = httplib.HTTPConnection(u.hostname, u.port)
	def pushreq(self, req):
		assert(req.url.hostname == self.hostname)
		assert(req.url.port == self.port)
		self.push(req)
	def _do_work(self, req):
		try:
			self.conn.request(req.method, req.url.path,
					req.content, req.headers)
			r = self.conn.getresponse()
			d = r.read()
		except Exception, e:
			r = e
			d = None
		req.cb(r, d)

class WebThread:
	def __init__(self, login = None, pwd = None):
		self.login = login
		self.pwd = pwd
		self.conn = {}

	def get_conn(self, url):
		conn = self.conn.get((url.hostname, url.port), None)
		if conn is None:
			conn = WebConn(url)
			self.conn[(url.hostname, url.port)] = conn
		return conn

	def get_girl_list(self, url, cb, err = None):
		url = urlparse(url)
		def Closure(r, data):
			if isinstance(r, Exception) or r.status != 200:
				if err:
					err(r)
				return
			h = CamRipper()
			h.feed(data)
			for cam in h.cams:
				cb(cam.name, cam.url, cam.thumb)

		conn = self.get_conn(url)
		req = WebReq(Closure, url)
		conn.pushreq(req)

	def get_image(self, url, cb, err = None):
		url = urlparse(url)
		def Closure(r, data):
			if isinstance(r, Exception) or r.status != 200:
				if err:
					err(r)
				return
			cb(data)

		conn = self.get_conn(url)
		req = WebReq(Closure, url)
		conn.pushreq(req)

	def get_girl(self, url, cb, err = None):
		url = urlparse(url)
		def Closure(r, data):
			if isinstance(r, Exception) or r.status != 200:
				if err:
					err(r)
				return
			h = WebParser()
			h.feed(data)
			cb(h.result, url)

		conn = self.get_conn(url)
		req = WebReq(Closure, url)
		conn.pushreq(req)
