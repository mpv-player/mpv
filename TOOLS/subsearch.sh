#!/bin/sh
#
# Collects all the appropriate subtitle files in a given directory and
# it's subdirectories. No matter what the filename is.
# Use this together as: mplayer -sub `subsearch3.sh` movie
# Author: Alex
#

[ $1 ] && cd `dirname $1`

_sub_names=""

one_dir_search() {
    for i in $dir/*
    do
	case "`echo $i | tr [:upper:] [:lower:]`" in
		*.sub|*.srt|*.mps|*.txt) _sub_names="$i,$_sub_names" ;;
		*) ;;
	esac
    done
}

dir="."
one_dir_search

# add subdirectories too
for dir in *
do
    [ -d $dir ] && one_dir_search
done

_len="`echo $_sub_names | wc -c`"
_len=$((_len-2))
echo $_sub_names | cut -b -"$_len"
