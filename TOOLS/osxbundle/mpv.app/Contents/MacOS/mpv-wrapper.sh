#!/bin/bash -l
export MPVBUNDLE="true"
$SHELL -c "$(dirname "$0")/mpv --player-operation-mode=pseudo-gui"
