#!/bin/sh

# This script walks through the master (stdin) help/message file, and
# prints (stdout) only those messages which are missing from the help
# file given as parameter ($1).
#
# Example: help_diff.sh help_mp-hu.h < help_mp-en.h > missing.h

sed "`sed '/^#define *[^ ][^ ]* .*$/!d;s@^#define *\([^ ][^ ]*\) .*$@/^#define *\1 /d@' < help_mp-$1.h ; echo '/^#define/!d'`" < help_mp-en.h
