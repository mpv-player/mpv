#!/bin/sh
# Create the messages header file from the master source file or a translation.
# Missing messages are filled in from the master message file and, if
# requested, character set conversion is performed.

if test -z $2 ; then
    echo "Error: missing parameters"
    echo "Usage: $0 <messages file> <character set>"
    exit 1
fi

MASTER=help/help_mp-en.h
TARGET=help_mp.h

TRANSLATION=$1
CHARSET=$2

missing_messages(){
curr=""

while read -r line; do
    if echo "$line" | grep -q '^#define' ; then
        curr=`printf "%s\n" "$line" | cut -d ' ' -f 2`
        if grep -q "^#define $curr[	 ]" "$TRANSLATION" ; then
            curr=""
        fi
    else
        if [ -z "$line" ]; then
            curr=""
        fi
    fi

    if [ -n "$curr" ]; then
        printf "%s\n" "$line"
    fi
done
}

cat <<EOF > "$TARGET"
/* WARNING! This is a generated file, do NOT edit.
 * See the help/ subdirectory for the editable files. */

#ifndef MPLAYER_HELP_MP_H
#define MPLAYER_HELP_MP_H

EOF

cat "$TRANSLATION" >> "$TARGET"

cat <<EOF >> "$TARGET"

/* untranslated messages from the English master file */

EOF

if test "$MASTER" != "$TRANSLATION" ; then
    missing_messages < "$MASTER" >> "$TARGET"
fi

cat <<EOF >> "$TARGET"

#endif /* MPLAYER_HELP_MP_H */
EOF

if test $CHARSET != UTF-8 ; then
    iconv -f UTF-8 -t "$CHARSET" "$TARGET" > "${TARGET}.tmp"
    mv "${TARGET}.tmp" "$TARGET"
fi
