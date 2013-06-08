.. _input:

INPUT.CONF
==========

The input.conf file consists of a list of key bindings, for example:

| s screenshot      # take a screenshot with the s key

Each line maps a key to an input command. Keys are specified with their literal
value (upper case if combined with ``Shift``), or a name for special keys. For
example, ``a`` maps to the ``a`` key without shift, and ``A`` maps to ``a``
with shift.

A list of special keys can be obtained with

| **mpv** --input-keylist

In general, keys can be combined with ``Shift``, ``Ctrl`` and ``Alt``:

| ctrl+q quit

**mpv** can be started in input test mode, which displays key bindings and the
commands they're bound to on the OSD, instead of running the commands:

| **mpv** --input-test --demuxer=rawvideo --rawvideo=w=1280:h=720 /dev/zero

(Commands which normally close the player will not work in this mode, and you
must kill **mpv** externally to make it exit.)

General input command syntax
----------------------------

`[Shift+][Ctrl+][Alt+][Meta+]<key> [<prefixes>] <command> (<argument>)*`

Newlines always start a new binding. ``#`` starts a comment (outside of quoted
string arguments). To bind commands to the ``#`` key, ``SHARP`` can be used.

<key> is either the literal character the key produces (ASCII or unicode
character), or a symbol name.

Arguments are separated by whitespace. This applies even to string arguments.
For this reason, string arguments should be quoted with ``"``. Inside quotes,
C style escaping can be used.

Optional arguments can be skipped with ``-``.

List of input commands
----------------------

ignore
    Use this to "block" keys that should be unbound, and do nothing. Useful for
    disabling default bindings, without disabling all bindings with
    ``--no-input-default-bindings``.

seek <seconds> [relative|absolute|absolute-percent|- [default-precise|exact|keyframes]]
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

frame_step
    Play one frame, then pause.

frame_back_step
    Go back by one frame, then pause. Note that this can be very slow (it tries
    to be precise, not fast), and sometimes fails to behave as expected. How
    well this works depends on whether precise seeking works correctly (e.g.
    see the ``--hr-seek-demuxer-offset`` option). Video filters or other video
    postprocessing that modifies timing of frames (e.g. deinterlacing) should
    usually work, but might make backstepping silently behave incorrectly in
    corner cases.

    This doesn't work with audio-only playback.

set <property> "<value>"
    Set the given property to the given value.

add <property> [<value>]
    Add the given value to the property. On overflow or underflow, clamp the
    property to the maximum. If <value> is omitted, assume ``1``.

cycle <property> [up|down]
    Cycle the given property. ``up`` and ``down`` set the cycle direction. On
    overflow, set the property back to the minimum, on underflow set it to the
    maximum. If ``up`` or ``down`` is omitted, assume ``up``.

speed_mult <value>
    Multiply the ``speed`` property by the given value.

screenshot [subtitles|video|window|- [single|each-frame]]
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

screenshot "<filename>" [subtitles|video|window]
    Take a screenshot and save it to a given file. Note that the file will be
    saved in the format as set by ``--screenshot-format``, regardless of what
    extension the given filename might have.

    The second argument is as in ``screenshot``.

    This never shows anything on OSD. If the file already exists, or if
    taking the screenshot or if there is an error writing the fie, the command
    fails silently.

playlist_next [weak|force]
    Go to the next entry on the playlist.

    weak (default)
        If the last file on the playlist is currently played, do nothing.
    force
        Terminate playback if there are no more files on the playlist.

playlist_prev [weak|force]
    Go to the previous entry on the playlist.

    weak (default)
        If the first file on the playlist is currently played, do nothing.
    force
        Terminate playback if the first file is being played.

loadfile "<file>" [replace|append]
    Load the given file and play it.

    Second argument:

    <replace> (default)
        Stop playback of the current file, and play the new file immediately.
    <append>
        Append the file to the playlist.

loadlist "<playlist>" [replace|append]
    Load the given playlist file (like ``--playlist``).

playlist_clear
    Clear the playlist, except the currently played file.

run "<command>"
    Run the given command with ``/bin/sh -c``. The string is expanded like in
    property_expansion_.

quit [<code>]
    Exit the player using the given exit code.

quit_watch_later
    Exit player, and store current playback position. Playing that file later
    will seek to the previous position on start.

sub_add "<file>"
    Load the given subtitle file. It's not selected as current subtitle after
    loading.

sub_remove [<id>]
    Remove the given subtitle track. If the ``id`` argument is missing, remove
    the current track. (Works on external subtitle files only.)

sub_reload [<id>]
    Reload the given subtitle tracks. If the ``id`` argument is missing, remove
    the current track. (Works on external subtitle files only.)

    This works by unloading and re-adding the subtitle track.

sub_step <skip>
    Change subtitle timing such, that the subtitle event after the next <skip>
    subtitle events is displayed. <skip> can be negative to step back.

osd [<level>]
    Toggle OSD level. If <level> is specified, set the OSD mode
    (see ``--osd-level`` for valid values).

print_text "<string>"
    Print text to stdout. The string can contain properties (see
    property_expansion_).

show_text "<string>" [<duration>|- [<level>]]
    Show text on the OSD. The string can contain properties, which are expanded
    as described in property_expansion_. This can be used to show playback
    time, filename, and so on.

    <duration> is the time in ms to show the message. By default, it uses the
    same value as ``--osd-duration``.

    <level> is the minimum OSD level to show the text (see ``--osd-level``).

show_progress
    Show the progress bar, the elapsed time and the total duration of the file
    on the OSD.

Input commands that are possibly subject to change
--------------------------------------------------

af_switch "filter1=params,filter2,..."
    Replace the current filter chain with the given list.

af_add "filter1=params,filter2,..."
    Add the given list of audio filters to the audio filter chain.

af_del "filter1,filter2,..."
    Remove the given list of audio filters.

af_clr
    Remove all audio filters. (Conversion filters will be re-added
    automatically if needed.)

vf set|add|toggle|del "filter1=params,filter2,..."
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

    You can assign labels to filter by prefixing them with ``@name:`` (where
    ``name`` is a user-chosen arbitrary identifiers). Labels can be used to
    refer to filters by name in all of the filter chain modification commands.
    For ``add``, using an already used label will replace the existing filter.

    *EXAMPLE for input.conf*:

    - ``a vf set flip`` turn video upside-down on the ``a`` key
    - ``b vf set ""`` remove all video filters on ``b``
    - ``c vf toggle lavfi=gradfun`` toggle debanding on ``c``

Undocumented commands: tv_start_scan, tv_step_channel, tv_step_norm,
tv_step_chanlist, tv_set_channel, tv_last_channel, tv_set_freq, tv_step_freq,
tv_set_norm, dvb_set_channel, radio_step_channel, radio_set_channel,
radio_set_freq, radio_step_freq (all of these should be replaced by properties),
stop (questionable use), get_property (?), af_cmdline, vo_cmdline (experimental).

Input command prefixes
----------------------

osd-auto (default)
    Use the default behavior for this command.
no-osd
    Do not use any OSD for this command.
osd-bar
    If possible, show a bar with this command. Seek commands will show the
    progress bar, property changing commands may show the newly set value.
osd-msg
    If possible, show an OSD message with this command. Seek command show
    the current playback time, property changing commands show the newly set
    value as text.
osd-msg-bar
    Combine osd-bar and osd-msg.
raw
    Don't expand properties in string arguments. (Like ``"${property-name}"``.)
expand-properties (default)
    All string arguments are expanded as described in property_expansion_.


All of the osd prefixes are still overridden by the global ``--osd-level``
settings.

Undocumented prefixes: pausing, pausing_keep, pausing_toggle,
pausing_keep_force. (Should these be made official?)

Properties
----------

Properties are used to set mpv options during runtime, or to query arbitrary
information. They can be manipulated with the ``set``/``add``/``cycle``
commands, and retrieved with ``show_text``, or anything else that uses property
expansion. (See property_expansion_.)

The ``W`` column indicates whether the property is generally writeable. If an
option is referenced, the property should take/return exactly the same values
as the option.

=========================== = ==================================================
Name                        W Comment
=========================== = ==================================================
osd-level                   x see ``--osd-level``
osd-scale                   x osd font size multiplicator, see ``--osd-scale``
loop                        x see ``--loop``
speed                       x see ``--speed``
filename                      currently played file (path stripped)
path                          currently played file (full path)
media-title                   filename or libquvi QUVIPROP_PAGETITLE
demuxer
stream-path                   filename (full path) of stream layer filename
stream-pos                  x byte position in source stream
stream-start                  start byte offset in source stream
stream-end                    end position in bytes in source stream
stream-length                 length in bytes (${stream-end} - ${stream-start})
stream-time-pos             x time position in source stream (also see time-pos)
length                        length of the current file in seconds
avsync                        last A/V synchronization difference
percent-pos                 x position in current file (0-100)
ratio-pos                   x position in current file (0.0-1.0)
time-pos                    x position in current file in seconds
time-remaining                estimated remaining length of the file in seconds
chapter                     x current chapter number
chapters                      number of chapters
chapter-list                  chapter list as string, current entries marked
edition                     x current MKV edition number
titles                        number of DVD titles
editions                      number of MKV editions
angle                       x current DVD angle
metadata                      metadata key/value pairs
metadata/<key>                value of metadata entry <key>
pause                       x pause status (bool)
cache                         network cache fill state (0-100)
pts-association-mode        x see ``--pts-association-mode``
hr-seek                     x see ``--hr-seek``
volume                      x current volume (0-100)
mute                        x current mute status (bool)
audio-delay                 x see ``--audio-delay``
audio-format                  audio format (string)
audio-codec                   audio codec selected for decoding
audio-bitrate                 audio bitrate
samplerate                    audio samplerate
channels                      number of audio channels
aid                         x current audio track (similar to ``--aid``)
audio                       x alias for ``aid``
balance                     x audio channel balance
fullscreen                  x see ``--fullscreen``
deinterlace                 x deinterlacing, if available (bool)
colormatrix                 x see ``--colormatrix``
colormatrix-input-range     x see ``--colormatrix-input-range``
colormatrix-output-range    x see ``--colormatrix-output-range``
ontop                       x see ``--ontop``
border                      x see ``--border``
framedrop                   x see ``--framedrop``
gamma                       x see ``--gamma``
brightness                  x see ``--brightness``
contrast                    x see ``--contrast``
saturation                  x see ``--saturation``
hue                         x see ``--hue``
panscan                     x see ``--panscan``
video-format                  video format (string)
video-codec                   video codec selected for decoding
video-bitrate                 video bitrate
width                         video width (container or decoded size)
height                        video height
fps                           container FPS (may contain bogus values)
dwidth                        video width (after filters and aspect scaling)
dheight                       video height
aspect                      x video aspect
vid                         x current video track (similar to ``--vid``)
video                       x alias for ``vid``
program                     x switch TS program (write-only)
sid                         x current subtitle track (similar to ``--sid``)
sub                         x alias for ``sid``
sub-delay                   x see ``--sub-delay``
sub-pos                     x see ``--sub-pos``
sub-visibility              x whether current subtitle is rendered
sub-forced-only             x see ``--sub-forced-only``
sub-scale                   x subtitle font size multiplicator
ass-use-margins             x see ``--ass-use-margins``
ass-vsfilter-aspect-compat  x see ``--ass-vsfilter-aspect-compat``
ass-style-override          x see ``--ass-style-override``
stream-capture              x a filename, see ``--capture``
tv-brightness               x
tv-contrast                 x
tv-saturation               x
tv-hue                      x
playlist-pos                  current position on playlist
playlist-count                number of total playlist entries
playlist                      playlist, current entry marked
track-list                    list of audio/video/sub tracks, cur. entr. marked
chapter-list                  list of chapters, current entry marked
=========================== = ==================================================

.. _property_expansion:

Property expansion
------------------

All string arguments to input commands as well as certain options (like
``--playing-msg``) are subject to property expansion.

*EXAMPLE for input.conf*:

- ``i show_text "Filename: ${filename}"`` shows the filename of the current file
  when pressing the ``i`` key

Within ``input.conf``, property expansion can be inhibited by putting the
``raw`` prefix in front of commands.

The following expansions are supported:

\${NAME}
    Expands to the value of the property ``NAME``. If retrieving the property
    fails, expand to an error string. (Use ``${NAME:}`` with a trailing
    ``:`` to expand to an empty string instead.)
    If ``NAME`` is prefixed with ``=``, expand to the raw value of the property
    (see below).
\${NAME:STR}
    Expands to the value of the property ``NAME``, or ``STR`` if the
    property can't be retrieved. ``STR`` is expanded recursively.
\${!NAME:STR}
    Expands to ``STR`` (recursively) if the property ``NAME`` can't be
    retrieved.
\${?NAME:STR}
    Expands to ``STR`` (recursively) if the property ``NAME`` is available.
\$\$
    Expands to ``$``.
\$}
    Expands to ``}``. (To produce this character inside recursive
    expansion.)
\$>
    Disable property expansion and special handling of ``$`` for the rest
    of the string.

In places where property expansion is allowed, C-style escapes are often
accepted as well. Example:

- ``\n`` becomes a newline character
- ``\\`` expands to ``\``

Raw and formatted properties
----------------------------

Normally, properties are formatted as human readable text, meant to be
displayed on OSD or on the terminal. It is possible to retrieve an unformatted
(raw) value from a property by prefixing its name with ``=``. These raw values
can be parsed by scripts, and follow the same conventions as the options
associated with the properties.

*EXAMPLE*:

- ``${time-pos}`` expands to ``00:14:23`` (if playback position is at 15 minutes
  32 seconds)
- ``${=time-pos}`` expands to ``863.4`` (same time, plus 400 milliseconds -
  milliseconds are normally not shown in the formatted case)

Sometimes, the difference in amount of information carried by raw and formatted
property values can be rather big. In some cases, raw values have more
information, like higher precision than seconds with ``time-pos``. Sometimes
it's the other way around, e.g. ``aid`` shows track title and language in the
formatted case, but only the track number if it's raw.
