#!/bin/sh

OS=`uname -s`
case "$OS" in
     Linux)
	last_cvs_update=`date -r CVS/Entries +%y%m%d-%H:%M 2>/dev/null`
	;;
     BSD/OS)
	LS=`ls -lT CVS/Entries`
	month=`echo $LS | awk -F" " '{print $6}'`
	day=`echo $LS | awk -F" " '{print $7}'`
	hms=`echo $LS | awk -F" " '{print $8}'`
	hour=`echo $hms | awk -F":" '{print $1}'`
	minute=`echo $hms | awk -F":" '{print $2}'`
	year=`echo $LS | awk -F" " '{print $9}'`
	last_cvs_update="${year}${month}${day}-${hour}:${minute}"
	;;
     Darwin) 
        # darwin's date has different meaning for -r
	last_cvs_update=`date +%y%m%d-%H:%M`
	;;
     *)
	last_cvs_update=`date +%y%m%d-%H:%M`
	;;
esac

extra=""
if test "$1" ; then
 extra="-$1"
fi
echo "#define VERSION \"CVS-${last_cvs_update}${extra} \"" >version.h
