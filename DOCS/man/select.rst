SELECT
======

console can present a list of items to browse and select from with the
``mp.input.select`` API. ``select.lua`` is a builtin client of this API
providing script bindings that gather and format the data to be selected in the
console and do operations on the selected item. It can be disabled using the
``--load-select=no`` option.

Key bindings
------------

When using ``mp.input.select``, the key bindings listed in `CONSOLE`_ are
extended with the following:

ENTER, Ctrl+j and Ctrl+m
    Select the focused item.

UP and Ctrl+p
    Focus the item above, or the last one when the first item is selected.

DOWN and Ctrl+n
    Focus the item below, or the first one when the last item is selected.

PGUP and Ctrl+b
    Scroll up one page.

PGDN and Ctrl+f
    Scroll down one page.

Ctrl+y
    Copy the focused item to the clipboard.

MBTN_LEFT
    Select the item under the cursor, or close the console if clicking outside
    of the menu rectangle.

WHEEL_UP
    Scroll up.

WHEEL_DOWN
    Scroll down.

Typing printable characters does a fuzzy search of the presented items.

If the query starts with ``'``, only exact matches are filtered. You can also
specify multiple search terms delimited by spaces, and only items matching all
terms are filtered.

Script bindings
---------------

By default select.lua's script bindings are bound to key sequences starting with
``g`` listed in `Keyboard Control`_. The names of the script bindings listed
below can be used to bind them to different keys.

.. admonition:: Example to rebind playlist selection in input.conf

    Ctrl+p script-binding select/select-playlist

Available script bindings are:

``select-playlist``
    Select a playlist entry. ``--osd-playlist-entry`` determines how playlist
    entries are formatted.

``select-sid``
    Select a subtitle track, or disable the current one.

``select-secondary-sid``
    Select a secondary subtitle track, or disable the current one.

``select-aid``
    Select an audio track, or disable the current one.

``select-vid``
    Select a video track, or disable the current one.

``select-track``
    Select a track of any type, or disable a selected track.

``select-chapter``
    Select a chapter.

``select-edition``
    Select an MKV edition or DVD/Blu-ray title.

``select-subtitle-line``
    Select a subtitle line to seek to. This doesn't work with image subtitles.

    This currently requires ``ffmpeg`` in ``PATH``, or in the same folder as mpv
    on Windows.

``select-audio-device``
    Select an audio device.

``select-watch-history``
    Select a file from the watch history. Requires ``--save-watch-history``.

    If you don't already use ``--autocreate-playlist``, it is recommended to
    enable it with this script binding to populate the playlist with the other
    files in the entry's directory.

    .. admonition:: Example for input.conf to play files adjacent to the history entry

        g-h script-binding select/select-watch-history; no-osd set autocreate-playlist filter

``select-watch-later``
    Select a file from watch later config files (see `RESUMING PLAYBACK`_) to
    resume playing. Requires ``--write-filename-in-watch-later-config``. This
    doesn't work with ``--ignore-path-in-watch-later-config``.

    If you don't already use ``--autocreate-playlist``, it is recommended to
    enable it with this script binding to populate the playlist with the other
    files in the entry's directory.

    .. admonition:: Example for input.conf to play files adjacent to the watch later entry

        g-w script-binding select/select-watch-later; no-osd set autocreate-playlist filter

``select-binding``
    List the defined input bindings. You can also select one to run the
    associated command.

``show-properties``
    List the names and values of all properties. You can also select one to
    print its value on the OSD, which is useful for long values that get
    clipped.

``menu``
    Show a menu with miscellaneous entries.

Configuration
-------------

This script can be customized through a config file ``script-opts/select.conf``
placed in mpv's user directory and through the ``--script-opts`` command-line
option. The configuration syntax is described in `mp.options functions`_.

Configurable options
~~~~~~~~~~~~~~~~~~~~

``history_date_format``
    Default: %Y-%m-%d %H:%M:%S

    The format of dates of history entries. This is passed to Lua's ``os.date``,
    which uses the same formats as ``strftime(3)``.

``hide_history_duplicates``
    Default: yes

    Whether to show only the last of history entries with the same path.
