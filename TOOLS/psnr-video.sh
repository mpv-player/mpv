#!/bin/bash
# Helper script to ease comparing the PSNR between two video files
# Copyleft 2005 by Matthias Wieser
# Copyleft 2005 by Ivo
# This file comes under GPL, see http://www.gnu.org/copyleft/gpl.html for more
# information on its licensing.

warning_frame_number () {
	echo "Files have differing numbers of frames!"
	echo "$FILE1 has `ls -1 ${TEMPDIR}/FILE1/*ppm | wc -l` frames,"
	echo "$FILE2 has `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` frames."
	echo "Processing the first `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` frames."
}


TEMPDIR="/tmp/psnr_video"
WORKDIR=`pwd`
OUTFILE=psnr.dat
ERRFILE=errorsum.del

exit=0
if [[ `which pnmpsnr 2> /dev/null` = "" ]]
then
	echo
	echo "To use this script you have to install the program \"pnmpsnr\" which is"
	echo " included in the netpbm package."
	echo
	exit=1
fi

if [[ `which bc 2> /dev/null` = "" ]]
then
	echo
	echo "To use this script you have to install the GNU command line calculator \"bc\"."
	echo
	exit=1
fi

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

   exit=1
fi

if [ "$exit" -eq 1 ]; then
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
	echo "MPlayer options for ${FILE2}: $FILE2_Options"
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
	mplayer $FILE1_Options -frames $LastFrame -nosound -vo pnm ${WORKDIR}/$FILE1 > /dev/null
else
	mplayer $FILE1_Options -nosound -vo pnm ${WORKDIR}/$FILE1 > /dev/null
fi

###  File 2
echo
echo "############## $FILE2 #################"

cd ${TEMPDIR}/FILE2

rm -f *ppm

if [ $LastFrame -ge 0 ]; then
	mplayer $FILE2_Options -frames $LastFrame -nosound -vo pnm ${WORKDIR}/$FILE2 > /dev/null
else
	mplayer $FILE2_Options -nosound -vo pnm ${WORKDIR}/$FILE2 > /dev/null
fi


###  PSNR

echo
echo "############## PSNR Calculation #################"

if [[ `ls -1 ${TEMPDIR}/FILE1/*ppm | wc -l` = `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` ]]
then
	echo
else
	warning_frame_number
	echo
fi


cd ${TEMPDIR}/FILE2
#rm ../$OUTFILE
echo "File;Y;Cb;Cr" > ../$OUTFILE
echo "0" > $ERRFILE
i=0
for FILE in `ls -1 *.ppm`
        do
        echo $FILE
                echo -n "$FILE" >> ../$OUTFILE
                echo -n ";"     >> ../$OUTFILE

        YCBCR=`pnmpsnr ../FILE1/$FILE $FILE 2>&1 | tail -n 3 | cut -f 3 -d ':' | \
            ( read Y X; read CB X; read CR X; echo "$Y;$CB;$CR;")`
         Y=`echo $YCBCR | cut -f 1 -d ';'`
        CB=`echo $YCBCR | cut -f 2 -d ';'`
        CR=`echo $YCBCR | cut -f 3 -d ';'`
        echo $YCBCR >> ../$OUTFILE

        ALL=`echo "(-10)*l((e(-$Y/10*l(10))+e(-$CB/10*l(10))/4+e(-$CR/10*l(10))/4)/1.5)/l(10)" | bc -l`
        echo "$ALL" >> ../$OUTFILE
        ERROR=`echo "scale=30; (e(-1*$Y/10*l(10))+e(-1*$CB/10*l(10))/4+e(-1*$CR/10*l(10))/4)/1.5" | bc -l`
        ERRORSUM=`cat $ERRFILE`
        echo `echo "scale=30; $ERROR + $ERRORSUM" | bc -l` > $ERRFILE

        i=$(($i+1))
	if [[ $i = $LastFrame ]]
	then
		break
	fi
done

ERRORSUM=`cat $ERRFILE`
PSNR=`echo "-10*l($ERRORSUM/$i)/l(10)" | bc -l`
echo "PSNR:;$PSNR" >> ../$OUTFILE

cd ..
mv $OUTFILE ${WORKDIR}/

if [[ `ls -1 ${TEMPDIR}/FILE1/*ppm | wc -l` = `ls -1 ${TEMPDIR}/FILE2/*ppm | wc -l` ]]
then
        echo
else
	warning_frame_number
        echo
fi

cd ..
rm -r ${TEMPDIR}

echo "Created ${WORKDIR}/$OUTFILE"
echo
