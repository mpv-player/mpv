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

_color=yes
_head=yes
_cvs=yes
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
}

disable_all_tests() {
    _spaces=no
    _extensions=no
    _crlf=no
    _trailws=no
    _rcsid=no
    _oll=no
}

printoption() {
    echo "  -(no)$1  $2 [default: $3]"
}

printhead() {
    test "$_head" = "yes" && echo -e "$COLB$1$COLE"
}

all_filenames() {
    test "$_files" != "" && echo "$_files" && return

    if [ "$_cvs" == "no" ]; then
        find . -type f \
        | grep -v "\.\#\|\~$\|\.depend\|\/CVS\/\|config.mak\|^\./config\.h" \
        | grep -v "^\./version\.h\|\.o$\|\.a$"
    else
        list_cvs .
    fi
}

list_cvs() {
    for i in `grep "^/" $1/CVS/Entries | cut -d '/' -f 2`; do
        echo $1/$i
    done
    for j in `grep "^D/" $1/CVS/Entries | cut -d '/' -f 2`; do
        list_cvs $1/$j
    done
}

# -----------------------------------------------------------------------------

# Parse command line

for i in "$@"; do
    case "$i" in
    -help)
        echo -e "\n$0 [options] [files]\n"
        echo -e "options:\n"
        printoption "spaces    " "test for spaces in filenames" "$_spaces"
        printoption "extensions" "test for uppercase extensions" "$_extensions"
        printoption "crlf      " "test for MSDOS line endings" "$_crlf"
        printoption "trailws   " "test for trailing whitespace" "$_trailws"
        printoption "rcsid     " "test for missing RCS Id's" "$_rcsid"
        printoption "oll       " "test for overly long lines" "$_oll"
        echo
        printoption "all       " "enable all tests" "no"
        echo
        printoption "color     " "colored output" "$_color"
        printoption "head      " "print heading for each test" "$_head"
        printoption "cvs       " "use CVS/ to determine which files to check" \
                                                                        "$_cvs"
        echo -e "\nIf no files are specified, the whole tree is traversed."
        echo -e "If there are, -(no)cvs has no effect.\n"
        exit
        ;;
    -oll)
        _oll=yes
        ;;
    -nooll)
        _oll=no
        ;;
    -cvs)
        _cvs=yes
        ;;
    -nocvs)
        _cvs=no
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
        disable_all
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

if [ "$_color" == "yes" ]; then
    COLB="\e[36m"
    COLE="\e[m"
else
    COLB=""
    COLE=""
fi

# Generate filelist once so -cvs isn't _that_ much slower than -nocvs anymore

filelist=`all_filenames`

# -----------------------------------------------------------------------------

# DO CHECKS

# -----------------------------------------------------------------------------

if [ "$_spaces" == "yes" ]; then
    printhead "checking for spaces in filenames ..."
    find . | grep " "
fi

# -----------------------------------------------------------------------------

if [ "$_extensions" == "yes" ]; then
    printhead "checking for uppercase extensions ..."
    echo $filelist | grep "\.[[:upper:]]\+$" | grep -v "\.S$"
fi

# -----------------------------------------------------------------------------

if [ "$_crlf" == "yes" ]; then
    printhead "checking for MSDOS line endings ..."
    grep -l -I "
" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_trailws" == "yes" ]; then
    printhead "checking for trailing whitespace ..."
    grep -l -I "[[:space:]]\+$" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_rcsid" == "yes" ]; then
    printhead "checking for missing RCS \$Id\$ or \$Revision\$ tags ..."
    grep -L -I "\$\(Id\|Revision\)[[:print:]]\+\$" $filelist
fi

# -----------------------------------------------------------------------------

if [ "$_oll" == "yes" ]; then
    printhead "checking for overly long lines (over 79 characters) ..."
    grep -l -I "^[[:print:]]\{80,\}$" $filelist
fi

# -----------------------------------------------------------------------------

# End

