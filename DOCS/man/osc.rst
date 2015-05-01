ON SCREEN CONTROLLER
====================

The On Screen Controller (short: OSC) is a minimal GUI integrated with mpv to
offer basic mouse-controllability. It is intended to make interaction easier
for new users and to enable precise and direct seeking.

The OSC is enabled by default if mpv was compiled with lua support. It can be
disabled entirely using the ``--osc=no`` option.

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
    | Shows current cache fill status (only visible when below 45%)

time remaining
    | Shows remaining playback time timestamp

    =============   ================================================
    left-click      toggle between total and remaining time
    =============   ================================================

Key Bindings
~~~~~~~~~~~~

These key bindings are active by default if nothing else is already bound to
these keys. In case of collision, the function needs to be bound to a
different key. See the `Script Commands`_ section.

=============   ================================================
del             Hide the OSC permanently until mpv is restarted.
=============   ================================================

Configuration
-------------

The OSC offers limited configuration through a config file
``lua-settings/osc.conf`` placed in mpv's user dir and through the
``--script-opts`` command-line option. Options provided through the command-line
will override those from the config file.

Config Syntax
~~~~~~~~~~~~~

The config file must exactly follow the following syntax::

    # this is a comment
    optionA=value1
    optionB=value2

``#`` can only be used at the beginning of a line and there may be no
spaces around the ``=`` or anywhere else.

Command-line Syntax
~~~~~~~~~~~~~~~~~~~

To avoid collisions with other scripts, all options need to be prefixed with
``osc-``.

Example::

    --script-opts=osc-optionA=value1,osc-optionB=value2


Configurable Options
~~~~~~~~~~~~~~~~~~~~

``showwindowed``
    | Default: yes
    | Enable the OSC when windowed

``showfullscreen``
    | Default: yes
    | Enable the OSC when fullscreen

``scalewindowed``
    | Default: 1.0
    | Scale factor of the OSC when windowed

``scalefullscreen``
    | Default: 1.0
    | Scale factor of the OSC when fullscreen

``scaleforcedwindow``
    | Default: 2.0
    | Scale factor of the OSC when rendered on a forced (dummy) window

``vidscale``
    | Default: yes
    | Scale the OSC with the video
    | ``no`` tries to keep the OSC size constant as much as the window size allows

``valign``
    | Default: 0.8
    | Vertical alignment, -1 (top) to 1 (bottom)

``halign``
    | Default: 0.0
    | Horizontal alignment, -1 (left) to 1 (right)

``boxalpha``
    | Default: 80
    | Alpha of the background box, 0 (opaque) to 255 (fully transparent)

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
      it will hide immediately if the mouse enters it. The deadzone starts
      at the window border opposite to the OSC and the size controls how much
      of the window it will span. Values between 0 and 1.

``minmousemove``
    | Default: 3
    | Minimum amount of pixels the mouse has to move between ticks to make
      the OSC show up

``layout``
    | Default: box
    | The layout for the OSC. Currently available are: box, slimbox,
      bottombar and topbar.

``seekbarstyle``
    | Default: slider
    | Sets the style of the seekbar, slider (diamond marker) or bar (fill)

Script Commands
~~~~~~~~~~~~~~~

The OSC script listens to certain script commands. These commands can bound
in ``input.conf``, or sent by other scripts.

``enable-osc``
    Undoes ``disable-osc`` or the effect of the ``del`` key.

``disable-osc``
    Hide the OSC permanently. This is also what the ``del`` key does.

``osc-message``
    Show a message on screen using the OSC. First argument is the message,
    second the duration in seconds.


Example

You could put this into ``input.conf`` to hide the OSC with the ``a`` key and
to unhide it with ``b``::

    a script_message disable-osc
    b script_message enable-osc

