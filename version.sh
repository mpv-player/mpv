#!/bin/sh

last_cvs_update=`date -r CVS/Entries +%y%m%d-%H:%M 2>/dev/null`
if test $? -ne 0 ; then
        # probably no gnu date installed(?), use current date
        last_cvs_update=`date +%y%m%d-%H:%M`
elif test `uname -s` = 'Darwin' ; then
        # darwin's date has different meaning for -r
        last_cvs_update=`date +%y%m%d-%H:%M`
fi

extra=""
if test $1 ; then
 extra="-$1"
fi
echo "#define VERSION \"CVS-${last_cvs_update}${extra} \"" >version.h
