#!/bin/sh

# Author:  thuglife, mennucc1
#

set -e

arch=$(dpkg --print-installation-architecture)

[ -d /usr/lib/win32 ] || mkdir -v /usr/lib/win32
cd /usr/lib/win32
[ -d mplayer_win32_codecs ] || mkdir -v mplayer_win32_codecs
   
INSTALL () { 
    filename="$1"
    site="$2"
    url="$site/$filename"

    cd /usr/lib/win32/mplayer_win32_codecs

    if [ -r $filename.list ] ; then
      #if we stop the script, we don't want to redownload things
      #fixme we should check timestamps
      echo you have already downloaded and installed $filename
    else
     wget $url || return 1
     case "$filename" in 
      *.tar.gz)
            tar xvzf $filename > $filename.list
            rm $filename
          ;;
      *.tgz)
            tar xvzf $filename > $filename.list
            rm $filename
          ;;
      *.tar.bz2)
            tar  --bzip2 -xvf $filename > $filename.list
            rm $filename
          ;;
     esac
     cd ..
     ln -sbf mplayer_win32_codecs/*/* . 
     echo "Installed Succesfully!"
    fi
}


if [ `whoami` != root ]; then
    echo "You must be a root to start this script. Login As root first!"
    exit 1
else

case "$1" in
 install)
  if [ "$arch" = "i386" ]; then
     
   mainurl=''

   pref=/usr/lib/win32/mplayer_win32_codecs/bestsite

   #distribute the load
   if [ -r $pref ] ; then
    mainurl=`cat $pref `
   else
    if [ -f /usr/bin/netselect ] ; then   
     echo  Choosing best mirror using netselect....
        /usr/bin/netselect \
          http://www1.mplayerhq.hu/MPlayer/releases/codecs/ \
          http://www2.mplayerhq.hu/MPlayer/releases/codecs/ \
          http://ftp.lug.udel.edu/MPlayer/releases/codecs/ \
           | awk '{print $2}' > $pref
     mainurl=`cat $pref `
    else
     echo "(If you install 'netselect', it will select the best mirror for you"
     echo "  you may wish to stop this script and rerun after installation)"
     sleep 2
    fi
   fi

   #sanity check, in case netselect fails
   mainhost=`echo $mainurl | sed 's|http://||;s|ftp://||;s|/.*||g'`
   echo Test if $mainhost exists and is ping-able...
   if [ "$mainurl" = '' ] || ! ping -c1 "$mainhost" > /dev/null ; then
     domain=`hostname -f | sed 's/.*\.//g' `
     mainurl=http://www1.mplayerhq.hu/MPlayer/releases/codecs/
     if [ "$domain" = 'edu' -o "$domain" = 'com' ] ; then
       mainurl=http://ftp.lug.udel.edu/MPlayer/releases/codecs/
     fi
     if [ "$domain" = 'de' -o "$domain" = 'it' ] ; then
       mainurl=http://www2.mplayerhq.hu/MPlayer/releases/codecs/
     fi
   fi

   #INSTALL win32.tar.gz http://ers.linuxforum.hu/             

   INSTALL win32codecs-lite.tar.bz2 $mainurl
   #INSTALL w32codec.tar.bz2 http://www.mplayerhq.hu/MPlayer/releases/      
   INSTALL rp9codecs.tar.bz2 $mainurl
   INSTALL qt6dlls.tar.bz2 $mainurl
  elif [ "$arch" = "alpha" ]; then
   INSTALL rp8codecs-alpha.tar.bz2 $mainurl
  elif [ "$arch" = "powerpc" ]; then
   INSTALL rp8codecs-ppc.tar.bz2 $mainurl
   INSTALL xanimdlls-ppc.tar.bz2 $mainurl
  else
   echo "Sorry, no codecs for your arch. Sorry dude :("
      exit 1
	    
  fi
	
	;;
    
    uninstall)
	cd /usr/lib/win32/
	rm -rf mplayer_win32_codecs      
	#FIXME we need a better clean system
	if [ -r /usr/bin/symlinks ] ; then
         symlinks -d .
	else
	 echo "please install the package 'symlinks' and run 'symlinks -d /usr/lib/win32' "
	fi
	echo "Uninstalled Succesfully!"
	
	;;
	
    *)
	echo "Usage: {install|uninstall}"
	exit 1

	;;	    

esac


exit 0

fi

