#!/bin/bash
export MPVBUNDLE="true"
$SHELL -l -c "$(dirname "$0")/mpv --player-operation-mode=pseudo-gui"
