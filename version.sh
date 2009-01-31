#!/bin/sh

test "$1" && extra="-$1"

svn_revision=`LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2`
test $svn_revision || svn_revision=`grep revision .svn/entries 2>/dev/null | cut -d '"' -f2`
test $svn_revision || svn_revision=`sed -n -e '/^dir$/{n;p;q;}' .svn/entries 2>/dev/null`
test $svn_revision || svn_revision=UNKNOWN

NEW_REVISION="#define VERSION \"SVN-r${svn_revision}${extra}\""
OLD_REVISION=`cat version.h 2> /dev/null`
TITLE='#define MP_TITLE "%s "VERSION" (C) 2000-2009 MPlayer Team\n"'

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    cat <<EOF > version.h
$NEW_REVISION
$TITLE
EOF
fi
