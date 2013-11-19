#!/usr/bin/env python

# This script simply downloads waf to the current directory

WAFRELEASE = "waf-1.7.13"
SHA256HASH = "03cc750049350ee01cdbc584b70924e333fcc17ba4a2d04648dab1535538a873"

try:
    from urllib.request import urlopen
except:
    from urllib2 import urlopen

import os, sys, stat, hashlib

waf = urlopen("http://waf.googlecode.com/files/" + WAFRELEASE).read()

if (SHA256HASH == hashlib.sha256(waf).hexdigest()):
    with open("waf", "wb") as wf:
        wf.write(waf)

    os.chmod("waf", os.stat("waf").st_mode | stat.S_IXUSR)
    print("Checksum verified.")
else:
    print("The checksum of the downloaded file does not match!")
    print("Please download and verify the file manually.")

    sys.exit(1)
