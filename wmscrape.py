#!/usr/bin/python

import pymayhem
from urlparse import urlparse
import httplib

def main(argv):
	for url in argv[1:]:
		u = urlparse(url)
		if u.scheme != 'http':
			raise Exception, 'HTTP URLs only'
		conn = httplib.HTTPConnection(u.hostname, u.port)
		conn.request('GET', u.path)
		r = conn.getresponse()
		if r.status != 200:
			raise Exception, r.status, r.reason
		html = r.read()

		print '$PageUrl:', url
		h = pymayhem.WebParser()
		h.feed(html)
		for (k, v) in h.result.items():
			print '%s: %s'%(k, v)
		print

	return True

if __name__ == '__main__':
	from sys import argv
	raise SystemExit, not main(argv)
