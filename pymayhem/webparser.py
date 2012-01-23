from HTMLParser import HTMLParser
from urllib import unquote
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

class WebParser(HTMLParser):
	def __init__(self):
		HTMLParser.__init__(self)
		self.stk = []
		self.result = {}

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
			self.result[k] = unquote(v)
