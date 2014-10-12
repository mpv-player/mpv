#!/bin/sh

case "$0" in
    */*)
        MYDIR=${0%/*}
        ;;
    *)
        MYDIR=.
        ;;
esac

: ${MPV:=mpv}
: ${ILDETECT_MPV:=$MPV}
: ${ILDETECT_MPVFLAGS:=--start=35% --length=35}
: ${ILDETECT_DRY_RUN:=}
: ${ILDETECT_QUIET:=}
: ${ILDETECT_RUN_INTERLACED_ONLY:=}
: ${ILDETECT_FORCE_RUN:=}
: ${MAKE:=make}

# exit status:
# 0 progressive
# 1 telecine
# 2 interlaced
# 8 unknown
# 15 compile fail
# 16 detect fail
# 17+ mpv's status | 16

$MAKE -C "$MYDIR" ildetect.so || exit 15

testfun()
{
    $ILDETECT_MPV "$@" \
        --vf=dlopen="$MYDIR/ildetect.so" \
        --o= --vo=null --no-audio --untimed \
        $ILDETECT_MPVFLAGS \
        | { if [ -n "$ILDETECT_QUIET" ]; then cat; else tee /dev/stderr; fi } \
        | grep "^ildetect:"
}

out=`testfun "$@"`
case "$out" in
    *"probably: PROGRESSIVE"*)
        [ -n "$ILDETECT_DRY_RUN" ] || \
            [ -n "$ILDETECT_RUN_INTERLACED_ONLY" ] || \
            $ILDETECT_MPV "$@"
        r=$?
        [ $r -eq 0 ] || exit $(($r | 16))
        exit 0
        ;;
    *"probably: TELECINED"*)
        out2=`ILDETECT_MPVFLAGS="$ILDETECT_MPVFLAGS --vf-pre=pullup,scale" testfun "$@"`
        case "$out2" in
            *"probably: TELECINED"*|*"probably: INTERLACED"*)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" -vf-pre yadif
                r=$?
                [ $r -eq 0 ] || exit $(($r | 16))
                exit 2
                ;;
            *)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" -vf-pre pullup
                r=$?
                [ $r -eq 0 ] || exit $(($r | 16))
                exit 1
                ;;
        esac
        ;;
    *"probably: INTERLACED"*)
        [ -n "$ILDETECT_DRY_RUN" ] || \
            $ILDETECT_MPV "$@" -vf-pre yadif
        r=$?
        [ $r -eq 0 ] || exit $(($r | 16))
        exit 2
        ;;
    *"probably: "*)
        [ -n "$ILDETECT_FORCE_RUN" ] || exit 8
        [ -n "$ILDETECT_DRY_RUN" ] || \
            $ILDETECT_MPV "$@" -vf-pre yadif
        r=$?
        [ $r -eq 0 ] || exit $(($r | 16))
        exit 0
        ;;
    *)
        exit 16
        ;;
esac
