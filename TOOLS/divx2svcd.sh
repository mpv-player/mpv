#!/bin/bash

# (c) 2003 Vajna Miklos <mainroot@freemail.hu>
# divx2svcd for MPlayer
# distributed under GPL License

# simple utility that creates a SVCD from a video in an AVI container

# The newest version of this utility can be found at
# http://vmiklos.uw.hu/divx2svcd/divx2svcd
# MPlayer available at
# http://www.mplayerhq.hu/MPlayer/releases/MPlayer-1.0pre3try2.tar.bz2

###changelog###
#nobody cares about it :-)
cat >/dev/null <<EOF
0.5.1
- faster code by not re-mplexing one-cd-size or smaller videos

0.5.0
- needless for dumpvideo patch ;-)

0.4.9
- changed default bitrate to 1600 for better quality
- fix for burning more than one cd
- fix for wrong parameter help

0.4.8
- small fixes

0.4.7
- fixed bug, when there is no sub available

0.4.6
- support for burning the svcd with cdrecord
- lots of paranoid options for better quality from Denes Balatoni

0.4.5
- support for filenames including spaces

0.4.4
- support for checking all applications this script uses
- this changelog

0.4.3
- advanced detectation of movie aspect (mpeg4 codec, mpeg container)

0.4.2
- advanced vf options for movies with non-standard aspect

0.4.1
- checking for available sub

0.4.0
- support for tcmplex-panteltje
- support for libavcodec audio encoder

0.3.1-0.3.2
- small fixes

0.3
- almost totally rewritten from scratch
  based on the idea of Denes Balatoni <pnis@coder.hu>
- support for toolame instead of mp2enc
- suppert for libavcodec mpeg2video codec instead of mpeg2enc

0.2
- support for tcmplex instead of mplex

0.1rc2-rc4
- small bugfixes

0.1rc1
- initial release

EOF


###preparing###
#help

usage()
{
	cat <<EOF
Usage: `basename $0` input_avi [options]

Options:
-b|--bitrate xx	bitrate of mp2 video stream [1375]
-s|--cdsize xx	size of the cd we split the video to [795]
-w|--writecd	enables burning [disable]
-d|--device xx	scsi cd-recording device if you are using linux 2.4.x [0,0,0]
-c|--clean	clean up svcd images you just created
-h|--help	this help screen
EOF

}

#initializating constants
version='0.5.1'
bitrate=1375
cdsize=795
burning=0
cleaning=0
dev4='0,0,0'
firstcd=1

#paranoid options
paraopts='vrc_override=1,10,708:vqcomp=0.1:vratetol=10000000:vrc_buf_size=917:vrc_maxrate=2500:intra_matrix=8,9,12,22,26,27,29,34,9,10,14,26,27,29,34,37,12,14,18,27,29,34,37,38,22,26,27,31,36,37,38,40,26,27,29,36,39,38,40,48,27,29,34,37,38,40,48,58,29,34,37,38,40,48,58,69,34,37,38,40,48,58,69,79:inter_matrix=16,18,20,22,24,26,28,30,18,20,22,24,26,28,30,32,20,22,24,26,28,30,32,34,22,24,26,30,32,32,34,36,24,26,28,32,34,34,36,38,26,28,30,32,34,36,38,40,28,30,32,34,36,38,42,42,30,32,34,36,38,40,42,44'

#header
echo "DivX2SvcD $version (C) 2003-2004 Vajna Miklos"
echo

#checking for ls
ls=`which ls`

#checking for bc
which bc >/dev/null 2>&1
bcbin=`which bc 2>/dev/null`
if [ $? != 0 ]; then
	cat <<EOF
ERROR: Can't find bc. You can download it at
ftp://ftp.ibiblio.org/pub/gnu/bc/bc-1.06.tar.gz
EOF
exit 1
fi

#checking for vcdimager
which vcdimager >/dev/null 2>&1
bcbin=`which vcdimager 2>/dev/null`
if [ $? != 0 ]; then
	cat <<EOF
ERROR: Can't find vcdimager. You can download it at http://www.vcdimager.org
/pub/vcdimager/vcdimager-0.7_UNSTABLE/vcdimager-0.7.14.tar.gz
EOF
exit 1
fi

#checking which mplex utility we have to use
which tcmplex-panteltje >/dev/null 2>&1
if [ $? = 0 ]; then
	tcp_path=`which tcmplex-panteltje 2>&1`
else
	tcp_path="x"
fi
which tcmplex >/dev/null 2>&1
if [ $? = 0 ]; then
	tc_path=`which tcmplex 2>&1`
else
	tc_path="x"
fi

if [ -x $tcp_path ]; then
	tcbin=tcmplex-panteltje
	tcopt=-0
elif [ -x $tc_path ]; then
	tcbin=tcmplex
	tcopt=-p
else
	cat <<EOF
ERROR: Can't find any sutable mplex utility. You can download
tcmplex-panteltje at http://sunsite.rediris.es/
sites2/ibiblio.org/linux/apps/video/tcmplex-panteltje-0.3.tgz
EOF
exit 1
fi

#pharsing parameters

if [ $# -le 0 ]; then
	echo "Missing parameter!"
	usage
	exit 1
fi

case $1 in
	-h)
		usage
		exit 1
	;;
	-*)
		echo "Missing parameter!"
		usage
		exit 1
	;;
	*)
		input=`echo $1 |sed 's/\\ / /'`
		if [ "$input" = "`basename "$input"`" ]; then
		        input="`pwd`/$1"
		fi
		nev=`basename "$input" .avi`
		shift 1
	;;
esac

while [ "$1"x != "x" ]; do
   case $1 in
      -b|--bitrate)
        bitrate=$2
	shift 1
        ;;
      -s|--cdsize)
      	cdsize="$2"
	shift 1
	;;
      -d|--device)
        dev4="$2"
	shift 1
	;;
      -w|--write)
        burning=1
	;;
      -c|--clean)
        cleaning=1
	;;
      -h|--help)
      usage
        exit 0
	;;
   esac
   shift 1
done

#checking for cd-recording device
if [ "$burning" = 1 ]; then
echo -n "Searching for cdrecorder device... "

if [ `uname -r |cut -d '.' -f 2` = 4 ]; then
	#linux 2.4.x
	dev="dev=$dev4"
	echo "$dev4"
elif [ `uname -r |cut -d '.' -f 2` = 6 ]; then
	#linux 2.6.x
	if [ -e /dev/cdrecorder ]; then
		dev='dev=/dev/cdrecorder'
		echo "/dev/cdrecorder"
	else
		cat <<EOF
ERROR: Device file /dev/cdrecorder not found. Please link your
cd-recording device to /dev/cdrecorder!
Example: 'cd /dev; ln -s hdc cdrecorder'
EOF
		exit 1
	fi
else
	cat <<EOF
ERROR: Linux 2.4 or 2.6 series not found. You can download it at
http://www.kernel.org/ ;-)
EOF
	exit 1
fi

#checking for cdrecord
which cdrecord >/dev/null 2>&1
cdrbin=`which cdrecord 2>/dev/null`
if [ $? != 0 ]; then
	cat <<EOF
ERROR: Can't find cdrecord. You can download it at
ftp://ftp.berlios.de/pub/cdrecord/alpha/cdrtools-2.01a20.tar.gz
EOF
exit 1
else #checking for version >= 2.01a14
	echo -n "Checking for cdrecord version >= 2.01a14... "
	$cdrbin cuefile=a 2>&1 |grep 'Bad Option' >/dev/null 2>&1
	if [ "$?" = 0 ]; then
	cat <<EOF
ERROR: Can't find cdrecord version >= 2.01a14. You can download it at
ftp://ftp.berlios.de/pub/cdrecord/alpha/cdrtools-2.01a20.tar.gz
EOF
	else
	        echo "`$cdrbin -version |cut -d ' ' -f 2`"
	fi
fi
fi

#checking for sub avariable

if [ -f "$nev.sub" ]; then
	subopts=$nev.sub
else
	subopts=''
fi

if [ "x$subopts" = "x" ]; then
	subs=''
else
	subs='-sub '
fi

#checking for what height needed
inputwidth=`mplayer -vo null -ao null "$input" -frames 1 2>/dev/null |grep '=>'|cut -d ' ' -f 5|cut -d x -f 1`
inputheight=`mplayer -vo null -ao null "$input" -frames 1 2>/dev/null |grep '=>'|cut -d ' ' -f 5|cut -d x -f 2`
svcdaspect=`echo -e "scale=10\n1.596/($inputwidth/$inputheight)"|bc /dev/stdin`
height=`echo -e "scale=10\n$svcdaspect*480"|bc /dev/stdin|cut -d . -f 1`

#checking for ratios less than 1.33
istoohigh=`expr $height \> 577`
if [ "$istoohigh" = 1 ]; then
	height=576
fi

#find out the vf options
if [ "$height" = 576 ]; then
	vfopts='-vf scale=480:576'
else
	#-vf processes filters in reverse order
	exy=`echo -e "scale=10\n(576-$height)/2"|bc /dev/stdin|cut -d . -f 1`
	vfopts="-vf scale=480:$height,expand=480:576:0:$exy:1"
	echo "Using filter options: '$vfopts'"
fi

#finish displaying informations
if [ "$burning" = 1 ]; then
#asking for cd
cat <<EOF

Please insert a blank cd in your cdwriter.
(If you are using a rewritable media, 
don't forgot to blank it before using divx2svcd.)
Press any key when your are ready.
EOF
read -n 1 i
fi


###start working###
#encoding
mencoder -ofps 25 -oac lavc "$input" -ovc lavc -lavcopts vcodec=mpeg2video:vbitrate=$bitrate:acodec=mp2:abitrate=128:keyint=25:aspect=4/3:$paraopts -o "${nev}2.avi" -srate 44100 -of mpeg -channels 2 $vfopts $subs "$subopts"

videosize=`$ls -l "${nev}2.avi"|tr -s ' '|cut -d ' ' -f5`
if ! [ `echo $(( $cdsize*1048576 < $videosize ))` = "1" ]; then
	#video is smaller, than $cdsize
	mv ${nev}2.avi ${nev}00.mpg
else
	#splitting
	mplayer -dumpvideo -dumpfile "$nev.m2v" "${nev}2.avi"
	mplayer -dumpaudio -dumpfile "$nev.mp2" "${nev}2.avi"
	rm "${nev}2.avi"
	echo "maxFileSize = $cdsize" > template
	$tcbin -i "$nev.m2v" $tcopt "$nev.mp2" -o "$nev.mpg" -m s -F template
	rm template
	rm "$nev.m2v" "$nev.mp2"
fi

for i in *mpg
do
	nev2=`basename "$i" .mpg`
	#creating images
	vcdimager -t svcd -c "$nev2.cue" -b "$nev2.bin" "$i"
	#burning if needs
	if [ "$burning" = 1 ]; then
		if [ "$firstcd" != 1 ]; then
			cat <<EOF

Please insert an another blank cd in your cdwriter.
Press any key when your are ready.
EOF
			read -n 1 i
		else
			firstcd=2
		fi
		$cdrbin -v -dao $dev speed=12 gracetime=2 driveropts=burnfree -eject cuefile="$nev2.cue"
	fi
	#cleaning if needs
	if [ "$cleaning" = 1 ]; then
		rm -f "$nev2.cue" "$nev2.bin"
	fi
done
rm -f "$nev"*mpg
