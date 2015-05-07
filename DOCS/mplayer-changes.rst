CHANGES FROM OTHER VERSIONS OF MPLAYER
======================================

**mpv** is based on mplayer2, which in turn is based on the original MPlayer
(also called mplayer, mplayer-svn, mplayer1). Many changes have been made, a
large part of which is incompatible or completely changes how the player
behaves. Although there are still many similarities to its ancestors, **mpv**
should generally be treated as a completely different program.

.. note::
    These lists are incomplete.

General Changes from MPlayer to mpv
-----------------------------------

This listing is about changes introduced by mplayer2 and mpv relatively to
MPlayer.

Player
~~~~~~

* New name for the binary (``mpv``). New location for config files (either
  ``~/.config/mpv/mpv.conf``, or if you want, ``~/.mpv/config``).
* Encoding functionality (replacement for MEncoder, see the `ENCODING`_ section).
* Support for Lua scripting (see the `LUA SCRIPTING`_ section).
* Better pause handling (e.g. do not unpause on a command).
* Precise seeking support.
* Improvements in audio/video sync handling.
* Do not lose settings when playing a new file in the same player instance.
* Slave mode compatibility broken (see below).
* Re-enable screensaver while the player is paused.
* Allow resuming playback at a later point with ``Shift+q``, also see the
  ``quit_watch_later`` input command.
* ``--keep-open`` option to stop the player from closing the window and
  exiting after playback ends.
* A client API, that allows embedding **mpv** into applications
  (see ``libmpv/client.h`` in the sources).

Input
~~~~~

* Improved default keybindings. MPlayer bindings are also available (see
  ``etc/mplayer-input.conf`` in the source tree).
* Improved responsiveness on user input.
* Support for modifier keys (alt, shift, ctrl) in input.conf.
* Allow customizing whether a key binding for seeking shows the video time, the
  OSD bar, or nothing (see the `Input Command Prefixes`_ section).
* Support mapping multiple commands to one key.
* Classic LIRC support was removed. Install remotes as input devices instead.
  This way they will send X11 key events to the mpv window, which can be bound
  using the normal ``input.conf``.
  Also see: http://github.com/mpv-player/mpv/wiki/IR-remotes
* Joystick support was removed. It was considered useless and was the cause
  of some problems (e.g. a laptop's accelerator being recognized as joystick).

Audio
~~~~~

* Support for gapless audio (see the ``--gapless-audio`` option).
* Support for OSS4 volume control.
* Improved support for PulseAudio.
* Make ``--softvol`` default (**mpv** is not a mixer control panel).
* By default, do pitch correction if playback speed is increased.
* Improved downmixing and output of surround audio:

  - Instead of using hardcoded pan filters to do remixing, libavresample is used
  - Channel maps are used to identify the channel layout, so e.g. ``3.0`` and
    ``2.1`` audio can be distinguished.

Video
~~~~~

* Wayland support.
* Native support for VAAPI and VDA. Improved VDPAU video output.
* Improved OpenGL output (see the ``opengl-hq`` video output).
* Make hardware decoding work with the ``opengl`` video output.
* Support for libavfilter (for video->video and audio->audio). This allows
  using most of FFmpeg's filters, which improve greatly on the old MPlayer
  filters in features, performance, and correctness.
* More correct color reproduction (color matrix generation), including support
  for BT.2020 (Ultra HD) and linear XYZ (Digital Cinema) inputs.
* Support for color managed displays, via ICC profiles.
* High-quality image resamplers (see the ``opengl`` ``scale`` suboption).
* Support for scaling in (sigmoidized) linear light.
* Better subtitle rendering using libass by default.
* Improvements when playing multiple files (``-fixed-vo`` is default, do not
  reset settings by default when playing a new file).
* Replace image video outputs (``--vo=jpeg`` etc.) with ``--vo=image``.
* Removal of ``--vo=gif89a``, ``--vo=md5sum``, ``--vo=yuv4mpeg``, as encoding
  can handle these use cases. For yuv4mpeg, for example, use::

    mpv input.mkv -o output.y4m --no-audio --oautofps --oneverdrop

* Image subtitles (DVDs etc.) are rendered in color and use more correct
  positioning (color for image subs can be disabled with ``--sub-gray``).

OSD and terminal
~~~~~~~~~~~~~~~~

* Cleaned up terminal output: nicer status line, less useless noise.
* Improved OSD rendering using libass, with full Unicode support.
* New OSD bar with chapter marks. Not positioned in the middle of the video
  (this can be customized with the ``--osd-bar-align-y`` option).
* Display list of chapters and audio/subtitle tracks on OSD (see the
  `Properties`_ section).

Screenshots
~~~~~~~~~~~

* Instant screenshots without 1-frame delay.
* Support for taking screenshots even with hardware decoding.
* Support for saving screenshots as JPEG or PNG.
* Support for configurable file names.
* Support for taking screenshots with or without subtitles.

Note that the ``screenshot`` video filter is not needed anymore, and should not
be put into the mpv config file.

Miscellaneous
~~~~~~~~~~~~~

* Better MKV support (e.g. ordered chapters, 3D metadata).
* Matroska edition switching at runtime.
* Support for playing URLs of popular streaming sites directly.
  (e.g. ``mpv https://www.youtube.com/watch?v=...``).
  Requires a recent version of ``youtube-dl`` to be installed. Can be
  disabled with ``ytdl=no`` in the mpv config file.
* Support for precise scrolling which scales the parameter of commands. If the
  input doesn't support precise scrolling the scale factor stays 1.
* Allow changing/adjusting video filters at runtime. (This is also used to make
  the ``D`` key insert vf_yadif if deinterlacing is not supported otherwise).
* Improved support for .cue files.

Mac OS X
~~~~~~~~

* Native OpenGL backend.
* Cocoa event loop is independent from MPlayer's event loop, so user
  actions like accessing menus and live resizing do not block the playback.
* Apple Remote support.
* Media Keys support.
* VDA support using libavcodec hwaccel API instead of FFmpeg's decoder with up
  to 2-2.5x reduction in CPU usage.

Windows
~~~~~~~

* Improved support for Unicode file names.
* Improved window handling.
* Do not block playback when moving the window.
* Improved Direct3D video output.
* Added WASAPI audio output.

Internal changes
~~~~~~~~~~~~~~~~

* Switch to GPLv2+ (see ``Copyright`` file for details).
* Removal of lots of cruft:

  - Internal GUI (replaced by the OSC, see the `ON SCREEN CONTROLLER`_ section).
  - MEncoder (replaced by native encoding, see the `ENCODING`_ section).
  - OSD menu.
  - Kernel video drivers for Linux 2.4 (including VIDIX).
  - Teletext support.
  - Support for dead platforms.
  - Most built-in demuxers have been replaced by their libavformat counterparts.
  - Built-in network support has been replaced by libavformat's (which also
    supports https URLs).
  - Embedded copies of libraries (such as FFmpeg).

* General code cleanups (including refactoring or rewrites of many parts).
* New build system.
* Many bug fixes and removal of long-standing issues.
* Generally preferring FFmpeg/Libav over internal demuxers, decoders, and
  filters.

Detailed Listing of User-visible Changes
----------------------------------------

This listing is about changed command line switches, slave commands, and similar
things. Completely removed features are not listed.

Command Line Switches
~~~~~~~~~~~~~~~~~~~~~

* There is a new command line syntax, which is generally preferred over the old
  syntax. ``-optname optvalue`` becomes ``--optname=optvalue``.

  The old syntax will not be removed. However, the new syntax is mentioned in
  all documentation and so on, and unlike the old syntax is not ambiguous,
  so it is a good thing to know about this change.
* In general, negating switches like ``-noopt`` now have to be written as
  ``-no-opt`` or ``--no-opt``.
* Per-file options are not the default anymore. You can explicitly specify
  file-local options. See ``Usage`` section.
* Many options have been renamed, removed or changed semantics. Some options
  that are required for a good playback experience with MPlayer are now
  superfluous or even worse than the defaults, so make sure to read the manual
  before trying to use your existing configuration with **mpv**.
* Table of renamed/replaced switches:

    =========================== ========================================
    Old                         New
    =========================== ========================================
    ``-no<opt>``                ``--no-<opt>`` (add a dash)
    ``-a52drc level``           ``--ad-lavc-ac3drc=level``
    ``-ac spdifac3``            ``--ad=spdif:ac3`` (see ``--ad=help``)
    ``-af volnorm``             ``--af=drc`` (renamed)
    ``-afm hwac3``              ``--ad=spdif:ac3,spdif:dts``
    ``-ao alsa:device=hw=0.3``  ``--ao=alsa:device=[hw:0,3]``
    ``-aspect``                 ``--video-aspect``
    ``-ass-bottom-margin``      ``--vf=sub=bottom:top``
    ``-ass``                    ``--sub-ass``
    ``-audiofile-cache``        (removed; the main cache settings are used)
    ``-audiofile``              ``--audio-file``
    ``-benchmark``              ``--untimed`` (no stats)
    ``-capture``                ``--stream-capture=<filename>``
    ``-channels``               ``--audio-channels`` (changed semantics)
    ``-cursor-autohide-delay``  ``--cursor-autohide``
    ``-delay``                  ``--audio-delay``
    ``-dumpstream``             ``--stream-dump=<filename>``
    ``-dvdangle``               ``--dvd-angle``
    ``-endpos``                 ``--length``
    ``-fixed-vo``               (removed; always the default)
    ``-font``                   ``--osd-font``
    ``-forcedsubsonly``         ``--sub-forced-only``
    ``-forceidx``               ``--index``
    ``-format``                 ``--audio-format``
    ``-fsmode-dontuse``         (removed)
    ``-fstype``                 ``--x11-netwm`` (changed semantics)
    ``-hardframedrop``          ``--framedrop=hard``
    ``-identify``               (removed; use TOOLS/mpv_identify.sh)
    ``-idx``                    ``--index``
    ``-lavdopts ...``           ``--vd-lavc-...``
    ``-lavfdopts``              ``--demuxer-lavf-...``
    ``-loop 0``                 ``--loop=inf``
    ``-mixer-channel``          AO suboptions (``alsa``, ``oss``)
    ``-mixer``                  AO suboptions (``alsa``, ``oss``)
    ``-mouse-movements``        ``--input-cursor``
    ``-msgcolor``               ``--msg-color``
    ``-msglevel``               ``--msg-level`` (changed semantics)
    ``-msgmodule``              ``--msg-module``
    ``-name``                   ``--x11-name``
    ``-noar``                   ``--no-input-appleremote``
    ``-noautosub``              ``--no-sub-auto``
    ``-noconsolecontrols``      ``--no-input-terminal``
    ``-nosound``                ``--no-audio``
    ``-osdlevel``               ``--osd-level``
    ``-panscanrange``           ``--video-zoom``, ``--video-pan-x/y``
    ``-playing-msg``            ``--term-playing-msg``
    ``-pp ...``                 ``'--vf=lavfi=[pp=...]'``
    ``-pphelp``                 (See FFmpeg libavfilter documentation.)
    ``-rawaudio ...``           ``--demuxer-rawaudio-...``
    ``-rawvideo ...``           ``--demuxer-rawvideo-...``
    ``-spugauss``               ``--sub-gauss``
    ``-srate``                  ``--audio-samplerate``
    ``-ss``                     ``--start``
    ``-ssf <sub>``              ``--sws-...``
    ``-stop-xscreensaver``      ``--stop-screensaver``
    ``-sub-fuzziness``          ``--sub-auto``
    ``-sub``                    ``--sub-file``
    ``-subcp``                  ``--sub-codepage``
    ``-subdelay``               ``--sub-delay``
    ``-subfile``                ``--sub-file``
    ``-subfont-*``              ``--sub-text-*``, ``--osd-*``
    ``-subfont-text-scale``     ``--sub-scale``
    ``-subfont``                ``--sub-text-font``
    ``-subfps``                 ``--sub-fps``
    ``-subpos``                 ``--sub-pos``
    ``-sws``                    ``--sws-scaler``
    ``-tvscan``                 ``--tv-scan``
    ``-use-filename-title``     ``--title='${filename}'``
    ``-vc ffh264vdpau`` (etc.)  ``--hwdec=vdpau``
    ``-vobsub``                 ``--sub-file`` (pass the .idx file)
    ``-x W``, ``-y H``          ``--geometry=WxH`` + ``--no-keepaspect``
    ``-xineramascreen``         ``--screen`` (different values)
    ``-xy W``                   ``--autofit=W``
    ``-zoom``                   Inverse available as ``--video-unscaled``
    ``dvdnav://``               ``dvdnav://menu``
    ``dvd://1``                 ``dvd://0`` (0-based offset)
    =========================== ========================================

.. note::

    ``-opt val`` becomes ``--opt=val``.

.. note::

    Quite some video filters, video outputs, audio filters, audio outputs, had
    changes in their option parsing. These aren't mentioned in the table above.

    Also, some video and audio filters have been removed, and you have to use
    libavfilter (using ``--vf=lavfi=[...]`` or ``--af=lavfi=[...]``) to get
    them back.

input.conf and Slave Commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Table of renamed input commands:

    This lists only commands that are not always gracefully handled by the
    internal legacy translation layer. If an input.conf contains any legacy
    commands, a warning will be printed when starting the player. The warnings
    also show the replacement commands.

    Properties containing ``_`` to separate words use ``-`` instead.

    +--------------------------------+----------------------------------------+
    | Old                            | New                                    |
    +================================+========================================+
    | ``pt_step 1 [0|1]``            | ``playlist_next [weak|force]``         |
    |                                | (translation layer cannot deal with    |
    |                                | whitespace)                            |
    +--------------------------------+----------------------------------------+
    | ``pt_step -1 [0|1]``           | ``playlist_prev [weak|force] (same)``  |
    +--------------------------------+----------------------------------------+
    | ``switch_ratio [<ratio>]``     | ``set video-aspect <ratio>``           |
    |                                |                                        |
    |                                | ``set video-aspect 0`` (reset aspect)  |
    +--------------------------------+----------------------------------------+
    | ``step_property_osd <prop>``   | ``cycle <prop> <step>`` (wraps),       |
    | ``<step> <dir>``               | ``add <prop> <step>`` (clamps).        |
    |                                | ``<dir>`` parameter unsupported. Use   |
    |                                | a negative ``<step>`` instead.         |
    +--------------------------------+----------------------------------------+
    | ``step_property <prop>``       | Prefix ``cycle`` or ``add`` with       |
    | ``<step> <dir>``               | ``no-osd``: ``no-osd cycle <prop>``    |
    |                                | ``<step>``                             |
    +--------------------------------+----------------------------------------+
    | ``osd_show_property_text``     | ``show_text <text>``                   |
    | ``<text>``                     | The property expansion format string   |
    |                                | syntax slightly changed.               |
    +--------------------------------+----------------------------------------+
    | ``osd_show_text``              | Now does the same as                   |
    |                                | ``osd_show_property_text``. Use the    |
    |                                | ``raw`` prefix to disable property     |
    |                                | expansion.                             |
    +--------------------------------+----------------------------------------+
    | ``show_tracks``                | ``show_text ${track-list}``            |
    +--------------------------------+----------------------------------------+
    | ``show_chapters``              | ``show_text ${chapter-list}``          |
    +--------------------------------+----------------------------------------+
    | ``af_switch``, ``af_add``, ... | ``af set|add|...``                     |
    +--------------------------------+----------------------------------------+
    | ``tv_start_scan``              | ``set tv-scan yes``                    |
    +--------------------------------+----------------------------------------+
    | ``tv_set_channel <val>``       | ``set tv-channel <val>``               |
    +--------------------------------+----------------------------------------+
    | ``tv_step_channel``            | ``cycle tv-channel``                   |
    +--------------------------------+----------------------------------------+
    | ``dvb_set_channel <v1> <v2>``  | ``set dvb-channel <v1>-<v2>``          |
    +--------------------------------+----------------------------------------+
    | ``dvb_step_channel``           | ``cycle dvb-channel``                  |
    +--------------------------------+----------------------------------------+
    | ``tv_set_freq <val>``          | ``set tv-freq <val>``                  |
    +--------------------------------+----------------------------------------+
    | ``tv_step_freq``               | ``cycle tv-freq``                      |
    +--------------------------------+----------------------------------------+
    | ``tv_set_norm <norm>``         | ``set tv-norm <norm>``                 |
    +--------------------------------+----------------------------------------+
    | ``tv_step_norm``               | ``cycle tv-norm``                      |
    +--------------------------------+----------------------------------------+

    .. note::

        Due to lack of hardware and users using the TV/DVB/PVR features, and
        due to the need to cleanup the related command code, it's possible
        that the new commands are buggy or behave worse. This can be improved
        if testers are available. Otherwise, some of the TV code will be
        removed at some point.

Slave mode
~~~~~~~~~~

* Slave mode was removed. A proper slave mode application needed tons of code
  and hacks to get
  it right. The main problem is that slave mode is a bad and incomplete
  interface, and to get around that, applications parsed output messages
  intended for users. It is hard to know which messages exactly are parsed by
  slave mode applications. This makes it virtually impossible to improve
  terminal output intended for users without possibly breaking something.

  This is absolutely insane, and since initial improvements to **mpv** quickly
  made slave mode incompatible to most applications, it was removed as useless
  cruft. The client API (see below) is provided instead.

  ``--identify`` was replaced by the ``TOOLS/mpv_identify.sh`` wrapper script.

* For some time (until including release 0.4.x), mpv supported a
  ``--slave-broken`` option. The following options are equivalent:

  ::

        --input-file=/dev/stdin --input-terminal=no


  Assuming the system supports ``/dev/stdin``.

  (The option was readded in 0.5.1 and sets exactly these options.)

* A JSON RPC protocol giving access to the client API is also supported. See
  `JSON IPC`_ for more information.

* **mpv** also provides a client API, which can be used to embed the player
  by loading it as shared library. (See ``libmpv/client.h`` in the sources.)
  It might also be possible to implement a custom slave mode-like protocol
  using Lua scripting.

Policy for Removed Features
---------------------------

**mpv** is in active development. If something is in the way of more important
development (such as fixing bugs or implementing new features), we sometimes
remove features. Usually this happens only with old features that either seem
to be useless, or are not used by anyone. Often these are obscure, or
"inherited", or were marked experimental, but never received any particular
praise by any users.

Sometimes, features are replaced by something new. The new code will be either
simpler or more powerful, but doesn't necessarily provide everything the old
feature did.

We can not exclude that we accidentally remove features that are actually
popular. Generally, we do not know how much a specific functionality is used.
If you miss a feature and think it should be re-added, please open an issue
on the mpv bug tracker. Hopefully, a solution can be found. Often, it turns
out that re-adding something is not much of a problem, or that there are
better alternatives.

Why this Fork?
--------------

mplayer2 is practically dead, and mpv started out as a branch containing
new/experimental development. (Some of it was merged right *after* the fork
was made public, seemingly as an acknowledgment that development, or at
least merging, should have been more active.)

MPlayer is focused on not breaking anything, but is stuck with a horrible
codebase resistant to cleanup. (Unless you do what mpv did - merciless and
consequent pruning of bad, old code.) Cleanup and keeping broken things
conflict, so the kind of development mpv strives for can't be done within
MPlayer due to clashing development policies.

Additionally, mplayer2 already had lots of changes over MPlayer, which would
have needed to be backported to the MPlayer codebase. This would not only
have been hard (several years of diverging development), but also would have
been impossible due to the aforementioned MPlayer development policy.
