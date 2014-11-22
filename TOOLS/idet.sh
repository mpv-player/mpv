#!/bin/sh

: "${MPV:=mpv}"
: "${ILDETECT_MPV:=$MPV}"
: "${ILDETECT_MPVFLAGS:=--start=35% --length=35}"
: "${ILDETECT_DRY_RUN:=}"
: "${ILDETECT_QUIET:=}"
: "${ILDETECT_RUN_INTERLACED_ONLY:=}"
: "${ILDETECT_FORCE_RUN:=}"

# This script uses ffmpeg's "idet" filter for interlace detection. In the
# long run this should replace ildetect.sh+ildetect.so.

# exit status:
# 0 progressive
# 1 telecine
# 2 interlaced
# 8 unknown
# 16 detect fail
# 17+ mpv's status | 16

testfun()
{
    $ILDETECT_MPV "$@" \
        --vf-add=lavfi="[idet]" --msg-level ffmpeg=v \
        --o= --vo=null --no-audio --untimed \
        $ILDETECT_MPVFLAGS \
        | { if [ -n "$ILDETECT_QUIET" ]; then cat; else tee /dev/stderr; fi } \
        | grep "Parsed_idet_0: Multi frame detection: "
}

judge()
{
    tff=0
    bff=0
    progressive=0
    undetermined=0
    while read -r _ _ _ _ _ _ tff1 _ bff1 _ progressive1 _ undetermined1 _; do
        case "$tff1$bff1$progressive1$undetermined1" in
            *[!0-9]*)
                printf >&2 'ERROR: Unrecognized idet output: %s\n' "$out"
                exit 16
                ;;
        esac
        tff=$((tff + tff1))
        bff=$((bff + bff1))
        progressive=$((progressive + progressive1))
        undetermined=$((undetermined + undetermined1))
    done <<EOF
$(testfun "$@")
EOF

    interlaced=$((bff + tff))
    determined=$((interlaced + progressive))

    if [ "$undetermined" -gt "$determined" ] || [ "$determined" -lt 250 ]; then
        echo >&2 "ERROR: Less than 50% or 250 frames are determined."
        [ -n "$ILDETECT_FORCE_RUN" ] || exit 8
        echo >&2 "Assuming interlacing."
        if [ "$tff" -gt $((bff * 10)) ]; then
            verdict="interlaced-tff"
        elif [ "$bff" -gt $((tff * 10)) ]; then
            verdict="interlaced-bff"
        else
            verdict="interlaced"
        fi
    elif [ $((interlaced * 20)) -gt "$progressive" ]; then
        # At least 5% of the frames are interlaced!
        if [ "$tff" -gt $((bff * 10)) ]; then
            verdict="interlaced-tff"
        elif [ "$bff" -gt $((tff * 10)) ]; then
            verdict="interlaced-bff"
        else
            echo >&2 "ERROR: Content is interlaced, but can't determine field order."
            [ -n "$ILDETECT_FORCE_RUN" ] || exit 8
            echo >&2 "Assuming interlacing with default field order."
            verdict="interlaced"
        fi
    else
        # Likely progressive
        verdict="progressive"
    fi

    printf '%s\n' "$verdict"
}

judge "$@" --vf-clr
case "$verdict" in
    progressive)
        [ -n "$ILDETECT_DRY_RUN" ] || \
            [ -n "$ILDETECT_RUN_INTERLACED_ONLY" ] || \
            $ILDETECT_MPV "$@"
        r=$?
        [ $r -eq 0 ] || exit $((r | 16))
        exit 0
        ;;
    interlaced-tff)
        judge "$@" --vf-clr --vf-pre=pullup --field-dominance=top
        case "$verdict" in
            progressive)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" --vf-pre=pullup --field-dominance=top
                r=$?
                [ $r -eq 0 ] || exit $((r | 16))
                exit 1
                ;;
            *)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" --vf-pre=yadif --field-dominance=top
                r=$?
                [ $r -eq 0 ] || exit $((r | 16))
                exit 2
                ;;
        esac
        ;;
    interlaced-bff)
        judge "$@" --vf-clr --vf-pre=pullup --field-dominance=bottom
        case "$verdict" in
            progressive)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" --vf-pre=pullup --field-dominance=bottom
                r=$?
                [ $r -eq 0 ] || exit $((r | 16))
                exit 1
                ;;
            *)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" --vf-pre=yadif --field-dominance=bottom
                r=$?
                [ $r -eq 0 ] || exit $((r | 16))
                exit 2
                ;;
        esac
        ;;
    interlaced)
        judge "$@" --vf-clr --vf-pre=pullup
        case "$verdict" in
            progressive)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" --vf-pre=pullup
                r=$?
                [ $r -eq 0 ] || exit $((r | 16))
                exit 1
                ;;
            *)
                [ -n "$ILDETECT_DRY_RUN" ] || \
                    $ILDETECT_MPV "$@" --vf-pre=yadif
                r=$?
                [ $r -eq 0 ] || exit $((r | 16))
                exit 2
                ;;
        esac
        ;;
    *)
        echo >&2 "ERROR: Internal error."
        exit 16
        ;;
esac
