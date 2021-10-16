COMMAND INTERFACE
=================

The mpv core can be controlled with commands and properties. A number of ways
to interact with the player use them: key bindings (``input.conf``), OSD
(showing information with properties), JSON IPC, the client API (``libmpv``),
and the classic slave mode.

input.conf
----------

The input.conf file consists of a list of key bindings, for example::

    s screenshot      # take a screenshot with the s key
    LEFT seek 15      # map the left-arrow key to seeking forward by 15 seconds

Each line maps a key to an input command. Keys are specified with their literal
value (upper case if combined with ``Shift``), or a name for special keys. For
example, ``a`` maps to the ``a`` key without shift, and ``A`` maps to ``a``
with shift.

The file is located in the mpv configuration directory (normally at
``~/.config/mpv/input.conf`` depending on platform). The default bindings are
defined here::

    https://github.com/mpv-player/mpv/blob/master/etc/input.conf

A list of special keys can be obtained with

    ``mpv --input-keylist``

In general, keys can be combined with ``Shift``, ``Ctrl`` and ``Alt``::

    ctrl+q quit

**mpv** can be started in input test mode, which displays key bindings and the
commands they're bound to on the OSD, instead of executing the commands::

    mpv --input-test --force-window --idle

(Only closing the window will make **mpv** exit, pressing normal keys will
merely display the binding, even if mapped to quit.)

Also see `Key names`_.

input.conf syntax
-----------------

``[Shift+][Ctrl+][Alt+][Meta+]<key> [{<section>}] <command> ( ; <command> )*``

Note that by default, the right Alt key can be used to create special
characters, and thus does not register as a modifier. The option
``--no-input-right-alt-gr`` changes this behavior.

Newlines always start a new binding. ``#`` starts a comment (outside of quoted
string arguments). To bind commands to the ``#`` key, ``SHARP`` can be used.

``<key>`` is either the literal character the key produces (ASCII or Unicode
character), or a symbolic name (as printed by ``--input-keylist``).

``<section>`` (braced with ``{`` and ``}``) is the input section for this
command.

``<command>`` is the command itself. It consists of the command name and
multiple (or none) arguments, all separated by whitespace. String arguments
should be quoted, typically with ``"``. See ``Flat command syntax``.

You can bind multiple commands to one key. For example:

| a show-text "command 1" ; show-text "command 2"

It's also possible to bind a command to a sequence of keys:

| a-b-c show-text "command run after a, b, c have been pressed"

(This is not shown in the general command syntax.)

If ``a`` or ``a-b`` or ``b`` are already bound, this will run the first command
that matches, and the multi-key command will never be called. Intermediate keys
can be remapped to ``ignore`` in order to avoid this issue. The maximum number
of (non-modifier) keys for combinations is currently 4.

Key names
---------

All mouse and keyboard input is to converted to mpv-specific key names. Key
names are either special symbolic identifiers representing a physical key, or a
text key names, which are unicode code points encoded as UTF-8. These are what
keyboard input would normally produce, for example ``a`` for the A key. As a
consequence, mpv uses input translated by the current OS keyboard layout, rather
than physical scan codes.

Currently there is the hardcoded assumption that every text key can be
represented as a single unicode code point (in NFKC form).

All key names can be combined with the modifiers ``Shift``, ``Ctrl``, ``Alt``,
``Meta``. They must be prefixed to the actual key name, where each modifier
is followed by a ``+`` (for example ``ctrl+q``).

The ``Shift`` modifier requires some attention. For instance ``Shift+2`` should
usually be specified as key-name ``@`` at ``input.conf``, and similarly the
combination ``Alt+Shift+2`` is usually ``Alt+@``, etc. Special key names like
``Shift+LEFT`` work as expected. If in doubt - use ``--input-test`` to check
how a key/combination is seen by mpv.

Symbolic key names and modifier names are case-insensitive. Unicode key names
are case-sensitive because input bindings typically respect the shift key.

Another type of key names are hexadecimal key names, that serve as fallback
for special keys that are neither unicode, nor have a special mpv defined name.
They will break as soon as mpv adds proper names for them, but can enable you
to use a key at all if that does not happen.

All symbolic names are listed by ``--input-keylist``. ``--input-test`` is a
special mode that prints all input on the OSD.

Comments on some symbolic names:

``KP*``
    Keypad names. Behavior varies by backend (whether they implement this, and
    on how they treat numlock), but typically, mpv tries to map keys on the
    keypad to separate names, even if they produce the same text as normal keys.

``MOUSE_BTN*``, ``MBTN*``
    Various mouse buttons.

    Depending on backend, the mouse wheel might also be represented as a button.
    In addition, ``MOUSE_BTN3`` to ``MOUSE_BTN6`` are deprecated aliases for
    ``WHEEL_UP``, ``WHEEL_DOWN``, ``WHEEL_LEFT``, ``WHEEL_RIGHT``.

    ``MBTN*`` are aliases for ``MOUSE_BTN*``.

``WHEEL_*``
    Mouse wheels (typically).

``AXIS_*``
    Deprecated aliases for ``WHEEL_*``.

``*_DBL``
    Mouse button double clicks.

``MOUSE_MOVE``, ``MOUSE_ENTER``, ``MOUSE_LEAVE``
    Emitted by mouse move events. Enter/leave happens when the mouse enters or
    leave the mpv window (or the current mouse region, using the deprecated
    mouse region input section mechanism).

``CLOSE_WIN``
    Pseudo key emitted when closing the mpv window using the OS window manager
    (for example, by clicking the close button in the window title bar).

``GAMEPAD_*``
    Keys emitted by the SDL gamepad backend.

``UNMAPPED``
    Pseudo-key that matches any unmapped key. (You should probably avoid this
    if possible, because it might change behavior or get removed in the future.)

``ANY_UNICODE``
    Pseudo-key that matches any key that produces text. (You should probably
    avoid this if possible, because it might change behavior or get removed in
    the future.)

Flat command syntax
-------------------

This is the syntax used in input.conf, and referred to "input.conf syntax" in
a number of other places.

|
| ``<command>  ::= [<prefixes>] <command_name> (<argument>)*``
| ``<argument> ::= (<unquoted> | " <double_quoted> " | ' <single_quoted> ' | `X <custom_quoted> X`)``

``command_name`` is an unquoted string with the command name itself. See
`List of Input Commands`_ for a list.

Arguments are separated by whitespaces even if the command expects only one
argument. Arguments with whitespaces or other special characters must be quoted,
or the command cannot be parsed correctly.

Double quotes interpret JSON/C-style escaping, like ``\t`` or ``\"`` or ``\\``.
JSON escapes according to RFC 8259, minus surrogate pair escapes. This is the
only form which allows newlines at the value - as ``\n``.

Single quotes take the content literally, and cannot include the single-quote
character at the value.

Custom quotes also take the content literally, but are more flexible than single
quotes. They start with ````` (back-quote) followed by any ASCII character,
and end at the first occurance of the same pair in reverse order, e.g.
```-foo-``` or ````bar````. The final pair sequence is not allowed at the
value - in these examples ``-``` and `````` respectively. In the second
example the last character of the value also can't be a back-quote.

Mixed quoting at the same argument, like ``'foo'"bar"``, is not supported.

Note that argument parsing and property expansion happen at different stages.
First, arguments are determined as described above, and then, where applicable,
properties are expanded - regardless of argument quoting. However, expansion
can still be prevented with the ``raw`` prefix or ``$>``. See `Input Command
Prefixes`_ and `Property Expansion`_.

Commands specified as arrays
----------------------------

This applies to certain APIs, such as ``mp.commandv()`` or
``mp.command_native()`` (with array parameters) in Lua scripting, or
``mpv_command()`` or ``mpv_command_node()`` (with MPV_FORMAT_NODE_ARRAY) in the
C libmpv client API.

The command as well as all arguments are passed as a single array. Similar to
the `Flat command syntax`_, you can first pass prefixes as strings (each as
separate array item), then the command name as string, and then each argument
as string or a native value.

Since these APIs pass arguments as separate strings or native values, they do
not expect quotes, and do support escaping. Technically, there is the input.conf
parser, which first splits the command string into arguments, and then invokes
argument parsers for each argument. The input.conf parser normally handles
quotes and escaping. The array command APIs mentioned above pass strings
directly to the argument parsers, or can sidestep them by the ability to pass
non-string values.

Property expansion is disabled by default for these APIs. This can be changed
with the ``expand-properties`` prefix. See `Input Command Prefixes`_.

Sometimes commands have string arguments, that in turn are actually parsed by
other components (e.g. filter strings with ``vf add``) - in these cases, you
you would have to double-escape in input.conf, but not with the array APIs.

For complex commands, consider using `Named arguments`_ instead, which should
give slightly more compatibility. Some commands do not support named arguments
and inherently take an array, though.

Named arguments
---------------

This applies to certain APIs, such as ``mp.command_native()`` (with tables that
have string keys) in Lua scripting, or ``mpv_command_node()`` (with
MPV_FORMAT_NODE_MAP) in the C libmpv client API.

The name of the command is provided with a ``name`` string field. The name of
each command is defined in each command description in the
`List of Input Commands`_. ``--input-cmdlist`` also lists them. See the
``subprocess`` command for an example.

Some commands do not support named arguments (e.g. ``run`` command). You need
to use APIs that pass arguments as arrays.

Named arguments are not supported in the "flat" input.conf syntax, which means
you cannot use them for key bindings in input.conf at all.

Property expansion is disabled by default for these APIs. This can be changed
with the ``expand-properties`` prefix. See `Input Command Prefixes`_.

List of Input Commands
----------------------

Commands with parameters have the parameter name enclosed in ``<`` / ``>``.
Don't add those to the actual command. Optional arguments are enclosed in
``[`` / ``]``. If you don't pass them, they will be set to a default value.

Remember to quote string arguments in input.conf (see `Flat command syntax`_).

``ignore``
    Use this to "block" keys that should be unbound, and do nothing. Useful for
    disabling default bindings, without disabling all bindings with
    ``--no-input-default-bindings``.

``seek <target> [<flags>]``
    Change the playback position. By default, seeks by a relative amount of
    seconds.

    The second argument consists of flags controlling the seek mode:

    relative (default)
        Seek relative to current position (a negative value seeks backwards).
    absolute
        Seek to a given time (a negative value starts from the end of the file).
    absolute-percent
        Seek to a given percent position.
    relative-percent
        Seek relative to current position in percent.
    keyframes
        Always restart playback at keyframe boundaries (fast).
    exact
        Always do exact/hr/precise seeks (slow).

    Multiple flags can be combined, e.g.: ``absolute+keyframes``.

    By default, ``keyframes`` is used for ``relative``, ``relative-percent``,
    and ``absolute-percent`` seeks, while ``exact`` is used for ``absolute``
    seeks.

    Before mpv 0.9, the ``keyframes`` and ``exact`` flags had to be passed as
    3rd parameter (essentially using a space instead of ``+``). The 3rd
    parameter is still parsed, but is considered deprecated.

``revert-seek [<flags>]``
    Undoes the ``seek`` command, and some other commands that seek (but not
    necessarily all of them). Calling this command once will jump to the
    playback position before the seek. Calling it a second time undoes the
    ``revert-seek`` command itself. This only works within a single file.

    The first argument is optional, and can change the behavior:

    mark
        Mark the current time position. The next normal ``revert-seek`` command
        will seek back to this point, no matter how many seeks happened since
        last time.
    mark-permanent
        If set, mark the current position, and do not change the mark position
        before the next ``revert-seek`` command that has ``mark`` or
        ``mark-permanent`` set (or playback of the current file ends). Until
        this happens, ``revert-seek`` will always seek to the marked point. This
        flag cannot be combined with ``mark``.

    Using it without any arguments gives you the default behavior.

``frame-step``
    Play one frame, then pause. Does nothing with audio-only playback.

``frame-back-step``
    Go back by one frame, then pause. Note that this can be very slow (it tries
    to be precise, not fast), and sometimes fails to behave as expected. How
    well this works depends on whether precise seeking works correctly (e.g.
    see the ``--hr-seek-demuxer-offset`` option). Video filters or other video
    post-processing that modifies timing of frames (e.g. deinterlacing) should
    usually work, but might make backstepping silently behave incorrectly in
    corner cases. Using ``--hr-seek-framedrop=no`` should help, although it
    might make precise seeking slower.

    This does not work with audio-only playback.

``set <name> <value>``
    Set the given property or option to the given value.

``add <name> [<value>]``
    Add the given value to the property or option. On overflow or underflow,
    clamp the property to the maximum. If ``<value>`` is omitted, assume ``1``.

``cycle <name> [<value>]``
    Cycle the given property or option. The second argument can be ``up`` or
    ``down`` to set the cycle direction. On overflow, set the property back to
    the minimum, on underflow set it to the maximum. If ``up`` or ``down`` is
    omitted, assume ``up``.

    Whether or not key-repeat is enabled by default depends on the property.
    Currently properties with continuous values are repeatable by default (like
    ``volume``), while discrete values are not (like ``osd-level``).

``multiply <name> <value>``
    Similar to ``add``, but multiplies the property or option with the numeric
    value.

``screenshot <flags>``
    Take a screenshot.

    Multiple flags are available (some can be combined with ``+``):

    <subtitles> (default)
        Save the video image, in its original resolution, and with subtitles.
        Some video outputs may still include the OSD in the output under certain
        circumstances.
    <video>
        Like ``subtitles``, but typically without OSD or subtitles. The exact
        behavior depends on the selected video output.
    <window>
        Save the contents of the mpv window. Typically scaled, with OSD and
        subtitles. The exact behavior depends on the selected video output, and
        if no support is available, this will act like ``video``.
    <each-frame>
        Take a screenshot each frame. Issue this command again to stop taking
        screenshots. Note that you should disable frame-dropping when using
        this mode - or you might receive duplicate images in cases when a
        frame was dropped. This flag can be combined with the other flags,
        e.g. ``video+each-frame``.

    Older mpv versions required passing ``single`` and ``each-frame`` as
    second argument (and did not have flags). This syntax is still understood,
    but deprecated and might be removed in the future.

    If you combine this command with another one using ``;``, you can use the
    ``async`` flag to make encoding/writing the image file asynchronous. For
    normal standalone commands, this is always asynchronous, and the flag has
    no effect. (This behavior changed with mpv 0.29.0.)

``screenshot-to-file <filename> <flags>``
    Take a screenshot and save it to a given file. The format of the file will
    be guessed by the extension (and ``--screenshot-format`` is ignored - the
    behavior when the extension is missing or unknown is arbitrary).

    The second argument is like the first argument to ``screenshot`` and
    supports ``subtitles``, ``video``, ``window``.

    If the file already exists, it's overwritten.

    Like all input command parameters, the filename is subject to property
    expansion as described in `Property Expansion`_.

``playlist-next <flags>``
    Go to the next entry on the playlist.

    First argument:

    weak (default)
        If the last file on the playlist is currently played, do nothing.
    force
        Terminate playback if there are no more files on the playlist.

``playlist-prev <flags>``
    Go to the previous entry on the playlist.

    First argument:

    weak (default)
        If the first file on the playlist is currently played, do nothing.
    force
        Terminate playback if the first file is being played.

``playlist-play-index <integer|current|none>``
    Start (or restart) playback of the given playlist index. In addition to the
    0-based playlist entry index, it supports the following values:

    <current>
        The current playlist entry (as in ``playlist-current-pos``) will be
        played again (unload and reload). If none is set, playback is stopped.
        (In corner cases, ``playlist-current-pos`` can point to a playlist entry
        even if playback is currently inactive,

    <none>
        Playback is stopped. If idle mode (``--idle``) is enabled, the player
        will enter idle mode, otherwise it will exit.

    This comm and is similar to ``loadfile`` in that it only manipulates the
    state of what to play next, without waiting until the current file is
    unloaded, and the next one is loaded.

    Setting ``playlist-pos`` or similar properties can have a similar effect to
    this command. However, it's more explicit, and guarantees that playback is
    restarted if for example the new playlist entry is the same as the previous
    one.

``loadfile <url> [<flags> [<options>]]``
    Load the given file or URL and play it. Technically, this is just a playlist
    manipulation command (which either replaces the playlist or appends an entry
    to it). Actual file loading happens independently. For example, a
    ``loadfile`` command that replaces the current file with a new one returns
    before the current file is stopped, and the new file even begins loading.

    Second argument:

    <replace> (default)
        Stop playback of the current file, and play the new file immediately.
    <append>
        Append the file to the playlist.
    <append-play>
        Append the file, and if nothing is currently playing, start playback.
        (Always starts with the added file, even if the playlist was not empty
        before running this command.)

    The third argument is a list of options and values which should be set
    while the file is playing. It is of the form ``opt1=value1,opt2=value2,..``.
    When using the client API, this can be a ``MPV_FORMAT_NODE_MAP`` (or a Lua
    table), however the values themselves must be strings currently. These
    options are set during playback, and restored to the previous value at end
    of playback (see `Per-File Options`_).

``loadlist <url> [<flags>]``
    Load the given playlist file or URL (like ``--playlist``).

    Second argument:

    <replace> (default)
        Stop playback and replace the internal playlist with the new one.
    <append>
        Append the new playlist at the end of the current internal playlist.
    <append-play>
        Append the new playlist, and if nothing is currently playing, start
        playback. (Always starts with the new playlist, even if the internal
        playlist was not empty before running this command.)

``playlist-clear``
    Clear the playlist, except the currently played file.

``playlist-remove <index>``
    Remove the playlist entry at the given index. Index values start counting
    with 0. The special value ``current`` removes the current entry. Note that
    removing the current entry also stops playback and starts playing the next
    entry.

``playlist-move <index1> <index2>``
    Move the playlist entry at index1, so that it takes the place of the
    entry index2. (Paradoxically, the moved playlist entry will not have
    the index value index2 after moving if index1 was lower than index2,
    because index2 refers to the target entry, not the index the entry
    will have after moving.)

``playlist-shuffle``
    Shuffle the playlist. This is similar to what is done on start if the
    ``--shuffle`` option is used.

``playlist-unshuffle``
    Attempt to revert the previous ``playlist-shuffle`` command. This works
    only once (multiple successive ``playlist-unshuffle`` commands do nothing).
    May not work correctly if new recursive playlists have been opened since
    a ``playlist-shuffle`` command.

``run <command> [<arg1> [<arg2> [...]]]``
    Run the given command. Unlike in MPlayer/mplayer2 and earlier versions of
    mpv (0.2.x and older), this doesn't call the shell. Instead, the command
    is run directly, with each argument passed separately. Each argument is
    expanded like in `Property Expansion`_.

    This command has a variable number of arguments, and cannot be used with
    named arguments.

    The program is run in a detached way. mpv doesn't wait until the command
    is completed, but continues playback right after spawning it.

    To get the old behavior, use ``/bin/sh`` and ``-c`` as the first two
    arguments.

    .. admonition:: Example

        ``run "/bin/sh" "-c" "echo ${title} > /tmp/playing"``

        This is not a particularly good example, because it doesn't handle
        escaping, and a specially prepared file might allow an attacker to
        execute arbitrary shell commands. It is recommended to write a small
        shell script, and call that with ``run``.

``subprocess``
    Similar to ``run``, but gives more control about process execution to the
    caller, and does does not detach the process.

    You can avoid blocking until the process terminates by running this command
    asynchronously. (For example ``mp.command_native_async()`` in Lua scripting.)

    This has the following named arguments. The order of them is not guaranteed,
    so you should always call them with named arguments, see `Named arguments`_.

    ``args`` (``MPV_FORMAT_NODE_ARRAY[MPV_FORMAT_STRING]``)
        Array of strings with the command as first argument, and subsequent
        command line arguments following. This is just like the ``run`` command
        argument list.

        The first array entry is either an absolute path to the executable, or
        a filename with no path components, in which case the executable is
        searched in the directories in the ``PATH`` environment variable. On
        Unix, this is equivalent to ``posix_spawnp`` and ``execvp`` behavior.

    ``playback_only`` (``MPV_FORMAT_FLAG``)
        Boolean indicating whether the process should be killed when playback
        terminates (optional, default: true). If enabled, stopping playback
        will automatically kill the process, and you can't start it outside of
        playback.

    ``capture_size`` (``MPV_FORMAT_INT64``)
        Integer setting the maximum number of stdout plus stderr bytes that can
        be captured (optional, default: 64MB). If the number of bytes exceeds
        this, capturing is stopped. The limit is per captured stream.

    ``capture_stdout`` (``MPV_FORMAT_FLAG``)
        Capture all data the process outputs to stdout and return it once the
        process ends (optional, default: no).

    ``capture_stderr`` (``MPV_FORMAT_FLAG``)
        Same as ``capture_stdout``, but for stderr.

    ``detach`` (``MPV_FORMAT_FLAG``)
        Whether to run the process in detached mode (optional, default: no). In
        this mode, the process is run in a new process session, and the command
        does not wait for the process to terminate. If neither
        ``capture_stdout`` nor ``capture_stderr`` have been set to true,
        the command returns immediately after the new process has been started,
        otherwise the command will read as long as the pipes are open.

    ``env`` (``MPV_FORMAT_NODE_ARRAY[MPV_FORMAT_STRING]``)
        Set a list of environment variables for the new process (default: empty).
        If an empty list is passed, the environment of the mpv process is used
        instead. (Unlike the underlying OS mechanisms, the mpv command cannot
        start a process with empty environment. Fortunately, that is completely
        useless.) The format of the list is as in the ``execle()`` syscall. Each
        string item defines an environment variable as in ``NANME=VALUE``.

        On Lua, you may use ``utils.get_env_list()`` to retrieve the current
        environment if you e.g. simply want to add a new variable.

    ``stdin_data`` (``MPV_FORMAT_STRING``)
        Feed the given string to the new process' stdin. Since this is a string,
        you cannot pass arbitrary binary data. If the process terminates or
        closes the pipe before all data is written, the remaining data is
        silently discarded. Probably does not work on win32.

    ``passthrough_stdin`` (``MPV_FORMAT_FLAG``)
        If enabled, wire the new process' stdin to mpv's stdin (default: no).
        Before mpv 0.33.0, this argument did not exist, but the behavior was as
        if this was set to true.

    The command returns the following result (as ``MPV_FORMAT_NODE_MAP``):

    ``status`` (``MPV_FORMAT_INT64``)
        The raw exit status of the process. It will be negative on error. The
        meaning of negative values is undefined, other than meaning error (and
        does not correspond to OS low level exit status values).

        On Windows, it can happen that a negative return value is returned
        even if the process exits gracefully, because the win32 ``UINT`` exit
        code is assigned to an ``int`` variable before being set as ``int64_t``
        field in the result map. This might be fixed later.

    ``stdout`` (``MPV_FORMAT_BYTE_ARRAY``)
        Captured stdout stream, limited to ``capture_size``.

    ``stderr`` (``MPV_FORMAT_BYTE_ARRAY``)
        Same as ``stdout``, but for stderr.

    ``error_string`` (``MPV_FORMAT_STRING``)
        Empty string if the process exited gracefully. The string ``killed`` if
        the process was terminated in an unusual way. The string ``init`` if the
        process could not be started.

        On Windows, ``killed`` is only returned when the process has been
        killed by mpv as a result of ``playback_only`` being set to true.

    ``killed_by_us`` (``MPV_FORMAT_FLAG``)
        Whether the process has been killed by mpv, for example as a result of
        ``playback_only`` being set to true, aborting the command (e.g. by
        ``mp.abort_async_command()``), or if the player is about to exit.

    Note that the command itself will always return success as long as the
    parameters are correct. Whether the process could be spawned or whether
    it was somehow killed or returned an error status has to be queried from
    the result value.

    This command can be asynchronously aborted via API.

    In all cases, the subprocess will be terminated on player exit. Also see
    `Asynchronous command details`_. Only the ``run`` command can start
    processes in a truly detached way.

    .. admonition:: Warning

        Don't forget to set the ``playback_only`` field if you want the command
        run while the player is in idle mode, or if you don't want that end of
        playback kills the command.

    .. admonition:: Example

        ::

            local r = mp.command_native({
                name = "subprocess",
                playback_only = false,
                capture_stdout = true,
                args = {"cat", "/proc/cpuinfo"},
            })
            if r.status == 0 then
                print("result: " .. r.stdout)
            end

        This is a fairly useless Lua example, which demonstrates how to run
        a process in a blocking manner, and retrieving its stdout output.

``quit [<code>]``
    Exit the player. If an argument is given, it's used as process exit code.

``quit-watch-later [<code>]``
    Exit player, and store current playback position. Playing that file later
    will seek to the previous position on start. The (optional) argument is
    exactly as in the ``quit`` command.

``sub-add <url> [<flags> [<title> [<lang>]]]``
    Load the given subtitle file or stream. By default, it is selected as
    current subtitle  after loading.

    The ``flags`` argument is one of the following values:

    <select>

        Select the subtitle immediately (default).

    <auto>

        Don't select the subtitle. (Or in some special situations, let the
        default stream selection mechanism decide.)

    <cached>

        Select the subtitle. If a subtitle with the same filename was already
        added, that one is selected, instead of loading a duplicate entry.
        (In this case, title/language are ignored, and if the was changed since
        it was loaded, these changes won't be reflected.)

    The ``title`` argument sets the track title in the UI.

    The ``lang`` argument sets the track language, and can also influence
    stream selection with ``flags`` set to ``auto``.

``sub-remove [<id>]``
    Remove the given subtitle track. If the ``id`` argument is missing, remove
    the current track. (Works on external subtitle files only.)

``sub-reload [<id>]``
    Reload the given subtitle tracks. If the ``id`` argument is missing, reload
    the current track. (Works on external subtitle files only.)

    This works by unloading and re-adding the subtitle track.

``sub-step <skip> <flags>``
    Change subtitle timing such, that the subtitle event after the next
    ``<skip>`` subtitle events is displayed. ``<skip>`` can be negative to step
    backwards.

    Secondary argument:

    primary (default)
        Steps through the primary subtitles.
    secondary
        Steps through the secondary subtitles.

``sub-seek <skip> <flags>``
    Seek to the next (skip set to 1) or the previous (skip set to -1) subtitle.
    This is similar to ``sub-step``, except that it seeks video and audio
    instead of adjusting the subtitle delay.

    Secondary argument:

    primary (default)
        Seeks through the primary subtitles.
    secondary
        Seeks through the secondary subtitles.

    For embedded subtitles (like with Matroska), this works only with subtitle
    events that have already been displayed, or are within a short prefetch
    range.

``print-text <text>``
    Print text to stdout. The string can contain properties (see
    `Property Expansion`_). Take care to put the argument in quotes.

``show-text <text> [<duration>|-1 [<level>]]``
    Show text on the OSD. The string can contain properties, which are expanded
    as described in `Property Expansion`_. This can be used to show playback
    time, filename, and so on.

    <duration>
        The time in ms to show the message for. By default, it uses the same
        value as ``--osd-duration``.

    <level>
        The minimum OSD level to show the text at (see ``--osd-level``).

``expand-text <string>``
    Property-expand the argument and return the expanded string. This can be
    used only through the client API or from a script using
    ``mp.command_native``. (see `Property Expansion`_).

``expand-path "<string>"``
    Expand a path's double-tilde placeholders into a platform-specific path.
    As ``expand-text``, this can only be used through the client API or from
    a script using ``mp.command_native``.

    .. admonition:: Example

        ``mp.osd_message(mp.command_native({"expand-path", "~~home/"}))``

        This line of Lua would show the location of the user's mpv
        configuration directory on the OSD.

``show-progress``
    Show the progress bar, the elapsed time and the total duration of the file
    on the OSD.

``write-watch-later-config``
    Write the resume config file that the ``quit-watch-later`` command writes,
    but continue playback normally.

``delete-watch-later-config [<filename>]``
    Delete any existing resume config file that was written by
    ``quit-watch-later`` or ``write-watch-later-config``. If a filename is
    specified, then the deleted config is for that file; otherwise, it is the
    same one as would be written by ``quit-watch-later`` or
    ``write-watch-later-config`` in the current circumstance.

``stop [<flags>]``
    Stop playback and clear playlist. With default settings, this is
    essentially like ``quit``. Useful for the client API: playback can be
    stopped without terminating the player.

    The first argument is optional, and supports the following flags:

    keep-playlist
        Do not clear the playlist.


``mouse <x> <y> [<button> [<mode>]]``
    Send a mouse event with given coordinate (``<x>``, ``<y>``).

    Second argument:

    <button>
        The button number of clicked mouse button. This should be one of 0-19.
        If ``<button>`` is omitted, only the position will be updated.

    Third argument:

    <single> (default)
        The mouse event represents regular single click.

    <double>
        The mouse event represents double-click.

``keypress <name>``
    Send a key event through mpv's input handler, triggering whatever
    behavior is configured to that key. ``name`` uses the ``input.conf``
    naming scheme for keys and modifiers. Useful for the client API: key events
    can be sent to libmpv to handle internally.

``keydown <name>``
    Similar to ``keypress``, but sets the ``KEYDOWN`` flag so that if the key is
    bound to a repeatable command, it will be run repeatedly with mpv's key
    repeat timing until the ``keyup`` command is called.

``keyup [<name>]``
    Set the ``KEYUP`` flag, stopping any repeated behavior that had been
    triggered. ``name`` is optional. If ``name`` is not given or is an
    empty string, ``KEYUP`` will be set on all keys. Otherwise, ``KEYUP`` will
    only be set on the key specified by ``name``.

``keybind <name> <command>``
    Binds a key to an input command. ``command`` must be a complete command
    containing all the desired arguments and flags. Both ``name`` and
    ``command`` use the ``input.conf`` naming scheme. This is primarily
    useful for the client API.

``audio-add <url> [<flags> [<title> [<lang>]]]``
    Load the given audio file. See ``sub-add`` command.

``audio-remove [<id>]``
    Remove the given audio track. See ``sub-remove`` command.

``audio-reload [<id>]``
    Reload the given audio tracks. See ``sub-reload`` command.

``video-add <url> [<flags> [<title> [<lang> [<albumart>]]]]``
    Load the given video file. See ``sub-add`` command for common options.

    ``albumart`` (``MPV_FORMAT_FLAG``)
        If enabled, mpv will load the given video as album art.

``video-remove [<id>]``
    Remove the given video track. See ``sub-remove`` command.

``video-reload [<id>]``
    Reload the given video tracks. See ``sub-reload`` command.

``rescan-external-files [<mode>]``
    Rescan external files according to the current ``--sub-auto``,
    ``--audio-file-auto`` and ``--cover-art-auto`` settings. This can be used
    to auto-load external files *after* the file was loaded.

    The ``mode`` argument is one of the following:

    <reselect> (default)
        Select the default audio and subtitle streams, which typically selects
        external files with the highest preference. (The implementation is not
        perfect, and could be improved on request.)

    <keep-selection>
        Do not change current track selections.


Input Commands that are Possibly Subject to Change
--------------------------------------------------

``af <operation> <value>``
    Change audio filter chain. See ``vf`` command.

``vf <operation> <value>``
    Change video filter chain.

    The semantics are exactly the same as with option parsing (see
    `VIDEO FILTERS`_). As such the text below is a redundant and incomplete
    summary.

    The first argument decides what happens:

    <set>
        Overwrite the previous filter chain with the new one.

    <add>
        Append the new filter chain to the previous one.

    <toggle>
        Check if the given filter (with the exact parameters) is already in the
        video chain. If it is, remove the filter. If it isn't, add the filter.
        (If several filters are passed to the command, this is done for
        each filter.)

        A special variant is combining this with labels, and using ``@name``
        without filter name and parameters as filter entry. This toggles the
        enable/disable flag.

    <remove>
        Like ``toggle``, but always remove the given filter from the chain.

    <del>
        Remove the given filters from the video chain. Unlike in the other
        cases, the second parameter is a comma separated list of filter names
        or integer indexes. ``0`` would denote the first filter. Negative
        indexes start from the last filter, and ``-1`` denotes the last
        filter. Deprecated, use ``remove``.

    <clr>
        Remove all filters. Note that like the other sub-commands, this does
        not control automatically inserted filters.

    The argument is always needed. E.g. in case of ``clr`` use ``vf clr ""``.

    You can assign labels to filter by prefixing them with ``@name:`` (where
    ``name`` is a user-chosen arbitrary identifier). Labels can be used to
    refer to filters by name in all of the filter chain modification commands.
    For ``add``, using an already used label will replace the existing filter.

    The ``vf`` command shows the list of requested filters on the OSD after
    changing the filter chain. This is roughly equivalent to
    ``show-text ${vf}``. Note that auto-inserted filters for format conversion
    are not shown on the list, only what was requested by the user.

    Normally, the commands will check whether the video chain is recreated
    successfully, and will undo the operation on failure. If the command is run
    before video is configured (can happen if the command is run immediately
    after opening a file and before a video frame is decoded), this check can't
    be run. Then it can happen that creating the video chain fails.

    .. admonition:: Example for input.conf

        - ``a vf set vflip`` turn the video upside-down on the ``a`` key
        - ``b vf set ""`` remove all video filters on ``b``
        - ``c vf toggle gradfun`` toggle debanding on ``c``

    .. admonition:: Example how to toggle disabled filters at runtime

        - Add something like ``vf-add=@deband:!gradfun`` to ``mpv.conf``.
          The ``@deband:`` is the label, an arbitrary, user-given name for this
          filter entry. The ``!`` before the filter name disables the filter by
          default. Everything after this is the normal filter name and possibly
          filter parameters, like in the normal ``--vf`` syntax.
        - Add ``a vf toggle @deband`` to ``input.conf``. This toggles the
          "disabled" flag for the filter with the label ``deband`` when the
          ``a`` key is hit.

``cycle-values [<"!reverse">] <property> <value1> [<value2> [...]]``
    Cycle through a list of values. Each invocation of the command will set the
    given property to the next value in the list. The command will use the
    current value of the property/option, and use it to determine the current
    position in the list of values. Once it has found it, it will set the
    next value in the list (wrapping around to the first item if needed).

    This command has a variable number of arguments, and cannot be used with
    named arguments.

    The special argument ``!reverse`` can be used to cycle the value list in
    reverse. The only advantage is that you don't need to reverse the value
    list yourself when adding a second key binding for cycling backwards.

``enable-section <name> [<flags>]``
    This command is deprecated, except for mpv-internal uses.

    Enable all key bindings in the named input section.

    The enabled input sections form a stack. Bindings in sections on the top of
    the stack are preferred to lower sections. This command puts the section
    on top of the stack. If the section was already on the stack, it is
    implicitly removed beforehand. (A section cannot be on the stack more than
    once.)

    The ``flags`` parameter can be a combination (separated by ``+``) of the
    following flags:

    <exclusive>
        All sections enabled before the newly enabled section are disabled.
        They will be re-enabled as soon as all exclusive sections above them
        are removed. In other words, the new section shadows all previous
        sections.
    <allow-hide-cursor>
        This feature can't be used through the public API.
    <allow-vo-dragging>
        Same.

``disable-section <name>``
    This command is deprecated, except for mpv-internal uses.

    Disable the named input section. Undoes ``enable-section``.

``define-section <name> <contents> [<flags>]``
    This command is deprecated, except for mpv-internal uses.

    Create a named input section, or replace the contents of an already existing
    input section. The ``contents`` parameter uses the same syntax as the
    ``input.conf`` file (except that using the section syntax in it is not
    allowed), including the need to separate bindings with a newline character.

    If the ``contents`` parameter is an empty string, the section is removed.

    The section with the name ``default`` is the normal input section.

    In general, input sections have to be enabled with the ``enable-section``
    command, or they are ignored.

    The last parameter has the following meaning:

    <default> (also used if parameter omitted)
        Use a key binding defined by this section only if the user hasn't
        already bound this key to a command.
    <force>
        Always bind a key. (The input section that was made active most recently
        wins if there are ambiguities.)

    This command can be used to dispatch arbitrary keys to a script or a client
    API user. If the input section defines ``script-binding`` commands, it is
    also possible to get separate events on key up/down, and relatively detailed
    information about the key state. The special key name ``unmapped`` can be
    used to match any unmapped key.

``overlay-add <id> <x> <y> <file> <offset> <fmt> <w> <h> <stride>``
    Add an OSD overlay sourced from raw data. This might be useful for scripts
    and applications controlling mpv, and which want to display things on top
    of the video window.

    Overlays are usually displayed in screen resolution, but with some VOs,
    the resolution is reduced to that of the video's. You can read the
    ``osd-width`` and ``osd-height`` properties. At least with ``--vo-xv`` and
    anamorphic video (such as DVD), ``osd-par`` should be read as well, and the
    overlay should be aspect-compensated.

    This has the following named arguments. The order of them is not guaranteed,
    so you should always call them with named arguments, see `Named arguments`_.

    ``id`` is an integer between 0 and 63 identifying the overlay element. The
    ID can be used to add multiple overlay parts, update a part by using this
    command with an already existing ID, or to remove a part with
    ``overlay-remove``. Using a previously unused ID will add a new overlay,
    while reusing an ID will update it.

    ``x`` and ``y`` specify the position where the OSD should be displayed.

    ``file`` specifies the file the raw image data is read from. It can be
    either a numeric UNIX file descriptor prefixed with ``@`` (e.g. ``@4``),
    or a filename. The file will be mapped into memory with ``mmap()``,
    copied, and unmapped before the command returns (changed in mpv 0.18.1).

    It is also possible to pass a raw memory address for use as bitmap memory
    by passing a memory address as integer prefixed with an ``&`` character.
    Passing the wrong thing here will crash the player. This mode might be
    useful for use with libmpv. The ``offset`` parameter is simply added to the
    memory address (since mpv 0.8.0, ignored before).

    ``offset`` is the byte offset of the first pixel in the source file.
    (The current implementation always mmap's the whole file from position 0 to
    the end of the image, so large offsets should be avoided. Before mpv 0.8.0,
    the offset was actually passed directly to ``mmap``, but it was changed to
    make using it easier.)

    ``fmt`` is a string identifying the image format. Currently, only ``bgra``
    is defined. This format has 4 bytes per pixels, with 8 bits per component.
    The least significant 8 bits are blue, and the most significant 8 bits
    are alpha (in little endian, the components are B-G-R-A, with B as first
    byte). This uses premultiplied alpha: every color component is already
    multiplied with the alpha component. This means the numeric value of each
    component is equal to or smaller than the alpha component. (Violating this
    rule will lead to different results with different VOs: numeric overflows
    resulting from blending broken alpha values is considered something that
    shouldn't happen, and consequently implementations don't ensure that you
    get predictable behavior in this case.)

    ``w``, ``h``, and ``stride`` specify the size of the overlay. ``w`` is the
    visible width of the overlay, while ``stride`` gives the width in bytes in
    memory. In the simple case, and with the ``bgra`` format, ``stride==4*w``.
    In general, the total amount of memory accessed is ``stride * h``.
    (Technically, the minimum size would be ``stride * (h - 1) + w * 4``, but
    for simplicity, the player will access all ``stride * h`` bytes.)

    .. note::

        Before mpv 0.18.1, you had to do manual "double buffering" when updating
        an overlay by replacing it with a different memory buffer. Since mpv
        0.18.1, the memory is simply copied and doesn't reference any of the
        memory indicated by the command's arguments after the commend returns.
        If you want to use this command before mpv 0.18.1, reads the old docs
        to see how to handle this correctly.

``overlay-remove <id>``
    Remove an overlay added with ``overlay-add`` and the same ID. Does nothing
    if no overlay with this ID exists.

``osd-overlay``
    Add/update/remove an OSD overlay.

    (Although this sounds similar to ``overlay-add``, ``osd-overlay`` is for
    text overlays, while ``overlay-add`` is for bitmaps. Maybe ``overlay-add``
    will be merged into ``osd-overlay`` to remove this oddity.)

    You can use this to add text overlays in ASS format. ASS has advanced
    positioning and rendering tags, which can be used to render almost any kind
    of vector graphics.

    This command accepts the following parameters:

    ``id``
        Arbitrary integer that identifies the overlay. Multiple overlays can be
        added by calling this command with different ``id`` parameters. Calling
        this command with the same ``id`` replaces the previously set overlay.

        There is a separate namespace for each libmpv client (i.e. IPC
        connection, script), so IDs can be made up and assigned by the API user
        without conflicting with other API users.

        If the libmpv client is destroyed, all overlays associated with it are
        also deleted. In particular, connecting via ``--input-ipc-server``,
        adding an overlay, and disconnecting will remove the overlay immediately
        again.

    ``format``
        String that gives the type of the overlay. Accepts the following values
        (HTML rendering of this is broken, view the generated manpage instead,
        or the raw RST source):

        ``ass-events``
            The ``data`` parameter is a string. The string is split on the
            newline character. Every line is turned into the ``Text`` part of
            a ``Dialogue`` ASS event. Timing is unused (but behavior of timing
            dependent ASS tags may change in future mpv versions).

            Note that it's better to put multiple lines into ``data``, instead
            of adding multiple OSD overlays.

            This provides 2 ASS ``Styles``. ``OSD`` contains the text style as
            defined by the current ``--osd-...`` options. ``Default`` is
            similar, and contains style that ``OSD`` would have if all options
            were set to the default.

            In addition, the ``res_x`` and ``res_y`` options specify the value
            of the ASS ``PlayResX`` and ``PlayResY`` header fields. If ``res_y``
            is set to 0, ``PlayResY`` is initialized to an arbitrary default
            value (but note that the default for this command is 720, not 0).
            If ``res_x`` is set to 0, ``PlayResX`` is set based on ``res_y``
            such that a virtual ASS pixel has a square pixel aspect ratio.

        ``none``
            Special value that causes the overlay to be removed. Most parameters
            other than ``id`` and ``format`` are mostly ignored.

    ``data``
        String defining the overlay contents according to the ``format``
        parameter.

    ``res_x``, ``res_y``
        Used if ``format`` is set to ``ass-events`` (see description there).
        Optional, defaults to 0/720.

    ``z``
        The Z order of the overlay. Optional, defaults to 0.

        Note that Z order between different overlays of different formats is
        static, and cannot be changed (currently, this means that bitmap
        overlays added by ``overlay-add`` are always on top of the ASS overlays
        added by ``osd-overlay``). In addition, the builtin OSD components are
        always below any of the custom OSD. (This includes subtitles of any kind
        as well as text rendered by ``show-text``.)

        It's possible that future mpv versions will randomly change how Z order
        between different OSD formats and builtin OSD is handled.

    ``hidden``
        If set to true, do not display this (default: false).

    ``compute_bounds``
        If set to true, attempt to determine bounds and write them to the
        command's result value as ``x0``, ``x1``, ``y0``, ``y1`` rectangle
        (default: false). If the rectangle is empty, not known, or somehow
        degenerate, it is not set. ``x1``/``y1`` is the coordinate of the
        bottom exclusive corner of the rectangle.

        The result value may depend on the VO window size, and is based on the
        last known window size at the time of the call. This means the results
        may be different from what is actually rendered.

        For ``ass-events``, the result rectangle is recomputed to ``PlayRes``
        coordinates (``res_x``/``res_y``). If window size is not known, a
        fallback is chosen.

        You should be aware that this mechanism is very inefficient, as it
        renders the full result, and then uses the bounding box of the rendered
        bitmap list (even if ``hidden`` is set). It will flush various caches.
        Its results also depend on the used libass version.

        This feature is experimental, and may change in some way again.

    .. note::

        Always use named arguments (``mpv_command_node()``). Lua scripts should
        use the ``mp.create_osd_overlay()`` helper instead of invoking this
        command directly.

``script-message [<arg1> [<arg2> [...]]]``
    Send a message to all clients, and pass it the following list of arguments.
    What this message means, how many arguments it takes, and what the arguments
    mean is fully up to the receiver and the sender. Every client receives the
    message, so be careful about name clashes (or use ``script-message-to``).

    This command has a variable number of arguments, and cannot be used with
    named arguments.

``script-message-to <target> [<arg1> [<arg2> [...]]]``
    Same as ``script-message``, but send it only to the client named
    ``<target>``. Each client (scripts etc.) has a unique name. For example,
    Lua scripts can get their name via ``mp.get_script_name()``. Note that
    client names only consist of alphanumeric characters and ``_``.

    This command has a variable number of arguments, and cannot be used with
    named arguments.

``script-binding <name>``
    Invoke a script-provided key binding. This can be used to remap key
    bindings provided by external Lua scripts.

    The argument is the name of the binding.

    It can optionally be prefixed with the name of the script, using ``/`` as
    separator, e.g. ``script-binding scriptname/bindingname``. Note that script
    names only consist of alphanumeric characters and ``_``.

    For completeness, here is how this command works internally. The details
    could change any time. On any matching key event, ``script-message-to``
    or ``script-message`` is called (depending on whether the script name is
    included), with the following arguments:

    1. The string ``key-binding``.
    2. The name of the binding (as established above).
    3. The key state as string (see below).
    4. The key name (since mpv 0.15.0).
    5. The text the key would produce, or empty string if not applicable.

    The 5th argument is only set if no modifiers are present (using the shift
    key with a letter is normally not emitted as having a modifier, and results
    in upper case text instead, but some backends may mess up).

    The key state consists of 2 characters:

    1. One of ``d`` (key was pressed down), ``u`` (was released), ``r`` (key
       is still down, and was repeated; only if key repeat is enabled for this
       binding), ``p`` (key was pressed; happens if up/down can't be tracked).
    2. Whether the event originates from the mouse, either ``m`` (mouse button)
       or ``-`` (something else).

    Future versions can add more arguments and more key state characters to
    support more input peculiarities.

``ab-loop``
    Cycle through A-B loop states. The first command will set the ``A`` point
    (the ``ab-loop-a`` property); the second the ``B`` point, and the third
    will clear both points.

``drop-buffers``
    Drop audio/video/demuxer buffers, and restart from fresh. Might help with
    unseekable streams that are going out of sync.
    This command might be changed or removed in the future.

``screenshot-raw [<flags>]``
    Return a screenshot in memory. This can be used only through the client
    API. The MPV_FORMAT_NODE_MAP returned by this command has the ``w``, ``h``,
    ``stride`` fields set to obvious contents. The ``format`` field is set to
    ``bgr0`` by default. This format is organized as ``B8G8R8X8`` (where ``B``
    is the LSB). The contents of the padding ``X`` are undefined. The ``data``
    field is of type MPV_FORMAT_BYTE_ARRAY with the actual image data. The image
    is freed as soon as the result mpv_node is freed. As usual with client API
    semantics, you are not allowed to write to the image data.

    The ``stride`` is the number of bytes from a pixel at ``(x0, y0)`` to the
    pixel at ``(x0, y0 + 1)``. This can be larger than ``w * 4`` if the image
    was cropped, or if there is padding. This number can be negative as well.
    You access a pixel with ``byte_index = y * stride + x * 4`` (assuming the
    ``bgr0`` format).

    The ``flags`` argument is like the first argument to ``screenshot`` and
    supports ``subtitles``, ``video``, ``window``.

``vf-command <label> <command> <argument>``
    Send a command to the filter with the given ``<label>``. Use ``all`` to send
    it to all filters at once. The command and argument string is filter
    specific. Currently, this only works with the ``lavfi`` filter - see
    the libavfilter documentation for which commands a filter supports.

    Note that the ``<label>`` is a mpv filter label, not a libavfilter filter
    name.

``af-command <label> <command> <argument>``
    Same as ``vf-command``, but for audio filters.

``apply-profile <name> [<mode>]``
    Apply the contents of a named profile. This is like using ``profile=name``
    in a config file, except you can map it to a key binding to change it at
    runtime.

    The mode argument:

    ``default``
        Apply the profile. Default if the argument is omitted.

    ``restore``
        Restore options set by a previous ``apply-profile`` command for this
        profile. Only works if the profile has ``profile-restore`` set to a
        relevant mode. Prints a warning if nothing could be done. See
        `Runtime profiles`_ for details.

``load-script <filename>``
    Load a script, similar to the ``--script`` option. Whether this waits for
    the script to finish initialization or not changed multiple times, and the
    future behavior is left undefined.

    On success, returns a ``mpv_node`` with a ``client_id`` field set to the
    return value of the ``mpv_client_id()`` API call of the newly created script
    handle.

``change-list <name> <operation> <value>``
    This command changes list options as described in `List Options`_. The
    ``<name>`` parameter is the normal option name, while ``<operation>`` is
    the suffix or action used on the option.

    Some operations take no value, but the command still requires the value
    parameter. In these cases, the value must be an empty string.

    .. admonition:: Example

        ``change-list glsl-shaders append file.glsl``

        Add a filename to the ``glsl-shaders`` list. The command line
        equivalent is ``--glsl-shaders-append=file.glsl`` or alternatively
        ``--glsl-shader=file.glsl``.

``dump-cache <start> <end> <filename>``
    Dump the current cache to the given filename. The ``<filename>`` file is
    overwritten if it already exists. ``<start>`` and ``<end>`` give the
    time range of what to dump. If no data is cached at the given time range,
    nothing may be dumped (creating a file with no packets).

    Dumping a larger part of the cache will freeze the player. No effort was
    made to fix this, as this feature was meant mostly for creating small
    excerpts.

    See ``--stream-record`` for various caveats that mostly apply to this
    command too, as both use the same underlying code for writing the output
    file.

    If ``<filename>`` is an empty string, an ongoing ``dump-cache`` is stopped.

    If ``<end>`` is ``no``, then continuous dumping is enabled. Then, after
    dumping the existing parts of the cache, anything read from network is
    appended to the cache as well. This behaves similar to ``--stream-record``
    (although it does not conflict with that option, and they can be both active
    at the same time).

    If the ``<end>`` time is after the cache, the command will _not_ wait and
    write newly received data to it.

    The end of the resulting file may be slightly damaged or incomplete at the
    end. (Not enough effort was made to ensure that the end lines up properly.)

    Note that this command will finish only once dumping ends. That means it
    works similar to the ``screenshot`` command, just that it can block much
    longer. If continuous dumping is used, the command will not finish until
    playback is stopped, an error happens, another ``dump-cache`` command is
    run, or an API like ``mp.abort_async_command`` was called to explicitly stop
    the command. See `Synchronous vs. Asynchronous`_.

    .. note::

        This was mostly created for network streams. For local files, there may
        be much better methods to create excerpts and such. There are tons of
        much more user-friendly Lua scripts, that will reencode parts of a file
        by spawning a separate instance of ``ffmpeg``. With network streams,
        this is not that easily possible, as the stream would have to be
        downloaded again. Even if ``--stream-record`` is used to record the
        stream to the local filesystem, there may be problems, because the
        recorded file is still written to.

    This command is experimental, and all details about it may change in the
    future.

``ab-loop-dump-cache <filename>``
    Essentially calls ``dump-cache`` with the current AB-loop points as
    arguments. Like ``dump-cache``, this will overwrite the file at
    ``<filename>``. Likewise, if the B point is set to ``no``, it will enter
    continuous dumping after the existing cache was dumped.

    The author reserves the right to remove this command if enough motivation
    is found to move this functionality to a trivial Lua script.

``ab-loop-align-cache``
    Re-adjust the A/B loop points to the start and end within the cache the
    ``ab-loop-dump-cache`` command will (probably) dump. Basically, it aligns
    the times on keyframes. The guess might be off especially at the end (due to
    granularity issues due to remuxing). If the cache shrinks in the meantime,
    the points set by the command will not be the effective parameters either.

    This command has an even more uncertain future than ``ab-loop-dump-cache``
    and might disappear without replacement if the author decides it's useless.

Undocumented commands: ``ao-reload`` (experimental/internal).

List of events
~~~~~~~~~~~~~~

This is a partial list of events. This section describes what
``mpv_event_to_node()`` returns, and which is what scripting APIs and the JSON
IPC sees. Note that the C API has separate C-level declarations with
``mpv_event``, which may be slightly different.

Note that events are asynchronous: the player core continues running while
events are delivered to scripts and other clients. In some cases, you can hooks
to enforce synchronous execution.

All events can have the following fields:

``event``
    Name as the event (as returned by ``mpv_event_name()``).

``id``
    The ``reply_userdata`` field (opaque user value). If ``reply_userdata`` is 0,
    the field is not added.

``error``
    Set to an error string (as returned by ``mpv_error_string()``). This field
    is missing if no error happened, or the event type does not report error.
    Most events leave this unset.

This list uses the event name field value, and the C API symbol in brackets:

``start-file`` (``MPV_EVENT_START_FILE``)
    Happens right before a new file is loaded. When you receive this, the
    player is loading the file (or possibly already done with it).

    This has the following fields:

    ``playlist_entry_id``
        Playlist entry ID of the file being loaded now.

``end-file`` (``MPV_EVENT_END_FILE``)
    Happens after a file was unloaded. Typically, the player will load the
    next file right away, or quit if this was the last file.

    The event has the following fields:

    ``reason``
        Has one of these values:

        ``eof``
            The file has ended. This can (but doesn't have to) include
            incomplete files or broken network connections under
            circumstances.

        ``stop``
            Playback was ended by a command.

        ``quit``
            Playback was ended by sending the quit command.

        ``error``
            An error happened. In this case, an ``error`` field is present with
            the error string.

        ``redirect``
            Happens with playlists and similar. Details see
            ``MPV_END_FILE_REASON_REDIRECT`` in the C API.

        ``unknown``
            Unknown. Normally doesn't happen, unless the Lua API is out of sync
            with the C API. (Likewise, it could happen that your script gets
            reason strings that did not exist yet at the time your script was
            written.)

    ``playlist_entry_id``
        Playlist entry ID of the file that was being played or attempted to be
        played. This has the same value as the ``playlist_entry_id`` field in the
        corresponding ``start-file`` event.

    ``file_error``
        Set to mpv error string describing the approximate reason why playback
        failed. Unset if no error known. (In Lua scripting, this value was set
        on the ``error`` field directly. This is deprecated since mpv 0.33.0.
        In the future, this ``error`` field will be unset for this specific
        event.)

    ``playlist_insert_id``
        If loading ended, because the playlist entry to be played was for example
        a playlist, and the current playlist entry is replaced with a number of
        other entries. This may happen at least with MPV_END_FILE_REASON_REDIRECT
        (other event types may use this for similar but different purposes in the
        future). In this case, playlist_insert_id will be set to the playlist
        entry ID of the first inserted entry, and playlist_insert_num_entries to
        the total number of inserted playlist entries. Note this in this specific
        case, the ID of the last inserted entry is playlist_insert_id+num-1.
        Beware that depending on circumstances, you may observe the new playlist
        entries before seeing the event (e.g. reading the "playlist" property or
        getting a property change notification before receiving the event).
        If this is 0 in the C API, this field isn't added.

    ``playlist_insert_num_entries``
        See playlist_insert_id. Only present if playlist_insert_id is present.

``file-loaded``  (``MPV_EVENT_FILE_LOADED``)
    Happens after a file was loaded and begins playback.

``seek`` (``MPV_EVENT_SEEK``)
    Happens on seeking. (This might include cases when the player seeks
    internally, even without user interaction. This includes e.g. segment
    changes when playing ordered chapters Matroska files.)

``playback-restart`` (``MPV_EVENT_PLAYBACK_RESTART``)
    Start of playback after seek or after file was loaded.

``shutdown`` (``MPV_EVENT_SHUTDOWN``)
    Sent when the player quits, and the script should terminate. Normally
    handled automatically. See `Details on the script initialization and lifecycle`_.

``log-message`` (``MPV_EVENT_LOG_MESSAGE``)
    Receives messages enabled with ``mpv_request_log_messages()`` (Lua:
    ``mp.enable_messages``).

    This contains, in addition to the default event fields, the following
    fields:

    ``prefix``
        The module prefix, identifies the sender of the message. This is what
        the terminal player puts in front of the message text when using the
        ``--v`` option, and is also what is used for ``--msg-level``.

    ``level``
        The log level as string. See ``msg.log`` for possible log level names.
        Note that later versions of mpv might add new levels or remove
        (undocumented) existing ones.

    ``text``
        The log message. The text will end with a newline character. Sometimes
        it can contain multiple lines.

    Keep in mind that these messages are meant to be hints for humans. You
    should not parse them, and prefix/level/text of messages might change
    any time.

``hook``
    The event has the following fields:

    ``hook_id``
        ID to pass to ``mpv_hook_continue()``. The Lua scripting wrapper
        provides a better API around this with ``mp.add_hook()``.

``get-property-reply`` (``MPV_EVENT_GET_PROPERTY_REPLY``)
    See C API.

``set-property-reply`` (``MPV_EVENT_SET_PROPERTY_REPLY``)
    See C API.

``command-reply`` (``MPV_EVENT_COMMAND_REPLY``)
    This is one of the commands for which the ```error`` field is meaningful.

    JSON IPC and Lua and possibly other backends treat this specially and may
    not pass the actual event to the user. See C API.

    The event has the following fields:

    ``result``
        The result (on success) of any ``mpv_node`` type, if any.

``client-message`` (``MPV_EVENT_CLIENT_MESSAGE``)
    Lua and possibly other backends treat this specially and may not pass the
    actual event to the user.

    The event has the following fields:

    ``args``
        Array of strings with the message data.

``video-reconfig`` (``MPV_EVENT_VIDEO_RECONFIG``)
    Happens on video output or filter reconfig.

``audio-reconfig`` (``MPV_EVENT_AUDIO_RECONFIG``)
    Happens on audio output or filter reconfig.

``property-change`` (``MPV_EVENT_PROPERTY_CHANGE``)
    Happens when a property that is being observed changes value.

    The event has the following fields:

    ``name``
        The name of the property.

    ``data``
        The new value of the property.

The following events also happen, but are deprecated: ``tracks-changed``,
``track-switched``, ``pause``, ``unpause``, ``metadata-update``, ``idle``,
``tick``, ``chapter-change``. Use ``mpv_observe_property()``
(Lua: ``mp.observe_property()``) instead.

Hooks
~~~~~

Hooks are synchronous events between player core and a script or similar. This
applies to client API (including the Lua scripting interface). Normally,
events are supposed to be asynchronous, and the hook API provides an awkward
and obscure way to handle events that require stricter coordination. There are
no API stability guarantees made. Not following the protocol exactly can make
the player freeze randomly. Basically, nobody should use this API.

The C API is described in the header files. The Lua API is described in the
Lua section.

Before a hook is actually invoked on an API clients, it will attempt to return
new values for all observed properties that were changed before the hook. This
may make it easier for an application to set defined "barriers" between property
change notifications by registering hooks. (That means these hooks will have an
effect, even if you do nothing and make them continue immediately.)

The following hooks are currently defined:

``on_load``
    Called when a file is to be opened, before anything is actually done.
    For example, you could read and write the ``stream-open-filename``
    property to redirect an URL to something else (consider support for
    streaming sites which rarely give the user a direct media URL), or
    you could set per-file options with by setting the property
    ``file-local-options/<option name>``. The player will wait until all
    hooks are run.

    Ordered after ``start-file`` and before ``playback-restart``.

``on_load_fail``
    Called after after a file has been opened, but failed to. This can be
    used to provide a fallback in case native demuxers failed to recognize
    the file, instead of always running before the native demuxers like
    ``on_load``. Demux will only be retried if ``stream-open-filename``
    was changed. If it fails again, this hook is _not_ called again, and
    loading definitely fails.

    Ordered after ``on_load``, and before ``playback-restart`` and ``end-file``.

``on_preloaded``
    Called after a file has been opened, and before tracks are selected and
    decoders are created. This has some usefulness if an API users wants
    to select tracks manually, based on the set of available tracks. It's
    also useful to initialize ``--lavfi-complex`` in a specific way by API,
    without having to "probe" the available streams at first.

    Note that this does not yet apply default track selection. Which operations
    exactly can be done and not be done, and what information is available and
    what is not yet available yet, is all subject to change.

    Ordered after ``on_load_fail`` etc. and before ``playback-restart``.

``on_unload``
    Run before closing a file, and before actually uninitializing
    everything. It's not possible to resume playback in this state.

    Ordered before ``end-file``. Will also happen in the error case (then after
    ``on_load_fail``).

``on_before_start_file``
    Run before a ``start-file`` event is sent. (If any client changes the
    current playlist entry, or sends a quit command to the player, the
    corresponding event will not actually happen after the hook returns.)
    Useful to drain property changes before a new file is loaded.

``on_after_end_file``
    Run after an ``end-file`` event. Useful to drain property changes after a
    file has finished.

Input Command Prefixes
----------------------

These prefixes are placed between key name and the actual command. Multiple
prefixes can be specified. They are separated by whitespace.

``osd-auto``
    Use the default behavior for this command. This is the default for
    ``input.conf`` commands. Some libmpv/scripting/IPC APIs do not use this as
    default, but use ``no-osd`` instead.
``no-osd``
    Do not use any OSD for this command.
``osd-bar``
    If possible, show a bar with this command. Seek commands will show the
    progress bar, property changing commands may show the newly set value.
``osd-msg``
    If possible, show an OSD message with this command. Seek command show
    the current playback time, property changing commands show the newly set
    value as text.
``osd-msg-bar``
    Combine osd-bar and osd-msg.
``raw``
    Do not expand properties in string arguments. (Like ``"${property-name}"``.)
    This is the default for some libmpv/scripting/IPC APIs.
``expand-properties``
    All string arguments are expanded as described in `Property Expansion`_.
    This is the default for ``input.conf`` commands.
``repeatable``
    For some commands, keeping a key pressed doesn't run the command repeatedly.
    This prefix forces enabling key repeat in any case. For a list of commands:
    the first command determines the repeatability of the whole list (up to and
    including version 0.33 - a list was always repeatable).
``async``
    Allow asynchronous execution (if possible). Note that only a few commands
    will support this (usually this is explicitly documented). Some commands
    are asynchronous by default (or rather, their effects might manifest
    after completion of the command). The semantics of this flag might change
    in the future. Set it only if you don't rely on the effects of this command
    being fully realized when it returns. See `Synchronous vs. Asynchronous`_.
``sync``
    Allow synchronous execution (if possible). Normally, all commands are
    synchronous by default, but some are asynchronous by default for
    compatibility with older behavior.

All of the osd prefixes are still overridden by the global ``--osd-level``
settings.

Synchronous vs. Asynchronous
----------------------------

The ``async`` and ``sync`` prefix matter only for how the issuer of the command
waits on the completion of the command. Normally it does not affect how the
command behaves by itself. There are the following cases:

- Normal input.conf commands are always run asynchronously. Slow running
  commands are queued up or run in parallel.
- "Multi" input.conf commands (1 key binding, concatenated with ``;``) will be
  executed in order, except for commands that are async (either prefixed with
  ``async``, or async by default for some commands). The async commands are
  run in a detached manner, possibly in parallel to the remaining sync commands
  in the list.
- Normal Lua and libmpv commands (e.g. ``mpv_command()``) are run in a blocking
  manner, unless the ``async`` prefix is used, or the command is async by
  default. This means in the sync case the caller will block, even if the core
  continues playback. Async mode runs the command in a detached manner.
- Async libmpv command API (e.g. ``mpv_command_async()``) never blocks the
  caller, and always notify their completion with a message. The ``sync`` and
  ``async`` prefixes make no difference.
- Lua also provides APIs for running async commands, which behave similar to the
  C counterparts.
- In all cases, async mode can still run commands in a synchronous manner, even
  in detached mode. This can for example happen in cases when a command does not
  have an  asynchronous implementation. The async libmpv API still never blocks
  the caller in these cases.

Before mpv 0.29.0, the ``async`` prefix was only used by screenshot commands,
and made them run the file saving code in a detached manner. This is the
default now, and ``async`` changes behavior only in the ways mentioned above.

Currently the following commands have different waiting characteristics with
sync vs. async: sub-add, audio-add, sub-reload, audio-reload,
rescan-external-files, screenshot, screenshot-to-file, dump-cache,
ab-loop-dump-cache.

Asynchronous command details
----------------------------

On the API level, every asynchronous command is bound to the context which
started it. For example, an asynchronous command started by ``mpv_command_async``
is bound to the ``mpv_handle`` passed to the function. Only this ``mpv_handle``
receives the completion notification (``MPV_EVENT_COMMAND_REPLY``), and only
this handle can abort a still running command directly. If the ``mpv_handle`` is
destroyed, any still running async. commands started by it are terminated.

The scripting APIs and JSON IPC give each script/connection its own implicit
``mpv_handle``.

If the player is closed, the core may abort all pending async. commands on its
own (like a forced ``mpv_abort_async_command()`` call for each pending command
on behalf of the API user). This happens at the same time ``MPV_EVENT_SHUTDOWN``
is sent, and there is no way to prevent this.

Input Sections
--------------

Input sections group a set of bindings, and enable or disable them at once.
In ``input.conf``, each key binding is assigned to an input section, rather
than actually having explicit text sections.

See also: ``enable-section`` and ``disable-section`` commands.

Predefined bindings:

``default``
    Bindings without input section are implicitly assigned to this section. It
    is enabled by default during normal playback.
``encode``
    Section which is active in encoding mode. It is enabled exclusively, so
    that bindings in the ``default`` sections are ignored.

Properties
----------

Properties are used to set mpv options during runtime, or to query arbitrary
information. They can be manipulated with the ``set``/``add``/``cycle``
commands, and retrieved with ``show-text``, or anything else that uses property
expansion. (See `Property Expansion`_.)

The property name is annotated with RW to indicate whether the property is
generally writable.

If an option is referenced, the property will normally take/return exactly the
same values as the option. In these cases, properties are merely a way to change
an option at runtime.

Property list
-------------

.. note::

    Most options can be set as runtime via properties as well. Just remove the
    leading ``--`` from the option name. These are not documented below, see
    `OPTIONS`_ instead. Only properties which do not exist as option with the
    same name, or which have very different behavior from the options are
    documented below.

    Properties marked as (RW) are writeable, while those that aren't are
    read-only.

``audio-speed-correction``, ``video-speed-correction``
    Factor multiplied with ``speed`` at which the player attempts to play the
    file. Usually it's exactly 1. (Display sync mode will make this useful.)

    OSD formatting will display it in the form of ``+1.23456%``, with the number
    being ``(raw - 1) * 100`` for the given raw property value.

``display-sync-active``
    Whether ``--video-sync=display`` is actually active.

``filename``
    Currently played file, with path stripped. If this is an URL, try to undo
    percent encoding as well. (The result is not necessarily correct, but
    looks better for display purposes. Use the ``path`` property to get an
    unmodified filename.)

    This has a sub-property:

    ``filename/no-ext``
        Like the ``filename`` property, but if the text contains a ``.``, strip
        all text after the last ``.``. Usually this removes the file extension.

``file-size``
    Length in bytes of the source file/stream. (This is the same as
    ``${stream-end}``. For segmented/multi-part files, this will return the
    size of the main or manifest file, whatever it is.)

``estimated-frame-count``
    Total number of frames in current file.

    .. note:: This is only an estimate. (It's computed from two unreliable
              quantities: fps and stream length.)

``estimated-frame-number``
    Number of current frame in current stream.

    .. note:: This is only an estimate. (It's computed from two unreliable
              quantities: fps and possibly rounded timestamps.)

``pid``
    Process-id of mpv.

``path``
    Full path of the currently played file. Usually this is exactly the same
    string you pass on the mpv command line or the ``loadfile`` command, even
    if it's a relative path. If you expect an absolute path, you will have to
    determine it yourself, for example by using the ``working-directory``
    property.

``stream-open-filename``
    The full path to the currently played media. This is different from
    ``path`` only in special cases. In particular, if ``--ytdl=yes`` is used,
    and the URL is detected by ``youtube-dl``, then the script will set this
    property to the actual media URL. This property should be set only during
    the ``on_load`` or ``on_load_fail`` hooks, otherwise it will have no effect
    (or may do something implementation defined in the future). The property is
    reset if playback of the current media ends.

``media-title``
    If the currently played file has a ``title`` tag, use that.

    Otherwise, return the ``filename`` property.

``file-format``
    Symbolic name of the file format. In some cases, this is a comma-separated
    list of format names, e.g. mp4 is ``mov,mp4,m4a,3gp,3g2,mj2`` (the list
    may grow in the future for any format).

``current-demuxer``
    Name of the current demuxer. (This is useless.)

    (Renamed from ``demuxer``.)

``stream-path``
    Filename (full path) of the stream layer filename. (This is probably
    useless and is almost never different from ``path``.)

``stream-pos``
    Raw byte position in source stream. Technically, this returns the position
    of the most recent packet passed to a decoder.

``stream-end``
    Raw end position in bytes in source stream.

``duration``
    Duration of the current file in seconds. If the duration is unknown, the
    property is unavailable. Note that the file duration is not always exactly
    known, so this is an estimate.

    This replaces the ``length`` property, which was deprecated after the
    mpv 0.9 release. (The semantics are the same.)

``avsync``
    Last A/V synchronization difference. Unavailable if audio or video is
    disabled.

``total-avsync-change``
    Total A-V sync correction done. Unavailable if audio or video is
    disabled.

``decoder-frame-drop-count``
    Video frames dropped by decoder, because video is too far behind audio (when
    using ``--framedrop=decoder``). Sometimes, this may be incremented in other
    situations, e.g. when video packets are damaged, or the decoder doesn't
    follow the usual rules. Unavailable if video is disabled.

    ``drop-frame-count`` is a deprecated alias.

``frame-drop-count``
    Frames dropped by VO (when using ``--framedrop=vo``).

    ``vo-drop-frame-count`` is a deprecated alias.

``mistimed-frame-count``
    Number of video frames that were not timed correctly in display-sync mode
    for the sake of keeping A/V sync. This does not include external
    circumstances, such as video rendering being too slow or the graphics
    driver somehow skipping a vsync. It does not include rounding errors either
    (which can happen especially with bad source timestamps). For example,
    using the ``display-desync`` mode should never change this value from 0.

``vsync-ratio``
    For how many vsyncs a frame is displayed on average. This is available if
    display-sync is active only. For 30 FPS video on a 60 Hz screen, this will
    be 2. This is the moving average of what actually has been scheduled, so
    24 FPS on 60 Hz will never remain exactly on 2.5, but jitter depending on
    the last frame displayed.

``vo-delayed-frame-count``
    Estimated number of frames delayed due to external circumstances in
    display-sync mode. Note that in general, mpv has to guess that this is
    happening, and the guess can be inaccurate.

``percent-pos`` (RW)
    Position in current file (0-100). The advantage over using this instead of
    calculating it out of other properties is that it properly falls back to
    estimating the playback position from the byte position, if the file
    duration is not known.

``time-pos`` (RW)
    Position in current file in seconds.

``time-start``
    Deprecated. Always returns 0. Before mpv 0.14, this used to return the start
    time of the file (could affect e.g. transport streams). See
    ``--rebase-start-time`` option.

``time-remaining``
    Remaining length of the file in seconds. Note that the file duration is not
    always exactly known, so this is an estimate.

``audio-pts``
    Current audio playback position in current file in seconds. Unlike time-pos,
    this updates more often than once per frame. For audio-only files, it is
    mostly equivalent to time-pos, while for video-only files this property is
    not available.

``playtime-remaining``
    ``time-remaining`` scaled by the current ``speed``.

``playback-time`` (RW)
    Position in current file in seconds. Unlike ``time-pos``, the time is
    clamped to the range of the file. (Inaccurate file durations etc. could
    make it go out of range. Useful on attempts to seek outside of the file,
    as the seek target time is considered the current position during seeking.)

``chapter`` (RW)
    Current chapter number. The number of the first chapter is 0.

``edition`` (RW)
    Current MKV edition number. Setting this property to a different value will
    restart playback. The number of the first edition is 0.

    Before mpv 0.31.0, this showed the actual edition selected at runtime, if
    you didn't set the option or property manually. With mpv 0.31.0 and later,
    this strictly returns the user-set option or property value, and the
    ``current-edition`` property was added to return the runtime selected
    edition (this matters with ``--edition=auto``, the default).

``current-edition``
    Currently selected edition. This property is unavailable if no file is
    loaded, or the file has no editions. (Matroska files make a difference
    between having no editions and a single edition, which will be reflected by
    the property, although in practice it does not matter.)

``chapters``
    Number of chapters.

``editions``
    Number of MKV editions.

``edition-list``
    List of editions, current entry marked. Currently, the raw property value
    is useless.

    This has a number of sub-properties. Replace ``N`` with the 0-based edition
    index.

    ``edition-list/count``
        Number of editions. If there are no editions, this can be 0 or 1 (1
        if there's a useless dummy edition).

    ``edition-list/N/id`` (RW)
        Edition ID as integer. Use this to set the ``edition`` property.
        Currently, this is the same as the edition index.

    ``edition-list/N/default``
        Whether this is the default edition.

    ``edition-list/N/title``
        Edition title as stored in the file. Not always available.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each edition)
                "id"                MPV_FORMAT_INT64
                "title"             MPV_FORMAT_STRING
                "default"           MPV_FORMAT_FLAG

``metadata``
    Metadata key/value pairs.

    If the property is accessed with Lua's ``mp.get_property_native``, this
    returns a table with metadata keys mapping to metadata values. If it is
    accessed with the client API, this returns a ``MPV_FORMAT_NODE_MAP``,
    with tag keys mapping to tag values.

    For OSD, it returns a formatted list. Trying to retrieve this property as
    a raw string doesn't work.

    This has a number of sub-properties:

    ``metadata/by-key/<key>``
        Value of metadata entry ``<key>``.

    ``metadata/list/count``
        Number of metadata entries.

    ``metadata/list/N/key``
        Key name of the Nth metadata entry. (The first entry is ``0``).

    ``metadata/list/N/value``
        Value of the Nth metadata entry.

    ``metadata/<key>``
        Old version of ``metadata/by-key/<key>``. Use is discouraged, because
        the metadata key string could conflict with other sub-properties.

    The layout of this property might be subject to change. Suggestions are
    welcome how exactly this property should work.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_MAP
            (key and string value for each metadata entry)

``filtered-metadata``
    Like ``metadata``, but includes only fields listed in the ``--display-tags``
    option. This is the same set of tags that is printed to the terminal.

``chapter-metadata``
    Metadata of current chapter. Works similar to ``metadata`` property. It
    also allows the same access methods (using sub-properties).

    Per-chapter metadata is very rare. Usually, only the chapter name
    (``title``) is set.

    For accessing other information, like chapter start, see the
    ``chapter-list`` property.

``vf-metadata/<filter-label>``
    Metadata added by video filters. Accessed by the filter label,
    which, if not explicitly specified using the ``@filter-label:`` syntax,
    will be ``<filter-name>NN``.

    Works similar to ``metadata`` property. It allows the same access
    methods (using sub-properties).

    An example of this kind of metadata are the cropping parameters
    added by ``--vf=lavfi=cropdetect``.

``af-metadata/<filter-label>``
    Equivalent to ``vf-metadata/<filter-label>``, but for audio filters.

``idle-active``
    Returns ``yes``/true if no file is loaded, but the player is staying around
    because of the ``--idle`` option.

    (Renamed from ``idle``.)

``core-idle``
    Whether the playback core is paused. This can differ from ``pause`` in
    special situations, such as when the player pauses itself due to low
    network cache.

    This also returns ``yes``/true if playback is restarting or if nothing is
    playing at all. In other words, it's only ``no``/false if there's actually
    video playing. (Behavior since mpv 0.7.0.)

``cache-speed``
    Current I/O read speed between the cache and the lower layer (like network).
    This gives the number bytes per seconds over a 1 second window (using
    the type ``MPV_FORMAT_INT64`` for the client API).

    This is the same as ``demuxer-cache-state/raw-input-rate``.

``demuxer-cache-duration``
    Approximate duration of video buffered in the demuxer, in seconds. The
    guess is very unreliable, and often the property will not be available
    at all, even if data is buffered.

``demuxer-cache-time``
    Approximate time of video buffered in the demuxer, in seconds. Same as
    ``demuxer-cache-duration`` but returns the last timestamp of buffered
    data in demuxer.

``demuxer-cache-idle``
    Whether the demuxer is idle, which means that the demuxer cache is filled
    to the requested amount, and is currently not reading more data.

``demuxer-cache-state``
    Each entry in ``seekable-ranges`` represents a region in the demuxer cache
    that can be seeked to, with a ``start`` and ``end`` fields containing the
    respective timestamps. If there are multiple demuxers active, this only
    returns information about the "main" demuxer, but might be changed in
    future to return unified information about all demuxers. The ranges are in
    arbitrary order. Often, ranges will overlap for a bit, before being joined.
    In broken corner cases, ranges may overlap all over the place.

    The end of a seek range is usually smaller than the value returned by the
    ``demuxer-cache-time`` property, because that property returns the guessed
    buffering amount, while the seek ranges represent the buffered data that
    can actually be used for cached seeking.

    ``bof-cached`` indicates whether the seek range with the lowest timestamp
    points to the beginning of the stream (BOF). This implies you cannot seek
    before this position at all. ``eof-cached`` indicates whether the seek range
    with the highest timestamp points to the end of the stream (EOF). If both
    ``bof-cached`` and ``eof-cached`` are true, and there's only 1 cache range,
    the entire stream is cached.

    ``fw-bytes`` is the number of bytes of packets buffered in the range
    starting from the current decoding position. This is a rough estimate
    (may not account correctly for various overhead), and stops at the
    demuxer position (it ignores seek ranges after it).

    ``file-cache-bytes`` is the number of bytes stored in the file cache. This
    includes all overhead, and possibly unused data (like pruned data). This
    member is missing if the file cache wasn't enabled with
    ``--cache-on-disk=yes``.

    ``cache-end`` is ``demuxer-cache-time``. Missing if unavailable.

    ``reader-pts`` is the approximate timestamp of the start of the buffered
    range. Missing if unavailable.

    ``cache-duration`` is ``demuxer-cache-duration``. Missing if unavailable.

    ``raw-input-rate`` is the estimated input rate of the network layer (or any
    other byte-oriented input layer) in bytes per second. May be inaccurate or
    missing.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_MAP
            "seekable-ranges"   MPV_FORMAT_NODE_ARRAY
                MPV_FORMAT_NODE_MAP
                    "start"             MPV_FORMAT_DOUBLE
                    "end"               MPV_FORMAT_DOUBLE
            "bof-cached"        MPV_FORMAT_FLAG
            "eof-cached"        MPV_FORMAT_FLAG
            "fw-bytes"          MPV_FORMAT_INT64
            "file-cache-bytes"  MPV_FORMAT_INT64
            "cache-end"         MPV_FORMAT_DOUBLE
            "reader-pts"        MPV_FORMAT_DOUBLE
            "cache-duration"    MPV_FORMAT_DOUBLE
            "raw-input-rate"    MPV_FORMAT_INT64

    Other fields (might be changed or removed in the future):

    ``eof``
        Whether the reader thread has hit the end of the file.

    ``underrun``
        Whether the reader thread could not satisfy a decoder's request for a
        new packet.

    ``idle``
        Whether the thread is currently not reading.

    ``total-bytes``
        Sum of packet bytes (plus some overhead estimation) of the entire packet
        queue, including cached seekable ranges.

``demuxer-via-network``
    Whether the stream demuxed via the main demuxer is most likely played via
    network. What constitutes "network" is not always clear, might be used for
    other types of untrusted streams, could be wrong in certain cases, and its
    definition might be changing. Also, external files (like separate audio
    files or streams) do not influence the value of this property (currently).

``demuxer-start-time``
    The start time reported by the demuxer in fractional seconds.

``paused-for-cache``
    Whether playback is paused because of waiting for the cache.

``cache-buffering-state``
    The percentage (0-100) of the cache fill status until the player will
    unpause (related to ``paused-for-cache``).

``eof-reached``
    Whether the end of playback was reached. Note that this is usually
    interesting only if ``--keep-open`` is enabled, since otherwise the player
    will immediately play the next file (or exit or enter idle mode), and in
    these cases the ``eof-reached`` property will logically be cleared
    immediately after it's set.

``seeking``
    Whether the player is currently seeking, or otherwise trying to restart
    playback. (It's possible that it returns ``yes``/true while a file is
    loaded. This is because the same underlying code is used for seeking and
    resyncing.)

``mixer-active``
    Whether the audio mixer is active.

    This option is relatively useless. Before mpv 0.18.1, it could be used to
    infer behavior of the ``volume`` property.

``ao-volume`` (RW)
    System volume. This property is available only if mpv audio output is
    currently active, and only if the underlying implementation supports volume
    control. What this option does depends on the API. For example, on ALSA
    this usually changes system-wide audio, while with PulseAudio this controls
    per-application volume.

``ao-mute`` (RW)
    Similar to ``ao-volume``, but controls the mute state. May be unimplemented
    even if ``ao-volume`` works.

``audio-codec``
    Audio codec selected for decoding.

``audio-codec-name``
    Audio codec.

``audio-params``
    Audio format as output by the audio decoder.
    This has a number of sub-properties:

    ``audio-params/format``
        The sample format as string. This uses the same names as used in other
        places of mpv.

    ``audio-params/samplerate``
        Samplerate.

    ``audio-params/channels``
        The channel layout as a string. This is similar to what the
        ``--audio-channels`` accepts.

    ``audio-params/hr-channels``
        As ``channels``, but instead of the possibly cryptic actual layout
        sent to the audio device, return a hopefully more human readable form.
        (Usually only ``audio-out-params/hr-channels`` makes sense.)

    ``audio-params/channel-count``
        Number of audio channels. This is redundant to the ``channels`` field
        described above.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_MAP
            "format"            MPV_FORMAT_STRING
            "samplerate"        MPV_FORMAT_INT64
            "channels"          MPV_FORMAT_STRING
            "channel-count"     MPV_FORMAT_INT64
            "hr-channels"       MPV_FORMAT_STRING

``audio-out-params``
    Same as ``audio-params``, but the format of the data written to the audio
    API.

``colormatrix``
    Redirects to ``video-params/colormatrix``. This parameter (as well as
    similar ones) can be overridden with the ``format`` video filter.

``colormatrix-input-range``
    See ``colormatrix``.

``colormatrix-primaries``
    See ``colormatrix``.

``hwdec`` (RW)
    Reflects the ``--hwdec`` option.

    Writing to it may change the currently used hardware decoder, if possible.
    (Internally, the player may reinitialize the decoder, and will perform a
    seek to refresh the video properly.) You can watch the other hwdec
    properties to see whether this was successful.

    Unlike in mpv 0.9.x and before, this does not return the currently active
    hardware decoder. Since mpv 0.18.0, ``hwdec-current`` is available for
    this purpose.

``hwdec-current``
    The current hardware decoding in use. If decoding is active, return one of
    the values used by the ``hwdec`` option/property. ``no``/false indicates
    software decoding. If no decoder is loaded, the property is unavailable.

``hwdec-interop``
    This returns the currently loaded hardware decoding/output interop driver.
    This is known only once the VO has opened (and possibly later). With some
    VOs (like ``gpu``), this might be never known in advance, but only when
    the decoder attempted to create the hw decoder successfully. (Using
    ``--gpu-hwdec-interop`` can load it eagerly.) If there are multiple
    drivers loaded, they will be separated by ``,``.

    If no VO is active or no interop driver is known, this property is
    unavailable.

    This does not necessarily use the same values as ``hwdec``. There can be
    multiple interop drivers for the same hardware decoder, depending on
    platform and VO.

``video-format``
    Video format as string.

``video-codec``
    Video codec selected for decoding.

``width``, ``height``
    Video size. This uses the size of the video as decoded, or if no video
    frame has been decoded yet, the (possibly incorrect) container indicated
    size.

``video-params``
    Video parameters, as output by the decoder (with overrides like aspect
    etc. applied). This has a number of sub-properties:

    ``video-params/pixelformat``
        The pixel format as string. This uses the same names as used in other
        places of mpv.

    ``video-params/hw-pixelformat``
        The underlying pixel format as string. This is relevant for some cases
        of hardware decoding and unavailable otherwise.

    ``video-params/average-bpp``
        Average bits-per-pixel as integer. Subsampled planar formats use a
        different resolution, which is the reason this value can sometimes be
        odd or confusing. Can be unavailable with some formats.

    ``video-params/w``, ``video-params/h``
        Video size as integers, with no aspect correction applied.

    ``video-params/dw``, ``video-params/dh``
        Video size as integers, scaled for correct aspect ratio.

    ``video-params/aspect``
        Display aspect ratio as float.

    ``video-params/par``
        Pixel aspect ratio.

    ``video-params/colormatrix``
        The colormatrix in use as string. (Exact values subject to change.)

    ``video-params/colorlevels``
        The colorlevels as string. (Exact values subject to change.)

    ``video-params/primaries``
        The primaries in use as string. (Exact values subject to change.)

    ``video-params/gamma``
        The gamma function in use as string. (Exact values subject to change.)

    ``video-params/sig-peak``
        The video file's tagged signal peak as float.

    ``video-params/light``
        The light type in use as a string. (Exact values subject to change.)

    ``video-params/chroma-location``
        Chroma location as string. (Exact values subject to change.)

    ``video-params/rotate``
        Intended display rotation in degrees (clockwise).

    ``video-params/stereo-in``
        Source file stereo 3D mode. (See the ``format`` video filter's
        ``stereo-in`` option.)

    ``video-params/alpha``
        Alpha type. If the format has no alpha channel, this will be unavailable
        (but in future releases, it could change to ``no``). If alpha is
        present, this is set to ``straight`` or ``premul``.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_MAP
            "pixelformat"       MPV_FORMAT_STRING
            "hw-pixelformat"    MPV_FORMAT_STRING
            "w"                 MPV_FORMAT_INT64
            "h"                 MPV_FORMAT_INT64
            "dw"                MPV_FORMAT_INT64
            "dh"                MPV_FORMAT_INT64
            "aspect"            MPV_FORMAT_DOUBLE
            "par"               MPV_FORMAT_DOUBLE
            "colormatrix"       MPV_FORMAT_STRING
            "colorlevels"       MPV_FORMAT_STRING
            "primaries"         MPV_FORMAT_STRING
            "gamma"             MPV_FORMAT_STRING
            "sig-peak"          MPV_FORMAT_DOUBLE
            "light"             MPV_FORMAT_STRING
            "chroma-location"   MPV_FORMAT_STRING
            "rotate"            MPV_FORMAT_INT64
            "stereo-in"         MPV_FORMAT_STRING
            "average-bpp"       MPV_FORMAT_INT64
            "alpha"             MPV_FORMAT_STRING

``dwidth``, ``dheight``
    Video display size. This is the video size after filters and aspect scaling
    have been applied. The actual video window size can still be different
    from this, e.g. if the user resized the video window manually.

    These have the same values as ``video-out-params/dw`` and
    ``video-out-params/dh``.

``video-dec-params``
    Exactly like ``video-params``, but no overrides applied.

``video-out-params``
    Same as ``video-params``, but after video filters have been applied. If
    there are no video filters in use, this will contain the same values as
    ``video-params``. Note that this is still not necessarily what the video
    window uses, since the user can change the window size, and all real VOs
    do their own scaling independently from the filter chain.

    Has the same sub-properties as ``video-params``.

``video-frame-info``
    Approximate information of the current frame. Note that if any of these
    are used on OSD, the information might be off by a few frames due to OSD
    redrawing and frame display being somewhat disconnected, and you might
    have to pause and force a redraw.

    This has a number of sub-properties:

    ``video-frame-info/picture-type``
        The type of the picture. It can be "I" (intra), "P" (predicted), "B"
        (bi-dir predicted) or unavailable.

    ``video-frame-info/interlaced``
        Whether the content of the frame is interlaced.

    ``video-frame-info/tff``
        If the content is interlaced, whether the top field is displayed first.

    ``video-frame-info/repeat``
        Whether the frame must be delayed when decoding.

``container-fps``
    Container FPS. This can easily contain bogus values. For videos that use
    modern container formats or video codecs, this will often be incorrect.

    (Renamed from ``fps``.)

``estimated-vf-fps``
    Estimated/measured FPS of the video filter chain output. (If no filters
    are used, this corresponds to decoder output.) This uses the average of
    the 10 past frame durations to calculate the FPS. It will be inaccurate
    if frame-dropping is involved (such as when framedrop is explicitly
    enabled, or after precise seeking). Files with imprecise timestamps (such
    as Matroska) might lead to unstable results.

``window-scale`` (RW)
    Window size multiplier. Setting this will resize the video window to the
    values contained in ``dwidth`` and ``dheight`` multiplied with the value
    set with this property. Setting ``1`` will resize to original video size
    (or to be exact, the size the video filters output). ``2`` will set the
    double size, ``0.5`` halves the size.

    Note that setting a value identical to its previous value will not resize
    the window. That's because this property mirrors the ``window-scale``
    option, and setting an option to its previous value is ignored. If this
    value is set while the window is in a fullscreen, the multiplier is not
    applied until the window is taken out of that state. Writing this property
    to a maximized window can unmaximize the window depending on the OS and
    window manager. If the window does not unmaximize, the multiplier will be
    applied if the user unmaximizes the window later.

    See ``current-window-scale`` for the value derived from the actual window
    size.

    Since mpv 0.31.0, this always returns the previously set value (or the
    default value), instead of the value implied by the actual window size.
    Before mpv 0.31.0, this returned what ``current-window-scale`` returns now,
    after the window was created.

``current-window-scale`` (RW)
    The ``window-scale`` value calculated from the current window size. This
    has the same value as ``window-scale`` if the window size was not changed
    since setting the option, and the window size was not restricted in other
    ways. If the window is fullscreened, this will return the scale value
    calculated from the last non-fullscreen size of the window. The property
    is unavailable if no video is active.

    When setting this property in the fullscreen or maximized state, the behavior
    is the same as window-scale. In all ther cases, setting the value of this
    property will always resize the window. This does not affect the value of
    ``window-scale``.

``focused``
    Whether the window has focus. Might not be supported by all VOs.

``display-names``
    Names of the displays that the mpv window covers. On X11, these
    are the xrandr names (LVDS1, HDMI1, DP1, VGA1, etc.). On Windows, these
    are the GDI names (\\.\DISPLAY1, \\.\DISPLAY2, etc.) and the first display
    in the list will be the one that Windows considers associated with the
    window (as determined by the MonitorFromWindow API.) On macOS these are the
    Display Product Names as used in the System Information and only one display
    name is returned since a window can only be on one screen.

``display-fps``
    The refresh rate of the current display. Currently, this is the lowest FPS
    of any display covered by the video, as retrieved by the underlying system
    APIs (e.g. xrandr on X11). It is not the measured FPS. It's not necessarily
    available on all platforms. Note that any of the listed facts may change
    any time without a warning.

    Writing to this property is deprecated. It has the same effect as writing to
    ``override-display-fps``. Since mpv 0.31.0, this property is unavailable
    if no display FPS was reported (e.g. if no video is active), while in older
    versions, it returned the ``--display-fps`` option value.

``estimated-display-fps``
    The actual rate at which display refreshes seem to occur, measured by
    system time. Only available if display-sync mode (as selected by
    ``--video-sync``) is active.

``vsync-jitter``
    Estimated deviation factor of the vsync duration.

``display-width``, ``display-height``
    The current display's horizontal and vertical resolution in pixels. Whether
    or not these values update as the mpv window changes displays depends on
    the windowing backend. It may not be available on all platforms.

``display-hidpi-scale``
    The HiDPI scale factor as reported by the windowing backend. If no VO is
    active, or if the VO does not report a value, this property is unavailable.
    It may be saner to report an absolute DPI, however, this is the way HiDPI
    support is implemented on most OS APIs. See also ``--hidpi-window-scale``.

``video-aspect`` (RW)
    Deprecated. This is tied to ``--video-aspect-override``, but always
    reports the current video aspect if video is active.

    The read and write components of this option can be split up into
    ``video-params/aspect`` and ``video-aspect-override`` respectively.

``osd-width``, ``osd-height``
    Last known OSD width (can be 0). This is needed if you want to use the
    ``overlay-add`` command. It gives you the actual OSD/window size (not
    including decorations drawn by the OS window manager).

    Alias to ``osd-dimensions/w`` and ``osd-dimensions/h``.

``osd-par``
    Last known OSD display pixel aspect (can be 0).

    Alias to ``osd-dimensions/osd-par``.

``osd-dimensions``
    Last known OSD dimensions.

    Has the following sub-properties (which can be read as ``MPV_FORMAT_NODE``
    or Lua table with ``mp.get_property_native``):

    ``osd-dimensions/w``
        Size of the VO window in OSD render units (usually pixels, but may be
        scaled pixels with VOs like ``xv``).

    ``osd-dimensions/h``
        Size of the VO window in OSD render units,

    ``osd-dimensions/par``
        Pixel aspect ratio of the OSD (usually 1).

    ``osd-dimensions/aspect``
        Display aspect ratio of the VO window. (Computing from the properties
        above.)

    ``osd-dimensions/mt``, ``osd-dimensions/mb``, ``osd-dimensions/ml``, ``osd-dimensions/mr``
        OSD to video margins (top, bottom, left, right). This describes the
        area into which the video is rendered.

    Any of these properties may be unavailable or set to dummy values if the
    VO window is not created or visible.

``mouse-pos``
    Read-only - last known mouse position, normalizd to OSD dimensions.

    Has the following sub-properties (which can be read as ``MPV_FORMAT_NODE``
    or Lua table with ``mp.get_property_native``):

    ``mouse-pos/x``, ``mouse-pos/y``
        Last known coordinates of the mouse pointer.

    ``mouse-pos/hover``
        Boolean - whether the mouse pointer hovers the video window. The
        coordinates should be ignored when this value is false, because the
        video backends update them only when the pointer hovers the window.

``sub-text``
    The current subtitle text regardless of sub visibility. Formatting is
    stripped. If the subtitle is not text-based (i.e. DVD/BD subtitles), an
    empty string is returned.

    This property is experimental and might be removed in the future.

``sub-text-ass``
    Like ``sub-text``, but return the text in ASS format. Text subtitles in
    other formats are converted. For native ASS subtitles, events that do
    not contain any text (but vector drawings etc.) are not filtered out. If
    multiple events match with the current playback time, they are concatenated
    with line breaks. Contains only the "Text" part of the events.

    This property is not enough to render ASS subtitles correctly, because ASS
    header and per-event metadata are not returned. You likely need to do
    further filtering on the returned string to make it useful.

    This property is experimental and might be removed in the future.

``secondary-sub-text``
    Same as ``sub-text``, but for the secondary subtitles.

``sub-start``
    The current subtitle start time (in seconds). If there's multiple current
    subtitles, returns the first start time. If no current subtitle is present
    null is returned instead.

``secondary-sub-start``
    Same as ``sub-start``, but for the secondary subtitles.

``sub-end``
    The current subtitle end time (in seconds). If there's multiple current
    subtitles, return the last end time. If no current subtitle is present, or
    if it's present but has unknown or incorrect duration, null is returned
    instead.

``secondary-sub-end``
    Same as ``sub-end``, but for the secondary subtitles.

``playlist-pos`` (RW)
    Current position on playlist. The first entry is on position 0. Writing to
    this property may start playback at the new position.

    In some cases, this is not necessarily the currently playing file. See
    explanation of ``current`` and ``playing`` flags in ``playlist``.

    If there the playlist is empty, or if it's non-empty, but no entry is
    "current", this property returns -1. Likewise, writing -1 will put the
    player into idle mode (or exit playback if idle mode is not enabled). If an
    out of range index is written to the property, this behaves as if writing -1.
    (Before mpv 0.33.0, instead of returning -1, this property was unavailable
    if no playlist entry was current.)

    Writing the current value back to the property is subject to change.
    Currently, it will restart playback of the playlist entry. But in the
    future, writing the current value will be ignored. Use the
    ``playlist-play-index`` command to get guaranteed behavior.

``playlist-pos-1`` (RW)
    Same as ``playlist-pos``, but 1-based.

``playlist-current-pos`` (RW)
    Index of the "current" item on playlist. This usually, but not necessarily,
    the currently playing item (see ``playlist-playing-pos``). Depending on the
    exact internal state of the player, it may refer to the playlist item to
    play next, or the playlist item used to determine what to play next.

    For reading, this is exactly the same as ``playlist-pos``.

    For writing, this *only* sets the position of the "current" item, without
    stopping playback of the current file (or starting playback, if this is done
    in idle mode). Use -1 to remove the current flag.

    This property is only vaguely useful. If set during playback, it will
    typically cause the playlist entry *after* it to be played next. Another
    possibly odd observable state is that if ``playlist-next`` is run during
    playback, this property is set to the playlist entry to play next (unlike
    the previous case). There is an internal flag that decides whether the
    current playlist entry or the next one should be played, and this flag is
    currently inaccessible for API users. (Whether this behavior will kept is
    possibly subject to change.)

``playlist-playing-pos``
    Index of the "playing" item on playlist. A playlist item is "playing" if
    it's being loaded, actually playing, or being unloaded. This property is set
    during the ``MPV_EVENT_START_FILE`` (``start-file``) and the
    ``MPV_EVENT_START_END`` (``end-file``) events. Outside of that, it returns
    -1. If the playlist entry was somehow removed during playback, but playback
    hasn't stopped yet, or is in progress of being stopped, it also returns -1.
    (This can happen at least during state transitions.)

    In the "playing" state, this is usually the same as ``playlist-pos``, except
    during state changes, or if ``playlist-current-pos`` was written explicitly.

``playlist-count``
    Number of total playlist entries.

``playlist``
    Playlist, current entry marked. Currently, the raw property value is
    useless.

    This has a number of sub-properties. Replace ``N`` with the 0-based playlist
    entry index.

    ``playlist/count``
        Number of playlist entries (same as ``playlist-count``).

    ``playlist/N/filename``
        Filename of the Nth entry.

    ``playlist/N/playing``
        ``yes``/true if the ``playlist-playing-pos`` property points to this
        entry, ``no``/false or unavailable otherwise.

    ``playlist/N/current``
        ``yes``/true if the ``playlist-current-pos`` property points to this
        entry, ``no``/false or unavailable otherwise.

    ``playlist/N/title``
        Name of the Nth entry. Only available if the playlist file contains
        such fields, and only if mpv's parser supports it for the given
        playlist format.

    ``playlist/N/id``
        Unique ID for this entry. This is an automatically assigned integer ID
        that is unique for the entire life time of the current mpv core
        instance. Other commands, events, etc. use this as ``playlist_entry_id``
        fields.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each playlist entry)
                "filename"  MPV_FORMAT_STRING
                "current"   MPV_FORMAT_FLAG (might be missing; since mpv 0.7.0)
                "playing"   MPV_FORMAT_FLAG (same)
                "title"     MPV_FORMAT_STRING (optional)
                "id"        MPV_FORMAT_INT64

``track-list``
    List of audio/video/sub tracks, current entry marked. Currently, the raw
    property value is useless.

    This has a number of sub-properties. Replace ``N`` with the 0-based track
    index.

    ``track-list/count``
        Total number of tracks.

    ``track-list/N/id``
        The ID as it's used for ``-sid``/``--aid``/``--vid``. This is unique
        within tracks of the same type (sub/audio/video), but otherwise not.

    ``track-list/N/type``
        String describing the media type. One of ``audio``, ``video``, ``sub``.

    ``track-list/N/src-id``
        Track ID as used in the source file. Not always available. (It is
        missing if the format has no native ID, if the track is a pseudo-track
        that does not exist in this way in the actual file, or if the format
        is handled by libavformat, and the format was not whitelisted as having
        track IDs.)

    ``track-list/N/title``
        Track title as it is stored in the file. Not always available.

    ``track-list/N/lang``
        Track language as identified by the file. Not always available.

    ``track-list/N/image``
        ``yes``/true if this is a video track that consists of a single
        picture, ``no``/false or unavailable otherwise. The heuristic used to
        determine if a stream is an image doesn't attempt to detect images in
        codecs normally used for videos. Otherwise, it is reliable.

    ``track-list/N/albumart``
        ``yes``/true if this is an image embedded in an audio file or external
        cover art, ``no``/false or unavailable otherwise.

    ``track-list/N/default``
        ``yes``/true if the track has the default flag set in the file,
        ``no``/false or unavailable otherwise.

    ``track-list/N/forced``
        ``yes``/true if the track has the forced flag set in the file,
        ``no``/false or unavailable otherwise.

    ``track-list/N/codec``
        The codec name used by this track, for example ``h264``. Unavailable
        in some rare cases.

    ``track-list/N/external``
        ``yes``/true if the track is an external file, ``no``/false or
        unavailable otherwise. This is set for separate subtitle files.

    ``track-list/N/external-filename``
        The filename if the track is from an external file, unavailable
        otherwise.

    ``track-list/N/selected``
        ``yes``/true if the track is currently decoded, ``no``/false or
        unavailable otherwise.

    ``track-list/N/main-selection``
        It indicates the selection order of tracks for the same type.
        If a track is not selected, or is selected by the ``--lavfi-complex``,
        it is not available. For subtitle tracks, ``0`` represents the ``sid``,
        and ``1`` represents the ``secondary-sid``.

    ``track-list/N/ff-index``
        The stream index as usually used by the FFmpeg utilities. Note that
        this can be potentially wrong if a demuxer other than libavformat
        (``--demuxer=lavf``) is used. For mkv files, the index will usually
        match even if the default (builtin) demuxer is used, but there is
        no hard guarantee.

    ``track-list/N/decoder-desc``
        If this track is being decoded, the human-readable decoder name,

    ``track-list/N/demux-w``, ``track-list/N/demux-h``
        Video size hint as indicated by the container. (Not always accurate.)

    ``track-list/N/demux-channel-count``
        Number of audio channels as indicated by the container. (Not always
        accurate - in particular, the track could be decoded as a different
        number of channels.)

    ``track-list/N/demux-channels``
        Channel layout as indicated by the container. (Not always accurate.)

    ``track-list/N/demux-samplerate``
        Audio sample rate as indicated by the container. (Not always accurate.)

    ``track-list/N/demux-fps``
        Video FPS as indicated by the container. (Not always accurate.)

    ``track-list/N/demux-bitrate``
        Audio average bitrate, in bits per second. (Not always accurate.)

    ``track-list/N/demux-rotation``
        Video clockwise rotation metadata, in degrees.

    ``track-list/N/demux-par``
        Pixel aspect ratio.

    ``track-list/N/audio-channels`` (deprecated)
        Deprecated alias for ``track-list/N/demux-channel-count``.

    ``track-list/N/replaygain-track-peak``, ``track-list/N/replaygain-track-gain``
        Per-track replaygain values. Only available for audio tracks with
        corresponding information stored in the source file.

    ``track-list/N/replaygain-album-peak``, ``track-list/N/replaygain-album-gain``
        Per-album replaygain values. If the file has per-track but no per-album
        information, the per-album values will be copied from the per-track
        values currently. It's possible that future mpv versions will make
        these properties unavailable instead in this case.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each track)
                "id"                MPV_FORMAT_INT64
                "type"              MPV_FORMAT_STRING
                "src-id"            MPV_FORMAT_INT64
                "title"             MPV_FORMAT_STRING
                "lang"              MPV_FORMAT_STRING
                "image"             MPV_FORMAT_FLAG
                "albumart"          MPV_FORMAT_FLAG
                "default"           MPV_FORMAT_FLAG
                "forced"            MPV_FORMAT_FLAG
                "selected"          MPV_FORMAT_FLAG
                "main-selection"    MPV_FORMAT_INT64
                "external"          MPV_FORMAT_FLAG
                "external-filename" MPV_FORMAT_STRING
                "codec"             MPV_FORMAT_STRING
                "ff-index"          MPV_FORMAT_INT64
                "decoder-desc"      MPV_FORMAT_STRING
                "demux-w"           MPV_FORMAT_INT64
                "demux-h"           MPV_FORMAT_INT64
                "demux-channel-count" MPV_FORMAT_INT64
                "demux-channels"    MPV_FORMAT_STRING
                "demux-samplerate"  MPV_FORMAT_INT64
                "demux-fps"         MPV_FORMAT_DOUBLE
                "demux-bitrate"     MPV_FORMAT_INT64
                "demux-rotation"    MPV_FORMAT_INT64
                "demux-par"         MPV_FORMAT_DOUBLE
                "audio-channels"    MPV_FORMAT_INT64
                "replaygain-track-peak" MPV_FORMAT_DOUBLE
                "replaygain-track-gain" MPV_FORMAT_DOUBLE
                "replaygain-album-peak" MPV_FORMAT_DOUBLE
                "replaygain-album-gain" MPV_FORMAT_DOUBLE

``current-tracks/...``
    This gives access to currently selected tracks. It redirects to the correct
    entry in ``track-list``.

    The following sub-entries are defined: ``video``, ``audio``, ``sub``,
    ``sub2``

    For example, ``current-tracks/audio/lang`` returns the current audio track's
    language field (the same value as ``track-list/N/lang``).

    If tracks of the requested type are selected via ``--lavfi-complex``, the
    first one is returned.

``chapter-list``
    List of chapters, current entry marked. Currently, the raw property value
    is useless.

    This has a number of sub-properties. Replace ``N`` with the 0-based chapter
    index.

    ``chapter-list/count``
        Number of chapters.

    ``chapter-list/N/title``
        Chapter title as stored in the file. Not always available.

    ``chapter-list/N/time``
        Chapter start time in seconds as float.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each chapter)
                "title" MPV_FORMAT_STRING
                "time"  MPV_FORMAT_DOUBLE

``af``, ``vf`` (RW)
    See ``--vf``/``--af`` and the ``vf``/``af`` command.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each filter entry)
                "name"      MPV_FORMAT_STRING
                "label"     MPV_FORMAT_STRING [optional]
                "enabled"   MPV_FORMAT_FLAG [optional]
                "params"    MPV_FORMAT_NODE_MAP [optional]
                    "key"   MPV_FORMAT_STRING
                    "value" MPV_FORMAT_STRING

    It's also possible to write the property using this format.

``seekable``
    Whether it's generally possible to seek in the current file.

``partially-seekable``
    Whether the current file is considered seekable, but only because the cache
    is active. This means small relative seeks may be fine, but larger seeks
    may fail anyway. Whether a seek will succeed or not is generally not known
    in advance.

    If this property returns ``yes``/true, so will ``seekable``.

``playback-abort``
    Whether playback is stopped or is to be stopped. (Useful in obscure
    situations like during ``on_load`` hook processing, when the user can stop
    playback, but the script has to explicitly end processing.)

``cursor-autohide`` (RW)
    See ``--cursor-autohide``. Setting this to a new value will always update
    the cursor, and reset the internal timer.

``osd-sym-cc``
    Inserts the current OSD symbol as opaque OSD control code (cc). This makes
    sense only with the ``show-text`` command or options which set OSD messages.
    The control code is implementation specific and is useless for anything else.

``osd-ass-cc``
    ``${osd-ass-cc/0}`` disables escaping ASS sequences of text in OSD,
    ``${osd-ass-cc/1}`` enables it again. By default, ASS sequences are
    escaped to avoid accidental formatting, and this property can disable
    this behavior. Note that the properties return an opaque OSD control
    code, which only makes sense for the ``show-text`` command or options
    which set OSD messages.

    .. admonition:: Example

        - ``--osd-msg3='This is ${osd-ass-cc/0}{\\b1}bold text'``
        - ``show-text "This is ${osd-ass-cc/0}{\\b1}bold text"``

    Any ASS override tags as understood by libass can be used.

    Note that you need to escape the ``\`` character, because the string is
    processed for C escape sequences before passing it to the OSD code. See
    `Flat command syntax`_ for details.

    A list of tags can be found here: http://docs.aegisub.org/latest/ASS_Tags/

``vo-configured``
    Whether the VO is configured right now. Usually this corresponds to whether
    the video window is visible. If the ``--force-window`` option is used, this
    usually always returns ``yes``/true.

``vo-passes``
    Contains introspection about the VO's active render passes and their
    execution times. Not implemented by all VOs.

    This is further subdivided into two frame types, ``vo-passes/fresh`` for
    fresh frames (which have to be uploaded, scaled, etc.) and
    ``vo-passes/redraw`` for redrawn frames (which only have to be re-painted).
    The number of passes for any given subtype can change from frame to frame,
    and should not be relied upon.

    Each frame type has a number of further sub-properties. Replace ``TYPE``
    with the frame type, ``N`` with the 0-based pass index, and ``M`` with the
    0-based sample index.

    ``vo-passes/TYPE/count``
        Number of passes.

    ``vo-passes/TYPE/N/desc``
        Human-friendy description of the pass.

    ``vo-passes/TYPE/N/last``
        Last measured execution time, in nanoseconds.

    ``vo-passes/TYPE/N/avg``
        Average execution time of this pass, in nanoseconds. The exact
        timeframe varies, but it should generally be a handful of seconds.

    ``vo-passes/TYPE/N/peak``
        The peak execution time (highest value) within this averaging range, in
        nanoseconds.

    ``vo-passes/TYPE/N/count``
        The number of samples for this pass.

    ``vo-passes/TYPE/N/samples/M``
        The raw execution time of a specific sample for this pass, in
        nanoseconds.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_MAP
        "TYPE" MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP
                "desc"    MPV_FORMAT_STRING
                "last"    MPV_FORMAT_INT64
                "avg"     MPV_FORMAT_INT64
                "peak"    MPV_FORMAT_INT64
                "count"   MPV_FORMAT_INT64
                "samples" MPV_FORMAT_NODE_ARRAY
                     MP_FORMAT_INT64

    Note that directly accessing this structure via subkeys is not supported,
    the only access is through aforementioned ``MPV_FORMAT_NODE``.

``perf-info``
    Further performance data. Querying this property triggers internal
    collection of some data, and may slow down the player. Each query will reset
    some internal state. Property change notification doesn't and won't work.
    All of this may change in the future, so don't use this. The builtin
    ``stats`` script is supposed to be the only user; since it's bundled and
    built with the source code, it can use knowledge of mpv internal to render
    the information properly. See ``stats`` script description for some details.

``video-bitrate``, ``audio-bitrate``, ``sub-bitrate``
    Bitrate values calculated on the packet level. This works by dividing the
    bit size of all packets between two keyframes by their presentation
    timestamp distance. (This uses the timestamps are stored in the file, so
    e.g. playback speed does not influence the returned values.) In particular,
    the video bitrate will update only per keyframe, and show the "past"
    bitrate. To make the property more UI friendly, updates to these properties
    are throttled in a certain way.

    The unit is bits per second. OSD formatting turns these values in kilobits
    (or megabits, if appropriate), which can be prevented by using the
    raw property value, e.g. with ``${=video-bitrate}``.

    Note that the accuracy of these properties is influenced by a few factors.
    If the underlying demuxer rewrites the packets on demuxing (done for some
    file formats), the bitrate might be slightly off. If timestamps are bad
    or jittery (like in Matroska), even constant bitrate streams might show
    fluctuating bitrate.

    How exactly these values are calculated might change in the future.

    In earlier versions of mpv, these properties returned a static (but bad)
    guess using a completely different method.

``packet-video-bitrate``, ``packet-audio-bitrate``, ``packet-sub-bitrate``
    Old and deprecated properties for ``video-bitrate``, ``audio-bitrate``,
    ``sub-bitrate``. They behave exactly the same, but return a value in
    kilobits. Also, they don't have any OSD formatting, though the same can be
    achieved with e.g. ``${=video-bitrate}``.

    These properties shouldn't be used anymore.

``audio-device-list``
    The list of discovered audio devices. This is mostly for use with the
    client API, and reflects what ``--audio-device=help`` with the command line
    player returns.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each device entry)
                "name"          MPV_FORMAT_STRING
                "description"   MPV_FORMAT_STRING

    The ``name`` is what is to be passed to the ``--audio-device`` option (and
    often a rather cryptic audio API-specific ID), while ``description`` is
    human readable free form text. The description is set to the device name
    (minus mpv-specific ``<driver>/`` prefix) if no description is available
    or the description would have been an empty string.

    The special entry with the name set to ``auto`` selects the default audio
    output driver and the default device.

    The property can be watched with the property observation mechanism in
    the client API and in Lua scripts. (Technically, change notification is
    enabled the first time this property is read.)

``audio-device`` (RW)
    Set the audio device. This directly reads/writes the ``--audio-device``
    option, but on write accesses, the audio output will be scheduled for
    reloading.

    Writing this property while no audio output is active will not automatically
    enable audio. (This is also true in the case when audio was disabled due to
    reinitialization failure after a previous write access to ``audio-device``.)

    This property also doesn't tell you which audio device is actually in use.

    How these details are handled may change in the future.

``current-vo``
    Current video output driver (name as used with ``--vo``).

``current-ao``
    Current audio output driver (name as used with ``--ao``).

``shared-script-properties`` (RW)
    This is a key/value map of arbitrary strings shared between scripts for
    general use. The player itself does not use any data in it (although some
    builtin scripts may). The property is not preserved across player restarts.

    This is very primitive, inefficient, and annoying to use. It's a makeshift
    solution which could go away any time (for example, when a better solution
    becomes available). This is also why this property has an annoying name. You
    should avoid using it, unless you absolutely have to.

    Lua scripting has helpers starting with ``utils.shared_script_property_``.
    They are undocumented because you should not use this property. If you still
    think you must, you should use the helpers instead of the property directly.

    You are supposed to use the ``change-list`` command to modify the contents.
    Reading, modifying, and writing the property manually could data loss if two
    scripts update different keys at the same time due to lack of
    synchronization. The Lua helpers take care of this.

    (There is no way to ensure synchronization if two scripts try to update the
    same key at the same time.)

``working-directory``
    The working directory of the mpv process. Can be useful for JSON IPC users,
    because the command line player usually works with relative paths.

``protocol-list``
    List of protocol prefixes potentially recognized by the player. They are
    returned without trailing ``://`` suffix (which is still always required).
    In some cases, the protocol will not actually be supported (consider
    ``https`` if ffmpeg is not compiled with TLS support).

``decoder-list``
    List of decoders supported. This lists decoders which can be passed to
    ``--vd`` and ``--ad``.

    ``codec``
        Canonical codec name, which identifies the format the decoder can
        handle.

    ``driver``
        The name of the decoder itself. Often, this is the same as ``codec``.
        Sometimes it can be different. It is used to distinguish multiple
        decoders for the same codec.

    ``description``
        Human readable description of the decoder and codec.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each decoder entry)
                "codec"         MPV_FORMAT_STRING
                "driver"        MPV_FORMAT_STRING
                "description"   MPV_FORMAT_STRING

``encoder-list``
    List of libavcodec encoders. This has the same format as ``decoder-list``.
    The encoder names (``driver`` entries) can be passed to ``--ovc`` and
    ``--oac`` (without the ``lavc:`` prefix required by ``--vd`` and ``--ad``).

``demuxer-lavf-list``
    List of available libavformat demuxers' names. This can be used to check
    for support for a specific format or use with ``--demuxer-lavf-format``.

``input-key-list``
    List of `Key names`_, same as output by ``--input-keylist``.

``mpv-version``
    The mpv version/copyright string. Depending on how the binary was built, it
    might contain either a release version, or just a git hash.

``mpv-configuration``
    The configuration arguments which were passed to the build system
    (typically the way ``./waf configure ...`` was invoked).

``ffmpeg-version``
    The contents of the ``av_version_info()`` API call. This is a string which
    identifies the build in some way, either through a release version number,
    or a git hash. This applies to Libav as well (the property is still named
    the same.) This property is unavailable if mpv is linked against older
    FFmpeg and Libav versions.

``libass-version``
    The value of ``ass_library_version()``. This is an integer, encoded in a
    somewhat weird form (apparently "hex BCD"), indicating the release version
    of the libass library linked to mpv.

``options/<name>`` (RW)
    The value of option ``--<name>``. Most options can be changed at runtime by
    writing to this property. Note that many options require reloading the file
    for changes to take effect. If there is an equivalent property, prefer
    setting the property instead.

    There shouldn't be any reason to access ``options/<name>`` instead of
    ``<name>``, except in situations in which the properties have different
    behavior or conflicting semantics.

``file-local-options/<name>`` (RW)
    Similar to ``options/<name>``, but when setting an option through this
    property, the option is reset to its old value once the current file has
    stopped playing. Trying to write an option while no file is playing (or
    is being loaded) results in an error.

    (Note that if an option is marked as file-local, even ``options/`` will
    access the local value, and the ``old`` value, which will be restored on
    end of playback, cannot be read or written until end of playback.)

``option-info/<name>``
    Additional per-option information.

    This has a number of sub-properties. Replace ``<name>`` with the name of
    a top-level option. No guarantee of stability is given to any of these
    sub-properties - they may change radically in the feature.

    ``option-info/<name>/name``
        The name of the option.

    ``option-info/<name>/type``
        The name of the option type, like ``String`` or ``Integer``. For many
        complex types, this isn't very accurate.

    ``option-info/<name>/set-from-commandline``
        Whether the option was set from the mpv command line. What this is set
        to if the option is e.g. changed at runtime is left undefined (meaning
        it could change in the future).

    ``option-info/<name>/set-locally``
        Whether the option was set per-file. This is the case with
        automatically loaded profiles, file-dir configs, and other cases. It
        means the option value will be restored to the value before playback
        start when playback ends.

    ``option-info/<name>/default-value``
        The default value of the option. May not always be available.

    ``option-info/<name>/min``, ``option-info/<name>/max``
        Integer minimum and maximum values allowed for the option. Only
        available if the options are numeric, and the minimum/maximum has been
        set internally. It's also possible that only one of these is set.

    ``option-info/<name>/choices``
        If the option is a choice option, the possible choices. Choices that
        are integers may or may not be included (they can be implied by ``min``
        and ``max``). Note that options which behave like choice options, but
        are not actual choice options internally, may not have this info
        available.

``property-list``
    The list of top-level properties.

``profile-list``
    The list of profiles and their contents. This is highly
    implementation-specific, and may change any time. Currently, it returns an
    array of options for each profile. Each option has a name and a value, with
    the value currently always being a string. Note that the options array is
    not a map, as order matters and duplicate entries are possible. Recursive
    profiles are not expanded, and show up as special ``profile`` options.

``command-list``
    The list of input commands. This returns an array of maps, where each map
    node represents a command. This map currently only has a single entry:
    ``name`` for the name of the command. (This property is supposed to be a
    replacement for ``--input-cmdlist``. The option dumps some more
    information, but it's a valid feature request to extend this property if
    needed.)

``input-bindings``
    The list of current input key bindings. This returns an array of maps,
    where each map node represents a binding for a single key/command. This map
    has the following entries:

    ``key``
        The key name. This is normalized and may look slightly different from
        how it was specified in the source (e.g. in input.conf).

    ``cmd``
        The command mapped to the key. (Currently, this is exactly the same
        string as specified in the source, other than stripping whitespace and
        comments. It's possible that it will be normalized in the future.)

    ``is_weak``
        If set to true, any existing and active user bindings will take priority.

    ``owner``
        If this entry exists, the name of the script (or similar) which added
        this binding.

    ``section``
        Name of the section this binding is part of. This is a rarely used
        mechanism. This entry may be removed or change meaning in the future.

    ``priority``
        A number. Bindings with a higher value are preferred over bindings
        with a lower value. If the value is negative, this binding is inactive
        and will not be triggered by input. Note that mpv does not use this
        value internally, and matching of bindings may work slightly differently
        in some cases. In addition, this value is dynamic and can change around
        at runtime.

    ``comment``
        If available, the comment following the command on the same line. (For
        example, the input.conf entry ``f cycle bla # toggle bla`` would
        result in an entry with ``comment = "toggle bla", cmd = "cycle bla"``.)

    This property is read-only, and change notification is not supported.
    Currently, there is no mechanism to change key bindings at runtime, other
    than scripts adding or removing their own bindings.

Inconsistencies between options and properties
----------------------------------------------

You can access (almost) all options as properties, though there are some
caveats with some properties (due to historical reasons):

``vid``, ``aid``, ``sid``
    While playback is active, these return the actually active tracks. For
    example, if you set ``aid=5``, and the currently played file contains no
    audio track with ID 5, the ``aid`` property will return ``no``.

    Before mpv 0.31.0, you could set existing tracks at runtime only.

``display-fps``
    This inconsistent behavior is deprecated. Post-deprecation, the reported
    value and the option value are cleanly separated (``override-display-fps``
    for the option value).

``vf``, ``af``
    If you set the properties during playback, and the filter chain fails to
    reinitialize, the option will be set, but the runtime filter chain does not
    change. On the other hand, the next video to be played will fail, because
    the initial filter chain cannot be created.

    This behavior changed in mpv 0.31.0. Before this, the new value was rejected
    *iff* a video (for ``vf``) or an audio (for ``af``) track was active. If
    playback was not active, the behavior was the same as the current one.

``playlist``
    The property is read-only and returns the current internal playlist. The
    option is for loading playlist during command line parsing. For client API
    uses, you should use the ``loadlist`` command instead.

``profile``, ``include``
    These are write-only, and will perform actions as they are written to,
    exactly as if they were used on the mpv CLI commandline. Their only use is
    when using libmpv before ``mpv_initialize()``, which in turn is probably
    only useful in encoding mode. Normal libmpv users should use other
    mechanisms, such as the ``apply-profile`` command, and the
    ``mpv_load_config_file`` API function. Avoid these properties.

Property Expansion
------------------

All string arguments to input commands as well as certain options (like
``--term-playing-msg``) are subject to property expansion. Note that property
expansion does not work in places where e.g. numeric parameters are expected.
(For example, the ``add`` command does not do property expansion. The ``set``
command is an exception and not a general rule.)

.. admonition:: Example for input.conf

    ``i show-text "Filename: ${filename}"``
        shows the filename of the current file when pressing the ``i`` key

Whether property expansion is enabled by default depends on which API is used
(see `Flat command syntax`_, `Commands specified as arrays`_ and `Named
arguments`_), but it can always be enabled with the ``expand-properties``
prefix or disabled with the ``raw`` prefix, as described in `Input Command
Prefixes`_.

The following expansions are supported:

``${NAME}``
    Expands to the value of the property ``NAME``. If retrieving the property
    fails, expand to an error string. (Use ``${NAME:}`` with a trailing
    ``:`` to expand to an empty string instead.)
    If ``NAME`` is prefixed with ``=``, expand to the raw value of the property
    (see section below).
``${NAME:STR}``
    Expands to the value of the property ``NAME``, or ``STR`` if the
    property cannot be retrieved. ``STR`` is expanded recursively.
``${?NAME:STR}``
    Expands to ``STR`` (recursively) if the property ``NAME`` is available.
``${!NAME:STR}``
    Expands to ``STR`` (recursively) if the property ``NAME`` cannot be
    retrieved.
``${?NAME==VALUE:STR}``
    Expands to ``STR`` (recursively) if the property ``NAME`` expands to a
    string equal to ``VALUE``. You can prefix ``NAME`` with ``=`` in order to
    compare the raw value of a property (see section below). If the property
    is unavailable, or other errors happen when retrieving it, the value is
    never considered equal.
    Note that ``VALUE`` can't contain any of the characters ``:`` or ``}``.
    Also, it is possible that escaping with ``"`` or ``%`` might be added in
    the future, should the need arise.
``${!NAME==VALUE:STR}``
    Same as with the ``?`` variant, but ``STR`` is expanded if the value is
    not equal. (Using the same semantics as with ``?``.)
``$$``
    Expands to ``$``.
``$}``
    Expands to ``}``. (To produce this character inside recursive
    expansion.)
``$>``
    Disable property expansion and special handling of ``$`` for the rest
    of the string.

In places where property expansion is allowed, C-style escapes are often
accepted as well. Example:

    - ``\n`` becomes a newline character
    - ``\\`` expands to ``\``

Raw and Formatted Properties
----------------------------

Normally, properties are formatted as human-readable text, meant to be
displayed on OSD or on the terminal. It is possible to retrieve an unformatted
(raw) value from a property by prefixing its name with ``=``. These raw values
can be parsed by other programs and follow the same conventions as the options
associated with the properties.

.. admonition:: Examples

    - ``${time-pos}`` expands to ``00:14:23`` (if playback position is at 14
      minutes 23 seconds)
    - ``${=time-pos}`` expands to ``863.4`` (same time, plus 400 milliseconds -
      milliseconds are normally not shown in the formatted case)

Sometimes, the difference in amount of information carried by raw and formatted
property values can be rather big. In some cases, raw values have more
information, like higher precision than seconds with ``time-pos``. Sometimes
it is the other way around, e.g. ``aid`` shows track title and language in the
formatted case, but only the track number if it is raw.
