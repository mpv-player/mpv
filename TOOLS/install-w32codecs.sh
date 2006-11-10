#!/bin/sh
set -e

# This script will download binary codecs for MPlayer unto a Debian system.

# Author:  thuglife, mennucc1
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

  #if [ ! -r mirrors ] || find  mirrors -mtime +20  ; then
    echo Downloading mirrors list..
    wget -nv -c -N $MYSITE/mirrors || true
  #fi
  if [ ! -r bestsites ] || [ mirrors -nt bestsites ] || \
    find  bestsites -mtime +20 > /dev/null ; then
    if which netselect > /dev/null  ; then
      echo  Choosing best mirrors using netselect....
      netselect  -s 5  $( cat mirrors ) | awk '{print $2}' > bestsites
    elif which fping > /dev/null ; then
      fping -C 1   $( sed   's#.*//##;s#/.*##' mirrors ) 2>&1 | \
        egrep -v 'bytes.*loss' | sort -n -k3  | \
        grep -v ': *-' |  awk '/:/{print $1}' | head -5 > bestsites
    else
      echo "(If you install 'netselect', it will select the best mirror for you"
      echo "  you may wish to stop this script and rerun after installation)"
      sleep 5
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
    cp $filename  $filename.bak
  fi

  if [  "$url" = @MAINSITE@ ] ; then
    cat $PREFDIR/bestsites |   while read mainsite ; do
      echo Downloading $filename from $mainsite ...
      wget -v -c -N $mainsite/$dir/$filename || true
      if [ -r "$filename" ] ; then
        UNPACK "$filename"
        [ -r $filename.bak ] && rm $filename.bak
        return 0
      fi
    done
  else
    wget -v -c -N $url/$dir/$filename || true
    if  [ -r "$filename" ] ; then
      UNPACK "$filename"
      [ -r $filename.bak ] && rm $filename.bak
      return 0
    fi
  fi
}




UNPACK ()
{
  filename="$1"
  if [ ! -r $filename.bak ] || ! cmp $filename.bak $filename ; then
    echo Installing $filename  ...
    if [ -r $filename.list  ] ; then
      tr '\n' '\000' < $filename.list | xargs -r0 rm  || true
      UNLINK $filename.list
      rm $filename.list
    fi

    case "$filename" in
      *.tar.gz)
        tar xvzf $filename > $filename.list
        #rm $filename
        ;;
      *.tgz)
        tar xvzf $filename > $filename.list
        #rm $filename
        ;;
      *.tar.bz2)
        tar  --bzip2 -xvf $filename > $filename.list
        #rm $filename
        ;;
    esac
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
    choosemirror
    cd $PREFDIR
    #if [ ! -r codecs_list ] || find  codecs_list -mtime +20  ; then
      echo 'Getting  codecs list ...'
      wget -nv -c -N $MYSITE/codecs_list || true
    #fi

    if  grep -q "^$dpkgarch" $PREFDIR/codecs_list   ] ; then
      egrep -v "^[[:space:]]*(#|$)" $PREFDIR/codecs_list | \
        while read arch url dir file info ; do
          if [ "$dpkgarch" = "$arch" ]; then
            echo Installing $file  $info...
            INSTALL "$url"  "$dir"  "$file"
            n=1
          fi
        done
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
