#!/usr/bin/env python

#usage: 
#
# vobshift.py in.idx out.idx -8.45
#
# this will read in in.idx,shift it by 8.45 seconds back,
# and save it as out.idx
#
# license: i don't care ;)
#

import datetime
import sys

def tripletize(line):
	begin = line[:11]
	middle = line[11:23]
	end = line[23:]
	return (begin,middle,end)

def text2delta(t):
	h = int( t[0:2] )
	m = int( t[3:5] )
	s = int( t[6:8] )
	milli = int( t[9:12] )
	return datetime.timedelta(hours=h,minutes=m,seconds=s,milliseconds=milli)

def delta2text(d):
	t = str(d)
	milli = t[8:11]
	if len(milli) == 0: #fix for .000 seconds
	    milli = '000'
	return '0'+t[:7]+':'+milli

def shift(line,seconds):
	triplet = tripletize(line)
	
	base = text2delta(triplet[1])
	base = base + datetime.timedelta(seconds=seconds)
	base = delta2text(base)

	return triplet[0]+base+triplet[2]

INFILE  =sys.argv[1]
OUTFILE =sys.argv[2]
DIFF    =float(sys.argv[3])

o = open(OUTFILE,'wt')


for line in open(INFILE):
    if line.startswith('timestamp'):
	line = shift(line,DIFF)
    
    o.write(line)

o.close()
