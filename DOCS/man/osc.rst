ON SCREEN CONTROLLER
====================

The On Screen Controller (short: OSC) is a minimal GUI integrated with mpv to
offer basic mouse-controllability. It is intended to make interaction easier
for new users and to enable precise and direct seeking.

The OSC is enabled by default if mpv was compiled with Lua support. It can be
disabled entirely using the ``--osc=no`` option.

Using the OSC
-------------

By default, the OSC will show up whenever the mouse is moved inside the
player window and will hide if the mouse is not moved outside the OSC for
0.5 seconds or if the mouse leaves the window.

The Interface
~~~~~~~~~~~~~

::

    +---------+----------+------------------------------------------+----------+
    | pl prev | pl next  |  title                                   |    cache |
    +------+--+---+------+---------+-----------+------+-------+-----+-----+----+
    | play | skip | skip | time    |  seekbar  | time | audio | sub | vol | fs |
    |      | back | frwd | elapsed |           | left |       |     |     |    |
    +------+------+------+---------+-----------+------+-------+-----+-----+----+


pl prev
    =============   ================================================
    left-click      play previous file in playlist
    right-click     show playlist
    shift+L-click   show playlist
    =============   ================================================

pl next
    =============   ================================================
    left-click      play next file in playlist
    right-click     show playlist
    shift+L-click   show playlist
    =============   ================================================

title
    | Displays current media-title, filename, custom title, or target chapter
      name while hovering the seekbar.

    =============   ================================================
    left-click      show playlist position and length and full title
    right-click     show filename
    =============   ================================================

cache
    | Shows current cache fill status

play
    =============   ================================================
    left-click      toggle play/pause
    =============   ================================================

skip back
    =============   ================================================
    left-click      go to beginning of chapter / previous chapter
    right-click     show chapters
    shift+L-click   show chapters
    =============   ================================================

skip frwd
    =============   ================================================
    left-click      go to next chapter
    right-click     show chapters
    shift+L-click   show chapters
    =============   ================================================

time elapsed
    | Shows current playback position timestamp

    =============   ================================================
    left-click      toggle displaying timecodes with milliseconds
    =============   ================================================

seekbar
    | Indicates current playback position and position of chapters

    =============   ================================================
    left-click      seek to position
    right-click     seek to the nearest chapter
    mouse wheel     seek forward/backward
    =============   ================================================

time left
    | Shows remaining playback time timestamp

    =============   ================================================
    left-click      toggle between total and remaining time
    =============   ================================================

audio and sub
    | Displays selected track and amount of available tracks

    =============   ================================================
    left-click      cycle audio/sub tracks forward
    right-click     cycle audio/sub tracks backwards
    shift+L-click   show available audio/sub tracks
    mouse wheel     cycle audio/sub tracks forward/backwards
    =============   ================================================

vol
    =============   ================================================
    left-click      toggle mute
    mouse wheel     volume up/down
    =============   ================================================

fs
    =============   ================================================
    left-click      toggle fullscreen
    =============   ================================================

Key Bindings
~~~~~~~~~~~~

These key bindings are active by default if nothing else is already bound to
these keys. In case of collision, the function needs to be bound to a
different key. See the `Script Commands`_ section.

=============   ================================================
del             Cycles visibility between never / auto (mouse-move) / always
=============   ================================================

Configuration
-------------

This script can be customized through a config file ``script-opts/osc.conf``
placed in mpv's user directory and through the ``--script-opts`` command-line
option. The configuration syntax is described in `mp.options functions`_.

Command-line Syntax
~~~~~~~~~~~~~~~~~~~

To avoid collisions with other scripts, all options need to be prefixed with
``osc-``.

Example::

    --script-opts=osc-optionA=value1,osc-optionB=value2


Configurable Options
~~~~~~~~~~~~~~~~~~~~

``layout``
    Default: bottombar

    The layout for the OSC. Currently available are: box, slimbox,
    bottombar and topbar. Default pre-0.21.0 was 'box'.

``seekbarstyle``
    Default: bar

    Sets the style of the playback position marker and overall shape
    of the seekbar: ``bar``, ``diamond`` or ``knob``.

``seekbarhandlesize``
    Default: 0.6

    Size ratio of the seek handle if ``seekbarstyle`` is set to ``diamond``
    or ``knob``. This is relative to the full height of the seekbar.

``seekbarkeyframes``
    Default: yes

    Controls the mode used to seek when dragging the seekbar. If set to ``yes``,
    default seeking mode is used (usually keyframes, but player defaults and
    heuristics can change it to exact). If set to ``no``, exact seeking on
    mouse drags will be used instead. Keyframes are preferred, but exact seeks
    may be useful in cases where keyframes cannot be found. Note that using
    exact seeks can potentially make mouse dragging much slower.

``seekrangestyle``
    Default: inverted

    Display seekable ranges on the seekbar. ``bar`` shows them on the full
    height of the bar, ``line`` as a thick line and ``inverted`` as a thin
    line that is inverted over playback position markers. ``none`` will hide
    them. Additionally, ``slider`` will show a permanent handle inside the seekbar
    with cached ranges marked inside. Note that these will look differently
    based on the seekbarstyle option. Also, ``slider`` does not work with
    ``seekbarstyle`` set to ``bar``.

``seekrangeseparate``
    Default: yes

    Controls whether to show line-style seekable ranges on top of the
    seekbar or separately if ``seekbarstyle`` is set to ``bar``.

``seekrangealpha``
    Default: 200

    Alpha of the seekable ranges, 0 (opaque) to 255 (fully transparent).

``scrollcontrols``
    Default: yes

    By default, going up or down with the mouse wheel can trigger certain
    actions (such as seeking) if the mouse is hovering an OSC element.
    Set to ``no`` to disable any special mouse wheel behavior.

``deadzonesize``
    Default: 0.5

    Size of the deadzone. The deadzone is an area that makes the mouse act
    like leaving the window. Movement there won't make the OSC show up and
    it will hide immediately if the mouse enters it. The deadzone starts
    at the window border opposite to the OSC and the size controls how much
    of the window it will span. Values between 0.0 and 1.0, where 0 means the
    OSC will always popup with mouse movement in the window, and 1 means the
    OSC will only show up when the mouse hovers it. Default pre-0.21.0 was 0.

``minmousemove``
    Default: 0

    Minimum amount of pixels the mouse has to move between ticks to make
    the OSC show up. Default pre-0.21.0 was 3.

``showwindowed``
    Default: yes

    Enable the OSC when windowed

``showfullscreen``
    Default: yes

    Enable the OSC when fullscreen

``idlescreen``
    Default: yes

    Show the mpv logo and message when idle

``scalewindowed``
    Default: 1.0

    Scale factor of the OSC when windowed.

``scalefullscreen``
    Default: 1.0

    Scale factor of the OSC when fullscreen

``vidscale``
    Default: auto

    Scale the OSC with the video.
    ``no`` tries to keep the OSC size constant as much as the window size allows.
    ``auto`` scales the OSC with the OSD, which is scaled with the window or kept at a
    constant size, depending on the ``--osd-scale-by-window`` option.

``valign``
    Default: 0.8

    Vertical alignment, -1 (top) to 1 (bottom)

``halign``
    Default: 0.0

    Horizontal alignment, -1 (left) to 1 (right)

``barmargin``
    Default: 0

    Margin from bottom (bottombar) or top (topbar), in pixels

``boxalpha``
    Default: 80

    Alpha of the background box, 0 (opaque) to 255 (fully transparent)

``hidetimeout``
    Default: 500

    Duration in ms until the OSC hides if no mouse movement, must not be
    negative

``fadeduration``
    Default: 200

    Duration of fade out in ms, 0 = no fade

``title``
    Default: ${media-title}

    String that supports property expansion that will be displayed as
    OSC title.
    ASS tags are escaped and newlines are converted to spaces.

``tooltipborder``
    Default: 1

    Size of the tooltip outline when using bottombar or topbar layouts

``timetotal``
    Default: no

    Show total time instead of time remaining

``remaining_playtime``
    Default: yes

    Whether the time-remaining display takes speed into account.
    ``yes`` - how much playback time remains at the current speed.
    ``no`` - how much video-time remains.

``timems``
    Default: no

    Display timecodes with milliseconds

``tcspace``
    Default: 100 (allowed: 50-200)

    Adjust space reserved for timecodes (current time and time remaining) in
    the ``bottombar`` and ``topbar`` layouts. The timecode width depends on the
    font, and with some fonts the spacing near the timecodes becomes too small.
    Use values above 100 to increase that spacing, or below 100 to decrease it.

``visibility``
    Default: auto (auto hide/show on mouse move)

    Also supports ``never`` and ``always``

``boxmaxchars``
    Default: 80

    Max chars for the osc title at the box layout. mpv does not measure the
    text width on screen and so it needs to limit it by number of chars. The
    default is conservative to allow wide fonts to be used without overflow.
    However, with many common fonts a bigger number can be used. YMMV.

``boxvideo``
    Default: no

    Whether to overlay the osc over the video (``no``), or to box the video
    within the areas not covered by the osc (``yes``). If this option is set,
    the osc may overwrite the ``--video-margin-ratio-*`` options, even if the
    user has set them. (It will not overwrite them if all of them are set to
    default values.) Additionally, ``visibility`` must be set to ``always``.
    Otherwise, this option does nothing.

    Currently, this is supported for the ``bottombar`` and ``topbar`` layout
    only. The other layouts do not change if this option is set. Separately,
    if window controls are present (see below), they will be affected
    regardless of which osc layout is in use.

    The border is static and appears even if the OSC is configured to appear
    only on mouse interaction. If the OSC is invisible, the border is simply
    filled with the background color (black by default).

    This currently still makes the OSC overlap with subtitles (if the
    ``--sub-use-margins`` option is set to ``yes``, the default). This may be
    fixed later.

    This does not work correctly with video outputs like ``--vo=xv``, which
    render OSD into the unscaled video.

``windowcontrols``
    Default: auto (Show window controls if there is no window border)

    Whether to show window management controls over the video, and if so,
    which side of the window to place them. This may be desirable when the
    window has no decorations, either because they have been explicitly
    disabled (``border=no``) or because the current platform doesn't support
    them (eg: gnome-shell with wayland).

    The set of window controls is fixed, offering ``minimize``, ``maximize``,
    and ``quit``. Not all platforms implement ``minimize`` and ``maximize``,
    but ``quit`` will always work.

``windowcontrols_alignment``
    Default: right

    If window controls are shown, indicates which side should they be aligned
    to.

    Supports ``left`` and ``right`` which will place the controls on those
    respective sides.

``windowcontrols_title``
    Default: ${media-title}

    String that supports property expansion that will be displayed as the
    windowcontrols title.
    ASS tags are escaped, and newlines and trailing slashes are stripped.

``greenandgrumpy``
    Default: no

    Set to ``yes`` to reduce festivity (i.e. disable santa hat in December.)

``livemarkers``
    Default: yes

    Update chapter markers positions on duration changes, e.g. live streams.
    The updates are unoptimized - consider disabling it on very low-end systems.

``chapters_osd``, ``playlist_osd``
    Default: yes

    Whether to display the chapters/playlist at the OSD when left-clicking the
    next/previous OSC buttons, respectively.

``chapter_fmt``
    Default: ``Chapter: %s``

    Template for the chapter-name display when hovering the seekbar.
    Use ``no`` to disable chapter display on hover. Otherwise it's a lua
    ``string.format`` template and ``%s`` is replaced with the actual name.

``unicodeminus``
    Default: no

    Use a Unicode minus sign instead of an ASCII hyphen when displaying
    the remaining playback time.

``background_color``
    Default: #000000

    Sets the background color of the OSC.

``timecode_color``
    Default: #FFFFFF

    Sets the color of the timecode and seekbar, of the OSC.

``title_color``
    Default: #FFFFFF

    Sets the color of the video title. Formatted as #RRGGBB.

``time_pos_color``
    Default: #FFFFFF

    Sets the color of the timecode at hover position in the seekbar.

``time_pos_outline_color``
    Default: #FFFFFF

    Sets the color of the timecode's outline at hover position in the seekbar.
    Also affects the timecode in the slimbox layout.

``buttons_color``
    Default: #FFFFFF

    Sets the colors of the big buttons.

``top_buttons_color``
    Default: #FFFFFF

    Sets the colors of the top buttons.

``small_buttonsL_color``
    Default: #FFFFFF

    Sets the colors of the small buttons on the left in the box layout.

``small_buttonsR_color``
    Default: #FFFFFF

    Sets the colors of the small buttons on the right in the box layout.

``held_element_color``
    Default: #999999

    Sets the colors of the elements that are being pressed or held down.

``tick_delay``
    Default: 1/60

    Sets the minimum interval between OSC redraws in seconds. This can be
    decreased on fast systems to make OSC rendering smoother.

    Ignored if ``tick_delay_follow_display_fps`` is set to yes and the VO
    supports the ``display-fps`` property.

``tick_delay_follow_display_fps``
    Default: no

    Use display fps to calculate the interval between OSC redraws.


Script Commands
~~~~~~~~~~~~~~~

The OSC script listens to certain script commands. These commands can bound
in ``input.conf``, or sent by other scripts.

``osc-visibility``
    Controls visibility mode ``never`` / ``auto`` (on mouse move) / ``always``
    and also ``cycle`` to cycle between the modes.

``osc-show``
    Triggers the OSC to show up, just as if user moved mouse.

Example

You could put this into ``input.conf`` to hide the OSC with the ``a`` key and
to set auto mode (the default) with ``b``::

    a script-message osc-visibility never
    b script-message osc-visibility auto

``osc-idlescreen``
    Controls the visibility of the mpv logo on idle. Valid arguments are ``yes``,
    ``no``, and ``cycle`` to toggle between yes and no.
