#!/bin/sh

# This script compares a translated help file to the master file and prints
# out any missing messages.  Needs the language code as parameter ($1).
#
# Example: help_diff.sh hu

exec < help_mp-en.h

while read line; do
	if echo "$line" | grep '^#define' > /dev/null 2>&1; then
		curr=`echo "$line" | cut -d ' ' -f 2`
		if grep "^#define $curr" help_mp-$1.h > /dev/null 2>&1; then
			true
		else
			echo "$line"
		fi
	fi
done
