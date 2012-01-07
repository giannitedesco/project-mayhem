#!/usr/bin/python

from HTMLParser import HTMLParser
from urlparse import urlparse
from urllib import unquote
import httplib

# parse dict-like syntax    
from pyparsing import (Suppress, Regex, quotedString, Word, alphas, Group, alphanums, oneOf, Forward, Optional, dictOf, delimitedList, removeQuotes)

LBRACK,RBRACK,LBRACE,RBRACE,COLON,COMMA = map(Suppress,"[]{}:,")
integer = Regex(r"[+-]?\d+").setParseAction(lambda t:int(t[0]))
real = Regex(r"[+-]?\d+\.\d*").setParseAction(lambda t:float(t[0]))
string_ = Word(alphas,alphanums+"_") | quotedString.setParseAction(removeQuotes)
bool_ = oneOf("true false").setParseAction(lambda t: t[0]=="true")
jsParser = Forward()

key = string_
dict_ = LBRACE - Optional(dictOf(key+COLON, jsParser+Optional(COMMA))) + RBRACE
list_ = LBRACK - Optional(delimitedList(jsParser)) + RBRACK
jsParser << (real | integer | string_ | bool_ | Group(list_ | dict_ ))

class WMParser(HTMLParser):
	def __init__(self):
		HTMLParser.__init__(self)
		self.stk = []

	def handle_starttag(self, tag, attrs):
		self.stk.append(tag)
	def handle_endtag(self, tag):
		self.stk.pop()
	def handle_data(self, data):
		b = 'var hClientFlashVars ='
		if len(self.stk) and not self.stk[-1:][0].lower() == 'script':
			return
		s = data.lstrip()
		s = s.rstrip()

		if s[:len(b)] != b:
			return

		s = s[len(b):s.index(';\n')].lstrip()
		obj = jsParser.parseString(s)
		for k, v in obj[0]:
			print '%s: %s'%(k, unquote(v))

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
		h = WMParser()
		h.feed(html)
		print

	return True

if __name__ == '__main__':
	from sys import argv
	raise SystemExit, not main(argv)
