#!/bin/sh

case "$0" in
    */*)
        MYDIR=${0%/*}
        ;;
    *)
        MYDIR=.
        ;;
esac

set -e

make -C "$MYDIR"

testfun()
{
    ${MPV:-mpv} "$@" \
        -vf dlopen="$MYDIR/ildetect.so" \
        -o /dev/null -of nut -ovc rawvideo -no-audio \
        | grep "^ildetect:"
}

out=`testfun "$@"`
echo
echo
echo "$out"
echo
echo
case "$out" in
    *"probably: PROGRESSIVE"*)
        ${MPV:-mpv} "$@"
        ;;
    *"probably: INTERLACED"*)
        ${MPV:-mpv} "$@" -vf-pre yadif
        ;;
    *"probably: TELECINED"*)
        ${MPV:-mpv} "$@" -vf-pre pullup
        ;;
    *)
        false
        ;;
esac
