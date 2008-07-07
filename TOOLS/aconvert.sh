#!/bin/sh

# Author: Jonas Jermann
# Description: A hack to allow mencoder to encode from an audio only file

if [ "$1" = "" ]; then
    echo "Usage: $0 <\"input file\"> <\"output file\"> <\"options\">"
    exit 0
fi

options=${3:-"-oac mp3lame"}
 
mencoder -demuxer rawvideo -rawvideo w=1:h=1 -ovc copy -of rawaudio -endpos `mplayer -identify $1 -frames 0 2>&1 | grep ID_LENGTH | cut -d "=" -f 2` -audiofile $1 -o $2 $options $1
