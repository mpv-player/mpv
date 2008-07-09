#!/bin/sh
#
# This is a wrapper around the -identify functionality.
# It is supposed to escape the output properly, so it can be easily
# used in shellscripts by 'eval'ing the output of this script.
#
# Written by Tobias Diedrich <ranma+mplayer@tdiedrich.de>
# Licensed under GNU GPL.

if [ -z "$1" ]; then
	echo "Usage: midentify.sh <file> [<file> ...]"
	exit 1
fi

mplayer -vo null -ao null -frames 0 -identify "$@" 2>/dev/null |
	sed -ne '/^ID_/ {
	                  s/[]()|&;<>`'"'"'\\!$" []/\\&/g;p
	                }'
