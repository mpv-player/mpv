#! /bin/bash

# This script walks through the master (stdin) help/message file, and
# prints (stdout) only those messages which are missing from the help
# file given as parameter ($1).
#
# Example: help_mp-en.sh help_mp-hu.h < help_mp-en.h > missing.h

curr="x"

while read -r line ; do

if ( echo $line | cut -d ' ' -f 1 | grep '^#define' > /dev/null ); then
    curr=`echo $line | cut -d ' ' -f 2`
    if ( grep "$curr " $1 > /dev/null ); then
	curr="x"
    fi
else
    if test x"$line" = x; then
	curr="x"
    fi
fi

if test $curr != "x" ; then
    echo "$line"
fi

done < help_mp-en.h
