#!/bin/sh

export LC_ALL=C

for ac_option do
  case "$ac_option" in
  --extra=*)
    extra="-$option"
    ;;
  --print)
    print=yes
    ;;
  *)
    echo "Unknown parameter: $option" >&2
    exit 1
    ;;

  esac
done

# Extract revision number from file used by daily tarball snapshots
# or from "git describe" output
git_revision=$(cat snapshot_version 2> /dev/null)
test "$git_revision" || test ! -e .git || git_revision="$(git rev-parse --short HEAD)"
test "$git_revision" && git_revision="git-$git_revision"
version="$git_revision"

# releases extract the version number from the VERSION file
releaseversion="$(cat VERSION 2> /dev/null)"
if test "$releaseversion" ; then
    test "$version" && version="-$version"
    version="$releaseversion$version"
fi

test "$version" || version=UNKNOWN

VERSION="${version}${extra}"

if test "$print" = yes ; then
    echo "$VERSION"
    exit 0
fi

NEW_REVISION="#define VERSION \"${VERSION}\""
OLD_REVISION=$(head -n 1 version.h 2> /dev/null)
BUILDDATE="#define BUILDDATE \"$(date)\""

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    cat <<EOF > version.h
$NEW_REVISION
$BUILDDATE
EOF
fi
