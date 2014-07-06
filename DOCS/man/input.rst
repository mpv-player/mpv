INPUT.CONF
==========

The input.conf file consists of a list of key bindings, for example::

    s screenshot      # take a screenshot with the s key

Each line maps a key to an input command. Keys are specified with their literal
value (upper case if combined with ``Shift``), or a name for special keys. For
example, ``a`` maps to the ``a`` key without shift, and ``A`` maps to ``a``
with shift.

A list of special keys can be obtained with

    ``mpv --input-keylist``

In general, keys can be combined with ``Shift``, ``Ctrl`` and ``Alt``::

    ctrl+q quit

**mpv** can be started in input test mode, which displays key bindings and the
commands they're bound to on the OSD, instead of executing the commands::

    mpv --input-test --force-window --idle

(Only closing the window will make **mpv** exit, pressing normal keys will
merely display the binding, even if mapped to quit.)

General Input Command Syntax
----------------------------

``[Shift+][Ctrl+][Alt+][Meta+]<key> [{<section>}] [<prefixes>] <command> (<argument>)* [; <command>]``

Note that by default, the right Alt key can be used to create special
characters, and thus does not register as a modifier. The option
``--no-input-right-alt-gr`` changes this behavior.

Newlines always start a new binding. ``#`` starts a comment (outside of quoted
string arguments). To bind commands to the ``#`` key, ``SHARP`` can be used.

``<key>`` is either the literal character the key produces (ASCII or Unicode
character), or a symbolic name (as printed by ``--input-keylist``).

``<section>`` (braced with ``{`` and ``}``) is the input section for this
command.

Arguments are separated by whitespace. This applies even to string arguments.
For this reason, string arguments should be quoted with ``"``. Inside quotes,
C-style escaping can be used.

You can bind multiple commands to one key. For example:

| a show_text "command 1" ; show_text "command 2"

It's also possible to bind a command to a sequence of keys:

| a-b-c show_text "command run after a, b, c have been pressed"

(This is not shown in the general command syntax.)

If ``a`` or ``a-b`` or ``b`` are already bound, this will run the first command
that matches, and the multi-key command will never be called. Intermediate keys
can be remapped to ``ignore`` in order to avoid this issue. The maximum number
of (non-modifier) keys for combinations is currently 4.

List of Input Commands
----------------------

``ignore``
    Use this to "block" keys that should be unbound, and do nothing. Useful for
    disabling default bindings, without disabling all bindings with
    ``--no-input-default-bindings``.

``seek <seconds> [relative|absolute|absolute-percent|- [default-precise|exact|keyframes]]``
    Change the playback position. By default, seeks by a relative amount of
    seconds.

    The second argument sets the seek mode:

    relative (default)
        Seek relative to current position (a negative value seeks backwards).
    absolute
        Seek to a given time.
    absolute-percent
        Seek to a given percent position.

    The third argument defines how exact the seek is:

    default-precise (default)
        Follow the default behavior as set by ``--hr-seek``, which by default
        does imprecise seeks (like ``keyframes``).
    exact
        Always do exact/hr/precise seeks (slow).
    keyframes
        Always restart playback at keyframe boundaries (fast).

``revert_seek``
    Undoes the ``seek`` command, and some other commands that seek (but not
    necessarily all of them). Calling this command once will jump to the
    playback position before the seek. Calling it a second time undoes the
    ``revert_seek`` command itself.

``frame_step``
    Play one frame, then pause.

``frame_back_step``
    Go back by one frame, then pause. Note that this can be very slow (it tries
    to be precise, not fast), and sometimes fails to behave as expected. How
    well this works depends on whether precise seeking works correctly (e.g.
    see the ``--hr-seek-demuxer-offset`` option). Video filters or other video
    postprocessing that modifies timing of frames (e.g. deinterlacing) should
    usually work, but might make backstepping silently behave incorrectly in
    corner cases. Using ``--hr-seek-framedrop=no`` should help, although it
    might make precise seeking slower.

    This does not work with audio-only playback.

``set <property> "<value>"``
    Set the given property to the given value.

``add <property> [<value>]``
    Add the given value to the property. On overflow or underflow, clamp the
    property to the maximum. If ``<value>`` is omitted, assume ``1``.

``cycle <property> [up|down]``
    Cycle the given property. ``up`` and ``down`` set the cycle direction. On
    overflow, set the property back to the minimum, on underflow set it to the
    maximum. If ``up`` or ``down`` is omitted, assume ``up``.

``multiply <property> <factor>``
    Multiplies the value of a property with the numeric factor.

``screenshot [subtitles|video|window|- [single|each-frame]]``
    Take a screenshot.

    First argument:

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

    Second argument:

    <single> (default)
        Take a single screenshot.
    <each-frame>
        Take a screenshot each frame. Issue this command again to stop taking
        screenshots.

``screenshot_to_file "<filename>" [subtitles|video|window]``
    Take a screenshot and save it to a given file. The format of the file will
    be guessed by the extension (and ``--screenshot-format`` is ignored - the
    behavior when the extension is missing or unknown is arbitrary).

    The second argument is like the first argument to ``screenshot``.

    This command tries to never overwrite files. If the file already exists,
    it fails.

    Like all input command parameters, the filename is subject to property
    expansion as described in `Property Expansion`_.

``playlist_next [weak|force]``
    Go to the next entry on the playlist.

    weak (default)
        If the last file on the playlist is currently played, do nothing.
    force
        Terminate playback if there are no more files on the playlist.

``playlist_prev [weak|force]``
    Go to the previous entry on the playlist.

    weak (default)
        If the first file on the playlist is currently played, do nothing.
    force
        Terminate playback if the first file is being played.

``loadfile "<file>" [replace|append [options]]``
    Load the given file and play it.

    Second argument:

    <replace> (default)
        Stop playback of the current file, and play the new file immediately.
    <append>
        Append the file to the playlist.

    The third argument is a list of options and values which should be set
    while the file is playing. It is of the form ``opt1=value1,opt2=value2,..``.
    Not all options can be changed this way. Some options require a restart
    of the player.

``loadlist "<playlist>" [replace|append]``
    Load the given playlist file (like ``--playlist``).

``playlist_clear``
    Clear the playlist, except the currently played file.

``playlist_remove current|<index>``
    Remove the playlist entry at the given index. Index values start counting
    with 0. The special value ``current`` removes the current entry. Note that
    removing the current entry also stops playback and starts playing the next
    entry.

``playlist_move <index1> <index2>``
    Move the playlist entry at index1, so that it takes the place of the
    entry index2. (Paradoxically, the moved playlist entry will not have
    the index value index2 after moving if index1 was lower than index2,
    because index2 refers to the target entry, not the index the entry
    will have after moving.)

``run "command" "arg1" "arg2" ...``
    (Unix only)
    Run the given command. Unlike in MPlayer/mplayer2 and earlier versions of
    mpv (0.2.x and older), this doesn't call the shell. Instead, the command
    is run directly, with each argument passed separately. Each argument is
    expanded like in `Property Expansion`_. Note that there is a static limit
    of (as of this writing) 9 arguments (this limit could be raised on demand).

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

``quit [<code>]``
    Exit the player. If an argument is given, it's used as process exit code.

``quit_watch_later [<code>]``
    Exit player, and store current playback position. Playing that file later
    will seek to the previous position on start. The (optional) argument is
    exactly as in the ``quit`` command.

``sub_add "<file>"``
    Load the given subtitle file. It is not selected as current subtitle after
    loading.

``sub_remove [<id>]``
    Remove the given subtitle track. If the ``id`` argument is missing, remove
    the current track. (Works on external subtitle files only.)

``sub_reload [<id>]``
    Reload the given subtitle tracks. If the ``id`` argument is missing, remove
    the current track. (Works on external subtitle files only.)

    This works by unloading and re-adding the subtitle track.

``sub_step <skip>``
    Change subtitle timing such, that the subtitle event after the next
    ``<skip>`` subtitle events is displayed. ``<skip>`` can be negative to step
    backwards.

``sub_seek <skip>``
    Seek to the next (skip set to 1) or the previous (skip set to -1) subtitle.
    This is similar to ``sub_step``, except that it seeks video and audio
    instead of adjusting the subtitle delay.

    Like with ``sub_step``, this works with external text subtitles only. For
    embedded text subtitles (like with Matroska), this works only with subtitle
    events that have already been displayed.

``osd [<level>]``
    Toggle OSD level. If ``<level>`` is specified, set the OSD mode
    (see ``--osd-level`` for valid values).

``print_text "<string>"``
    Print text to stdout. The string can contain properties (see
    `Property Expansion`_).

``show_text "<string>" [<duration>|- [<level>]]``
    Show text on the OSD. The string can contain properties, which are expanded
    as described in `Property Expansion`_. This can be used to show playback
    time, filename, and so on.

    <duration>
        The time in ms to show the message for. By default, it uses the same
        value as ``--osd-duration``.

    <level>
        The minimum OSD level to show the text at (see ``--osd-level``).

``show_progress``
    Show the progress bar, the elapsed time and the total duration of the file
    on the OSD.

``discnav "<command>"``
    Send a menu control command to the DVD/BD menu implementation. The following
    commands are defined: ``up``, ``down``, ``left``, ``right``,
    ``menu`` (request to enter menu), ``prev`` (previous screen),
    ``select`` (activate current button), ``mouse`` (the mouse was clicked),
    ``mouse_move`` (the mouse cursor changed position).

    ``mouse_move`` will use the current mouse position.

    Note that while the menu is active, the input section ``discnav-menu`` will
    be enabled, so different key bindings can be mapped for menu mode.

``write_watch_later_config``
    Write the resume config file that the ``quit_watch_later`` command writes,
    but continue playback normally.

Input Commands that are Possibly Subject to Change
--------------------------------------------------

``af set|add|toggle|del|clr "filter1=params,filter2,..."``
    Change audio filter chain. See ``vf`` command.

``vf set|add|toggle|del|clr "filter1=params,filter2,..."``
    Change video filter chain.

    The first argument decides what happens:

    set
        Overwrite the previous filter chain with the new one.

    add
        Append the new filter chain to the previous one.

    toggle
        Check if the given filter (with the exact parameters) is already
        in the video chain. If yes, remove the filter. If no, add the filter.
        (If several filters are passed to the command, this is done for
        each filter.)

    del
        Remove the given filters from the video chain. Unlike in the other
        cases, the second parameter is a comma separated list of filter names
        or integer indexes. ``0`` would denote the first filter. Negative
        indexes start from the last filter, and ``-1`` denotes the last
        filter.

    clr
        Remove all filters. Note that like the other sub-commands, this does
        not control automatically inserted filters.

    You can assign labels to filter by prefixing them with ``@name:`` (where
    ``name`` is a user-chosen arbitrary identifier). Labels can be used to
    refer to filters by name in all of the filter chain modification commands.
    For ``add``, using an already used label will replace the existing filter.

    The ``vf`` command shows the list of requested filters on the OSD after
    changing the filter chain. This is roughly equivalent to
    ``show_text ${vf}``. Note that auto-inserted filters for format conversion
    are not shown on the list, only what was requested by the user.

    Normally, the commands will check whether the video chain is recreated
    successfully, and will undo the operation on failure. If the command is run
    before video is configured (can happen if the command is run immediately
    after opening a file and before a video frame is decoded), this check can't
    be run. Then it can happen that creating the video chain fails.

    .. admonition:: Example for input.conf

        - ``a vf set flip`` turn video upside-down on the ``a`` key
        - ``b vf set ""`` remove all video filters on ``b``
        - ``c vf toggle lavfi=gradfun`` toggle debanding on ``c``

``cycle_values ["!reverse"] <property> "<value1>" "<value2>" ...``
    Cycle through a list of values. Each invocation of the command will set the
    given property to the next value in the list. The command maintains an
    internal counter which value to pick next, and which is initially 0. It is
    reset to 0 once the last value is reached.

    The internal counter is associated using the property name and the value
    list. If multiple commands (bound to different keys) use the same name
    and value list, they will share the internal counter.

    The special argument ``!reverse`` can be used to cycle the value list in
    reverse. Compared with a command that just lists the value in reverse, this
    command will actually share the internal counter with the forward-cycling
    key binding.

    Note that there is a static limit of (as of this writing) 10 arguments
    (this limit could be raised on demand).

``enable_section "<section>" [default|exclusive]``
    Enable all key bindings in the named input section.

    The enabled input sections form a stack. Bindings in sections on the top of
    the stack are preferred to lower sections. This command puts the section
    on top of the stack. If the section was already on the stack, it is
    implicitly removed beforehand. (A section cannot be on the stack more than
    once.)

    If ``exclusive`` is specified as second argument, all sections below the
    newly enabled section are disabled. They will be re-enabled as soon as
    all exclusive sections above them are removed.

``disable_section "<section>"``
    Disable the named input section. Undoes ``enable_section``.

``overlay_add <id> <x> <y> "<file>" <offset> "<fmt>" <w> <h> <stride>``
    Add an OSD overlay sourced from raw data. This might be useful for scripts
    and applications controlling mpv, and which want to display things on top
    of the video window.

    Overlays are usually displayed in screen resolution, but with some VOs,
    the resolution is reduced to that of the video's. You can read the
    ``osd-width`` and ``osd-height`` properties. At least with ``--vo-xv`` and
    anamorphic video (such as DVD), ``osd-par`` should be read as well, and the
    overlay should be aspect-compensated. (Future directions: maybe mpv should
    take care of some of these things automatically, but it's hard to tell
    where to draw the line.)

    ``id`` is an integer between 0 and 63 identifying the overlay element. The
    ID can be used to add multiple overlay parts, update a part by using this
    command with an already existing ID, or to remove a part with
    ``overlay_remove``. Using a previously unused ID will add a new overlay,
    while reusing an ID will update it. (Future directions: there should be
    something to ensure different programs wanting to create overlays don't
    conflict with each others, should that ever be needed.)

    ``x`` and ``y`` specify the position where the OSD should be displayed.

    ``file`` specifies the file the raw image data is read from. It can be
    either a numeric UNIX file descriptor prefixed with ``@`` (e.g. ``@4``),
    or a filename. The file will be mapped into memory with ``mmap()``. Some VOs
    will pass the mapped pointer directly to display APIs (e.g. opengl or
    vdpau), so no actual copying is involved. Truncating the source file while
    the overlay is active will crash the player. You shouldn't change the data
    while the overlay is active, because the data is essentially accessed at
    random points. Instead, call ``overlay_add`` again (preferably with a
    different memory region to prevent tearing).

    ``offset`` is the offset of the first pixel in the source file. It is
    passed directly to ``mmap`` and is subject to certain restrictions
    (see ``man mmap`` for details). In particular, this value has to be a
    multiple of the system's page size.

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

    .. admonition:: Warning

        When updating the overlay, you should prepare a second shared memory
        region (e.g. make use of the offset parameter) and add this as overlay,
        instead of reusing the same memory every time. Otherwise, you might
        get the equivalent of tearing, when your application and mpv write/read
        the buffer at the same time. Also, keep in mind that mpv might access
        an overlay's memory at random times whenever it feels the need to do
        so, for example when redrawing the screen.

``overlay_remove <id>``
    Remove an overlay added with ``overlay_add`` and the same ID. Does nothing
    if no overlay with this ID exists.

``script_message "<arg1>" "<arg2>" ...``
    Send a message to all clients, and pass it the following list of arguments.
    What this message means, how many arguments it takes, and what the arguments
    mean is fully up to the receiver and the sender. Every client receives the
    message, so be careful about name clashes (or use ``script_message_to``).

``script_message_to "<target>" "<arg1>" "<arg2>" ...``
    Same as ``script_message``, but send it only to the client named
    ``<target>``. Each client (scripts etc.) has a unique name. For example,
    Lua scripts can get their name via ``mp.get_script_name()``.

    (Scripts use this internally to dispatch key bindings, and this can also
    be used in input.conf to reassign such bindings.)


Undocumented commands: ``tv_last_channel`` (TV/DVB only), ``stop`` (questionable
use), ``get_property`` (?), ``vo_cmdline`` (experimental).

Input Command Prefixes
----------------------

These prefixes are placed between key name and the actual command. Multiple
prefixes can be specified. They are separated by whitespace.

``osd-auto`` (default)
    Use the default behavior for this command.
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
``expand-properties`` (default)
    All string arguments are expanded as described in `Property Expansion`_.


All of the osd prefixes are still overridden by the global ``--osd-level``
settings.

Input Sections
--------------

Input sections group a set of bindings, and enable or disable them at once.
In ``input.conf``, each key binding is assigned to an input section, rather
than actually having explicit text sections.

Also see ``enable_section`` and ``disable_section`` commands.

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
commands, and retrieved with ``show_text``, or anything else that uses property
expansion. (See `Property Expansion`_.)

The property name is annotated with RW to indicate whether the property is
generally writable.

If an option is referenced, the property will normally take/return exactly the
same values as the option. In these cases, properties are merely a way to change
an option at runtime.

Property list
-------------

``osd-level`` (RW)
    See ``--osd-level``.

``osd-scale`` (RW)
    OSD font size multiplicator, see ``--osd-scale``.

``loop`` (RW)
    See ``--loop``.

``loop-file`` (RW)
    See ``--loop-file`` (uses ``yes``/``no``).

``speed`` (RW)
    See ``--speed``.

``filename``
    Currently played file, with path stripped. If this is an URL, try to undo
    percent encoding as well. (The result is not necessarily correct, but
    looks better for display purposes. Use the ``path`` property to get an
    unmodified filename.)

``file-size``
    Length in bytes of the source file/stream. (This is the same as
    ``${stream-end}``. For ordered chapters and such, the
    size of the currently played segment is returned.)

``path``
    Full path of the currently played file.

``media-title``
    If libquvi is used and libquvi returns a page title for the currently
    played URL, return the page title.

    Otherwise, if the currently played file has a ``title`` tag, use that.

    Otherwise, if the media type is DVD, return the volume ID of DVD.

    Otherwise, return the ``filename`` property.

``demuxer``
    Name of the current demuxer. (This is useless.)

``stream-path``
    Filename (full path) of the stream layer filename. (This is probably
    useless. It looks like this can be different from ``path`` only when
    using e.g. ordered chapters.)

``stream-pos`` (RW)
    Raw byte position in source stream.

``stream-end``
    Raw end position in bytes in source stream.

``length``
    Length of the current file in seconds. If the length is unknown, the
    property is unavailable. Note that the file duration is not always exactly
    known, so this is an estimate.

``avsync``
    Last A/V synchronization difference. Unavailable if audio or video is
    disabled.

``total-avsync-change``
    Total A-V sync correction done. Unavailable if audio or video is
    disabled.

``drop-frame-count``
    Frames dropped because they arrived to late. Unavailable if video
    is disabled

``percent-pos`` (RW)
    Position in current file (0-100). The advantage over using this instead of
    calculating it out of other properties is that it properly falls back to
    estimating the playback position from the byte position, if the file
    duration is not known.

``ratio-pos`` (RW)
    Position in current file (0.0-1.0). higher precision that ``percent-pos``.

``time-pos`` (RW)
    Position in current file in seconds.

``time-start``
    Return the start time of the file. (Usually 0, but some kind of files,
    especially transport streams, can have a different start time.)

``time-remaining``
    Remaining length of the file in seconds. Note that the file duration is not
    always exactly known, so this is an estimate.

``playtime-remaining``
    ``time-remaining`` scaled by the the current ``speed``.

``playback-time``
    Return the playback time, which is the time difference between start PTS and current PTS.

``chapter`` (RW)
    Current chapter number. The number of the first chapter is 0.

``edition`` (RW)
    Current MKV edition number. Setting this property to a different value will
    restart playback. The number of the first edition is 0.

``disc-titles``
    Number of BD/DVD titles.

``disc-title`` (RW)
    Current BD/DVD title number. Writing works only for ``dvdnav://`` and
    ``bd://`` (and aliases for these).

``disc-menu-active``
    Return ``yes`` if the BD/DVD menu is active, or ``no`` on normal video
    playback. The property is unavailable when playing something that is not
    a BD or DVD. Use the ``discnav menu`` command to actually enter or leave
    menu mode.

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
            (key and string value for each metdata entry)

``chapter-metadata``
    Metadata of current chapter. Works similar to ``metadata`` property. It
    also allows the same access methods (using sub-properties).

    Per-chapter metadata is very rare. Usually, only the chapter name
    (``title``) is set.

    For accessing other information, like chapter start, see the
    ``chapter-list`` property.

``vf-metadata/<filter-label>``
    Metadata added by video filters. Accessed by the filter label,
    which if not explicitly specified using the ``@filter-label:`` syntax,
    will be ``<filter-name>NN``.

    Works similar to ``metadata`` property. It allows the same access
    methods (using sub-properties).

    An example of these kind of metadata are the cropping parameters
    added by ``--vf=lavfi=cropdetect``.

``pause`` (RW)
    Pause status. This is usually ``yes`` or ``no``. See ``--pause``.

``core-idle``
    Return ``yes`` if the playback core is paused, otherwise ``no``. This can
    be different ``pause`` in special situations, such as when the player
    pauses itself due to low network cache.

``cache``
    Network cache fill state (0-100.0).

``cache-size`` (RW)
    Total network cache size in KB. This is similar to ``--cache``. This allows
    to set the cache size at runtime. Currently, it's not possible to enable
    or disable the cache at runtime using this property, just to resize an
    existing cache.

    Note that this tries to keep the cache contents as far as possible. To make
    this easier, the cache resizing code will allocate the new cache while the
    old cache is still allocated.

    Don't use this when playing DVD or Bluray.

``cache-free`` (R)
    Total free cache size in KB. 

``cache-used`` (R)
    Total used cache size in KB. 

``paused-for-cache``
    Returns ``yes`` when playback is paused because of waiting for the cache.

``eof-reached``
    Returns ``yes`` if end of playback was reached, ``no`` otherwise. Note
    that this is usually interesting only if ``--keep-open`` is enabled,
    since otherwise the player will immediately play the next file (or exit
    or enter idle mode), and in these cases the ``eof-reached`` property will
    logically be cleared immediately after it's set.

``pts-association-mode`` (RW)
    See ``--pts-association-mode``.

``hr-seek`` (RW)
    See ``--hr-seek``.

``volume`` (RW)
    Current volume (0-100).

``mute`` (RW)
    Current mute status (``yes``/``no``).

``audio-delay`` (RW)
    See ``--audio-delay``.

``audio-format``
    Audio format as string.

``audio-codec``
    Audio codec selected for decoding.

``audio-bitrate``
    Audio bitrate. This is probably a very bad guess in most cases.

``audio-samplerate``
    Audio samplerate.

``audio-channels``
    Number of audio channels. The OSD value of this property is actually the
    channel layout, while the raw value returns the number of channels only.

``aid`` (RW)
    Current audio track (similar to ``--aid``).

``audio`` (RW)
    Alias for ``aid``.

``balance`` (RW)
    Audio channel balance. (The implementation of this feature is rather odd.
    It doesn't change the volumes of each channel, but instead sets up a pan
    matrix to mix the the left and right channels.)

``fullscreen`` (RW)
    See ``--fullscreen``.

``deinterlace`` (RW)
    See ``--deinterlace``.

``colormatrix`` (RW)
    See ``--colormatrix``.

``colormatrix-input-range`` (RW)
    See ``--colormatrix-input-range``.

``colormatrix-output-range`` (RW)
    See ``--colormatrix-output-range``.

``colormatrix-primaries`` (RW)
    See ``--colormatrix-primaries``.

``ontop`` (RW)
    See ``--ontop``.

``border`` (RW)
    See ``--border``.

``framedrop`` (RW)
    See ``--framedrop``.

``gamma`` (RW)
    See ``--gamma``.

``brightness`` (RW)
    See ``--brightness``.

``contrast`` (RW)
    See ``--contrast``.

``saturation`` (RW)
    See ``--saturation``.

``hue`` (RW)
    See ``--hue``.

``hwdec`` (RW)
    Return the current hardware decoder that is used. This uses the same values
    as the ``--hwdec`` option. If software decoding is active, this returns
    ``no``. You can write this property. Then the ``--hwdec`` option is set to
    the new value, and video decoding will be reinitialized (internally, the
    player will perform a seek to refresh the video properly).

    Note that you don't know the success of the operation immediately after
    writing this property. It happens with a delay as video is reinitialized.

``panscan`` (RW)
    See ``--panscan``.

``video-format``
    Video format as string.

``video-codec``
    Video codec selected for decoding.

``video-bitrate``
    Video bitrate (a bad guess).

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

    ``video-params/chroma-location``
        Chroma location as string. (Exact values subject to change.)

    ``video-params/rotate``
        Intended display rotation in degrees (clockwise).

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each track)
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
                "chroma-location"   MPV_FORMAT_STRING
                "rotate"            MPV_FORMAT_INT64

``dwidth``, ``dheight``
    Video display size. This is the video size after filters and aspect scaling
    have been applied. The actual video window size can still be different
    from this.

``video-out-params``
    Same as ``video-params``, but after video filters have been applied. If
    there are no video filters in use, this will contain the same values as
    ``video-params``. Note that this is still not necessarily what the video
    window uses, since all real VOs do their own scaling.

    Has the same sub-properties as ``video-params``.

``fps``
    Container FPS. This can easily contain bogus values. For videos that use
    modern container formats or video codecs, this will often be incorrect.

``estimated-vf-fps``
    Estimated/measured FPS of the video filter chain output. (If no filters
    are used, this corresponds to decoder output.) This uses the average of
    the 10 past frame durations to calculate the FPS. It will be inaccurate
    if framedropping is involved (such as when framedrop is explicitly
    enabled, or after precise seeking). Files with imprecise timestamps (such
    as Matroska) might lead to unstable results.

``window-scale`` (RW)
    Window size multiplier. Setting this will resize the video window to the
    values contained in ``dwidth`` and ``dheight`` multiplied with the value
    set with this property. Setting ``1`` will resize to original video size
    (or to be exactly, the size the video filters output). ``2`` will set the
    double size, ``0.5`` halves the size.

``video-aspect`` (RW)
    Video aspect, see ``--video-aspect``.

``osd-width``, ``osd-height``
    Last known OSD width (can be 0). This is needed if you want to use the
    ``overlay_add`` command. It gives you the actual OSD size, which can be
    different from the window size in some cases.

``osd-par``
    Last known OSD display pixel aspect (can be 0).

``vid`` (RW)
    Current video track (similar to ``--vid``).

``video`` (RW)
    Alias for ``vid``.

``video-align-x``, ``video-align-y`` (RW)
    See ``--video-align-x`` and ``--video-align-y``.

``video-pan-x``, ``video-pan-y`` (RW)
    See ``--video-pan-x`` and ``--video-pan-y``.

``video-zoom`` (RW)
    See ``--video-zoom``.

``video-unscaled`` (W)
    See ``--video-unscaled``.

``program`` (W)
    Switch TS program (write-only).

``sid`` (RW)
    Current subtitle track (similar to ``--sid``).

``secondary-sid`` (RW)
    Secondary subtitle track (see ``--secondary-sid``).

``sub`` (RW)
    Alias for ``sid``.

``sub-delay`` (RW)
    See ``--sub-delay``.

``sub-pos`` (RW)
    See ``--sub-pos``.

``sub-visibility`` (RW)
    See ``--sub-visibility``.

``sub-forced-only`` (RW)
    See ``--sub-forced-only``.

``sub-scale`` (RW)
    Subtitle font size multiplicator.

``ass-use-margins`` (RW)
    See ``--ass-use-margins``.

``ass-vsfilter-aspect-compat`` (RW)
    See ``--ass-vsfilter-aspect-compat``.

``ass-style-override`` (RW)
    See ``--ass-style-override``.

``stream-capture`` (RW)
    A filename, see ``--capture``. Setting this will start capture using the
    given filename. Setting it to an empty string will stop it.

``tv-brightness``, ``tv-contrast``, ``tv-saturation``, ``tv-hue`` (RW)
    TV stuff.

``playlist-pos`` (RW)
    Current position on playlist. The first entry is on position 0. Writing
    to the property will restart playback at the written entry.

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

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each playlist entry)
                "filename"  MPV_FORMAT_STRING

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
                "external"          MPV_FORMAT_FLAG
                "external-filename" MPV_FORMAT_STRING
                "codec"             MPV_FORMAT_STRING

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

``quvi-format`` (RW)
    See ``--quvi-format``. Cycling this property (``cycle``) will attempt to
    cycle through known format, although currently this feature doesn't work
    well at all.

``af`` (RW)
    See ``--af`` and the ``af`` command.

``vf`` (RW)
    See ``--vf`` and the ``vf`` command.

    When querying the property with the client API using ``MPV_FORMAT_NODE``,
    or with Lua ``mp.get_property_native``, this will return a mpv_node with
    the following contents:

    ::

        MPV_FORMAT_NODE_ARRAY
            MPV_FORMAT_NODE_MAP (for each filter entry)
                "name"      MPV_FORMAT_STRING
                "label"     MPV_FORMAT_STRING [optional]
                "params"    MPV_FORMAT_NODE_MAP [optional]
                    "key"   MPV_FORMAT_STRING
                    "value" MPV_FORMAT_STRING

    It's also possible to write the property using this format.

``seekable``
    Return whether it's generally possible to seek in the current file.

``options/<name>`` (RW)
    Read-only access to value of option ``--<name>``. Most options can be
    changed at runtime by writing to this property. Note that many options
    require reloading the file for changes to take effect. If there is an
    equivalent property, prefer setting the property instead.

``write_watch_later_config``
    Saves current playback position.

Property Expansion
------------------

All string arguments to input commands as well as certain options (like
``--term-playing-msg``) are subject to property expansion.

.. admonition:: Example for input.conf

    ``i show_text "Filename: ${filename}"``
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
