#!/bin/sh

# -----------------------------------------------------------------------------

# Check source-tree for anomalies
#
# (C)opyright 2005 by Ivo van Poorten
# Licensed under GNU General Public License version 2
#
# Thanks to Melchior Franz of the FlightGear project for the original idea
# of a source-tree checker and Torinthiel for the feedback along the way.

# $Id$

# -----------------------------------------------------------------------------

# Default settings

_spaces=yes
_extensions=yes
_crlf=yes
_trailws=no
_rcsid=no
_oll=no
_charset=no
_stupid=no
_showcont=no

_color=yes
_head=yes
_svn=yes
_files=

# -----------------------------------------------------------------------------

# Avoid locale problems

export LC_ALL=C

# -----------------------------------------------------------------------------

# Helper functions

enable_all_tests() {
    _spaces=yes
    _extensions=yes
    _crlf=yes
    _trailws=yes
    _rcsid=yes
    _oll=yes
    _charset=yes
    _stupid=yes
}

disable_all_tests() {
    _spaces=no
    _extensions=no
    _crlf=no
    _trailws=no
    _rcsid=no
    _oll=no
    _charset=no
    _stupid=no
}

printoption() {
    echo "  -(no)$1  $2 [default: $3]"
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
        svn info -R | sed -n '/Path:/bb; :a; d; b; :b; s/Path: /.\//; h; :c; n;
                              /Node Kind:/bd; bc; :d; /directory/ba; g; p;'
    fi
}

# -----------------------------------------------------------------------------

# Parse command line

for i in "$@"; do
    case "$i" in
    -help|--help|-h|-\?)
        echo -e "\n$0 [options] [files]\n"
        echo -e "options:\n"
        printoption "spaces    " "test for spaces in filenames" "$_spaces"
        printoption "extensions" "test for uppercase extensions" "$_extensions"
        printoption "crlf      " "test for MSDOS line endings" "$_crlf"
        printoption "trailws   " "test for trailing whitespace" "$_trailws"
        printoption "rcsid     " "test for missing RCS Id's" "$_rcsid"
        printoption "oll       " "test for overly long lines" "$_oll"
        printoption "charset   " "test for wrong charset" "$_charset"
        printoption "stupid    " "test for stupid code" "$_stupid"
        echo
        printoption "all       " "enable all tests" "no"
        echo  "                   (-noall can be specified as -none)"
        echo
        printoption "showcont  " "show offending content of file(s)" \
                                                                   "$_showcont"
        echo
        printoption "color     " "colored output" "$_color"
        printoption "head      " "print heading for each test" "$_head"
        printoption "svn       " \
                    "use svn info to determine which files to check" "$_svn"
        echo -e "\nIf no files are specified, the whole tree is traversed."
        echo -e "If there are, -(no)svn has no effect.\n"
        exit
        ;;
    -stupid)
        _stupid=yes
        ;;
    -nostupid)
        _stupid=no
        ;;
    -charset)
        _charset=yes
        ;;
    -nocharset)
        _charset=no
        ;;
    -oll)
        _oll=yes
        ;;
    -nooll)
        _oll=no
        ;;
    -svn)
        _svn=yes
        ;;
    -nosvn)
        _svn=no
        ;;
    -head)
        _head=yes
        ;;
    -nohead)
        _head=no
        ;;
    -color)
        _color=yes
        ;;
    -nocolor)
        _color=no
        ;;
    -spaces)
        _spaces=yes
        ;;
    -nospaces)
        _spaces=no
        ;;
    -extensions)
        _extensions=yes
        ;;
    -noextensions)
        _extensions=no
        ;;
    -crlf)
        _crlf=yes
        ;;
    -nocrlf)
        _crlf=no
        ;;
    -trailws)
        _trailws=yes
        ;;
    -notrailws)
        _trailws=no
        ;;
    -rcsid)
        _rcsid=yes
        ;;
    -norcsid)
        _rcsid=no
        ;;
    -all)
        enable_all_tests
        ;;
    -noall)
        disable_all_tests
        ;;
    -none)
        disable_all_tests
        ;;
    -showcont)
        _showcont=yes
        ;;
    -noshowcont)
        _showcont=no
        ;;
    -*)
        echo "unknown option: $i" >&2
        exit 0
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

# Generate filelist once so -svn isn't _that_ much slower than -nosvn anymore

filelist=`all_filenames`

if [ "$_showcont" = "yes" ]; then
  _diffopts="-u"
  _grepopts="-n -I"
else
  _diffopts="-q"
  _grepopts="-l -I"
fi

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

if [ "$_stupid" = "yes" ]; then
    printhead "checking for stupid code ..."

    # avoid false-positives in xpm files, docs, etc, only check .c and .h files
    chfilelist=`echo $filelist | tr ' ' '\n' | grep "[\.][ch]$"`

    for i in calloc malloc realloc memalign av_malloc av_mallocz faad_malloc \
             lzo_malloc safe_malloc mpeg2_malloc _ogg_malloc; do
        printhead "--> casting of void* $i()"
        grep $_grepopts "([ 	]*[a-zA-Z_]\+[ 	]*\*.*)[ 	]*$i" $chfilelist
    done

    for i in "" signed unsigned; do
        printhead "--> usage of sizeof($i char)"
        grep $_grepopts "sizeof[ 	]*([ 	]*$i[ 	]*char[ 	]*)" $chfilelist
    done

    for i in int8_t uint8_t; do
        printhead "--> usage of sizeof($i)"
        grep $_grepopts "sizeof[ 	]*([ 	]*$i[ 	]*)" $chfilelist
    done

    printhead "--> usage of &&1"
    grep $_grepopts "&&[ 	]*1" $chfilelist

    printhead "--> usage of ||0"
    grep $_grepopts "||[ 	]*0" $chfilelist

    # added a-fA-F_ to eliminate some false positives
    printhead "--> usage of *0"
    grep $_grepopts "[a-zA-Z0-9)]\+[ 	]*\*[ 	]*0[^.0-9xa-fA-F_]" $chfilelist

    printhead "--> usage of *1"
    grep $_grepopts "[a-zA-Z0-9)]\+[ 	]*\*[ 	]*1[^.0-9ea-fA-F_]" $chfilelist

    printhead "--> usage of +0"
    grep $_grepopts "[a-zA-Z0-9)]\+[ 	]*+[ 	]*0[^.0-9xa-fA-F_]" $chfilelist

    printhead "--> usage of -0"
    grep $_grepopts "[a-zA-Z0-9)]\+[ 	]*-[ 	]*0[^.0-9xa-fA-F_]" $chfilelist
fi

# -----------------------------------------------------------------------------
