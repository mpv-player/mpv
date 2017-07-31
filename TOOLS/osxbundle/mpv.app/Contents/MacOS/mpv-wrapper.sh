#!/bin/sh
export MPVBUNDLE="true"

# set the right args for the user specified standard shell
# to load the expected profiles and configs
args="-c"
case "$SHELL" in
    *bash) args="-l $args";;
    *zsh) args="-l -i $args";;
esac

cd "$(dirname "$0")"
$SHELL $args "./mpv --player-operation-mode=pseudo-gui"
