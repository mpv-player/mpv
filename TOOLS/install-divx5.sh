#!/bin/sh

# Author:  thuglife, mennucc1
#

set -e

site=http://download.divx.com/divx/
packagename=divx4linux501-20020418
filename=$packagename.tgz

if [ `whoami` != root ]; then
    echo "You must be a root to start this script. Login As root first!"
    exit 1
else


case "$1" in
    install)
        mkdir /var/tmp/mplayer$$
        cd /var/tmp/mplayer$$
        wget $site/$filename
        tar xzf $filename
        cd $packagename/
        sh install.sh
        cd ..
        rm -rf $filename
        rm -rf $packagename/
        echo "Installed Succesfully!"
        rmdir /var/tmp/mplayer$$	
	;;

    uninstall)
        rm -rf /usr/local/lib/libdivx{encore,decore}.so{,.0}
        echo "Uninstalled Succesfully!"
	
	;;
    *)
	echo "Usage: {install|uninstall}"
	exit 1
	
	;;
esac



exit 0

fi

