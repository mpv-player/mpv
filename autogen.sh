#! /bin/sh
#
# This script simply downloads waf to the project's root directory

WAFRELEASE=waf-1.7.13
SHA256HASH=03cc750049350ee01cdbc584b70924e333fcc17ba4a2d04648dab1535538a873

curl https://waf.googlecode.com/files/$WAFRELEASE > waf
if test -x $(which sha256sum) ; then
    if echo "$SHA256HASH waf" | sha256sum --strict --check - ; then
        echo "Checksum verified."
    else
        rm -f waf
        echo "The checksum of the downloaded file does not match!"
        echo "Please download and verify the file manually."
        exit 1
    fi
    chmod +x waf
else
    echo "sha256sum not found. It's up to you to verify the downloaded file."
fi
