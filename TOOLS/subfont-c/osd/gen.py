#!/usr/bin/python

from math import *
import sys
import string

k = (sqrt(2.)-1.)*4./3.

chars = []
encoding = []
count = 1
first = 1

def append(s):
    chars.append(s)

def rint(x):
    return int(round(x))
"""
    if x>=0:
	return int(x+0.5)
    else:
	return int(x-0.5)
"""

class vec:
    def __init__(self, x, y=0):
        if type(x) is type(()):
	    self.x, self.y = x
	else:
	    self.x = x
	    self.y = y
    def set(self, x, y):
        self.__init__(x, y)
    def move(self, x, y):
        self.x = self.x + x
        self.y = self.y + y
    def __add__(self, v):
        return vec(self.x+v.x, self.y+v.y)
    def __sub__(self, v):
        return vec(self.x-v.x, self.y-v.y)
    def int(self):
        return vec(rint(self.x), rint(self.y))
    def t(self):
        return (self.x, self.y)

class pvec(vec):
    def __init__(self, l, a):
        self.x = l * cos(a)
        self.y = l * sin(a)


pen = vec(0,0)

def moveto(x, y=0):
    global first
    dx = rint(x-pen.x)
    dy = rint(y-pen.y)
    if dx!=0:
        if dy!=0:
	    append("\t%i %i rmoveto" % (dx, dy))
	else:
	    append("\t%i hmoveto" % (dx))
    elif dy!=0:
	    append("\t%i vmoveto" % (dy))
    elif first:
	    append("\t0 hmoveto")
	    first = 0
    pen.x = pen.x+dx
    pen.y = pen.y+dx

def rlineto(v):
    if v.x!=0:
        if v.y!=0:
	    append("\t%i %i rlineto" % (v.x, v.y))
	else:
	    append("\t%i hlineto" % (v.x))
    elif v.y!=0:
	    append("\t%i vlineto" % (v.y))

def closepath():
    append("\tclosepath")

history = []
def movebase(x, y=0):
    history.append((x,y))
    pen.move(-x, -y)

def moveback():
    x, y = history.pop()
    pen.move(x, y)

def ellipse(rx, ry = None, half=0):
    # rx>0 => counter-clockwise (filled)
    # rx<0 => clockwise

    if ry==None: ry = abs(rx)

    dx1 = rint(k*rx)
    dx2 = rx-dx1

    dy1 = rint(k*ry)
    dy2 = ry-dy1

    rx = abs(rx)
    moveto(0, -ry)
    append("\t%i 0 %i %i 0 %i rrcurveto" % (+dx1, +dx2, +dy2, +dy1))
    append("\t0 %i %i %i %i 0 rrcurveto" % (+dy1, -dx2, +dy2, -dx1))
    if not half:
	append("\t%i 0 %i %i 0 %i rrcurveto" % (-dx1, -dx2, -dy2, -dy1))
	append("\t0 %i %i %i %i 0 rrcurveto" % (-dy1, +dx2, -dy2, +dx1))
    closepath()
    if half:
	pen.set(0, ry)
    else:
	pen.set(0, -ry)

circle = ellipse

def rect(w, h):
    moveto(0, 0)
    if w>0:
	append("\t%i hlineto" % (w))
	append("\t%i vlineto" % (h))
	append("\t%i hlineto" % (-w))
	pen.set(0, h)
    else:
	append("\t%i vlineto" % (h))
	append("\t%i hlineto" % (-w))
	append("\t%i vlineto" % (-h))
	pen.set(-w, 0)
    closepath()

def poly(p):
    moveto(0, 0)
    prev = vec(0, 0)
    for q in p:
        rlineto(vec(q)-prev)
	prev = vec(q)
    closepath()
    pen.set(prev.x, prev.y)

def line(w, l, a):
    vw = pvec(w*.5, a-pi*.5)
    vl = pvec(l, a)
    p = vw
    moveto(p.x, p.y)
    p0 = p
    #print '%%wla %i %i %.3f: %.3f %.3f' % (w, l, a, p0.x, p0.y)
    p = p+vl
    rlineto((p-p0).int())
    p0 = p
    #print '%%wla %i %i %.3f: %.3f %.3f' % (w, l, a, p0.x, p0.y)
    p = p-vw-vw
    rlineto((p-p0).int())
    p0 = p
    #print '%%wla %i %i %.3f: %.3f %.3f' % (w, l, a, p0.x, p0.y)
    p = p-vl
    #print '%%wla %i %i %.3f: %.3f %.3f' % (w, l, a, p.x, p.y)
    rlineto((p-p0).int())
    closepath()
    pen.set(p.x, p.y)


def begin(name, code, hsb, w):
    global first, count, history
    history = []
    pen.set(0, 0)
    append("""\
/uni%04X { %% %s
	%i %i hsbw""" % (code+0xE000, name, hsb, w))
    i = len(encoding)
    while i<code:
	encoding.append('dup %i /.notdef put' % (i,))
	i = i+1
    encoding.append('dup %i /uni%04X put' % (code, code+0xE000))
    count = count + 1
    first = 1


def end():
    append("""\
	endchar
} ND""")



########################################

r = 400
s = 375
hsb = 200	# horizontal side bearing
hsb2 = 30
over = 10	# overshoot
width = 2*r+2*over+2*hsb2

########################################
begin('play', 0x01, hsb, width)
poly((  (s,r),
	(0, 2*r),))
end()


########################################
w=150
begin('pause', 0x02, hsb, width)
rect(w, 2*r)
movebase(2*w)
rect(w, 2*r)
end()


########################################
begin('stop', 0x03, hsb, width)
rect(665, 720)
end()


########################################
begin('rewind', 0x04, hsb/2, width)
movebase(2*s+15)
poly((  (0, 2*r),
	(-s, r),))
movebase(-s-15)
poly((  (0, 2*r),
	(-s, r),))
end()


########################################
begin('fast forward', 0x05, hsb/2, width)
poly((  (s,r),
	(0, 2*r),))
movebase(s+15)
poly((  (s,r),
	(0, 2*r),))
end()


########################################
begin('clock', 0x06, hsb2, width)
movebase(r, r)
circle(r+over)
wc = 65
r0 = r-3*wc
n = 4
movebase(-wc/2, -wc/2)
rect(-wc, wc)
moveback()
for i in range(n):
    a = i*2*pi/n
    v = pvec(r0, a)
    movebase(v.x, v.y)
    line(-wc, r-r0, a)
    moveback()
hh = 11
mm = 8
line(-50, r*.5, pi/2-2*pi*(hh+mm/60.)/12)
line(-40, r*.9, pi/2-2*pi*mm/60.)
end()


########################################
begin('contrast', 0x07, hsb2, width)
movebase(r, r)
circle(r+over)
circle(-(r+over-80), half=1)
end()


########################################
begin('saturation', 0x08, hsb2, width)
movebase(r, r)
circle(r+over)
circle(-(r+over-80))

v = pvec(160, pi/2)
movebase(v.x, v.y)
circle(80)
moveback()

v = pvec(160, pi/2+pi*2/3)
movebase(v.x, v.y)
circle(80)
moveback()

v = pvec(160, pi/2-pi*2/3)
movebase(v.x, v.y)
circle(80)
end()


########################################
begin('volume', 0x09, 0, 1000)
poly((  (1000, 0),
	(0, 500),))
end()


########################################
begin('brightness', 0x0A, hsb2, width)
movebase(r, r)
circle(150)
circle(-100)

rb = 375
wb = 50
l = 140
n = 8
for i in range(n):
    a = i*2*pi/n
    v = pvec(l, a)
    movebase(v.x, v.y)
    line(wb, rb-l, a)
    moveback()
end()


########################################
begin('hue', 0x0B, hsb2, width)
movebase(r, r)
circle(r+over)
ellipse(-(322), 166)
movebase(0, 280)
circle(-(60))
end()


########################################
begin('progress [', 0x10, (334-182)/2, 334)
poly((  (182, 0),
	(182, 90),
	(145, 90),
	(145, 550),
	(182, 550),
	(182, 640),
	(0, 640),
))
end()


########################################
begin('progress |', 0x11, (334-166)/2, 334)
rect(166, 640)
end()


########################################
begin('progress ]', 0x12, (334-182)/2, 334)
poly((  (182, 0),
	(182, 640),
	(0, 640),
	(0, 550),
	(37, 550),
	(37, 90),
	(0, 90),
))
end()


########################################
begin('progress .', 0x13, (334-130)/2, 334)
movebase(0, (640-130)/2)
rect(130, 130)
end()



########################################
print """\
%!PS-AdobeFont-1.0: OSD 1.00
%%CreationDate: Sun Jul 22 12:38:28 2001
%
%%EndComments
12 dict begin
/FontInfo 9 dict dup begin
/version (Version 1.00) readonly def
/Notice (This is generated file.) readonly def
/FullName (OSD) readonly def
/FamilyName (OSD) readonly def
/Weight (Regular) readonly def
/ItalicAngle 0.000000 def
/isFixedPitch false def
/UnderlinePosition -133 def
/UnderlineThickness 49 def
end readonly def
/FontName /OSD def
/PaintType 0 def
/StrokeWidth 0 def
/FontMatrix [0.001 0 0 0.001 0 0] def
/FontBBox {0 -10 1000 810} readonly def
/Encoding 256 array"""

print string.join(encoding, '\n')
i = len(encoding)
while i<256:
    print 'dup %i /.notdef put' % i
    i = i+1


print """\
readonly def
currentdict end
currentfile eexec
dup /Private 15 dict dup begin
/RD{string currentfile exch readstring pop}executeonly def
/ND{noaccess def}executeonly def
/NP{noaccess put}executeonly def
/ForceBold false def
/BlueValues [ -10 0 800 810 640 650 720 730 ] def
/StdHW [ 65 ] def
/StdVW [ 65 ] def
/StemSnapH [ 65 800 ] def
/StemSnapV [ 65 150 ] def
/MinFeature {16 16} def
/password 5839 def
/Subrs 1 array
dup 0 {
	return
	} NP
 ND
2 index
/CharStrings %i dict dup begin""" % count

print """\
/.notdef {
	0 400 hsbw
	endchar
} ND"""

print string.join(chars, '\n')


print """\
end
end
readonly put
noaccess put
dup/FontName get exch definefont pop
mark currentfile closefile"""
