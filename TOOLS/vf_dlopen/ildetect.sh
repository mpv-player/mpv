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
        -o /dev/null -of rawvideo -ofopts-clr -ovc rawvideo -ovcopts-clr -no-audio \
        | tee /dev/stderr | grep "^ildetect:"
}

out=`testfun "$@"`
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
