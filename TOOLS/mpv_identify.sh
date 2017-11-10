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
            [ -n "$1" ] && set -- '' "$@"
            ;;
    esac

    if [ "$#" -lt 2 ]; then
        cat >&2 <<EOF
Usage 1 (for humans only): $0 filename.mkv
will print all property values.
Note that this output really shouldn't be parsed, as the
format is subject to change.

Usage 2 (for use by scripts): see top of this file

NOTE: for mkv with ordered chapters, this may
not always identify the specified file, but the
file providing the first chapter. Specify
--no-ordered-chapters to prevent this.
EOF
        return 2
    fi

    local LF="
"

    local nextprefix="$1"
    shift

    if [ -n "$nextprefix" ]; then
        # in case of error, we always want this unset
        unset "${nextprefix}path"
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
        duration

        audio
        audio-bitrate
        audio-codec
        audio-codec-name

        video
        angle
        video-bitrate
        video-codec
        video-format
        video-aspect
        container-fps
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
        propstr="${propstr}X-MIDENTIFY: $key \${=$key}$LF"
        key="$(printf '%s\n' "$key" | tr - _)"
        unset "$nextprefix$key"
    done

    local fileindex=0
    local prefix=
    local line
    while IFS= read -r line; do
        case "$line" in
            X-MIDENTIFY-START:)
                if [ -n "$nextprefix" ]; then
                    prefix="$nextprefix"
                    if [ "$fileindex" -gt 0 ]; then
                        nextprefix="${prefix%${fileindex}_}"
                    fi
                    fileindex="$((fileindex+1))"
                    nextprefix="${nextprefix}${fileindex}_"
                    for key in $allprops; do
                        key="$(printf '%s\n' "$key" | tr - _)"
                        unset "$nextprefix$key"
                    done
                else
                    if [ "$fileindex" -gt 0 ]; then
                        printf '\n'
                    fi
                    fileindex="$((fileindex+1))"
                fi
                ;;
            X-MIDENTIFY:\ *)
                local key="${line#X-MIDENTIFY: }"
                local value="${key#* }"
                key="${key%% *}"
                key="$(printf '%s\n' "$key" | tr - _)"
                if [ -n "$nextprefix" ]; then
                    if [ -z "$prefix" ]; then
                        echo >&2 "Got X-MIDENTIFY: without X-MIDENTIFY-START:"
                    elif [ -n "$value" ]; then
                        eval "$prefix$key"='"$value"'
                    fi
                else
                    if [ -n "$value" ]; then
                        printf '%s=%s\n' "$key" "$value"
                    fi
                fi
                ;;
        esac
    done <<EOF
$(${MPV:-mpv} --term-playing-msg="$propstr" --vo=null --ao=null \
              --frames=1 --quiet --no-cache --no-config -- "$@")
EOF
}

__midentify__main "$@"
