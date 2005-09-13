#!/bin/sh
# Helper script to ease comparing the PSNR between two video files
# Copyleft 2005 by Matthias Wieser
# This file comes under GPL, see http://www.gnu.org/copyleft/gpl.html for more
# information on its licensing.

TEMPDIR="/tmp/psnr_video"
WORKDIR=`pwd`/
if [ $# -le 1 ]; then
   echo
   echo "Usage: `basename $0` <file1> <file2> [<frames>] [<options1>] [<options2>]"
   echo
   echo " <file1> and <file2> are the files for which the PSNR should be calculated."
   echo " [<frames>]          is the number of frames to process, starting from frame 1."
   echo " [<options1>]        are additional MPlayer options for <file1>."
   echo " [<options2>]        are additional MPlayer options for <file2>."
   echo
   echo " Be aware that `basename $0` needs a lot of temporary space inside /tmp/."
   echo
   echo "Example:"
   echo "        ./`basename $0` ./orig.avi ./test.avi 250 \"\" \"-vf pp=ac\""
   echo

   exit 1
fi

FILE1=$1
FILE2=$2

LastFrame=-1
if [ $# -ge 3 ]; then
	LastFrame=$3
	echo
	echo "Will process $LastFrame frames."
fi

if [ $# -ge 4 ]; then
	FILE1_Options=$4
	echo "MPlayer options for ${FILE1}: $FILE1_Options"
fi

if [ $# -ge 5 ]; then
	FILE2_Options=$5
	echo "Mplayer options for ${FILE2}: $FILE2_Options"
fi

mkdir -p ${TEMPDIR}/FILE1
mkdir -p ${TEMPDIR}/FILE2

###  File 1
echo
echo "############## $FILE1 #################"

cd ${TEMPDIR}/FILE1

rm -f *ppm
rm -f *del

if [ $LastFrame -ge 0 ]; then
	mplayer $FILE1_Options -frames $LastFrame -nosound -vo pnm ${WORKDIR}$FILE1 >/dev/null
else
	mplayer $FILE1_Options -nosound -vo pnm ${WORKDIR}$FILE1 > /dev/null
fi
###  File 2

echo
echo "############## $FILE2 #################"

cd ${TEMPDIR}/FILE2

rm *ppm 2> /dev/null

if [ $LastFrame -ge 0 ]; then
	mplayer $FILE2_Options -frames $LastFrame -nosound -vo pnm ${WORKDIR}$FILE2 >/dev/null
else
	mplayer $FILE2_Options -nosound -vo pnm ${WORKDIR}$FILE2 >/dev/null
fi


###  PSNR

echo
echo "############## PSNR Calculation #################"

if [[ `ls -1 ${TEMPDIR}/FILE1/*ppm | wc -l` = `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` ]]
then
	echo 
else
	echo "Files have differing numbers of frames!"
	echo "$FILE1 has `ls -1 ${TEMPDIR}/FILE1/*ppm | wc -l` frames,"
	echo "$FILE2 has `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` frames."
	echo "Processing the first `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` frames."
	echo
fi


cd ${TEMPDIR}/FILE2
#rm ../psnr.dat
echo "File;Y;Cb;Cr" >../psnr.dat
echo "0" > errorsum.del
i=0
for FILE in `ls -1 *.ppm`
        do
        echo $FILE
                echo -n "$FILE">>../psnr.dat
                echo -n ";">>../psnr.dat
        pnmpsnr ../FILE1/$FILE $FILE 2> del.del
        grep "Y" del.del | dd bs=1c count=5 skip=29 of=del2.del 2>/dev/null
                Y=`cat del2.del`
               echo -n "$Y;">>../psnr.dat
        grep "Cb" del.del | dd bs=1c count=5 skip=29 of=del2.del 2>/dev/null
                CB=`cat del2.del`
               echo -n "$CB;">>../psnr.dat
        grep "Cr" del.del | dd bs=1c count=5 skip=29 of=del2.del 2>/dev/null
                CR=`cat del2.del`
               echo -n "$CR;">>../psnr.dat
         ALL=`echo "(-10)*l((e(-$Y/10*l(10))+e(-$CB/10*l(10))/4+e(-$CR/10*l(10))/4)/1.5)/l(10)"|bc -l`
         echo "$ALL">>../psnr.dat
        ERROR=`echo "scale=30; (e(-1*$Y/10*l(10))+e(-1*$CB/10*l(10))/4+e(-1*$CR/10*l(10))/4)/1.5"|bc -l`
        ERRORSUM=`cat errorsum.del`
        echo `echo "scale=30; $ERROR + $ERRORSUM" | bc -l` > errorsum.del
    i=$(($i+1))
	if [[ $i = $LastFrame ]]
	then
		break
	fi
done
ERRORSUM=`cat errorsum.del`
PSNR=`echo "-10*l($ERRORSUM/$i)/l(10)" | bc -l`
echo "PSNR:;$PSNR">>../psnr.dat
cd ..
mv psnr.dat ${WORKDIR}

if [[ `ls -1 ${TEMPDIR}/FILE1/*ppm | wc -l` = `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` ]]
then
        echo
else
        echo "Files have differing numbers of frames!"
        echo "$FILE1 has `ls -1 ${TEMPDIR}/FILE1/*ppm | wc -l` frames,"
        echo "$FILE2 has `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` frames."
        echo "Processed the first `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` frames."
        echo
fi

cd ..
rm -r ${TEMPDIR}

echo "Created ${WORKDIR}psnr.dat"
echo

