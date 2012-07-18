#!/usr/bin/python

from urlparse import urlparse
from Cookie import BaseCookie as Cookie
import httplib
import pymayhem

def findlogin(fa):
	for f in fa:
		for i in f:
			if i.name == 'trylogin':
				return f
	return None

def do_login(u, login, pwd):
	conn = httplib.HTTPConnection(u.hostname, u.port)
	conn.request('GET', u.path)
	r = conn.getresponse()
	if r.status != 200:
		raise Exception, r.status, r.reason
	h = pymayhem.FormRipper()
	h.feed(r.read())
	f = findlogin(h.forms)
	if f is None:
		raise ValueError, 'login form not found'

	f['sausr'] = login
	f['sapwd'] = pwd
	params = f.get_form_data()

	headers = {'Content-type': 'application/x-www-form-urlencoded',
			'Referer': u.geturl(),
			'User-Agent': 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:14.0) Gecko/20100101 Firefox/14.0.1',
			'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
			'Accept-Language': 'en-us,en;q=0.5'

			}

	lu = urlparse(f.action)
	conn = httplib.HTTPConnection(lu.hostname, lu.port)
	conn.request('POST', lu.path, params, headers)
	r = conn.getresponse()
	if r.status != 302:
		raise ValueError, 'bad response', r

	cookie = None
	location = None
	for (k, v) in r.getheaders():
		if k == 'set-cookie':
			cookie = v
		elif k == 'location':
			location = urlparse(v)

	c = Cookie(cookie)
	c = c.output(c.keys(), '', ', ').strip()
	q = location.query
	return (c, q)


def do_url(u, login = None, pwd = None):
	headers = {'Referer': u.geturl(),
		'User-Agent': 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:14.0) Gecko/20100101 Firefox/14.0.1',
		'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
		'Accept-Language': 'en-us,en;q=0.5',
		}
	path = u.path

	
	if login is not None and pwd is not None:
		(c, q) = do_login(u, login, pwd)
		headers['Cookie'] = c
		path = path + '?' + q

	conn = httplib.HTTPConnection(u.hostname, u.port)
	conn.request('GET', path, None, headers)
	r = conn.getresponse()
	if r.status != 200:
		raise Exception, r.status, r.reason
	h = pymayhem.WebParser()
	h.feed(r.read())
	print '$PageUrl:', u.geturl()
	for (k, v) in h.result.items():
		print '%s: %s'%(k, v)

	print

def main(argv):
	l = None
	p = None

	if argv[1] == '--login':
		l = argv[2]
		if argv[3] == '--password':
			p = argv[4]

	if l is not None:
		urls = argv[3:]
		if p is not None:
			urls = argv[5:]
	else:
		urls = argv[1:]

	for url in urls:
		u = urlparse(url)
		if u.scheme != 'http':
			raise Exception, 'HTTP URLs only'

		do_url(u, l, p)

	return True

if __name__ == '__main__':
	from sys import argv
	raise SystemExit, not main(argv)
