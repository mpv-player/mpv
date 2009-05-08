#!/bin/sh
set -e

# avoid insecure tempfile creation
umask 0022

# This script will download binary codecs for MPlayer unto a Debian system.

# Author: thuglife, mennucc1
#

CODECDIR=/usr/lib/codecs
PREFDIR=/var/lib/mplayer/prefs
MYSITE='http://people.debian.org/~mennucc1/mplayer'

dpkgarch=$(dpkg --print-installation-architecture)

[ -d $PREFDIR  ] || mkdir -v $PREFDIR
[ -d $CODECDIR ] || mkdir -v $CODECDIR
cd $CODECDIR
[ -d mplayer_binary_codecs ] || mkdir -v mplayer_binary_codecs


choosemirror ()
{
  cd $PREFDIR

  #if [ ! -r mirrors ] || find mirrors -mtime +20 ; then
    echo "Downloading mirrors list"
    wget -nv -c -N $MYSITE/mirrors || true
  #fi
  if [ ! -r bestsites ] || [ mirrors -nt bestsites ] || \
    find bestsites -mtime +20 | grep -q bestsites ; then
    if which netselect > /dev/null ; then
      echo Choosing best mirrors using netselect
      netselect -s 5 -t 5 $( cat mirrors ) | awk '{print $2}' > bestsites
    elif which fping > /dev/null ; then
     fping -C 1  $( sed   's#.*//##;s#/.*##' mirrors ) 2>&1 | \
       egrep -v 'bytes.*loss' | sort -n -k3 | \
       grep -v ': *-' | awk '/:/{print $1}' | head -5 | ( while read mainsite ; do
         grep $mainsite $PREFDIR/mirrors ; done ) > bestsites
    else
      echo "(If you install 'netselect' or 'fping', it will select the best mirror for you"
      echo "  you may wish to stop this script and rerun after installation)"
      sleep 3
      head -3 mirrors > bestsites
    fi
  fi
}



INSTALL () {
  filename="$3"
  dir="$2"
  url="$1"

  cd $CODECDIR/mplayer_binary_codecs

  if [ -r $filename ] ; then
    cp $filename $filename.bak
  fi

  if [ "$url" = @MAINSITE@ ] ; then
    cat $PREFDIR/bestsites | while read mainsite ; do
      echo Downloading $filename from $mainsite ...
      wget -c -N $mainsite/$dir/$filename || true
      if [ -r "$filename" ] ; then
        UNPACK "$filename"
        return 0
      fi
    done
  else
    wget -c -N $url/$dir/$filename || true
    if [ -r "$filename" ] ; then
      UNPACK "$filename"
      return 0
    fi
  fi
}




UNPACK ()
{
  filename="$1"
  if [ -r $filename.bak ] && cmp $filename.bak $filename && [ -r  $filename.list ] ; then
    echo It appears that $filename was already succesfully installed
    [ -r $filename.bak ] && rm $filename.bak
  else
    if grep -q " $filename$" $PREFDIR/MD5SUMS ; then
      echo Checking MD5 for $filename
      grep " $filename$" $PREFDIR/MD5SUMS | md5sum -c -
    else
      echo Warning: no MD5 for $filename were found. Hit enter to continue.
      read dummy
    fi
    echo Installing $filename ...
    if [ -r $filename.list ] ; then
      tr '\n' '\000' < $filename.list | xargs -r0 rm || true
      UNLINK $filename.list
      rm $filename.list
    fi

    tarfail () { echo FAILED $filename ; rm $filename.list ; exit 1 ; }

    case "$filename" in
      *.tar.gz)
        tar xvzf $filename > $filename.list || tarfail
        #rm $filename
        ;;
      *.tgz)
        tar xvzf $filename > $filename.list || tarfail
        #rm $filename
        ;;
      *.tar.bz2)
        tar --bzip2 -xvf $filename > $filename.list || tarfail
        #rm $filename
        ;;
    esac
    [ -r $filename.bak ] && rm $filename.bak
    LINK $filename.list
    echo "Installed $filename Succesfully!"
  fi
}

LINK () {
  cd $CODECDIR/
  cat $CODECDIR/mplayer_binary_codecs/$1 | while read f ; do
  ln -sbf mplayer_binary_codecs/"$f" .
  done
}

UNLINK () {
### FIXME
#  cd $CODECDIR
#  cat $CODECDIR/mplayer_binary_codecs/$1 | while f do
#  ln -sbf mplayer_binary_codecs/"$f"
#  done
  if which symlinks > /dev/null ; then
    symlinks -d $CODECDIR
  fi
}

if [ `whoami` != root ]; then
  echo "You must be 'root' to use this script. Login as root first!"
  exit 1
fi

case "$1" in
  install)
    if test -x /bin/bzip2 || test -x /usr/bin/bzip2 ; then : ; else
      echo You need to install bzip2
      exit 1
    fi
    choosemirror
    cd $PREFDIR
    #if [ ! -r codecs_list ] || find codecs_list -mtime +20 ; then
      echo "Getting codecs list"
      wget -nv -c -N $MYSITE/codecs_list || true
    #fi

    cd $PREFDIR
    echo Downloading MD5 sums from main site
    [ -r MD5SUMS ] && mv MD5SUMS MD5SUMS.bak
    if wget -nv -N http://www.mplayerhq.hu/MPlayer/releases/codecs/MD5SUMS ; then
      [ -r MD5SUMS.bak ] && rm MD5SUMS.bak
    else
      echo "failed"
      if [ -r MD5SUMS.bak ] ; then
        echo "trying to use backup"
        mv MD5SUMS.bak MD5SUMS
      fi
    fi

    if grep -q "^$dpkgarch" $PREFDIR/codecs_list ; then
      egrep -v "^[[:space:]]*(#|$)" $PREFDIR/codecs_list | \
        while read arch url dir file info ; do
          if [ "$dpkgarch" = "$arch" ]; then
            echo Downloading and installing $file $info...
            INSTALL "$url" "$dir" "$file"
          fi
        done
      needlibstd=no
      test "$dpkgarch" = "powerpc" && needlibstd=yes
      test "$dpkgarch" = "i386" && needlibstd=yes
      if test "$needlibstd" = "yes" && ! test -r /usr/lib/libstdc++.so.5 ; then
	echo "Warning: you need to install libstdc++ 5 libraries"
	echo -n "Do it now? "
	read R
	case $R in
         y*) apt-get install libstdc++5 ;;
          *) echo "If you change your mind, use the command"
             echo "  apt-get install libstdc++5" ;;
        esac
      fi
    else
      echo "Sorry, no codecs for your arch '$dpkgarch'. Sorry dude :("
      exit 1
    fi
    ;;

  uninstall)
    cd $CODECDIR/
    rm -rf mplayer_binary_codecs
    #FIXME we need a better clean system
    if which symlinks > /dev/null ; then
      symlinks -d .
    else
      echo "please install the package 'symlinks' and run 'symlinks -d $CODECDIR' "
    fi
    echo "Uninstalled Succesfully!"
    ;;

  *)
    echo "Usage: {install|uninstall}"
    echo "This program will install binary codecs for MPlayer."
    exit 1
    ;;

esac


exit 0
