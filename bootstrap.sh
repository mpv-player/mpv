#! /bin/sh
#
# This script simply downloads waf to the project's root directory

WAFRELEASE=waf-1.7.13
SHA256HASH=03cc750049350ee01cdbc584b70924e333fcc17ba4a2d04648dab1535538a873

curl https://waf.googlecode.com/files/$WAFRELEASE > waf
if test -x $(which shasum) ; then
    if echo "$SHA256HASH *waf" | shasum -c - ; then
        echo "Checksum verified."
    else
        rm -f waf
        echo "The checksum of the downloaded file does not match!"
        echo "Please download and verify the file manually."
        exit 1
    fi
    chmod +x waf
    echo "To build mpv, run: ./waf configure && ./waf build"
else
    echo "shasum not found. It's up to you to verify the downloaded file."
fi
