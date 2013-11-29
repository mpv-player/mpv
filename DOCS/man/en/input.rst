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

    mpv --input-test --demuxer=rawvideo --demuxer-rawvideo=w=1280:h=720 /dev/zero

(Commands which normally close the player will not work in this mode, and you
must kill **mpv** externally to make it exit.)

General Input Command Syntax
----------------------------

``[Shift+][Ctrl+][Alt+][Meta+]<key> [{<section>}] [<prefixes>] <command> (<argument>)* [; <command>]``

Newlines always start a new binding. ``#`` starts a comment (outside of quoted
string arguments). To bind commands to the ``#`` key, ``SHARP`` can be used.

``<key>`` is either the literal character the key produces (ASCII or Unicode
character), or a symbolic name (as printed by ``--input-keylist``).

``<section>`` (braced with ``{`` and ``}``) is the input section for this
command.

Arguments are separated by whitespace. This applies even to string arguments.
For this reason, string arguments should be quoted with ``"``. Inside quotes,
C-style escaping can be used.

Optional arguments can be skipped with ``-``.

You can bind multiple commands to one key. For example:

| a show_text "command 1" ; show_text "command 2"

Note that some magic is disabled for keys: seek commands inside lists are not
coalesced (seeking will appear slower), and no check is done for abort commands
(so these commands can't be used to abort playback if the network cache is
stuck).

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

``frame_step``
    Play one frame, then pause.

``frame_back_step``
    Go back by one frame, then pause. Note that this can be very slow (it tries
    to be precise, not fast), and sometimes fails to behave as expected. How
    well this works depends on whether precise seeking works correctly (e.g.
    see the ``--hr-seek-demuxer-offset`` option). Video filters or other video
    postprocessing that modifies timing of frames (e.g. deinterlacing) should
    usually work, but might make backstepping silently behave incorrectly in
    corner cases.

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

``loadfile "<file>" [replace|append]``
    Load the given file and play it.

    Second argument:

    <replace> (default)
        Stop playback of the current file, and play the new file immediately.
    <append>
        Append the file to the playlist.

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
    Run the given command. Unlike in MPlayer/mplayer2 and earlier versions of
    mpv (0.2.x and older), this doesn't call the shell. Instead, the command
    is run directly, with each argument passed separately. Each argument is
    expanded like in `Property Expansion`_. Note that there is a static limit
    of (as of this writing) 10 arguments (this limit could be raised on demand).

    To get the old behavior, use ``/bin/sh`` and ``-c`` as the first two
    arguments.

    .. admonition:: Example

        ``run "/bin/sh" "-c" "echo ${title} > /tmp/playing"``

        This is not a particularly good example, because it doesn't handle
        escaping, and a specially prepared file might allow an attacker to
        execute arbitrary shell commands. It is recommended to write a small
        shell script, and call that with ``run``.

``quit [<code>]``
    Exit the player using the given exit code.

``quit_watch_later``
    Exit player, and store current playback position. Playing that file later
    will seek to the previous position on start.

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

Undocumented commands: ``tv_start_scan``, ``tv_step_channel``, ``tv_step_norm``,
``tv_step_chanlist``, ``tv_set_channel``, ``tv_last_channel``, ``tv_set_freq``,
``tv_step_freq``, ``tv_set_norm``, ``dvb_set_channel``, ``radio_step_channel``,
``radio_set_channel``, ``radio_set_freq``, ``radio_step_freq`` (all of these
should be replaced by properties), ``stop`` (questionable use), ``get_property``
(?), ``vo_cmdline`` (experimental).

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

Undocumented prefixes: ``pausing``, ``pausing_keep``, ``pausing_toggle``,
``pausing_keep_force``. (Should these be made official?)

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

The ``W`` column indicates whether the property is generally writable. If an
option is referenced, the property should take/return exactly the same values
as the option.

=============================== = ==================================================
Name                            W Comment
=============================== = ==================================================
``osd-level``                   x see ``--osd-level``
``osd-scale``                   x osd font size multiplicator, see ``--osd-scale``
``loop``                        x see ``--loop``
``speed``                       x see ``--speed``
``filename``                      currently played file (path stripped)
``path``                          currently played file (full path)
``media-title``                   filename, title tag, or libquvi ``QUVIPROP_PAGETITLE``
``demuxer``
``stream-path``                   filename (full path) of stream layer filename
``stream-pos``                  x byte position in source stream
``stream-start``                  start byte offset in source stream
``stream-end``                    end position in bytes in source stream
``stream-length``                 length in bytes (``${stream-end} - ${stream-start}``)
``stream-time-pos``             x time position in source stream (also see ``time-pos``)
``length``                        length of the current file in seconds
``avsync``                        last A/V synchronization difference
``percent-pos``                 x position in current file (0-100)
``ratio-pos``                   x position in current file (0.0-1.0)
``time-pos``                    x position in current file in seconds
``time-remaining``                estimated remaining length of the file in seconds
``chapter``                     x current chapter number
``edition``                     x current MKV edition number
``titles``                        number of DVD titles
``chapters``                      number of chapters
``editions``                      number of MKV editions
``angle``                       x current DVD angle
``metadata``                      metadata key/value pairs
``metadata/<key>``                value of metadata entry ``<key>``
``chapter-metadata``              metadata of current chapter (works similar)
``pause``                       x pause status (bool)
``cache``                         network cache fill state (0-100)
``pts-association-mode``        x see ``--pts-association-mode``
``hr-seek``                     x see ``--hr-seek``
``volume``                      x current volume (0-100)
``mute``                        x current mute status (bool)
``audio-delay``                 x see ``--audio-delay``
``audio-format``                  audio format (string)
``audio-codec``                   audio codec selected for decoding
``audio-bitrate``                 audio bitrate
``samplerate``                    audio samplerate
``channels``                      number of audio channels
``aid``                         x current audio track (similar to ``--aid``)
``audio``                       x alias for ``aid``
``balance``                     x audio channel balance
``fullscreen``                  x see ``--fullscreen``
``deinterlace``                 x similar to ``--deinterlace``
``colormatrix``                 x see ``--colormatrix``
``colormatrix-input-range``     x see ``--colormatrix-input-range``
``colormatrix-output-range``    x see ``--colormatrix-output-range``
``ontop``                       x see ``--ontop``
``border``                      x see ``--border``
``framedrop``                   x see ``--framedrop``
``gamma``                       x see ``--gamma``
``brightness``                  x see ``--brightness``
``contrast``                    x see ``--contrast``
``saturation``                  x see ``--saturation``
``hue``                         x see ``--hue``
``panscan``                     x see ``--panscan``
``video-format``                  video format (string)
``video-codec``                   video codec selected for decoding
``video-bitrate``                 video bitrate
``width``                         video width (container or decoded size)
``height``                        video height
``fps``                           container FPS (may contain bogus values)
``dwidth``                        video width (after filters and aspect scaling)
``dheight``                       video height
``window-scale``                x window size multiplier (1 means video size)
``aspect``                      x video aspect
``osd-width``                     last known OSD width (can be 0)
``osd-height``                    last known OSD height (can be 0)
``osd-par``                       last known OSD display pixel aspect (can be 0)
``vid``                         x current video track (similar to ``--vid``)
``video``                       x alias for ``vid``
``video-align-x``               x see ``--video-align-x``
``video-align-y``               x see ``--video-align-y``
``video-pan-x``                 x see ``--video-pan-x``
``video-pan-y``                 x see ``--video-pan-y``
``video-zoom``                  x see ``--video-zoom``
``program``                     x switch TS program (write-only)
``sid``                         x current subtitle track (similar to ``--sid``)
``sub``                         x alias for ``sid``
``sub-delay``                   x see ``--sub-delay``
``sub-pos``                     x see ``--sub-pos``
``sub-visibility``              x see ``--sub-visibility``
``sub-forced-only``             x see ``--sub-forced-only``
``sub-scale``                   x subtitle font size multiplicator
``ass-use-margins``             x see ``--ass-use-margins``
``ass-vsfilter-aspect-compat``  x see ``--ass-vsfilter-aspect-compat``
``ass-style-override``          x see ``--ass-style-override``
``stream-capture``              x a filename, see ``--capture``
``tv-brightness``               x
``tv-contrast``                 x
``tv-saturation``               x
``tv-hue``                      x
``playlist-pos``                  current position on playlist
``playlist-count``                number of total playlist entries
``playlist``                      playlist, current entry marked
``track-list``                    list of audio/video/sub tracks, current entry marked
``chapter-list``                  list of chapters, current entry marked
``quvi-format``                 x see ``--quvi-format``
``af``                          x see ``--af``
``vf``                          x see ``--vf``
``options/name``                  read-only access to value of option ``--name``
=============================== = ==================================================

Property Expansion
------------------

All string arguments to input commands as well as certain options (like
``--playing-msg``) are subject to property expansion.

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
