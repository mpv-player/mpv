#!/bin/sh

test "$1" && extra="-$1"

# Extract revision number from file used by daily tarball snapshots
# or from "git describe" output
git_revision=$(cat snapshot_version 2> /dev/null)
test $git_revision || test ! -d .git || git_revision=`git describe --match "v[0-9]*" --always`
git_revision=$(expr "$git_revision" : v*'\(.*\)')
test $git_revision || git_revision=UNKNOWN

# releases extract the version number from the VERSION file
version=$(cat VERSION 2> /dev/null)
test $version || version=$git_revision

NEW_REVISION="#define VERSION \"${version}${extra}\""
OLD_REVISION=$(head -n 1 version.h 2> /dev/null)
TITLE='#define MP_TITLE "%s "VERSION" (C) 2000-2012 MPlayer Team\n"'

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    cat <<EOF > version.h
$NEW_REVISION
$TITLE
EOF
fi
