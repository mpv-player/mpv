#!/bin/bash
export MPVBUNDLE="true"
exec "$(dirname $0)"/mpv --player-operation-mode=pseudo-gui
