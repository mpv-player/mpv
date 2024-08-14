CONSOLE
=======

The console is a REPL for mpv input commands. It is displayed on the video
window. It also shows log messages. It can be disabled entirely using the
``--load-osd-console=no`` option.

Keybindings
-----------

\`
    Show the console.

ESC and Ctrl+[
    Hide the console.

ENTER, Ctrl+j and Ctrl+m
    Run the typed command.

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
    Complete the text at the cursor. The first press inserts the longest common
    prefix of the completions, and subsequent presses cycle through them.

Shift+TAB
    Cycle through the completions backwards.

Ctrl+l
    Clear all log messages from the console.

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
            Enter a file or URL to play. Tab completes paths in the filesystem.

Known issues
------------

- Pasting text is slow on Windows
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
    Default: unset (picks a hardcoded font depending on detected platform)

    Set the font used for the REPL and the console.
    This has to be a monospaced font for the completion suggestions to be
    aligned correctly.

``font_size``
    Default: 16

    Set the font size used for the REPL and the console. This will be
    multiplied by ``display-hidpi-scale``.

``border_size``
    Default: 1

    Set the font border size used for the REPL and the console.

``case_sensitive``
    Default: no on Windows, yes on other platforms.

    Whether Tab completion is case sensitive. Only works with ASCII characters.

``history_dedup``
    Default: true

    Remove duplicate entries in history as to only keep the latest one.

``font_hw_ratio``
    Default: auto

    The ratio of font height to font width.
    Adjusts table width of completion suggestions.
    Values in the range 1.8..2.5 make sense for common monospace fonts.
