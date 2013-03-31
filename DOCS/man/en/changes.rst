.. _changes:

CHANGES FROM OTHER VERSIONS OF MPLAYER
======================================

**mpv** is based on mplayer2, which in turn is based on the original
MPlayer (also called mplayer, mplayer-svn, mplayer1). Many changes
have been made. Some changes are incompatible, or completely change how the
player behaves.

General changes for mplayer-svn to mplayer2
-------------------------------------------

* Removal of the internal GUI, MEncoder, OSD menu
* Better pause handling (don't unpause on a command)
* Better MKV support (such as ordered chapters)
* vo_vdpau improvements
* Precise seeking support
* No embedded copy of ffmpeg and other libraries
* Native OpenGL backend for OSX
* General OSX improvements
* Improvements in audio/video sync handling
* Cleaned up terminal output
* Gapless audio support (``--gapless-audio``)
* Improved responsiveness on user input
* Support for modifier keys (alt, shift, ctrl) in input.conf
* OSS4 volume control
* More correct color reproduction (color matrix generation)
* Use libass for subtitle rendering by default (better quality)
* Generally preferring ffmpeg/libav over internal demuxers and decoders
* Improvements when playing multiple files (``--fixed-vo``)
* Screenshot improvements (instant screenshots without 1-frame delay)
* Improved support for PulseAudio
* General code cleanups
* Many more changes

General changes for mplayer2 to mpv
----------------------------------------

* Removal of lots of unneeded code to encourage developer activity (less
  obscure scary zombie code that kills any desire for hacking the codebase)
* Removal of dust and dead bodies (code-wise), such as kernel drivers for
  decades old hardware
* Removal of support for dead platforms
* Generally improved MS Windows support (dealing with unicode filenames,
  improved ``--vo=direct3d``, improved window handling)
* Better OSD rendering (using libass). This has full unicode support, and
  languages like Arabic should be better supported.
* Cleaned up terminal output (nicer status line, less useless noise)
* Support for playing URLs of popular streaming sites directly
  (e.g. ``mpv https://www.youtube.com/watch?v=...``)
* Improved OpenGL output (``--vo=opengl-hq``)
* Make ``--softvol`` default (**mpv** is not a mixer control panel)
* Improved support for .cue files
* Screenshot improvements (can save screenshots as JPG or PNG, configurable
  filenames, support for taking screenshots with or without subtitles - the
  ``screenshot`` video filter is not needed anymore, and should not be put
  into the mpv config file)
* Removal of teletext support
* Replace image VOs (``--vo=jpeg`` etc.) with ``--vo=image``
* Do not lose settings when playing a new file in the same player instance
* New location for config files, new name for the binary.
* Slave mode compatibility broken (see below)
* Encoding functionality (replacement for mencoder, see ``DOCS/encoding.rst``)
* Remove ``--vo=gif89a``, ``--vo=md5sum``, ``--vo=yuv4mpeg``, as encoding can
  handle these use cases. For yuv4mpeg, for example, use:
  ``mpv input.mkv -o output.y4m --no-audio``.
* Image subtitles (DVDs etc.) are rendered in color and use more correct
  positioning (color can be disabled with ``--sub-gray``)
* General code cleanups
* Many more changes

Detailed listing of user-visible changes
----------------------------------------

This listing is about changed command line switches, slave commands, and similar
things. Completely removed features are not listed.

Command line switches
~~~~~~~~~~~~~~~~~~~~~
* There is a new command line syntax, which is generally preferred over the old
  syntax. ``-optname optvalue`` becomes ``--optname=optvalue``.

  The old syntax will not be removed in the near future. However, the new
  syntax is mentioned in all documentation and so on, so it's a good thing to
  know about this change.

  (The new syntax was introduced in mplayer2.)
* In general, negating switches like ``-noopt`` now have to be written as
  ``-no-opt``, or better ``--no-opt``.
* Per-file options are not the default anymore. You can explicitly specify
  file local options. See ``Usage`` section.
* Table of renamed/replaced switches:

    =================================== ===================================
    Old                                 New
    =================================== ===================================
    -no<opt>                            --no-<opt> (add a dash)
    -nosound                            --no-audio
    -use-filename-title                 --title="${filename}"
    -loop 0                             --loop=inf
    -hardframedrop                      --framedrop=hard
    -osdlevel                           --osd-level
    -delay                              --audio-delay
    -subdelay                           --sub-delay
    -subpos                             --sub-pos
    -forcedsubsonly                     --sub-forced-only
    -ni                                 --avi-ni
    -benchmark                          --untimed (no stats)
    -xineramascreen                     --screen (different values)
    -ss                                 --start
    -endpos                             --length
    --cursor-autohide-delay             --cursor-autohide
    -sub-fuzziness                      --autosub-match
    -subfont                            --sub-text-font
    -font                               --osd-font
    -subfont-*                          --sub-text-*, --osd-*
    -subfont-text-scale                 --sub-scale
    -spugauss                           --sub-gauss
    -vobsub                             --sub (pass the .idx file)
    -ass-bottom-margin                  --vf=sub=bottom:top
    -vc ffh264vdpau (etc.)              --hwdec=vdpau
    -ac spdifac3                        --ad=spdif:ac3 (see --ad=help)
    -afm hwac3                          --ad=spdif:ac3,spdif:dts
    -x W, -y H                          --geometry=WxH + --no-keepaspect
    -xy W                               --autofit=W
    -a52drc level                       --ad-lavc-ac3drc=level
    =================================== ===================================

*NOTE*: ``-opt val`` becomes ``--opt=val``.

input.conf and slave commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Table of renamed input commands:

    This lists only commands that are not always gracefully handled by the
    internal legacy translation layer. If an input.conf contains any legacy
    commands, they will be displayed with ``-v`` when it is loaded, and show
    and the replacement commands.

    Properties containing ``_`` to separate words use ``-`` instead.

    +--------------------------------+----------------------------------------+
    | Old                            | New                                    |
    +================================+========================================+
    | pt_step 1 [0|1]                | playlist_next [weak|force]             |
    |                                | (translation layer can't deal with     |
    |                                | whitespace)                            |
    +--------------------------------+----------------------------------------+
    | pt_step -1 [0|1]               | playlist_prev [weak|force] (same)      |
    +--------------------------------+----------------------------------------+
    | switch_ratio [<ratio>]         | set aspect <ratio>                     |
    |                                | set aspect 0 (to reset aspect)         |
    +--------------------------------+----------------------------------------+
    | step_property_osd <prop> <step>| cycle <prop> <step> (wraps),           |
    | <dir>                          | add <prop> <step> (clamps).            |
    |                                | <dir> parameter unsupported. Use       |
    |                                | a negative step instead.               |
    +--------------------------------+----------------------------------------+
    | step_property <prop> <step>    | Prefix cycle or add with no-osd:       |
    | <dir>                          | no-osd cycle <prop> <step>             |
    +--------------------------------+----------------------------------------+
    | osd_show_property_text <text>  | show_text <text>                       |
    |                                | The property expansion format string   |
    |                                | syntax slightly changed.               |
    +--------------------------------+----------------------------------------+
    | osd_show_text                  | Now does the same as                   |
    |                                | osd_show_property_text.                |
    +--------------------------------+----------------------------------------+

Other
~~~~~

* The playtree has been removed. **mpv**'s internal playlist is a simple and
  flat list now. This makes the code easier, and makes **mpv** usage less
  confusing.
* Slave mode is broken. This mode is entirely insane in the ``old`` versions of
  mplayer. A proper slave mode application needed tons of code and hacks to get
  it right. The main problem is that slave mode is a bad and incomplete
  interface, and to get around that, applications parsed output messages
  intended for users. It's hard to know which messages exactly are parsed by
  slave mode applications. This makes it virtually impossible to improve
  terminal output intended for users without possibly breaking something.

  This is absolutely insane, and **mpv** will not try to keep slave mode
  compatible. If you're a developer of a slave mode application, contact us,
  and a new and better protocol can be developed.

Policy for removed features
---------------------------

Features are a good thing, because they make users happy. As such, it is
attempted to preserve useful features as far as possible. But if a feature is
likely to be not used by many, and causes problems otherwise, it will be
removed. Developers should not be burdened with fixing or cleaning up code that
has no actual use.

It's always possible to add back removed features. File a feature request if a
feature you relied on was removed, and you want it back. Though it might be
rejected in the worst case, it's much more likely that it will be either added
back, or that a better solution will be implemented.

Why this fork?
--------------

* mplayer-svn wants to maintain old code, even if it's very bad code. It seems
  mplayer2 was forked, because mplayer-svn developers refused to get rid of
  all the cruft. The mplayer2 and mplayer-svn codebases also deviated enough to
  make a reunification unlikely.
* mplayer2 development is slow, and it's hard to get in changes. Details
  withheld as to not turn this into a rant.
* mplayer-svn rarely merged from mplayer2, and mplayer2 practically stopped
  merging from mplayer-svn (not even code cleanups or new features are merged)
* **mpv** intents to continuously merge from mplayer-svn and mplayer2, while
  speeding up development. There is willingness for significant changes, even
  if this means breaking compatibility.
