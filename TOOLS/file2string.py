#!/usr/bin/env python3

# Convert the contents of a file into a C string constant.
# Note that the compiler will implicitly add an extra 0 byte at the end
# of every string, so code using the string may need to remove that to get
# the exact contents of the original file.

import sys

def main(infile):
    conv = ['\\' + ("%03o" % c) for c in range(256)]
    safe_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" \
                 "0123456789!#%&'()*+,-./:;<=>?[]^_{|}~ "
    for c in safe_chars:
        conv[ord(c)] = c
    for c, esc in ("\nn", "\tt", r"\\", '""'):
        conv[ord(c)] = '\\' + esc
    for line in infile:
        sys.stdout.write('"' + ''.join(conv[c] for c in line) + '"\n')

with open(sys.argv[1], 'rb') as infile:
    sys.stdout.write("// Generated from %s\n\n" % sys.argv[1])
    main(infile)
