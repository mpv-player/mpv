#!/usr/bin/env python

# This script simply downloads waf to the current directory

from __future__ import print_function
import os, sys, stat, hashlib, subprocess

WAFRELEASE = "waf-1.7.16"
WAFURL     = "http://ftp.waf.io/pub/release/" + WAFRELEASE
SHA256HASH = "b64dc26c882572415fd450b745006107965f3fe17b357e3eb43d6676c9635a61"

if os.path.exists("waf"):
    wafver = subprocess.check_output(['./waf', '--version']).decode()
    if WAFRELEASE.split('-')[1] == wafver.split(' ')[1]:
        print("Found 'waf', skipping download.")
        sys.exit(0)

try:
    from urllib.request import urlopen
except:
    from urllib2 import urlopen

print("Downloading %s..." % WAFURL)
waf = urlopen(WAFURL).read()

if SHA256HASH == hashlib.sha256(waf).hexdigest():
    with open("waf", "wb") as wf:
        wf.write(waf)

    os.chmod("waf", os.stat("waf").st_mode | stat.S_IXUSR)
    print("Checksum verified.")
else:
    print("The checksum of the downloaded file does not match!")
    print("Please download and verify the file manually.")

    sys.exit(1)
