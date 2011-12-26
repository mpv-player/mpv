#!/usr/bin/env python

# Script to embed arbitrary binary files in C header files.

CHARS_PER_LINE = 19

import sys
import os

if len(sys.argv) != 3:
    print("Embed binary files in C headers.")
    print("Usage: ")
    print("  bin_to_header.py infile outfile.h")
    print("outfile.h will be overwritten with the new contents.")
    sys.exit(1)

infile_name = sys.argv[1]
outfile_name = sys.argv[2]

varname = os.path.splitext(os.path.basename(outfile_name))[0]

infile = open(infile_name, "rb")
outfile = open(outfile_name, "w")

outfile.write("// Generated with " + " ".join(sys.argv) + "\n")
outfile.write("\nstatic const unsigned char " + varname + "[] = {\n")

while True:
    data = infile.read(CHARS_PER_LINE)
    if len(data) == 0:
        break
    outfile.write("    ")
    for c in data:
        # make it work both in Python 2.x (c is str) and 3.x (c is int)
        if type(c) != int:
            c = ord(c)
        outfile.write("{0:3},".format(c))
    outfile.write("\n")

outfile.write("};\n")

infile.close()
outfile.close()
