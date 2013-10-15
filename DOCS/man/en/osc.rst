ON SCREEN CONTROLLER
====================

The On Screen Controller (short: OSC) is a minimal GUI integrated with mpv to
offer basic mouse-controllability. It is intended to make interaction easier
for new users and to enable precise and direct seeking.

The OSC is enabled by default if mpv was compiled with lua support. It can be
disabled using ``--osc=no``.

Using the OSC
-------------

By default, the OSC will show up whenever the mouse is moved inside the
player window and will hide if the mouse is not moved outside the OSC for
0.5 seconds or if the mouse leaves the window.

The Interface
~~~~~~~~~~~~~

::

    +------------------+-----------+--------------------+
    | playlist prev    |   title   |      playlist next |
    +-------+------+---+--+------+-+----+------+--------+
    | audio | skip | seek |      | seek | skip |  full  |
    +-------+ back | back | play | frwd | frwd | screen |
    | sub   |      |      |      |      |      |        |
    +-------+------+------+------+------+------+--------+
    |                     seekbar                       |
    +----------------+--------------+-------------------+
    | time passed    | cache status |    time remaining |
    +----------------+--------------+-------------------+


playlist prev
    =============   ================================================
    left-click      play previous file in playlist
    shift+L-click   show playlist
    =============   ================================================

title
    | Displays current media-title or filename

    =============   ================================================
    left-click      show playlist position and length and full title
    right-click     show filename
    =============   ================================================

playlist next
    =============   ================================================
    left-click      play next file in playlist
    shift+L-click   show playlist
    =============   ================================================

audio and sub
    | Displays selected track and amount of available tracks

    =============   ================================================
    left-click      cycle audio/sub tracks forward
    right-click     cycle audio/sub tracks backwards
    shift+L-click   show available audio/sub tracks
    =============   ================================================

skip back
    =============   ================================================
    left-click      go to beginning of chapter / previous chapter
    shift+L-click   show chapters
    =============   ================================================

seek back
    =============   ================================================
    left-click      skip back  5 seconds
    right-click     skip back 30 seconds
    shift-L-click   skip back  1 frame
    =============   ================================================

play
    =============   ================================================
    left-click      toggle play/pause
    =============   ================================================

seek frwd
    =============   ================================================
    left-click      skip forward 10 seconds
    right-click     skip forward 60 seconds
    shift-L-click   skip forward  1 frame
    =============   ================================================

skip frwd
    =============   ================================================
    left-click      go to next chapter
    shift+L-click   show chapters
    =============   ================================================

fullscreen
    =============   ================================================
    left-click      toggle fullscreen
    =============   ================================================

seekbar
    | Indicates current playback position and position of chapters

    =============   ================================================
    left-click      seek to position
    =============   ================================================

time passed
    | Shows current playback position timestamp

    =============   ================================================
    left-click      toggle displaying timecodes with milliseconds
    =============   ================================================

cache status
    | Shows current cache fill status (only visible when below 48%)

time remaining
    | Shows remaining playback time timestamp

    =============   ================================================
    left-click      toggle between total and remaining time
    =============   ================================================


Configuration
-------------

The OSC offers limited configuration through a config file ``plugin_osc.conf``
placed in mpv's user dir.

Config Syntax
~~~~~~~~~~~~~

The config file must exactly follow the following syntax::

    # this is a comment
    parameter1=value1
    parameter2=value2

``#`` can only be used at the beginning of a line and there may be no
spaces around the ``=`` or anywhere else.

Configurable parameters
~~~~~~~~~~~~~~~~~~~~~~~

``showwindowed``
    | Default: yes
    | Show OSC when windowed?

``showfullscreen``
    | Default: yes
    | Show OSC when fullscreen?

``scalewindowed``
    | Default: 1
    | Scaling of the controller when windowed

``scalefullscreen``
    | Default: 1
    | Scaling of the controller when fullscreen

``scaleforcedwindow``
    | Default: 2
    | Scaling of the controller when rendered on a forced (dummy) window

``vidscale``
    | Default: yes
    | Scale the controller with the video?

``valign``
    | Default: 0.8
    | Vertical alignment, -1 (top) to 1 (bottom)

``halign``
    | Default: 0
    | Horizontal alignment, -1 (left) to 1 (right)

``hidetimeout``
    | Default: 500
    | Duration in ms until the OSC hides if no mouse movement, negative value
      disables auto-hide

``fadeduration``
    | Default: 200
    | Duration of fade out in ms, 0 = no fade

``deadzonesize``
    | Default: 0
    | Size of the deadzone. The deadzone is an area that makes the mouse act
      like leaving the window. Movement there won't make the OSC show up and
      it will hide immediately if the mouse enters it.

``minmousemove``
    | Default: 3
    | Minimum amount of pixels the mouse has to move between ticks to make
      the OSC show up

