#!/bin/sh

last_cvs_update=`date -r CVS/Entries +%y%m%d-%H:%M 2>/dev/null`
if [ $? -ne 0 ]; then
	# probably no gnu date installed(?), use current date
	last_cvs_update=`date +%y%m%d-%H:%M`
fi
echo "#define VERSION \"CVS-${last_cvs_update}\"" >version.h
