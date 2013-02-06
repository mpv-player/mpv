#!/bin/sh

export LC_ALL=C

test "$1" && extra="-$1"

# Extract revision number from file used by daily tarball snapshots
# or from "git describe" output
git_revision=$(cat snapshot_version 2> /dev/null)
test $git_revision || test ! -e .git || git_revision=`git rev-parse --short HEAD`
git_revision=$(expr "$git_revision" : v*'\(.*\)')
test $git_revision || git_revision=UNKNOWN

# releases extract the version number from the VERSION file
version=$(cat VERSION 2> /dev/null)
test $version || version=git-$git_revision

NEW_REVISION="#define VERSION \"${version}${extra}\""
OLD_REVISION=$(head -n 1 version.h 2> /dev/null)
BUILDDATE="#define BUILDDATE \"$(date)\""

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    cat <<EOF > version.h
$NEW_REVISION
$BUILDDATE
EOF
fi
