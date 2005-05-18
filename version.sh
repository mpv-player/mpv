#!/bin/sh

OS=`uname -s`
case "$OS" in
     CYGWIN*)
	last_cvs_update=`date -r CVS/Entries +%y%m%d-%H:%M 2>/dev/null`
	;;
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
	# Darwin/BSD 'date -r' does not print modification time
	LS=`ls -lT CVS/Entries`
	year=`echo $LS | cut -d' ' -f9 | cut -c 3-4`
	month=`echo $LS | awk -F" " '{printf "%.2d", \
		(index("JanFebMarAprMayJunJulAugSepOctNovDec",$7)+2)/3}'`
	day=`echo $LS | cut -d' ' -f6`
	hour=`echo $LS | cut -d' ' -f8 | cut -d: -f1`
	minute=`echo $LS | cut -d' ' -f8 | cut -d: -f2`
	last_cvs_update="${year}${month}${day}-${hour}:${minute}"
	;;
     *)
	last_cvs_update=`date +%y%m%d-%H:%M`
	;;
esac

extra=""
if test "$1" ; then
 extra="-$1"
fi
echo "#define VERSION \"dev-CVS-${last_cvs_update}${extra}\"" >version.h
