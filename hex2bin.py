#!/usr/bin/python

def main(argv):
	inf = file(argv[1])
	b = bytearray()
	width = 16

	while True:
		x = inf.readline()
		if x == '':
			break
		x = x.rstrip('\r\n')
		a = x.split(None, 3)
		if a[0] != '|' or a[2] != ':':
			raise Exception('Doesn\'t look like hex dump to me')
		ofs = int(a[1], 16)
		h = a[3][width + 1:]
		vals = bytearray(map(lambda x:int(x, 16), h.split()))
		b = b + vals

	outf = file(argv[2], 'w')
	outf.write(b)

	return True

if __name__ == '__main__':
	from sys import argv
	raise SystemExit, not main(argv)
