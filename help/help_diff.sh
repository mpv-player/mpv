#!/bin/sh

# This script walks through the master (stdin) help/message file, and
# prints (stdout) only those messages which are missing from the help
# file given as parameter ($1).
#
# Example: help_diff.sh help_mp-hu.h < help_mp-en.h > missing.h

curr=""

while read -r line; do
	if echo -E "$line" | grep -q '^#define'; then
		curr=`echo -E "$line" | cut -d ' ' -f 2`
		if grep -q "^#define $curr " $1; then
			curr=""
		fi
	else
		if [ -z "$line" ]; then
			curr=""
		fi
	fi

	if [ -n "$curr" ]; then
		echo -E "$line"
	fi
done


