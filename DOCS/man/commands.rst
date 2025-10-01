COMMANDS
========

This script allows running and completing input commands in the console
interactively, and also adds mpv's log to the console's log.

Keybindings
-----------

\`
    Open the console to enter commands.

Commands
--------

``script-binding commands/open``

    Open the console to enter commands.

``script-message-to commands type <text> [<cursor_pos>]``
    Show the console and pre-fill it with the provided text, optionally
    specifying the initial cursor position as a positive integer starting from
    1. The console is automatically closed after running the command.

    .. admonition:: Examples for input.conf

        ``% script-message-to commands type "seek  absolute-percent" 6``
            Enter a percent position to seek to.

        ``Ctrl+o script-message-to console type "loadfile ''" 11``
            Enter a file or URL to play, with autocompletion of paths in the
            filesystem.

Configuration
-------------

This script can be customized through a config file ``script-opts/commands.conf``
placed in mpv's user directory and through the ``--script-opts`` command-line
option. The configuration syntax is described in `mp.options functions`_.

Configurable Options
~~~~~~~~~~~~~~~~~~~~

``persist_history``
    Default: no

    Whether to save the command history to a file and load it.

``history_path``
    Default: ``~~state/command_history.txt``

    The file path for ``persist_history`` (see `FILES`_).

``remember_input``
    Default: yes

    Whether to remember the input line and cursor position when closing the
    console, and prefill it the next time it is opened.
