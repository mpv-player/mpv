mpv lua scripts
===============

The lua scripts in this folder can be loaded on a one-time basis by
adding the option

    --lua=/path/to/script.lua

to mpv's command line.

Unless otherwise specified, they are also suitable for inclusion in
the `~/.mpv/lua` directory where they will be loaded every time mpv
starts, obviating the need to load them with the above `--lua=...`
argument. This is acceptable as they do only basic argument parsing
and key-binding registration, until those bound keys are actually
pressed.  They should therefore not interfere with normal playback
(unless you have a conflicting user-defined key-binding, in which
case, you may want to modify either the `mp.add_key_binding()` calls
in the scripts, or your keybinding).
