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
multiple (or none) commands, all separated by whitespace. String arguments
need to be quoted with ``"``. Details see ``Flat command syntax``.

You can bind multiple commands to one key. For example:

| a show-text "command 1" ; show-text "command 2"

It's also possible to bind a command to a sequence of keys:

| a-b-c show-text "command run after a, b, c have been pressed"

(This is not shown in the general command syntax.)

If ``a`` or ``a-b`` or ``b`` are already bound, this will run the first command
that matches, and the multi-key command will never be called. Intermediate keys
can be remapped to ``ignore`` in order to avoid this issue. The maximum number
of (non-modifier) keys for combinations is currently 4.

Flat command syntax
-------------------

This is the syntax used in input.conf, and referred to "input.conf syntax" in
a number of other places.

``<command> ::= [<prefixes>] <command_name> (<argument>)*``
``<argument> ::= (<string> | " <quoted_string> " )``

``command_name`` is an unquoted string with the command name itself. See
`List of Input Commands`_ for a list.

Arguments are separated by whitespace. This applies even to string arguments.
For this reason, string arguments should be quoted with ``"``. If a string
argument contains spaces or certain special characters, quoting and possibly
escaping is mandatory, or the command cannot be parsed correctly.

Inside quotes, C-style escaping can be used. JSON escapes according to RFC 8259,
minus surrogate pair escapes, should be a safe subset that can be used.

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

Like with array commands, quoting and escaping is inherently not needed in the
normal case.

The name of each command is defined in each command description in the
`List of Input Commands`_. ``--input-cmdlist`` also lists them.

Some commands do not support named arguments (e.g. ``run`` command). You need
to use APIs that pass arguments as arrays.

Named arguments are not supported in the "flat" input.conf syntax, which means
you cannot use them for key bindings in input.conf at all.

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

    By default, ``keyframes`` is used for relative seeks, and ``exact`` is used
    for absolute seeks.

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

``loadfile <url> [<flags> [<options>]]``
    Load the given file or URL and play it.

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
    Not all options can be changed this way. Some options require a restart
    of the player.

``loadlist <url> [<flags>]``
    Load the given playlist file or URL (like ``--playlist``).

    Second argument:

    <replace> (default)
        Stop playback and replace the internal playlist with the new one.
    <append>
        Append the new playlist at the end of the current internal playlist.

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

    This has the following named arguments. The order of them is not guaranteed,
    so you should always call them with named arguments, see `Named arguments`_.

    ``args`` (``MPV_FORMAT_NODE_ARRAY[MPV_FORMAT_STRING]``)
        Array of strings with the command as first argument, and subsequent
        command line arguments following. This is just like the ``run`` command
        argument list.

        The first array entry is either an absolute path to the executable, or
        a filename with no path components, in which case the ``PATH``
        environment variable. On Unix, this is equivalent to ``posix_spawnp``
        and ``execvp`` behavior.

    ``playback_only`` (``MPV_FORMAT_FLAG``)
        Boolean indicating whether the process should be killed when playback
        terminates (optional, default: yes). If enabled, stopping playback
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

    The command returns the following result (as ``MPV_FORMAT_NODE_MAP``):

    ``status`` (``MPV_FORMAT_INT64``)
        The raw exit status of the process. It will be negative on error. The
        meaning of negative values is undefined, other than meaning error (and
        does not necessarily correspond to OS low level exit status values).

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
        killed by mpv as a result of ``playback_only`` being set to ``yes``.

    ``killed_by_us`` (``MPV_FORMAT_FLAG``)
        Set to ``yes`` if the process has been killed by mpv as a result
        of ``playback_only`` being set to ``yes``.

    Note that the command itself will always return success as long as the
    parameters are correct. Whether the process could be spawned or whether
    it was somehow killed or returned an error status has to be queried from
    the result value.

    This command can be asynchronously aborted via API.

    In all cases, the subprocess will be terminated on player exit. Only the
    ``run`` command can start processes in a truly detached way.

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

``sub-step <skip>``
    Change subtitle timing such, that the subtitle event after the next
    ``<skip>`` subtitle events is displayed. ``<skip>`` can be negative to step
    backwards.

``sub-seek <skip>``
    Seek to the next (skip set to 1) or the previous (skip set to -1) subtitle.
    This is similar to ``sub-step``, except that it seeks video and audio
    instead of adjusting the subtitle delay.

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

``show-progress``
    Show the progress bar, the elapsed time and the total duration of the file
    on the OSD.

``write-watch-later-config``
    Write the resume config file that the ``quit-watch-later`` command writes,
    but continue playback normally.

``stop``
    Stop playback and clear playlist. With default settings, this is
    essentially like ``quit``. Useful for the client API: playback can be
    stopped without terminating the player.

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

``audio-add <url> [<flags> [<title> [<lang>]]]``
    Load the given audio file. See ``sub-add`` command.

``audio-remove [<id>]``
    Remove the given audio track. See ``sub-remove`` command.

``audio-reload [<id>]``
    Reload the given audio tracks. See ``sub-reload`` command.

``rescan-external-files [<mode>]``
    Rescan external files according to the current ``--sub-auto`` and
    ``--audio-file-auto`` settings. This can be used to auto-load external
    files *after* the file was loaded.

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

    The first argument decides what happens:

    <set>
        Overwrite the previous filter chain with the new one.

    <add>
        Append the new filter chain to the previous one.

    <toggle>
        Check if the given filter (with the exact parameters) is already
        in the video chain. If yes, remove the filter. If no, add the filter.
        (If several filters are passed to the command, this is done for
        each filter.)

        A special variant is combining this with labels, and using ``@name``
        without filter name and parameters as filter entry. This toggles the
        enable/disable flag.

    <del>
        Remove the given filters from the video chain. Unlike in the other
        cases, the second parameter is a comma separated list of filter names
        or integer indexes. ``0`` would denote the first filter. Negative
        indexes start from the last filter, and ``-1`` denotes the last
        filter.

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

        - ``a vf set flip`` turn video upside-down on the ``a`` key
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
    Disable the named input section. Undoes ``enable-section``.

``define-section <name> <contents> [<flags>]``
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
    Lua scripts can get their name via ``mp.get_script_name()``.

    This command has a variable number of arguments, and cannot be used with
    named arguments.

``script-binding <name>``
    Invoke a script-provided key binding. This can be used to remap key
    bindings provided by external Lua scripts.

    The argument is the name of the binding.

    It can optionally be prefixed with the name of the script, using ``/`` as
    separator, e.g. ``script-binding scriptname/bindingname``.

    For completeness, here is how this command works internally. The details
    could change any time. On any matching key event, ``script-message-to``
    or ``script-message`` is called (depending on whether the script name is
    included), with the following arguments:

    1. The string ``key-binding``.
    2. The name of the binding (as established above).
    3. The key state as string (see below).
    4. The key name (since mpv 0.15.0).

    The key state consists of 2 letters:

    1. One of ``d`` (key was pressed down), ``u`` (was released), ``r`` (key
       is still down, and was repeated; only if key repeat is enabled for this
       binding), ``p`` (key was pressed; happens if up/down can't be tracked).
    2. Whether the event originates from the mouse, either ``m`` (mouse button)
       or ``-`` (something else).

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

``apply-profile <name>``
    Apply the contents of a named profile. This is like using ``profile=name``
    in a config file, except you can map it to a key binding to change it at
    runtime.

    There is no such thing as "unapplying" a profile - applying a profile
    merely sets all option values listed within the profile.

``load-script <filename>``
    Load a script, similar to the ``--script`` option. Whether this waits for
    the script to finish initialization or not changed multiple times, and the
    future behavior is left undefined.

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


Undocumented commands: ``tv-last-channel`` (TV/DVB only),
``ao-reload`` (experimental/internal).

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

The following hooks are currently defined:

``on_load``
    Called when a file is to be opened, before anything is actually done.
    For example, you could read and write the ``stream-open-filename``
    property to redirect an URL to something else (consider support for
    streaming sites which rarely give the user a direct media URL), or
    you could set per-file options with by setting the property
    ``file-local-options/<option name>``. The player will wait until all
    hooks are run.

``on_load_fail``
    Called after after a file has been opened, but failed to. This can be
    used to provide a fallback in case native demuxers failed to recognize
    the file, instead of always running before the native demuxers like
    ``on_load``. Demux will only be retried if ``stream-open-filename``
    was changed.

``on_preloaded``
    Called after a file has been opened, and before tracks are selected and
    decoders are created. This has some usefulness if an API users wants
    to select tracks manually, based on the set of available tracks. It's
    also useful to initialize ``--lavfi-complex`` in a specific way by API,
    without having to "probe" the available streams at first.

    Note that this does not yet apply default track selection. Which operations
    exactly can be done and not be done, and what information is available and
    what is not yet available yet, is all subject to change.

``on_unload``
    Run before closing a file, and before actually uninitializing
    everything. It's not possible to resume playback in this state.

Legacy hook API
~~~~~~~~~~~~~~~

.. warning::

    The legacy API is deprecated and will be removed soon.

There are two special commands involved. Also, the client must listen for
client messages (``MPV_EVENT_CLIENT_MESSAGE`` in the C API).

``hook-add <hook-name> <id> <priority>``
    Subscribe to the hook identified by the first argument (basically, the
    name of event). The ``id`` argument is an arbitrary integer chosen by the
    user. ``priority`` is used to sort all hook handlers globally across all
    clients. Each client can register multiple hook handlers (even for the
    same hook-name). Once the hook is registered, it cannot be unregistered.

    When a specific event happens, all registered handlers are run serially.
    This uses a protocol every client has to follow explicitly. When a hook
    handler is run, a client message (``MPV_EVENT_CLIENT_MESSAGE``) is sent to
    the client which registered the hook. This message has the following
    arguments:

    1. the string ``hook_run``
    2. the ``id`` argument the hook was registered with as string (this can be
       used to correctly handle multiple hooks registered by the same client,
       as long as the ``id`` argument is unique in the client)
    3. something undefined, used by the hook mechanism to track hook execution

    Upon receiving this message, the client can handle the event. While doing
    this, the player core will still react to requests, but playback will
    typically be stopped.

    When the client is done, it must continue the core's hook execution by
    running the ``hook-ack`` command.

``hook-ack <string>``
    Run the next hook in the global chain of hooks. The argument is the 3rd
    argument of the client message that starts hook execution for the
    current client.

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
    This prefix forces enabling key repeat in any case.
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
- In all cases, async mode can still run commands in a synchronous manner, even
  in detached mode. This can for example happen in cases when a command does not
  have an  asynchronous implementation. The async libmpv API still never blocks
  the caller in these cases.

Before mpv 0.29.0, the ``async`` prefix was only used by screenshot commands,
and made them run the file saving code in a detached manner. This is the
default now, and ``async`` changes behavior only in the ways mentioned above.

Currently the following commands have different waiting characteristics with
sync vs. async: sub-add, audio-add, sub-reload, audio-reload,
rescan-external-files, screenshot, screenshot-to-file.

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
    leading ``--`` from the option name. These are not documented. Only
    properties which do not exist as option with the same name, or which have
    very different behavior from the options are documented below.

``audio-speed-correction``, ``video-speed-correction``
    Factor multiplied with ``speed`` at which the player attempts to play the
    file. Usually it's exactly 1. (Display sync mode will make this useful.)

    OSD formatting will display it in the form of ``+1.23456%``, with the number
    being ``(raw - 1) * 100`` for the given raw property value.

``display-sync-active``
    Return whether ``--video-sync=display`` is actually active.

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

``path``
    Full path of the currently played file. Usually this is exactly the same
    string you pass on the mpv command line or the ``loadfile`` command, even
    if it's a relative path. If you expect an absolute path, you will have to
    determine it yourself, for example by using the ``working-directory``
    property.

``media-title``
    If the currently played file has a ``title`` tag, use that.

    Otherwise, if the media type is DVD, return the volume ID of DVD.

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

``audio-pts`` (R)
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

``disc-titles``
    Number of BD/DVD titles.

    This has a number of sub-properties. Replace ``N`` with the 0-based edition
    index.

    ``disc-titles/count``
        Number of titles.

    ``disc-titles/id``
        Title ID as integer. Currently, this is the same as the title index.

    ``disc-titles/length``
        Length in seconds. Can be unavailable in a number of cases (currently
        it works for libdvdnav only).

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each edition)
                "id"                MPV_FORMAT_INT64
                "length"            MPV_FORMAT_DOUBLE

``disc-title-list``
    List of BD/DVD titles.

``disc-title`` (RW)
    Current BD/DVD title number. Writing works only for ``dvdnav://`` and
    ``bd://`` (and aliases for these).

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

    ``edition-list/N/id``
        Edition ID as integer. Use this to set the ``edition`` property.
        Currently, this is the same as the edition index.

    ``edition-list/N/default``
        ``yes`` if this is the default edition, ``no`` otherwise.

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

``angle`` (RW)
    Current DVD angle.

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
    Return ``yes`` if no file is loaded, but the player is staying around
    because of the ``--idle`` option.

    (Renamed from ``idle``.)

``core-idle``
    Return ``yes`` if the playback core is paused, otherwise ``no``. This can
    be different ``pause`` in special situations, such as when the player
    pauses itself due to low network cache.

    This also returns ``yes`` if playback is restarting or if nothing is
    playing at all. In other words, it's only ``no`` if there's actually
    video playing. (Behavior since mpv 0.7.0.)

``cache-speed`` (R)
    Current I/O read speed between the cache and the lower layer (like network).
    This gives the number bytes per seconds over a 1 second window (using
    the type ``MPV_FORMAT_INT64`` for the client API).

``demuxer-cache-duration``
    Approximate duration of video buffered in the demuxer, in seconds. The
    guess is very unreliable, and often the property will not be available
    at all, even if data is buffered.

``demuxer-cache-time``
    Approximate time of video buffered in the demuxer, in seconds. Same as
    ``demuxer-cache-duration`` but returns the last timestamp of buffered
    data in demuxer.

``demuxer-cache-idle``
    Returns ``yes`` if the demuxer is idle, which means the demuxer cache is
    filled to the requested amount, and is currently not reading more data.

``demuxer-cache-state``
    Various undocumented or half-documented things.

    Each entry in ``seekable-ranges`` represents a region in the demuxer cache
    that can be seeked to. If there are multiple demuxers active, this only
    returns information about the "main" demuxer, but might be changed in
    future to return unified information about all demuxers. The ranges are in
    arbitrary order. Often, ranges will overlap for a bit, before being joined.
    In broken corner cases, ranges may overlap all over the place.

    The end of a seek range is usually smaller than the value returned by the
    ``demuxer-cache-time`` property, because that property returns the guessed
    buffering amount, while the seek ranges represent the buffered data that
    can actually be used for cached seeking.

    ``fw-bytes`` is the number of bytes of packets buffered in the range
    starting from the current decoding position.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_MAP
            "seekable-ranges"   MPV_FORMAT_NODE_ARRAY
                MPV_FORMAT_NODE_MAP
                    "start"             MPV_FORMAT_DOUBLE
                    "end"               MPV_FORMAT_DOUBLE
            "fw-bytes"          MPV_FORMAT_INT64

    Other fields (might be changed or removed in the future):

    ``eof``
        True if the reader thread has hit the end of the file.

    ``underrun``
        True if the reader thread could not satisfy a decoder's request for a
        new packet.

    ``idle``
        True if the thread is currently not reading.

    ``total-bytes``
        Sum of packet bytes (plus some overhead estimation) of the entire packet
        queue, including cached seekable ranges.

    ``fw-bytes``
        Sum of packet bytes (plus some overhead estimation) of the readahead
        packet queue (packets between current decoder reader positions and
        demuxer position).

``demuxer-via-network``
    Returns ``yes`` if the stream demuxed via the main demuxer is most likely
    played via network. What constitutes "network" is not always clear, might
    be used for other types of untrusted streams, could be wrong in certain
    cases, and its definition might be changing. Also, external files (like
    separate audio files or streams) do not influence the value of this
    property (currently).

``demuxer-start-time`` (R)
    Returns the start time reported by the demuxer in fractional seconds.

``paused-for-cache``
    Returns ``yes`` when playback is paused because of waiting for the cache.

``cache-buffering-state``
    Return the percentage (0-100) of the cache fill status until the player
    will unpause (related to ``paused-for-cache``).

``eof-reached``
    Returns ``yes`` if end of playback was reached, ``no`` otherwise. Note
    that this is usually interesting only if ``--keep-open`` is enabled,
    since otherwise the player will immediately play the next file (or exit
    or enter idle mode), and in these cases the ``eof-reached`` property will
    logically be cleared immediately after it's set.

``seeking``
    Returns ``yes`` if the player is currently seeking, or otherwise trying
    to restart playback. (It's possible that it returns ``yes`` while a file
    is loadedThis is because the same underlying code is used for seeking and
    resyncing.)

``mixer-active``
    Return ``yes`` if the audio mixer is active, ``no`` otherwise.

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

``colormatrix`` (R)
    Redirects to ``video-params/colormatrix``. This parameter (as well as
    similar ones) can be overridden with the ``format`` video filter.

``colormatrix-input-range`` (R)
    See ``colormatrix``.

``colormatrix-primaries`` (R)
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
    Return the current hardware decoding in use. If decoding is active, return
    one of the values used by the ``hwdec`` option/property. ``no`` indicates
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

    ``video-params/average-bpp``
        Average bits-per-pixel as integer. Subsampled planar formats use a
        different resolution, which is the reason this value can sometimes be
        odd or confusing. Can be unavailable with some formats.

    ``video-params/plane-depth``
        Bit depth for each color component as integer. This is only exposed
        for planar or single-component formats, and is unavailable for other
        formats.

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
        Source file stereo 3D mode. (See ``--video-stereo-mode`` option.)

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_MAP
            "pixelformat"       MPV_FORMAT_STRING
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

    Sub-properties::

        video-frame-info/picture-type
        video-frame-info/interlaced
        video-frame-info/tff
        video-frame-info/repeat

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

``window-minimized``
    Return whether the video window is minimized or not.

``display-names``
    Names of the displays that the mpv window covers. On X11, these
    are the xrandr names (LVDS1, HDMI1, DP1, VGA1, etc.). On Windows, these
    are the GDI names (\\.\DISPLAY1, \\.\DISPLAY2, etc.) and the first display
    in the list will be the one that Windows considers associated with the
    window (as determined by the MonitorFromWindow API.) On macOS these are the
    Display Product Names as used in the System Information and only one display
    name is returned since a window can only be on one screen.

``display-fps`` (RW)
    The refresh rate of the current display. Currently, this is the lowest FPS
    of any display covered by the video, as retrieved by the underlying system
    APIs (e.g. xrandr on X11). It is not the measured FPS. It's not necessarily
    available on all platforms. Note that any of the listed facts may change
    any time without a warning.

``estimated-display-fps``
    Only available if display-sync mode (as selected by ``--video-sync``) is
    active. Returns the actual rate at which display refreshes seem to occur,
    measured by system time.

``vsync-jitter``
    Estimated deviation factor of the vsync duration.

``video-aspect`` (RW)
    Video aspect, see ``--video-aspect``.

    If video is active, this reports the effective aspect value, instead of
    the value of the ``--video-aspect`` option.

``osd-width``, ``osd-height``
    Last known OSD width (can be 0). This is needed if you want to use the
    ``overlay-add`` command. It gives you the actual OSD size, which can be
    different from the window size in some cases.

``osd-par``
    Last known OSD display pixel aspect (can be 0).

``program`` (W)
    Switch TS program (write-only).

``dvb-channel`` (W)
    Pair of integers: card,channel of current DVB stream.
    Can be switched to switch to another channel on the same card.

``dvb-channel-name`` (RW)
    Name of current DVB program.
    On write, a channel-switch to the named channel on the same
    card is performed. Can also be used for channel switching.

``sub-text``
    Return the current subtitle text. Formatting is stripped. If a subtitle
    is selected, but no text is currently visible, or the subtitle is not
    text-based (i.e. DVD/BD subtitles), an empty string is returned.

    This property is experimental and might be removed in the future.

``tv-brightness``, ``tv-contrast``, ``tv-saturation``, ``tv-hue`` (RW)
    TV stuff.

``playlist-pos`` (RW)
    Current position on playlist. The first entry is on position 0. Writing
    to the property will restart playback at the written entry.

``playlist-pos-1`` (RW)
    Same as ``playlist-pos``, but 1-based.

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

    ``playlist/N/current``, ``playlist/N/playing``
        ``yes`` if this entry is currently playing (or being loaded).
        Unavailable or ``no`` otherwise. When changing files, ``current`` and
        ``playing`` can be different, because the currently playing file hasn't
        been unloaded yet; in this case, ``current`` refers to the new
        selection. (Since mpv 0.7.0.)

    ``playlist/N/title``
        Name of the Nth entry. Only available if the playlist file contains
        such fields, and only if mpv's parser supports it for the given
        playlist format.

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
        Track ID as used in the source file. Not always available.

    ``track-list/N/title``
        Track title as it is stored in the file. Not always available.

    ``track-list/N/lang``
        Track language as identified by the file. Not always available.

    ``track-list/N/albumart``
        ``yes`` if this is a video track that consists of a single picture,
        ``no`` or unavailable otherwise. This is used for video tracks that are
        really attached pictures in audio files.

    ``track-list/N/default``
        ``yes`` if the track has the default flag set in the file, ``no``
        otherwise.

    ``track-list/N/forced``
        ``yes`` if the track has the forced flag set in the file, ``no``
        otherwise.

    ``track-list/N/codec``
        The codec name used by this track, for example ``h264``. Unavailable
        in some rare cases.

    ``track-list/N/external``
        ``yes`` if the track is an external file, ``no`` otherwise. This is
        set for separate subtitle files.

    ``track-list/N/external-filename``
        The filename if the track is from an external file, unavailable
        otherwise.

    ``track-list/N/selected``
        ``yes`` if the track is currently decoded, ``no`` otherwise.

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
                "albumart"          MPV_FORMAT_FLAG
                "default"           MPV_FORMAT_FLAG
                "forced"            MPV_FORMAT_FLAG
                "selected"          MPV_FORMAT_FLAG
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
                "audio-channels"    MPV_FORMAT_INT64
                "replaygain-track-peak" MPV_FORMAT_DOUBLE
                "replaygain-track-gain" MPV_FORMAT_DOUBLE
                "replaygain-album-peak" MPV_FORMAT_DOUBLE
                "replaygain-album-gain" MPV_FORMAT_DOUBLE

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
    Return whether it's generally possible to seek in the current file.

``partially-seekable``
    Return ``yes`` if the current file is considered seekable, but only because
    the cache is active. This means small relative seeks may be fine, but larger
    seeks may fail anyway. Whether a seek will succeed or not is generally not
    known in advance.

    If this property returns true, ``seekable`` will also return true.

``playback-abort``
    Return whether playback is stopped or is to be stopped. (Useful in obscure
    situations like during ``on_load`` hook processing, when the user can
    stop playback, but the script has to explicitly end processing.)

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

        - ``--osd-status-msg='This is ${osd-ass-cc/0}{\\b1}bold text'``
        - ``show-text "This is ${osd-ass-cc/0}{\b1}bold text"``

    Any ASS override tags as understood by libass can be used.

    Note that you need to escape the ``\`` character, because the string is
    processed for C escape sequences before passing it to the OSD code.

    A list of tags can be found here: http://docs.aegisub.org/latest/ASS_Tags/

``vo-configured``
    Return whether the VO is configured right now. Usually this corresponds to
    whether the video window is visible. If the ``--force-window`` option is
    used, this is usually always returns ``yes``.

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
    Return the list of discovered audio devices. This is mostly for use with
    the client API, and reflects what ``--audio-device=help`` with the command
    line player returns.

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

``working-directory``
    Return the working directory of the mpv process. Can be useful for JSON IPC
    users, because the command line player usually works with relative paths.

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

``mpv-version``
    Return the mpv version/copyright string. Depending on how the binary was
    built, it might contain either a release version, or just a git hash.

``mpv-configuration``
    Return the configuration arguments which were passed to the build system
    (typically the way ``./waf configure ...`` was invoked).

``ffmpeg-version``
    Return the contents of the ``av_version_info()`` API call. This is a string
    which identifies the build in some way, either through a release version
    number, or a git hash. This applies to Libav as well (the property is
    still named the same.) This property is unavailable if mpv is linked against
    older FFmpeg and Libav versions.

``options/<name>`` (RW)
    Read-only access to value of option ``--<name>``. Most options can be
    changed at runtime by writing to this property. Note that many options
    require reloading the file for changes to take effect. If there is an
    equivalent property, prefer setting the property instead.

    There shouldn't be any reason to access ``options/<name>`` instead of
    ``<name>``, except in situations in which the properties have different
    behavior or conflicting semantics.

``file-local-options/<name>``
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
        Returns the name of the option.

    ``option-info/<name>/type``
        Return the name of the option type, like ``String`` or ``Integer``.
        For many complex types, this isn't very accurate.

    ``option-info/<name>/set-from-commandline``
        Return ``yes`` if the option was set from the mpv command line,
        ``no`` otherwise. What this is set to if the option is e.g. changed
        at runtime is left undefined (meaning it could change in the future).

    ``option-info/<name>/set-locally``
        Return ``yes`` if the option was set per-file. This is the case with
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
    Return the list of top-level properties.

``profile-list``
    Return the list of profiles and their contents. This is highly
    implementation-specific, and may change any time. Currently, it returns
    an array of options for each profile. Each option has a name and a value,
    with the value currently always being a string. Note that the options array
    is not a map, as order matters and duplicate entries are possible. Recursive
    profiles are not expanded, and show up as special ``profile`` options.

Inconsistencies between options and properties
----------------------------------------------

You can access (almost) all options as properties, though there are some
caveats with some properties (due to historical reasons):

``vid``, ``aid``, ``sid``
    While playback is active, you can set existing tracks only. (The option
    allows setting any track ID, and which tracks to enable is chosen at
    loading time.)

    Option changes at runtime are affected by this as well.

``video-aspect``
    While video is active, always returns the effective aspect ratio. Setting
    a special value (like ``no``, values ``<= 0``) will make the property
    set this as option, and return whatever actual aspect was derived from the
    option setting.

``display-fps``
    If a VO is created, this will return either the actual display FPS, or
    an invalid value, instead of the option value.

``vf``, ``af``
    If you set the properties during playback, and the filter chain fails to
    reinitialize, the new value will be rejected. Setting the option or
    setting the property outside of playback will always succeed/fail in the
    same way. Also, there are no ``vf-add`` etc. properties, but you can use
    the ``vf``/``af`` group of commands to achieve the same.

    Option changes at runtime are affected by this as well.

``edition``
    While a file is loaded, the property will always return the effective
    edition, and setting the ``auto`` value will show somewhat strange behavior
    (the property eventually switching to whatever is the default edition).

``playlist``
    The property is read-only and returns the current internal playlist. The
    option is for loading playlist during command line parsing. For client API
    uses, you should use the ``loadlist`` command instead.

``window-scale``
    Might verify the set value when setting while a window is created.

``audio-file``, ``sub-file``, ``external-file``
    These options/properties are actually lists of filenames. To make the
    command-line interface easier, each ``--audio-file=...`` option appends
    the full string to the internal list. However, when used as properties,
    every time you set the property as a string the internal list will be
    replaced with a single entry containing the string you set. ``,`` or other
    separators are never used. You have to use ``MPV_FORMAT_NODE_ARRAY`` (or
    corresponding API, e.g. ``mp.set_property_native()`` with a table in Lua)
    to set multiple entries.

    Strictly speaking, option access via API (e.g. ``mpv_set_option_string()``)
    has the same problem, and it's only a difference between CLI/API.

``playlist-pos``, ``chapter``
    These properties behave different from the deprecated options with the same
    names.

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

Within ``input.conf``, property expansion can be inhibited by putting the
``raw`` prefix in front of commands.

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
