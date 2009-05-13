#!/bin/bash
#
# QEPDVCD
#
# Most of this stuff comes straight from MPlayer documentation.
# Options are limited only to a small useful subset, if you
# want more control, RTFM and DIY.
#
# Version:          0.1
#
# Licence:          GPL
#
# Author:           Reynaldo H. Verdejo Pinochet <reynaldo@opendot.cl>
#
# Script:           MPlayer Sources. Anything supported to vcd/svcd pal/ntsc
#
# requires:         mencoder
#
# sugests:          vcdimager / cdrecord
#
# Thanks to:        Carlos Navarro Salas - name author ;)
#
#

# Defaults

MYNAME=`basename $0`
TARGET="svcd"
ENCQ="2"
ABPS="224"
VBPS="2000"
NORM="NTSC"
SPLIT="0"
TOLERANCE="85"
OUTNAME="mencodedvcd"
SUBFILENAME=0

EDLFILENAME="pass.edl"
HAVESUB=0
WORKDIR="."
RATIOX=4
RATIOY=3

OPTIONS="INPUTFILENAME TARGET ENCQ ABPS VBPS NORM SPLIT TOLERANCE OUTNAME"

usage()
{
echo ""
echo "usage $MYNAME inputfile [options]"
echo ""
echo "Options: [Default value]"
echo ""
echo "-t	Target svcd|vcd [svcd]"
echo "-q	Encoding quality 0|1|2 [2]"
echo "-a	Audio bitrate in kbps [224]"
echo "-v	Video bitrate in kbps [2000 For SVCD, 1150 For VCD]"
echo "-n	Norm NTSC|PAL [NTSC]"
echo "-d	Divide/split movie at given times time1:time2:... [no split]"
echo "-s	Shitty TV screen tolerance %, afects subtitle positioning [85]"
echo "-u	Subtitle file name [No subtitle]"
echo "-o	Output basename [mencodedvcd]"
echo ""
echo "In case you want to use -a/-v please read:"
echo "http://www.mplayerhq.hu/DOCS/HTML/en/menc-feat-vcd-dvd.html"
echo ""
}

test_needed()
{

for i in mencoder; do
	if [ -z "`which $i`" ]; then
		echo "[ERROR] mencoder not found in $PATH!"
		exit 1
	fi
done

}

test_sugested()
{

for i in vcdimager cdrecord; do
	if [ -z "`which $i`" ]; then
		echo "[WARNING] $i not found in $PATH!"
		echo "You'll likely need it after we finish."
		exit 1
	fi
done
}

test_needed
test_sugested

if [ $# -lt 1 ]; then
	echo ""
	echo "[ERROR] Input filename parameter is mandatory."
	echo ""
	usage
	exit 1
fi

case $1 in
	-*)
 		usage
		exit 1
		;;
	*)
		INPUTFILENAME=$1
		shift 1
		;;
esac

while [ "$1"x != "x" ]; do
	case $1 in
		-t)
		TARGET=$2
		shift 1
		;;
		-q)
		ENCQ=$2
		shift 1
		;;
		-a)
		ABPS=$2
		shift 1
		;;
		-v)
		VBPS=$2
		shift 1
		;;
		-n)
		NORM=$2
		shift 1
		;;
		-d)
		SPLIT=$2
		shift 1
		;;
		-s)
		TOLERANCE=$2
		shift 1
		;;
		-u)
		SUBFILENAME="$2"
		HAVESUB=1
		shift 1
		;;
		-o)
		OUTNAME=$2
		shift 1
		;;
	esac
	shift 1
done

echo ""
echo "[STATUS] Will re-encode using the following parameters:"
echo ""
for i in $OPTIONS ; do
	echo "$i ${!i}";
done

# Parameter Sanity Check ###########################################
# We need to check supplied params against known format constraints
####################################################################

if [ $TARGET = "svcd" ]; then
	if [ $ABPS -gt 384 ]; then
		echo "[ERROR] SVCD maximum abitrate is 384kbps."
		exit 1
	fi
	if [ $VBPS -gt 2600 ]; then
		echo "[ERROR] SVCD maximum vbitrate is 2600kbps."
		exit 1
	fi
else [ $TARGET = "vcd" ]
	if [ $ABPS -eq 224 ]; then
		echo "[ERROR] VCD abitrate must be 224kbps."
		exit 1
	fi
	if [ $VBPS -gt 1150 ]; then
		echo "[ERROR] VCD maximum vbitrate is 1150kbps."
		exit 1
	fi
fi

# Set encoding options ##############################################

if [ $TARGET = "svcd" ]; then
	FORMAT="xsvcd"
	VCODEC="mpeg2video"
	VRCMINRATE=4
	VRCMAXRATE=2500
	VRCBUFSIZE=917
	if [ $NORM = "NTSC" ]; then
		SCALEX=480
		SCALEY=480
		KEYINT=18
		OFPS="24000/1001"
		TELECINE=":telecine"
	else [ $NORM = "PAL" ]
		SCALEX=480
		SCALEY=576
		KEYINT=15
		OFPS=25
	fi
else [ $TARGET = "vcd" ]
	FORMAT="xvcd"
	VCODEC="mpeg1video"
	VRCMINRATE=$VBPS
	VRCMAXRATE=$VBPS
	VRCBUFSIZE=327
	if [ $NORM = "NTSC" ]; then
		SCALEX=352
		SCALEY=240
		KEYINT=18
		OFPS="24000/1001"
	else [ $NORM = "PAL" ]
		SCALEX=352
		SCALEY=288
		KEYINT=15
		OFPS=25
	fi
fi

# Start reencoding ###################################################

cd $WORKDIR

if [ $(($HAVESUB+1)) -eq 1 ]; then
	SUBTITLESTRING="/dev/null"
else
	SUBTITLESTRING="$SUBFILENAME"
fi

if [ $SPLIT = "0" ]; then
	CICLES=0
	TIMESTRING=""
else
	if [ -e $EDLFILENAME ]; then
		echo "[ERROR]"
		echo "The -d option needs to generate a temporary file called"
		echo "$EDLFILENAME. You already have one in this directory,"
		echo "please remove/rename it and run $MYNAME again."
		echo ""
		exit 1
	else
		EDLSTRING=$(echo $SPLIT | sed -e s/:/' '/g)
		EDLARRAY=($EDLSTRING)
		CICLES=$(echo $EDLSTRING | wc -w)
		TIMESTRING="-edl $EDLFILENAME -hr-edl-seek"
	fi
fi


for j in $(seq 0 $CICLES); do

	NEWNAME=$OUTNAME"_PART"$j".mpg"
	echo ""
	echo "Making $NEWNAME, wish me luck ;-)"
	echo ""

# Create EDLFILENAME #################################################

	if [ $CICLES -ge 1 ]; then
		for i in $(seq 0 $CICLES)
		do
			if [ $i -eq $j ]; then
				if [ $j -ne 0 ]; then
					echo "0 ${EDLARRAY[$(($i-1))]} 0" > $EDLFILENAME
				fi
				if [ $i -ne $CICLES ]; then
					echo "${EDLARRAY[$(($i))]} 999999 0" >> $EDLFILENAME
				fi
			fi
		done
	fi

# Mencoder Time ;-) ###################################################

mencoder \
-ovc lavc -oac lavc -vf expand=:::::$RATIOX/$RATIOY:1,scale=$SCALEX:$SCALEY,harddup \
-srate 44100 -af lavcresample=44100 -lavcopts acodec=mp2:abitrate=$ABPS:vcodec=$VCODEC:\
vbitrate=$VBPS:keyint=$KEYINT:mbd=$ENCQ:vrc_buf_size=$VRCBUFSIZE:vrc_maxrate=$VRCMAXRATE:\
vrc_minrate=$VRCMINRATE:vi_qfactor=0.1:vi_qoffset=1.5:aspect=$RATIOX/$RATIOY -of mpeg \
-mpegopts format=$FORMAT$TELECINE -sub $SUBTITLESTRING -subpos $TOLERANCE -subwidth \
$TOLERANCE -ofps $OFPS $TIMESTRING -o $NEWNAME $INPUTFILENAME

echo "Encoding of $NEWNAME finished."
echo "Run vcdimager -t svcd/vcd $NEWNAME and burn with cdrecord."

done

echo "$(($CICLES+1)) VCD/SVCD file(s) created!!!"
echo "Happy to be of some help ;-) have fun."
exit 0
