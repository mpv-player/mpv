#!/bin/sh

last_cvs_update=`date -r CVS/Entries +%y%m%d-%H:%M 2>/dev/null`
if [ $? -ne 0 ]; then
	# probably no gnu date installed(?), use current date
	last_cvs_update=`date +%y%m%d-%H:%M`
fi
cc=`cat config.mak |grep CC | cut -d '=' -f 2`
cc_version=`${cc} --version`

echo "#define VERSION \"CVS-${last_cvs_update}${cc}-${cc_version} \"" >version.h
