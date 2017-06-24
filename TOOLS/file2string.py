#!/usr/bin/env python

# Convert the contents of a file into a C string constant.
# Note that the compiler will implicitly add an extra 0 byte at the end
# of every string, so code using the string may need to remove that to get
# the exact contents of the original file.

#
# This file is part of mpv.
#
# mpv is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# mpv is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
#

from __future__ import unicode_literals
import sys

# Indexing a byte string yields int on Python 3.x, and a str on Python 2.x
def pord(c):
    return ord(c) if type(c) == str else c

def file2string(infilename, infile, outfile):
    outfile.write("// Generated from %s\n\n" % infilename)

    conv = ['\\' + ("%03o" % c) for c in range(256)]
    safe_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" \
                 "0123456789!#%&'()*+,-./:;<=>?[]^_{|}~ "

    for c in safe_chars:
        conv[ord(c)] = c
    for c, esc in ("\nn", "\tt", r"\\", '""'):
        conv[ord(c)] = '\\' + esc
    for line in infile:
        outfile.write('"' + ''.join(conv[pord(c)] for c in line) + '"\n')

if __name__ == "__main__":
    with open(sys.argv[1], 'rb') as infile:
        file2string(sys.argv[1], infile, sys.stdout)
