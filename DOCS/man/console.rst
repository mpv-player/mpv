CONSOLE
=======

This script provides the ability to process the user's textual input to other
scripts through the ``mp.input`` API. It also has a builtin mode of operation to
complete and run mpv input commands and print mpv's log. It can be displayed on
both the video window and the terminal. It can be disabled entirely using the
``--load-console=no`` option.

Keybindings
-----------

\`
    Show the console.

ESC and Ctrl+[
    Hide the console.

ENTER, Ctrl+j and Ctrl+m
    Select the first completion if one wasn't already manually selected, and run
    the typed command.

Shift+ENTER
    Type a literal newline character.

LEFT and Ctrl+b
    Move the cursor to the previous character.

RIGHT and Ctrl+f
    Move the cursor to the next character.

Ctrl+LEFT and Alt+b
    Move the cursor to the beginning of the current word, or if between words,
    to the beginning of the previous word.

Ctrl+RIGHT and Alt+f
    Move the cursor to the end of the current word, or if between words, to the
    end of the next word.

HOME and Ctrl+a
    Move the cursor to the start of the current line.

END and Ctrl+e
    Move the cursor to the end of the current line.

BACKSPACE and Ctrl+h
    Delete the previous character.

Ctrl+d
    Hide the console if the current line is empty, otherwise delete the next
    character.

Ctrl+BACKSPACE and Ctrl+w
    Delete text from the cursor to the beginning of the current word, or if
    between words, to the beginning of the previous word.

Ctrl+DEL and Alt+d
    Delete text from the cursor to the end of the current word, or if between
    words, to the end of the next word.

Ctrl+u
    Delete text from the cursor to the beginning of the current line.

Ctrl+k
    Delete text from the cursor to the end of the current line.

Ctrl+c
    Clear the current line.

UP and Ctrl+p
    Move back in the command history.

DOWN and Ctrl+n
    Move forward in the command history.

PGUP
    Go to the first command in the history.

PGDN
    Stop navigating the command history.

Ctrl+r
    Search the command history.

INSERT
    Toggle insert mode.

Ctrl+v
    Paste text (uses the clipboard on X11 and Wayland).

Shift+INSERT
    Paste text (uses the primary selection on X11 and Wayland).

TAB and Ctrl+i
    Cycle through completions.

Shift+TAB
    Cycle through the completions backwards.

Ctrl+l
    Clear all log messages from the console.

MBTN_MID
    Paste text (uses the primary selection on X11 and Wayland).

WHEEL_UP
    Move back in the command history.

WHEEL_DOWN
    Move forward in the command history.

Commands
--------

``script-message-to console type <text> [<cursor_pos>]``
    Show the console and pre-fill it with the provided text, optionally
    specifying the initial cursor position as a positive integer starting from
    1.

    .. admonition:: Examples for input.conf

        ``% script-message-to console type "seek  absolute-percent; keypress ESC" 6``
            Enter a percent position to seek to and close the console.

        ``Ctrl+o script-message-to console type "loadfile ''; keypress ESC" 11``
            Enter a file or URL to play, with autocompletion of paths in the
            filesystem.

Known issues
------------

- Non-ASCII keyboard input has restrictions
- The cursor keys move between Unicode code-points, not grapheme clusters

Configuration
-------------

This script can be customized through a config file ``script-opts/console.conf``
placed in mpv's user directory and through the ``--script-opts`` command-line
option. The configuration syntax is described in `mp.options functions`_.

Key bindings can be changed in a standard way, see for example stats.lua
documentation.

Configurable Options
~~~~~~~~~~~~~~~~~~~~

``font``
    Default: a monospace font depending on the platform

    Set the font used for the console.
    A monospaced font is necessary to align completions correctly in a grid.
    If the console was opened by calling ``mp.input.select`` and no font was
    configured, ``--osd-font`` is used, as alignment is not necessary in that
    case.

``font_size``
    Default: 24

    Set the font size used for the REPL and the console. This will be
    multiplied by ``display-hidpi-scale`` when the console is not scaled with
    the window.

``border_size``
    Default: 1.65

    Set the font border size used for the REPL and the console.

``background_alpha``
    Default: 20

    The transparency of the menu's background. Ranges from 0 (opaque) to 255
    (fully transparent).

``padding``
    Default: 10

    The padding of the menu.

``menu_outline_size``
    Default: 0

    The size of the menu's border.

``menu_outline_color``
    Default: #FFFFFF

    The color of the menu's border.

``corner_radius``
    Default: 8

    The radius of the menu's corners.

``margin_x``
    Default: same as ``--osd-margin-x``

    The margin from the left of the window.

``margin_y``
    Default: same as ``--osd-margin-y``

    The margin from the bottom of the window.

``scale_with_window``
    Default: ``auto``

    Whether to scale the console with the window height. Can be ``yes``, ``no``,
    or ``auto``, which follows the value of ``--osd-scale-by-window``.

``selected_color``
    Default: ``#222222``

    The color of the selected item.

``selected_back_color``
    Default: ``#FFFFFF``

    The background color of the selected item.

``case_sensitive``
    Default: no on Windows, yes on other platforms.

    Whether autocompletion is case sensitive. Only works with ASCII characters.

``history_dedup``
    Default: true

    Remove duplicate entries in history as to only keep the latest one.

``font_hw_ratio``
    Default: auto

    The ratio of font height to font width.
    Adjusts grid width of completions.
    Values in the range 1.8..2.5 make sense for common monospace fonts.
