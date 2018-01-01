#!/bin/sh

export LC_ALL=C

version_h="version.h"
print=yes

for ac_option do
  ac_arg=$(echo $ac_option | cut -d '=' -f 2-)
  case "$ac_option" in
  --extra=*)
    extra="-$ac_arg"
    ;;
  --versionh=*)
    version_h="$(pwd)/$ac_arg"
    print=no
    ;;
  --cwd=*)
    cwd="$ac_arg"
    ;;
  *)
    echo "Unknown parameter: $ac_option" >&2
    exit 1
    ;;

  esac
done

if test "$cwd" ; then
  cd "$cwd"
fi

# Extract revision number from file used by daily tarball snapshots
# or from "git describe" output
git_revision=$(cat snapshot_version 2> /dev/null)
test "$git_revision" || test ! -e .git || git_revision="$(git describe \
    --match "v[0-9]*" --always --tags --dirty | sed 's/^v//')"
version="$git_revision"

# other tarballs extract the version number from the VERSION file
if test ! "$version"; then
    version="$(cat VERSION 2> /dev/null)"
fi

test "$version" || version=UNKNOWN

VERSION="${version}${extra}"

if test "$print" = yes ; then
    echo "$VERSION"
    exit 0
fi

NEW_REVISION="#define VERSION \"${VERSION}\""
OLD_REVISION=$(head -n 1 "$version_h" 2> /dev/null)
BUILDDATE="#define BUILDDATE \"$(date)\""
MPVCOPYRIGHT="#define MPVCOPYRIGHT \"Copyright © 2000-2018 mpv/MPlayer/mplayer2 projects\""

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    cat <<EOF > "$version_h"
$NEW_REVISION
$BUILDDATE
$MPVCOPYRIGHT
EOF
fi
