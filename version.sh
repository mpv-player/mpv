#!/bin/sh

OS=`uname -s`
case "$OS" in
  CYGWIN*|Linux)
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
  Darwin|*BSD)
    # BSD 'date -r' does not print modification time
    # LC_ALL=C sets month/day order and English language in the date string
    # The if in the awk call works around wrong day/month order.
    last_cvs_update=`LC_ALL=C ls -lT CVS/Entries | \
      awk '{ \
        day=$7; \
        month=index(" JanFebMarAprMayJunJulAugSepOctNovDec", $6); \
        if(month==0) { \
          day=$6; \
          month=index(" JanFebMarAprMayJunJulAugSepOctNovDec",$7); } \
        printf("%s%.02d%.02d-%s", \
          substr($9, 3, 2), (month+1)/3, day, substr($8, 0, 5)); \
      }'`
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
