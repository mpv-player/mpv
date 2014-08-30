#!/bin/sh

# file identification script
#
# manual usage:
#   mpv_identify.sh foo.mkv
#
# sh/dash/ksh/bash usage:
#   . mpv_identify.sh FOO_ foo.mkv
# will fill properties into variables like FOO_length
#
# zsh usage:
#   mpv_identify() { emulate -L sh; . mpv_identify.sh "$@"; }
#   mpv_identify FOO_ foo.mkv
# will fill properties into variables like FOO_length
#
# When multiple files were specified, their info will be put into FOO_* for the
# first file, FOO_1_* for the second file, FOO_2_* for the third file, etc.

__midentify__main() {

    case "$0" in
        mpv_identify.sh|*/mpv_identify.sh)
            # we are NOT being sourced
            case "$1" in
                '')
                    ;;
                *)
                    set -- '' "$@"
                    ;;
            esac
            ;;
    esac

    if [ $# -lt 2 ]; then
        echo >&2 "Usage 1 (for humans only): $0 filename.mkv"
        echo >&2 "will print all property values."
        echo >&2 "Note that this output really shouldn't be parsed, as the"
        echo >&2 "format is subject to change."
        echo >&2
        echo >&2 "Usage 2 (for use by scripts): see top of this file"
        echo >&2
        echo >&2 "NOTE: for mkv with ordered chapters, this may"
        echo >&2 "not always identify the specified file, but the"
        echo >&2 "file providing the first chapter. Specify"
        echo >&2 "--no-ordered-chapters to prevent this."
        return 1
    fi

    if [ -z "$MPV" ]; then
        local MPV="mpv"
    fi

    local LF="
"

    local nextprefix=$1
    shift

    if [ -n "$nextprefix" ]; then
        # in case of error, we always want this unset
        eval unset $nextprefix'path'
    fi

    local allprops="
        filename
        path
        stream-start
        stream-end
        stream-length

        demuxer

        length
        chapters
        editions
        titles

        audio
        audio-bitrate
        audio-codec
        audio-format
        channels
        samplerate

        video
        angle
        video-bitrate
        video-codec
        video-format
        video-aspect
        fps
        width
        height
        dwidth
        dheight

        sub
    "
    # TODO add metadata support once mpv can do it

    local propstr="X-MIDENTIFY-START:$LF"
    local key
    for key in $allprops; do
        propstr=$propstr"X-MIDENTIFY: $key \${=$key}$LF"
        key=`echo "$key" | tr - _`
        eval unset $nextprefix$key
    done

    local output=`$MPV --term-playing-msg="$propstr" --vo=null --ao=null --frames=1 --quiet --no-cache --no-config "$@"`
    local fileindex=0
    local prefix=
    while :; do
        local line output
        case "$output" in
            '')
                break
                ;;
            *$LF*)
                line=${output%%$LF*}
                output=${output#*$LF}
                ;;
            *)
                line=$output
                output=
                ;;
        esac
        case "$line" in
            X-MIDENTIFY-START:)
                if [ -n "$nextprefix" ]; then
                    prefix=$nextprefix
                    if [ $fileindex -gt 0 ]; then
                        nextprefix=${prefix%$fileindex\_}
                    fi
                    fileindex=$(($fileindex+1))
                    nextprefix=$nextprefix$fileindex\_
                    for key in $allprops; do
                        key=`echo "$key" | tr - _`
                        eval unset $nextprefix$key
                    done
                else
                    if [ $fileindex -gt 0 ]; then
                        echo
                    fi
                    fileindex=$(($fileindex+1))
                fi
                ;;
            X-MIDENTIFY:\ *)
                key=${line#X-MIDENTIFY:\ }
                local value=${key#* }
                key=${key%% *}
                key=`echo "$key" | tr - _`
                if [ -n "$nextprefix" ]; then
                    if [ -z "$prefix" ]; then
                        echo >&2 "Got X-MIDENTIFY: without X-MIDENTIFY-START:"
                    elif [ -n "$value" ]; then
                        eval $prefix$key=\$value
                    fi
                else
                    if [ -n "$value" ]; then
                        echo "$key=$value"
                    fi
                fi
                ;;
        esac
    done
}

__midentify__main "$@"
