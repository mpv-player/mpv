#!/bin/sh

revision=r`grep revision .svn/entries | cut -d '"' -f 2 2> /dev/null`

test "$1" && extra="-$1"

echo "#define VERSION \"dev-SVN-${revision}${extra}\"" > version.h
