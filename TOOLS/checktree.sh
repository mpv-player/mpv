#!/bin/bash

# -----------------------------------------------------------------------------

# Check source-tree for anomalies
#
# Copyright (C) 2005-2007 by Ivo van Poorten
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
# Thanks to Melchior Franz of the FlightGear project for the original idea
# of a source-tree checker and Torinthiel for the feedback along the way.

# $Id$

# -----------------------------------------------------------------------------

# All yes/no flags. Spaces around flagnames are important!

testflags=" spaces extensions crlf tabs trailws rcsid oll charset stupid gnu \
res depr "
allflags="$testflags showcont color head svn "

# -----------------------------------------------------------------------------

# Avoid locale problems

export LC_ALL=C

# -----------------------------------------------------------------------------

# Helper functions

set_all_tests() {
    for i in $testflags ; do
        eval _$i=$1
    done
}

printoption() {
    test -n "$3" && def=$3 || eval def=\$_$1
    echo "  -(no)$1  $2 [default: $def]"
}

printhead() {
    test "$_head" = "yes" && echo -e "$COLB$1$COLE"
}

all_filenames() {
    test "$_files" != "" && echo "$_files" && return

    if [ "$_svn" = "no" ]; then
        find . -type f \
        | grep -v "\.\#\|\~$\|\.depend\|\/\.svn\/\|config.mak\|^\./config\.h" \
        | grep -v "^\./version\.h\|\.o$\|\.a$\|configure.log\|^\./help_mp.h"
    else
        for p in . libavcodec libavutil libavformat libpostproc ; do
            svn info -R $p 2>/dev/null | sed -n \
                '/Path:/bb; :a; d; b; :b; s/Path: /.\//; h; :c; n;
                 /Node Kind:/bd; bc; :d; /directory/ba; g; p;'
        done
    fi
}

# -----------------------------------------------------------------------------

# Default settings

set_all_tests no
_spaces=yes
_extensions=yes
_crlf=yes

_showcont=no
_color=yes
_head=yes
_svn=yes
_files=

# Parse command line

for i in "$@"; do
    case "$i" in
    -help|--help|-h|-\?)
        echo -e "\n$0 [options] [files]\n"
        echo -e "options:\n"
        printoption "spaces    " "test for spaces in filenames"
        printoption "extensions" "test for uppercase extensions"
        printoption "crlf      " "test for MSDOS line endings"
        printoption "tabs      " "test for tab characters"
        printoption "trailws   " "test for trailing whitespace"
        printoption "rcsid     " "test for missing RCS Id's"
        printoption "oll       " "test for overly long lines"
        printoption "charset   " "test for wrong charset"
        printoption "stupid    " "test for stupid code"
        printoption "gnu       " "test for GNUisms"
        printoption "res       " "test for reserved identifiers"
        printoption "depr      " "test for deprecated function calls"
        echo
        printoption "all       " "enable all tests" "no"
        echo  "                   (-noall can be specified as -none)"
        echo
        printoption "showcont  " "show offending content of file(s)"
        echo
        printoption "color     " "colored output"
        printoption "head      " "print heading for each test"
        printoption "svn       " \
                    "use svn info to determine which files to check"
        echo -e "\nIf no files are specified, the whole tree is traversed."
        echo -e "If there are, -(no)svn has no effect.\n"
        exit
        ;;
    -all)
        set_all_tests yes
        ;;
    -noall)
        set_all_tests no
        ;;
    -none)
        set_all_tests no
        ;;
    -*)
        var=`echo X$i | sed 's/^X-//'`
        val=yes
        case "$var" in
            no*)
                var=`echo "$var" | cut -c 3-`
                val=no
                ;;
        esac
        case "$allflags" in
            *\ $var\ *)
                eval _$var=$val
                ;;
            *)
                echo "unknown option: $i" >&2
                exit 0
                ;;
        esac
        ;;
    *)
        _files="$_files $i"
        ;;
    esac
done

# -----------------------------------------------------------------------------

# Set heading color

if [ "$_color" = "yes" ]; then
    COLB="\e[36m"
    COLE="\e[m"
else
    COLB=""
    COLE=""
fi

# Test presence of svn info

if [ "$_svn" = "yes" -a ! -d .svn ] ; then
    echo "No svn info available. Please use -nosvn." >&2
    exit 1
fi

# Generate filelist once so -svn isn't _that_ much slower than -nosvn anymore

filelist=`all_filenames`

case "$_stupid$_res$_depr$_gnu" in
    *yes*)
    # generate 'shortlist' to avoid false positives in xpm files, docs, etc,
    # when one only needs to check .c and .h files
    chfilelist=`echo $filelist | tr ' ' '\n' | grep "[\.][ch]$"`
    ;;
esac

if [ "$_showcont" = "yes" ]; then
  _diffopts="-u"
  _grepopts="-n -I"
else
  _diffopts="-q"
  _grepopts="-l -I"
fi

TAB=`echo " " | tr ' ' '\011'`

# -----------------------------------------------------------------------------

# DO CHECKS

# -----------------------------------------------------------------------------

if [ "$_spaces" = "yes" ]; then
    printhead "checking for spaces in filenames ..."
    find . | grep " "
fi

# -----------------------------------------------------------------------------

if [ "$_extensions" = "yes" ]; then
    printhead "checking for uppercase extensions ..."
    echo $filelist | grep "\.[[:upper:]]\+$" | grep -v "\.S$"
fi

# -----------------------------------------------------------------------------

if [ "$_crlf" = "yes" ]; then
    printhead "checking for MSDOS line endings ..."
    CR=`echo " " | tr ' ' '\015'`
    grep $_grepopts "$CR" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_tabs" = "yes" ]; then
    printhead "checking for TAB characters ..."
    grep $_grepopts "$TAB" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_trailws" = "yes" ]; then
    printhead "checking for trailing whitespace ..."
    grep $_grepopts "[[:space:]]\+$" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_rcsid" = "yes" ]; then
    printhead "checking for missing RCS \$Id\$ or \$Revision\$ tags ..."
    grep -L -I "\$\(Id\|Revision\)[[:print:]]\+\$" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_oll" = "yes" ]; then
    printhead "checking for overly long lines (over 79 characters) ..."
    grep $_grepopts "^[[:print:]]\{80,\}$" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_gnu" = "yes" -a -n "$chfilelist" ]; then
    printhead "checking for GNUisms ..."
    grep $_grepopts "case.*\.\.\..*:" $chfilelist
fi

# -----------------------------------------------------------------------------

if [ "$_res" = "yes" -a -n "$chfilelist" ]; then
    printhead "checking for reserved identifiers ..."
    grep $_grepopts "#[ $TAB]*define[ $TAB]\+_[[:upper:]].*" $chfilelist
    grep $_grepopts "#[ $TAB]*define[ $TAB]\+__.*" $chfilelist
fi

# -----------------------------------------------------------------------------

if [ "$_charset" = "yes" ]; then
    printhead "checking bad charsets ..."
    for I in $filelist ; do
      case "$I" in
        ./help/help_mp-*.h)
          ;;
        ./DOCS/*)
          ;;
        *.c|*.h)
          iconv -c -f ascii -t ascii "$I" | diff $_diffopts "$I" -
          ;;
      esac
    done
fi

# -----------------------------------------------------------------------------

if [ "$_stupid" = "yes" -a -n "$chfilelist" ]; then
    printhead "checking for stupid code ..."

    for i in calloc malloc realloc memalign av_malloc av_mallocz faad_malloc \
             lzo_malloc safe_malloc mpeg2_malloc _ogg_malloc; do
        printhead "--> casting of void* $i()"
        grep $_grepopts "([ $TAB]*[a-zA-Z_]\+[ $TAB]*\*.*)[ $TAB]*$i" \
                                                                    $chfilelist
    done

    for i in "" signed unsigned; do
        printhead "--> usage of sizeof($i char)"
        grep $_grepopts "sizeof[ $TAB]*([ $TAB]*$i[ $TAB]*char[ $TAB]*)" \
                                                                    $chfilelist
    done

    for i in int8_t uint8_t; do
        printhead "--> usage of sizeof($i)"
        grep $_grepopts "sizeof[ $TAB]*([ $TAB]*$i[ $TAB]*)" $chfilelist
    done

    printhead "--> usage of &&1"
    grep $_grepopts "&&[ $TAB]*1" $chfilelist

    printhead "--> usage of ||0"
    grep $_grepopts "||[ $TAB]*0" $chfilelist

    # added a-fA-F_ to eliminate some false positives
    printhead "--> usage of *0"
    grep $_grepopts "[a-zA-Z0-9)]\+[ $TAB]*\*[ $TAB]*0[^.0-9xa-fA-F_]" \
                                                                    $chfilelist

    printhead "--> usage of *1"
    grep $_grepopts "[a-zA-Z0-9)]\+[ $TAB]*\*[ $TAB]*1[^.0-9ea-fA-F_]" \
                                                                    $chfilelist

    printhead "--> usage of +0"
    grep $_grepopts "[a-zA-Z0-9)]\+[ $TAB]*+[ $TAB]*0[^.0-9xa-fA-F_]" \
                                                                    $chfilelist

    printhead "--> usage of -0"
    grep $_grepopts "[a-zA-Z0-9)]\+[ $TAB]*-[ $TAB]*0[^.0-9xa-fA-F_]" \
                                                                    $chfilelist
fi

# -----------------------------------------------------------------------------

if [ "$_depr" = "yes" -a -n "$chfilelist" ]; then
    printhead "checking for deprecated and obsolete function calls ..."

    for i in bcmp bcopy bzero getcwd getipnodebyname inet_ntoa inet_addr \
        atoq ecvt fcvt ecvt_r fcvt_r qecvt_r qfcvt_r finite ftime gcvt herror \
        hstrerror getpass getpw getutent getutid getutline pututline setutent \
        endutent utmpname gsignal ssignal gsignal_r ssignal_r infnan memalign \
        valloc re_comp re_exec drem dremf dreml rexec svc_getreq sigset \
        sighold sigrelse sigignore sigvec sigmask sigblock sigsetmask \
        siggetmask ualarm ulimit usleep statfs fstatfs ustat get_kernel_syms \
        query_module sbrk tempnam tmpnam mktemp mkstemp
    do
        printhead "--> $i()"
        grep $_grepopts "[^a-zA-Z0-9]$i[ $TAB]*(" $chfilelist
    done
fi
