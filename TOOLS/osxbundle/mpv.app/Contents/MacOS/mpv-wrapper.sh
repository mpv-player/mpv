#!/bin/bash -l
export MPVBUNDLE="true"
cd "$(dirname "$0")"
$SHELL -c "./mpv --player-operation-mode=pseudo-gui"
