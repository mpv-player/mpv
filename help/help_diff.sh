#!/bin/sh

# This script walks through the master (stdin) help/message file, and
# prints (stdout) only those messages which are missing from the help
# file given as parameter ($1).
#
# Example: help_diff.sh help_mp-hu.h < help_mp-en.h > missing.h

curr=""

while read -r line; do
	if echo "$line" | grep '^#define' > /dev/null 2>&1; then
		curr=`printf "%s\n" "$line" | cut -d ' ' -f 2`
		if grep "^#define $curr[	 ]" $1 > /dev/null 2>&1; then
			curr=""
		fi
	else
		if [ -z "$line" ]; then
			curr=""
		fi
	fi

	if [ -n "$curr" ]; then
		printf "%s\n" "$line"
	fi
done
