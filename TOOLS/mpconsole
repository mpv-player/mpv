#!/bin/sh
# Script to run mplayer on the console (fbdev/mga_vid/etc) without
# the console text and/or flashing cursor getting in the way.
# Written by Rich Felker.

trap "tput cnorm ; exit 1" SIGQUIT SIGINT EXIT
res=`PATH="$PATH:/usr/sbin" fbset | grep geometry | sed 's/^ *//'`
width=`echo "$res" | cut -d' ' -f2`
height=`echo "$res" | cut -d' ' -f3`
tput civis
clear
mplayer -vo mga -screenw "$width" -screenh "$height" "$@" >/dev/null 2>&1
