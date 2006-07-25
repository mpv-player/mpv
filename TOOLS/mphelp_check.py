#!/usr/bin/python

# Tool to compare MPlayer translation files against a base file. Reports
# conflicting arguments, extra strings not present in the base file and
# (optionally) missing strings.

# Written by Uoti Urpala

import sys
import re

def parse(filename):
    r = {}
    f = open(filename)
    it = iter(f)
    cur = ''
    for line in it:
        line = line.strip()
        if not line.startswith('#define'):
            while line and line[-1] == '\\':
                line = it.next().strip()
            continue
        _, name, value = line.split(None, 2)
        value = value.strip('"')
        while line[-1] == '\\':
            line = it.next().strip()
            value += line.rstrip('\\').strip('"')
        r[name] = value
    f.close()
    return r

def compare(base, other, show_missing=False):
    r = re.compile('%[^diouxXeEfFgGaAcspn%]*[diouxXeEfFgGaAcspn%]')
    missing = []
    for key in base:
        if key not in other:
            missing.append(key)
            continue
        if re.findall(r, base[key]) != re.findall(r, other[key]):
            print 'Mismatch: ', key
            print base[key]
            print other[key]
            print
        del other[key]
    if other:
        extra = other.keys()
        extra.sort()
        print 'Extra: ', ' '.join(extra)
    if show_missing and missing:
        missing.sort()
        print 'Missing: ', ' '.join(missing)

if len(sys.argv) < 3:
    print 'Usage:\n'+sys.argv[0]+' [--missing] base_helpfile otherfile1 '\
          '[otherfile2 ...]'
    sys.exit(1)
i = 1
show_missing = False
if sys.argv[i] in ( '--missing', '-missing' ):
    show_missing = True
    i = 2
base = parse(sys.argv[i])
for filename in sys.argv[i+1:]:
    print '*****', filename
    compare(base, parse(filename), show_missing)
    print '\n'
