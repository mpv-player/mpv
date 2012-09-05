.. _changes:

CHANGES FROM OTHER VERSIONS OF MPLAYER
======================================

xxx: since I don't have a new name yet, I'm referring to this version of mplayer
as **mplayer3**, I don't intend to use that name.

**mplayer3** is based on mplayer2, which in turn is based on the original
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

General changes for mplayer2 to mplayer3
----------------------------------------

* Removal of lots of unneeded code to encourage developer activity (less
  obscure scary zombie code that kills any desire for hacking the codebase)
* Removal of dust and dead bodies (code-wise), such as kernel drivers for
  decades old hardware
* Removal of support for dead platforms
* Generally improved MS Windows support (dealing with unicode filenames,
  improved ``vo_direct3d``, improve window handling)
* Better OSD rendering (using libass). This has full unicode support, and
  languages like Arabic should be better supported.
* Cleaned up terminal output (nicer status line, less useless noise)
* Support for playing URLs of popular streaming sites directly
  (e.g. ``mplayer3 https://www.youtube.com/watch?v=...``)
* Improved OpenGL output (``vo_gl3``)
* Make ``--softvol`` default (**mplayer3** is not a mixer control panel)
* Improved support for .cue files
* Screenshot improvements (can save screenshots as JPG, configurable filenames)
* Removal of teletext support
* Replace image VOs (``vo_jpeg`` etc.) with ``vo_image``
* Remove ``vo_gif89a``, ``vo_md5sum``, ``vo_yuv4mpeg`` (the plan is to merge
  divverent's encoding branch, which provides support for all of these)
* Do not lose settings when playing a new file in the same player instance
* New location for config files, new name for the binary. (Planned change.)
* Slave mode compatibility broken (see below)
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
* In general, negating a switch like ``-noopt`` now has to be written as
  ``-no-opt``, or better ``--no-opt``.
* Per-file options are not the default anymore. You can explicitly specify
  file local options. See ``Usage`` section.
* Table of renamed switches:

    =================================== ===================================
    Old                                 New
    =================================== ===================================
    -nosound                            --no-audio
    -use-filename-title                 --title="${filename}"
    -loop 0                             --loop=inf
    =================================== ===================================

input.conf and slave commands
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Table of renamed slave commands:

    =================================== ===================================
    Old                                 New
    =================================== ===================================
    pt_step 1 b                         playlist_next b
    pt_step -1 b                        playlist_prev b
    pt_clear                            playlist_clear
    =================================== ===================================

Other
~~~~~

* The playtree has been removed. **mplayer3**'s internal playlist is a simple and
  flat list now. This makes the code easier, and makes using it less confusing.
* Slave mode is broken. This mode is entirely insane in the ``old`` versions of
  mplayer. A proper slave mode application needed tons of code and hacks to get
  it right. The main problem is that slave mode is a bad and incomplete
  interface, and to get around that, applications parsed output messages
  intended for users. It's hard to know just which messages are parsed by some
  slave mode application, and as such it's virtually impossible to improve
  terminal output intended for users without possibly breaking something.

  This is absolutely insane, and **mplayer3** will not try to keep slave mode
  compatible. If you're a developer of a slave mode application, contact us,
  and a new and better protocol can be developed.

Policy for removed features
---------------------------

Features are a good thing, because they make users happy. As such, it is
attempted to preserve useful features as far as possible. But if a feature is
likely to be not used by many, and causes otherwise problems, it will be
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
* **mplayer3** intents to continuously merge from mplayer-svn and mplayer2, while
  speeding up development. There is willingness for significant changes, even
  if this means breaking compatibility.
