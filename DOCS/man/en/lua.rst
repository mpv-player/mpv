LUA SCRIPTING
=============

mpv can load Lua scripts. These scripts can be used to control mpv in a similar
way to slave mode. mpv provides the builtin module ``mp`` (can be loaded
with ``require 'mp'``), which provides functions to send commands to the
mpv core and to retrieve information about playback state, user settings,
file information, and so on.

.. admonition:: Warning

    Lua scripting is work in progress, and it's in a very early stage. When
    writing scripts, rely only on the features and functions documented here.
    Everything else is subject to change.

.. admonition:: Warning

    Nothing is finished and documented yet.
