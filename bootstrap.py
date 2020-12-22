#!/usr/bin/env python3

# This script simply downloads waf to the current directory

import os, sys, stat, hashlib, subprocess
from urllib.request import urlopen, URLError

WAFRELEASE = "waf-2.0.20"
WAFURLS    = ["https://waf.io/" + WAFRELEASE,
              "http://www.freehackers.org/~tnagy/release/" + WAFRELEASE]
SHA256HASH = "bf971e98edc2414968a262c6aa6b88541a26c3cd248689c89f4c57370955ee7f"

if os.path.exists("waf"):
    wafver = subprocess.check_output([sys.executable, './waf', '--version']).decode()
    if WAFRELEASE.split('-')[1] == wafver.split(' ')[1]:
        print("Found 'waf', skipping download.")
        sys.exit(0)

if "--no-download" in sys.argv[1:]:
    print("Did not find {} and no download was requested.".format(WAFRELEASE))
    sys.exit(1)

waf = None

for WAFURL in WAFURLS:
    try:
        print("Downloading {}...".format(WAFURL))
        waf = urlopen(WAFURL).read()
        break
    except URLError as err:
        print("Download failed! ({})".format(err))

if not waf:
    print("Could not download {}.".format(WAFRELEASE))

    sys.exit(1)

if SHA256HASH == hashlib.sha256(waf).hexdigest():
    # Upstream waf is not changing the default interpreter during
    # 2.0.x line due to compatibility reasons apparently. So manually
    # convert it to use python3 (the script works with both).
    expected = b"#!/usr/bin/env python\n"
    assert waf.startswith(expected)
    waf = b"#!/usr/bin/env python3\n" + waf[len(expected):]
    with open("waf", "wb") as wf:
        wf.write(waf)

    os.chmod("waf", os.stat("waf").st_mode | stat.S_IXUSR)
    print("Checksum verified.")
else:
    print("The checksum of the downloaded file does not match!")
    print(" - got:      {}".format(hashlib.sha256(waf).hexdigest()))
    print(" - expected: {}".format(SHA256HASH))
    print("Please download and verify the file manually.")

    sys.exit(1)
