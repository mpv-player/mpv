#!/bin/sh
#
# This script converts the subtitles in the current directory into
# MPsub format (into ./converted-subtitles/*)
#
# Gabucino. No warranty. :)
#

TMP="x2mpsub-$RANDOM"
mkdir "$TMP"
touch "$TMP/$TMP"

for x in *; do
    echo "Converting $x"
    mplayer "$TMP/$TMP" -sub "$x" -dumpmpsub -quiet > /dev/null 2> /dev/null
    mv dump.mpsub "$TMP/$x"
done

rm "$TMP/$TMP"
mv "$TMP" converted-subtitles
