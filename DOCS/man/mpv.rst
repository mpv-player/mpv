mpv
###

##############
a media player
##############

:Copyright: GPLv2+
:Manual section: 1
:Manual group: multimedia

.. contents:: Table of Contents

SYNOPSIS
========

| **mpv** [options] [file|URL|PLAYLIST|-]
| **mpv** [options] files

DESCRIPTION
===========

**mpv** is a media player based on MPlayer and mplayer2. It supports a wide variety of video
file formats, audio and video codecs, and subtitle types. Special input URL
types are available to read input from a variety of sources other than disk
files. Depending on platform, a variety of different video and audio output
methods are supported.

Usage examples to get you started quickly can be found at the end of this man
page.


INTERACTIVE CONTROL
===================

mpv has a fully configurable, command-driven control layer which allows you
to control mpv using keyboard, mouse, or remote control (there is no
LIRC support - configure remotes as input devices instead).

See the ``--input-`` options for ways to customize it.

The following listings are not necessarily complete. See ``etc/input.conf`` for
a list of default bindings. User ``input.conf`` files and Lua scripts can
define additional key bindings.

Keyboard Control
----------------

LEFT and RIGHT
    Seek backward/forward 5 seconds. Shift+arrow does a 1 second exact seek
    (see ``--hr-seek``).

UP and DOWN
    Seek forward/backward 1 minute. Shift+arrow does a 5 second exact seek (see
    ``--hr-seek``).

Ctrl+LEFT and Ctrl+RIGHT
    Seek to the previous/next subtitle. Subject to some restrictions and
    might not always work; see ``sub-seek`` command.

Ctrl+Shift+Left and Ctrl+Shift+Right
    Adjust subtitle delay so that the next or previous subtitle is displayed
    now. This is especially useful to sync subtitles to audio.

[ and ]
    Decrease/increase current playback speed by 10%.

{ and }
    Halve/double current playback speed.

BACKSPACE
    Reset playback speed to normal.

Shift+BACKSPACE
    Undo the last seek. This works only if the playlist entry was not changed.
    Hitting it a second time will go back to the original position.
    See ``revert-seek`` command for details.

Shift+Ctrl+BACKSPACE
    Mark the current position. This will then be used by ``Shift+BACKSPACE``
    as revert position (once you seek back, the marker will be reset). You can
    use this to seek around in the file and then return to the exact position
    where you left off.

< and >
    Go backward/forward in the playlist.

ENTER
    Go forward in the playlist.

p / SPACE
    Pause (pressing again unpauses).

\.
    Step forward. Pressing once will pause, every consecutive press will
    play one frame and then go into pause mode again.

,
    Step backward. Pressing once will pause, every consecutive press will
    play one frame in reverse and then go into pause mode again.

q
    Stop playing and quit.

Q
    Like ``q``, but store the current playback position. Playing the same file
    later will resume at the old playback position if possible.

/ and *
    Decrease/increase volume.

9 and 0
    Decrease/increase volume.

m
    Mute sound.

\_
    Cycle through the available video tracks.

\#
    Cycle through the available audio tracks.

f
    Toggle fullscreen (see also ``--fs``).

ESC
    Exit fullscreen mode.

T
    Toggle stay-on-top (see also ``--ontop``).

w and W
    Decrease/increase pan-and-scan range. The ``e`` key does the same as
    ``W`` currently, but use is discouraged.

o (also P)
    Show progression bar, elapsed time and total duration on the OSD.

O
    Toggle OSD states between normal and playback time/duration.

v
    Toggle subtitle visibility.

j and J
    Cycle through the available subtitles.

z and Z
    Adjust subtitle delay by +/- 0.1 seconds. The ``x`` key does the same as
    ``Z`` currently, but use is discouraged.

l
    Set/clear A-B loop points. See ``ab-loop`` command for details.

L
    Toggle infinite looping.

Ctrl + and Ctrl -
    Adjust audio delay (A/V sync) by +/- 0.1 seconds.

u
    Switch between applying no style overrides to SSA/ASS subtitles, and
    overriding them almost completely with the normal subtitle style. See
    ``--sub-ass-override`` for more info.

V
    Toggle subtitle VSFilter aspect compatibility mode. See
    ``--sub-ass-vsfilter-aspect-compat`` for more info.

r and R
    Move subtitles up/down. The ``t`` key does the same as ``R`` currently, but
    use is discouraged.

s
    Take a screenshot.

S
    Take a screenshot, without subtitles. (Whether this works depends on VO
    driver support.)

Ctrl s
    Take a screenshot, as the window shows it (with subtitles, OSD, and scaled
    video).

PGUP and PGDWN
    Seek to the beginning of the previous/next chapter. In most cases,
    "previous" will actually go to the beginning of the current chapter; see
    ``--chapter-seek-threshold``.

Shift+PGUP and Shift+PGDWN
    Seek backward or forward by 10 minutes. (This used to be mapped to
    PGUP/PGDWN without Shift.)

d
    Activate/deactivate deinterlacer.

A
    Cycle aspect ratio override.

Ctrl h
    Toggle hardware video decoding on/off.

Alt+LEFT, Alt+RIGHT, Alt+UP, Alt+DOWN
    Move the video rectangle (panning).

Alt + and Alt -
    Combining ``Alt`` with the ``+`` or ``-`` keys changes video zoom.

Alt+BACKSPACE
    Reset the pan/zoom settings.

F8
    Show the playlist and the current position in it (useful only if a UI window
    is used, broken on the terminal).

F9
    Show the list of audio and subtitle streams (useful only if a UI window  is
    used, broken on the terminal).

(The following keys are valid only when using a video output that supports the
corresponding adjustment.)

1 and 2
    Adjust contrast.

3 and 4
    Adjust brightness.

5 and 6
    Adjust gamma.

7 and 8
    Adjust saturation.

Alt+0 (and command+0 on OSX)
    Resize video window to half its original size.

Alt+1 (and command+1 on OSX)
    Resize video window to its original size.

Alt+2 (and command+2 on OSX)
    Resize video window to double its original size.

command + f (OSX only)
    Toggle fullscreen (see also ``--fs``).

(The following keys are valid if you have a keyboard with multimedia keys.)

PAUSE
    Pause.

STOP
    Stop playing and quit.

PREVIOUS and NEXT
    Seek backward/forward 1 minute.


If you miss some older key bindings, look at ``etc/restore-old-bindings.conf``
in the mpv git repository.

Mouse Control
-------------

button 3 and button 4
    Seek backward/forward 1 minute.

button 5 and button 6
    Decrease/increase volume.


USAGE
=====

Command line arguments starting with ``-`` are interpreted as options,
everything else as filenames or URLs. All options except *flag* options (or
choice options which include ``yes``) require a parameter in the form
``--option=value``.

One exception is the lone ``-`` (without anything else), which means media data
will be read from stdin. Also, ``--`` (without anything else) will make the
player interpret all following arguments as filenames, even if they start with
``-``. (To play a file named ``-``, you need to use ``./-``.)

Every *flag* option has a *no-flag* counterpart, e.g. the opposite of the
``--fs`` option is ``--no-fs``. ``--fs=yes`` is same as ``--fs``, ``--fs=no``
is the same as ``--no-fs``.

If an option is marked as *(XXX only)*, it will only work in combination with
the *XXX* option or if *XXX* is compiled in.

Legacy option syntax
--------------------

The ``--option=value`` syntax is not strictly enforced, and the alternative
legacy syntax ``-option value`` and ``--option value`` will also work. This is
mostly  for compatibility with MPlayer. Using these should be avoided. Their
semantics can change any time in the future.

For example, the alternative syntax will consider an argument following the
option a filename. ``mpv -fs no`` will attempt to play a file named ``no``,
because ``--fs`` is a flag option that requires no parameter. If an option
changes and its parameter becomes optional, then a command line using the
alternative syntax will break.

Currently, the parser makes no difference whether an option starts with ``--``
or a single ``-``. This might also change in the future, and ``--option value``
might always interpret ``value`` as filename in order to reduce ambiguities.

Escaping spaces and other special characters
--------------------------------------------

Keep in mind that the shell will partially parse and mangle the arguments you
pass to mpv. For example, you might need to quote or escape options and
filenames:

    ``mpv "filename with spaces.mkv" --title="window title"``

It gets more complicated if the suboption parser is involved. The suboption
parser puts several options into a single string, and passes them to a
component at once, instead of using multiple options on the level of the
command line.

The suboption parser can quote strings with ``"`` and ``[...]``.
Additionally, there is a special form of quoting with ``%n%`` described below.

For example, assume the hypothetical ``foo`` filter can take multiple options:

    ``mpv test.mkv --vf=foo:option1=value1:option2:option3=value3,bar``

This passes ``option1`` and ``option3`` to the ``foo`` filter, with ``option2``
as flag (implicitly ``option2=yes``), and adds a ``bar`` filter after that. If
an option contains spaces or characters like ``,`` or ``:``, you need to quote
them:

    ``mpv '--vf=foo:option1="option value with spaces",bar'``

Shells may actually strip some quotes from the string passed to the commandline,
so the example quotes the string twice, ensuring that mpv receives the ``"``
quotes.

The ``[...]`` form of quotes wraps everything between ``[`` and ``]``. It's
useful with shells that don't interpret these characters in the middle of
an argument (like bash). These quotes are balanced (since mpv 0.9.0): the ``[``
and ``]`` nest, and the quote terminates on the last ``]`` that has no matching
``[`` within the string. (For example, ``[a[b]c]`` results in ``a[b]c``.)

The fixed-length quoting syntax is intended for use with external
scripts and programs.

It is started with ``%`` and has the following format::

    %n%string_of_length_n

.. admonition:: Examples

    ``mpv '--vf=foo:option1=%11%quoted text' test.avi``

    Or in a script:

    ``mpv --vf=foo:option1=%`expr length "$NAME"`%"$NAME" test.avi``

Suboptions passed to the client API are also subject to escaping. Using
``mpv_set_option_string()`` is exactly like passing ``--name=data`` to the
command line (but without shell processing of the string). Some options
support passing values in a more structured way instead of flat strings, and
can avoid the suboption parsing mess. For example, ``--vf`` supports
``MPV_FORMAT_NODE``, which lets you pass suboptions as a nested data structure
of maps and arrays.

Paths
-----

Some care must be taken when passing arbitrary paths and filenames to mpv. For
example, paths starting with ``-`` will be interpreted as options. Likewise,
if a path contains the sequence ``://``, the string before that might be
interpreted as protocol prefix, even though ``://`` can be part of a legal
UNIX path. To avoid problems with arbitrary paths, you should be sure that
absolute paths passed to mpv start with ``/``, and prefix relative paths with
``./``.

Using the ``file://`` pseudo-protocol is discouraged, because it involves
strange URL unescaping rules.

The name ``-`` itself is interpreted as stdin, and will cause mpv to disable
console controls. (Which makes it suitable for playing data piped to stdin.)

The special argument ``--`` can be used to stop mpv from interpreting the
following arguments as options.

When using the client API, you should strictly avoid using ``mpv_command_string``
for invoking the ``loadfile`` command, and instead prefer e.g. ``mpv_command``
to avoid the need for filename escaping.

For paths passed to suboptions, the situation is further complicated by the
need to escape special characters. To work this around, the path can be
additionally wrapped in the fixed-length syntax, e.g. ``%n%string_of_length_n``
(see above).

Some mpv options interpret paths starting with ``~``. Currently, the prefix
``~~/`` expands to the mpv configuration directory (usually ``~/.config/mpv/``).
``~/`` expands to the user's home directory. (The trailing ``/`` is always
required.) There are the following paths as well:

================ ===============================================================
Name             Meaning
================ ===============================================================
``~~home/``      same as ``~~/``
``~~global/``    the global config path, if available (not on win32)
``~~osxbundle/`` the OSX bundle resource path (OSX only)
``~~desktop/``   the path to the desktop (win32, OSX)
================ ===============================================================


Per-File Options
----------------

When playing multiple files, any option given on the command line usually
affects all files. Example::

    mpv --a file1.mkv --b file2.mkv --c

=============== ===========================
File            Active options
=============== ===========================
file1.mkv       ``--a --b --c``
file2.mkv       ``--a --b --c``
=============== ===========================

(This is different from MPlayer and mplayer2.)

Also, if any option is changed at runtime (via input commands), they are not
reset when a new file is played.

Sometimes, it is useful to change options per-file. This can be achieved by
adding the special per-file markers ``--{`` and ``--}``. (Note that you must
escape these on some shells.) Example::

    mpv --a file1.mkv --b --\{ --c file2.mkv --d file3.mkv --e --\} file4.mkv --f

=============== ===========================
File            Active options
=============== ===========================
file1.mkv       ``--a --b --f``
file2.mkv       ``--a --b --f --c --d --e``
file3.mkv       ``--a --b --f --c --d --e``
file4.mkv       ``--a --b --f``
=============== ===========================

Additionally, any file-local option changed at runtime is reset when the current
file stops playing. If option ``--c`` is changed during playback of
``file2.mkv``, it is reset when advancing to ``file3.mkv``. This only affects
file-local options. The option ``--a`` is never reset here.


List Options
------------

Some options which store lists of option values can have action suffixes. For
example, you can set a ``,``-separated list of filters with ``--vf``, but the
option also allows you to append filters with ``--vf-append``.

Options for filenames do not use ``,`` as separator, but ``:`` (Unix) or ``;``
(Windows).

============= ===============================================
Suffix        Meaning
============= ===============================================
-add          Append 1 or more items (may become alias for -append)
-append       Append single item (avoids need for escaping)
-clr          Clear the option
-del          Delete an existing item by integer index
-pre          Prepend 1 or more items
-set          Set a list of items
-toggle       Append an item, or remove if if it already exists
============= ===============================================

Although some operations allow specifying multiple ``,``-separated items, using
this is strongly discouraged and deprecated, except for ``-set``.

Without suffix, the action taken is normally ``-set``.

Some options (like ``--sub-file``, ``--audio-file``, ``--glsl-shader``) are
aliases for the proper option with ``-append`` action. For example,
``--sub-file`` is an alias for ``--sub-files-append``.

Some options only support a subset of the above.

Options of this type can be changed at runtime using the ``change-list``
command, which takes the suffix as separate operation parameter.

Playing DVDs
------------

DVDs can be played with the ``dvd://[title]`` syntax. The optional
title specifier is a number which selects between separate video
streams on the DVD. If no title is given (``dvd://``) then the longest
title is selected automatically by the library. This is usually what
you want. mpv does not support DVD menus.

DVDs which have been copied on to a hard drive or other mounted
filesystem (by e.g. the ``dvdbackup`` tool) are accommodated by
specifying the path to the local copy: ``--dvd-device=PATH``.
Alternatively, running ``mpv PATH`` should auto-detect a DVD directory
tree and play the longest title.

.. note:: DVD library choices

    mpv uses a different default DVD library than MPlayer. MPlayer
    uses libdvdread by default, and mpv uses libdvdnav by default.
    Both libraries are developed in parallel, but libdvdnav is
    intended to support more sophisticated DVD features such as menus
    and multi-angle playback. mpv uses libdvdnav for files specified
    as either ``dvd://...`` or ``dvdnav://...``. To use libdvdread,
    which will produce behavior more like MPlayer, specify
    ``dvdread://...`` instead. Some users have experienced problems
    when using libdvdnav, in which playback gets stuck in a DVD menu
    stream. These problems are reported to go away when auto-selecting
    the title (``dvd://`` rather than ``dvd://1``) or when using
    libdvdread (e.g. ``dvdread://0``). There are also outstanding bugs
    in libdvdnav with seeking backwards and forwards in a video
    stream. Specify ``dvdread://...`` to fix such problems.

.. note:: DVD subtitles
    
    DVDs use image-based subtitles. Image subtitles are implemented as
    a bitmap video stream which can be superimposed over the main
    movie. mpv's subtitle styling and positioning options and keyboard
    shortcuts generally do not work with image-based subtitles.
    Exceptions include options like ``--stretch-dvd-subs`` and
    ``--stretch-image-subs-to-screen``.


CONFIGURATION FILES
===================

Location and Syntax
-------------------

You can put all of the options in configuration files which will be read every
time mpv is run. The system-wide configuration file 'mpv.conf' is in your
configuration directory (e.g. ``/etc/mpv`` or ``/usr/local/etc/mpv``), the
user-specific one is ``~/.config/mpv/mpv.conf``. For details and platform
specifics (in particular Windows paths) see the `FILES`_ section.

User-specific options override system-wide options and options given on the
command line override either. The syntax of the configuration files is
``option=value``. Everything after a *#* is considered a comment. Options
that work without values can be enabled by setting them to *yes* and disabled by
setting them to *no*. Even suboptions can be specified in this way.

.. admonition:: Example configuration file

    ::

        # Use GPU-accelerated video output by default.
        vo=gpu
        # Use quotes for text that can contain spaces:
        status-msg="Time: ${time-pos}"

Escaping spaces and special characters
--------------------------------------

This is done like with command line options. The shell is not involved here,
but option values still need to be quoted as a whole if it contains certain
characters like spaces. A config entry can be quoted with ``"``,
as well as with the fixed-length syntax (``%n%``) mentioned before. This is like
passing the exact contents of the quoted string as command line option. C-style
escapes are currently _not_ interpreted on this level, although some options do
this manually. (This is a mess and should probably be changed at some point.)

Putting Command Line Options into the Configuration File
--------------------------------------------------------

Almost all command line options can be put into the configuration file. Here
is a small guide:

======================= ========================
Option                  Configuration file entry
======================= ========================
``--flag``              ``flag``
``-opt val``            ``opt=val``
``--opt=val``           ``opt=val``
``-opt "has spaces"``   ``opt="has spaces"``
======================= ========================

File-specific Configuration Files
---------------------------------

You can also write file-specific configuration files. If you wish to have a
configuration file for a file called 'video.avi', create a file named
'video.avi.conf' with the file-specific options in it and put it in
``~/.config/mpv/``. You can also put the configuration file in the same directory
as the file to be played. Both require you to set the ``--use-filedir-conf``
option (either on the command line or in your global config file). If a
file-specific configuration file is found in the same directory, no
file-specific configuration is loaded from ``~/.config/mpv``. In addition, the
``--use-filedir-conf`` option enables directory-specific configuration files.
For this, mpv first tries to load a mpv.conf from the same directory
as the file played and then tries to load any file-specific configuration.


Profiles
--------

To ease working with different configurations, profiles can be defined in the
configuration files. A profile starts with its name in square brackets,
e.g. ``[my-profile]``. All following options will be part of the profile. A
description (shown by ``--profile=help``) can be defined with the
``profile-desc`` option. To end the profile, start another one or use the
profile name ``default`` to continue with normal options.

You can list profiles with ``--profile=help``, and show the contents of a
profile with ``--show-profile=<name>`` (replace ``<name>`` with the profile
name). You can apply profiles on start with the ``--profile=<name>`` option,
or at runtime with the ``apply-profile <name>`` command.

.. admonition:: Example mpv config file with profiles

    ::

        # normal top-level option
        fullscreen=yes

        # a profile that can be enabled with --profile=big-cache
        [big-cache]
        cache=123400
        demuxer-readahead-secs=20

        [slow]
        profile-desc="some profile name"
        # reference a builtin profile
        profile=gpu-hq

        [fast]
        vo=vdpau

        # using a profile again extends it
        [slow]
        framedrop=no
        # you can also include other profiles
        profile=big-cache


Auto profiles
-------------

Some profiles are loaded automatically. The following example demonstrates this:

.. admonition:: Auto profile loading

    ::

        [protocol.dvd]
        profile-desc="profile for dvd:// streams"
        alang=en

        [extension.flv]
        profile-desc="profile for .flv files"
        vf=flip

The profile name follows the schema ``type.name``, where type can be
``protocol`` for the input/output protocol in use (see ``--list-protocols``),
and ``extension`` for the extension of the path of the currently played file
(*not* the file format).

This feature is very limited, and there are no other auto profiles.

TAKING SCREENSHOTS
==================

Screenshots of the currently played file can be taken using the 'screenshot'
input mode command, which is by default bound to the ``s`` key. Files named
``mpv-shotNNNN.jpg`` will be saved in the working directory, using the first
available number - no files will be overwritten. In pseudo-GUI mode, the
screenshot will be saved somewhere else. See `PSEUDO GUI MODE`_.

A screenshot will usually contain the unscaled video contents at the end of the
video filter chain and subtitles. By default, ``S`` takes screenshots without
subtitles, while ``s`` includes subtitles.

Unlike with MPlayer, the ``screenshot`` video filter is not required. This
filter was never required in mpv, and has been removed.

TERMINAL STATUS LINE
====================

During playback, mpv shows the playback status on the terminal. It looks like
something like this:

    ``AV: 00:03:12 / 00:24:25 (13%) A-V: -0.000``

The status line can be overridden with the ``--term-status-msg`` option.

The following is a list of things that can show up in the status line. Input
properties, that can be used to get the same information manually, are also
listed.

- ``AV:`` or ``V:`` (video only) or ``A:`` (audio only)
- The current time position in ``HH:MM:SS`` format (``playback-time`` property)
- The total file duration (absent if unknown) (``length`` property)
- Playback speed, e.g. `` x2.0``. Only visible if the speed is not normal. This
  is the user-requested speed, and not the actual speed  (usually they should
  be the same, unless playback is too slow). (``speed`` property.)
- Playback percentage, e.g. ``(13%)``. How much of the file has been played.
  Normally calculated out of playback position and duration, but can fallback
  to other methods (like byte position) if these are not available.
  (``percent-pos`` property.)
- The audio/video sync as ``A-V:  0.000``. This is the difference between
  audio and video time. Normally it should be 0 or close to 0. If it's growing,
  it might indicate a playback problem. (``avsync`` property.)
- Total A/V sync change, e.g. ``ct: -0.417``. Normally invisible. Can show up
  if there is audio "missing", or not enough frames can be dropped. Usually
  this will indicate a problem. (``total-avsync-change`` property.)
- Encoding state in ``{...}``, only shown in encoding mode.
- Display sync state. If display sync is active (``display-sync-active``
  property), this shows ``DS: 2.500/13``, where the first number is average
  number of vsyncs per video frame (e.g. 2.5 when playing 24Hz videos on 60Hz
  screens), which might jitter if the ratio doesn't round off, or there are
  mistimed frames (``vsync-ratio``), and the second number of estimated number
  of vsyncs which took too long (``vo-delayed-frame-count`` property). The
  latter is a heuristic, as it's generally not possible to determine this with
  certainty.
- Dropped frames, e.g. ``Dropped: 4``. Shows up only if the count is not 0. Can
  grow if the video framerate is higher than that of the display, or if video
  rendering is too slow. May also be incremented on "hiccups" and when the video
  frame couldn't be displayed on time. (``vo-drop-frame-count`` property.)
  If the decoder drops frames, the number of decoder-dropped frames is appended
  to the display as well, e.g.: ``Dropped: 4/34``. This happens only if
  decoder frame dropping is enabled with the ``--framedrop`` options.
  (``drop-frame-count`` property.)
- Cache state, e.g. ``Cache:  2s+134KB``. Visible if the stream cache is enabled.
  The first value shows the amount of video buffered in the demuxer in seconds,
  the second value shows the sum of the demuxer forward cache size and the
  *additional* data buffered in the stream cache in kilobytes.
  (``demuxer-cache-duration``, ``demuxer-cache-state``, ``cache-used``
  properties.)


LOW LATENCY PLAYBACK
====================

mpv is optimized for normal video playback, meaning it actually tries to buffer
as much data as it seems to make sense. This will increase latency. Reducing
latency is possible only by specifically disabling features which increase
latency.

The builtin ``low-latency`` profile tries to apply some of the options which can
reduce latency. You can use  ``--profile=low-latency`` to apply all of them. You
can list the contents with ``--show-profile=low-latency`` (some of the options
are quite obscure, and may change every mpv release).

Be aware that some of the options can reduce playback quality.

Most latency is actually caused by inconvenient timing behavior. You can disable
this with ``--untimed``, but it will likely break, unless the stream has no
audio, and the input feeds data to the player at a constant rate.

Another common problem is with MJPEG streams. These do not signal the correct
framerate. Using ``--untimed`` or ``--no-correct-pts --fps=60`` might help.

For livestreams, data can build up due to pausing the stream, due to slightly
lower playback rate, or "buffering" pauses. If the demuxer cache is enabled,
these can be skipped manually. The experimental ``drop-buffers`` command can
be used to discard any buffered data, though it's very disruptive.

In some cases, manually tuning TCP buffer sizes and such can help to reduce
latency.

Additional options that can be tried:

- ``--opengl-glfinish=yes``, can reduce buffering in the graphics driver
- ``--opengl-swapinterval=0``, same
- ``--vo=xv``, same
- without audio ``--framedrop=no --speed=1.01`` may help for live sources
  (results can be mixed)


PROTOCOLS
=========

``http://...``, ``https://``, ...

    Many network protocols are supported, but the protocol prefix must always
    be specified. mpv will never attempt to guess whether a filename is
    actually a network address. A protocol prefix is always required.

    Note that not all prefixes are documented here. Undocumented prefixes are
    either aliases to documented protocols, or are just redirections to
    protocols implemented and documented in FFmpeg.

    ``data:`` is supported in FFmpeg (not in Libav), but needs to be in the
    format ``data://``. This is done to avoid ambiguity with filenames. You
    can also prefix it with ``lavf://`` or ``ffmpeg://``.

``ytdl://...``

    By default, the youtube-dl hook script (enabled by default for mpv CLI)
    only looks at http URLs. Prefixing an URL with ``ytdl://`` forces it to
    be always processed by the script. This can also be used to invoke special
    youtube-dl functionality like playing a video by ID or invoking search.

    Keep in mind that you can't pass youtube-dl command line options by this,
    and you have to use ``--ytdl-raw-options`` instead.

``-``

    Play data from stdin.

``smb://PATH``

    Play a path from  Samba share.

``bd://[title][/device]`` ``--bluray-device=PATH``

    Play a Blu-ray disc. Since libbluray 1.0.1, you can read from ISO files
    by passing them to ``--bluray-device``.

    ``title`` can be: ``longest`` or ``first`` (selects the default
    playlist); ``mpls/<number>`` (selects <number>.mpls playlist);
    ``<number>`` (select playlist with the same index). mpv will list
    the available playlists on loading.

    ``bluray://`` is an alias.

``dvd://[title|[starttitle]-endtitle][/device]`` ``--dvd-device=PATH``

    Play a DVD. DVD menus are not supported. If no title is given, the longest
    title is auto-selected.

    ``dvdnav://`` is an old alias for ``dvd://`` and does exactly the same
    thing.

``dvdread://...:``

    Play a DVD using the old libdvdread code. This is what MPlayer and
    older mpv versions used for ``dvd://``. Use is discouraged. It's
    provided only for compatibility and for transition, and to work
    around outstanding dvdnav bugs (see "DVD library choices" above).

``tv://[channel][/input_id]`` ``--tv-...``

    Analogue TV via V4L. Also useful for webcams. (Linux only.)

``pvr://`` ``--pvr-...``

    PVR. (Linux only.)

``dvb://[cardnumber@]channel`` ``--dvbin-...``

    Digital TV via DVB. (Linux only.)

``mf://[filemask|@listfile]`` ``--mf-...``

    Play a series of images as video.

``cdda://[device]`` ``--cdrom-device=PATH`` ``--cdda-...``

    Play CD.

``lavf://...``

    Access any FFmpeg/Libav libavformat protocol. Basically, this passed the
    string after the ``//`` directly to libavformat.

``av://type:options``

    This is intended for using libavdevice inputs. ``type`` is the libavdevice
    demuxer name, and ``options`` is the (pseudo-)filename passed to the
    demuxer.

    For example, ``mpv av://lavfi:mandelbrot`` makes use of the libavfilter
    wrapper included in libavdevice, and will use the ``mandelbrot`` source
    filter to generate input data.

    ``avdevice://`` is an alias.

``file://PATH``

    A local path as URL. Might be useful in some special use-cases. Note that
    ``PATH`` itself should start with a third ``/`` to make the path an
    absolute path.

``appending://PATH``

    Play a local file, but assume it's being appended to. This is useful for
    example for files that are currently being downloaded to disk. This will
    block playback, and stop playback only if no new data was appended after
    a timeout of about 2 seconds.

    Using this is still a bit of a bad idea, because there is no way to detect
    if a file is actually being appended, or if it's still written. If you're
    trying to play the  output of some program, consider using a pipe
    (``something | mpv -``). If it really has to be a file on disk, use tail to
    make it wait forever, e.g. ``tail -f -c +0 file.mkv | mpv -``.

``fd://123``

    Read data from the given file descriptor (for example 123). This is similar
    to piping data to stdin via ``-``, but can use an arbitrary file descriptor.
    mpv may modify some file descriptor properties when the stream layer "opens"
    it.

``fdclose://123``

    Like ``fd://``, but the file descriptor is closed after use. When using this
    you need to ensure that the same fd URL will only be used once.

``edl://[edl specification as in edl-mpv.rst]``

    Stitch together parts of multiple files and play them.

``null://``

    Simulate an empty file. If opened for writing, it will discard all data.
    The ``null`` demuxer will specifically pass autoprobing if this protocol
    is used (while it's not automatically invoked for empty files).

``memory://data``

    Use the ``data`` part as source data.

``hex://data``

    Like ``memory://``, but the string is interpreted as hexdump.

PSEUDO GUI MODE
===============

mpv has no official GUI, other than the OSC (`ON SCREEN CONTROLLER`_), which
is not a full GUI and is not meant to be. However, to compensate for the lack
of expected GUI behavior, mpv will in some cases start with some settings
changed to behave slightly more like a GUI mode.

Currently this happens only in the following cases:

- if started using the ``mpv.desktop`` file on Linux (e.g. started from menus
  or file associations provided by desktop environments)
- if started from explorer.exe on Windows (technically, if it was started on
  Windows, and all of the stdout/stderr/stdin handles are unset)
- started out of the bundle on OSX
- if you manually use ``--player-operation-mode=pseudo-gui`` on the command line

This mode applies options from the builtin profile ``builtin-pseudo-gui``, but
only if these haven't been set in the user's config file or on the command line.
Also, for compatibility with the old pseudo-gui behavior, the options in the
``pseudo-gui`` profile are applied unconditionally. In addition, the profile
makes sure to enable the pseudo-GUI mode, so that ``--profile=pseudo-gui``
works like in older mpv releases. The profiles are currently defined as follows:

::

    [builtin-pseudo-gui]
    terminal=no
    force-window=yes
    idle=once
    screenshot-directory=~~desktop/
    [pseudo-gui]
    player-operation-mode=pseudo-gui

.. warning::

    Currently, you can extend the ``pseudo-gui`` profile in the config file the
    normal way. This is deprecated. In future mpv releases, the behavior might
    change, and not apply your additional settings, and/or use a different
    profile name.


.. include:: options.rst

.. include:: ao.rst

.. include:: vo.rst

.. include:: af.rst

.. include:: vf.rst

.. include:: encode.rst

.. include:: input.rst

.. include:: osc.rst

.. include:: stats.rst

.. include:: lua.rst

.. include:: javascript.rst

.. include:: ipc.rst

.. include:: changes.rst

.. include:: libmpv.rst

ENVIRONMENT VARIABLES
=====================

There are a number of environment variables that can be used to control the
behavior of mpv.

``HOME``, ``XDG_CONFIG_HOME``
    Used to determine mpv config directory. If ``XDG_CONFIG_HOME`` is not set,
    ``$HOME/.config/mpv`` is used.

    ``$HOME/.mpv`` is always added to the list of config search paths with a
    lower priority.

``XDG_CONFIG_DIRS``
    If set, XDG-style system configuration directories are used. Otherwise,
    the UNIX convention (``PREFIX/etc/mpv/``) is used.

``MPV_HOME``
    Directory where mpv looks for user settings. Overrides ``HOME``, and mpv
    will try to load the config file as ``$MPV_HOME/mpv.conf``.

``MPV_VERBOSE`` (see also ``-v`` and ``--msg-level``)
    Set the initial verbosity level across all message modules (default: 0).
    This is an integer, and the resulting verbosity corresponds to the number
    of ``--v`` options passed to the command line.

``MPV_LEAK_REPORT``
    If set to ``1``, enable internal talloc leak reporting.

``LADSPA_PATH``
    Specifies the search path for LADSPA plugins. If it is unset, fully
    qualified path names must be used.

``DISPLAY``
    Standard X11 display name to use.

FFmpeg/Libav:
    This library accesses various environment variables. However, they are not
    centrally documented, and documenting them is not our job. Therefore, this
    list is incomplete.

    Notable environment variables:

    ``http_proxy``
        URL to proxy for ``http://`` and ``https://`` URLs.

    ``no_proxy``
        List of domain patterns for which no proxy should be used.
        List entries are separated by ``,``. Patterns can include ``*``.

libdvdcss:
    ``DVDCSS_CACHE``
        Specify a directory in which to store title key values. This will
        speed up descrambling of DVDs which are in the cache. The
        ``DVDCSS_CACHE`` directory is created if it does not exist, and a
        subdirectory is created named after the DVD's title or manufacturing
        date. If ``DVDCSS_CACHE`` is not set or is empty, libdvdcss will use
        the default value which is ``${HOME}/.dvdcss/`` under Unix and
        the roaming application data directory (``%APPDATA%``) under
        Windows. The special value "off" disables caching.

    ``DVDCSS_METHOD``
        Sets the authentication and decryption method that libdvdcss will use
        to read scrambled discs. Can be one of ``title``, ``key`` or ``disc``.

        key
           is the default method. libdvdcss will use a set of calculated
           player keys to try to get the disc key. This can fail if the drive
           does not recognize any of the player keys.

        disc
           is a fallback method when key has failed. Instead of using player
           keys, libdvdcss will crack the disc key using a brute force
           algorithm. This process is CPU intensive and requires 64 MB of
           memory to store temporary data.

        title
           is the fallback when all other methods have failed. It does not
           rely on a key exchange with the DVD drive, but rather uses a crypto
           attack to guess the title key. On rare cases this may fail because
           there is not enough encrypted data on the disc to perform a
           statistical attack, but on the other hand it is the only way to
           decrypt a DVD stored on a hard disc, or a DVD with the wrong region
           on an RPC2 drive.

    ``DVDCSS_RAW_DEVICE``
        Specify the raw device to use. Exact usage will depend on your
        operating system, the Linux utility to set up raw devices is raw(8)
        for instance. Please note that on most operating systems, using a raw
        device requires highly aligned buffers: Linux requires a 2048 bytes
        alignment (which is the size of a DVD sector).

    ``DVDCSS_VERBOSE``
        Sets the libdvdcss verbosity level.

        :0: Outputs no messages at all.
        :1: Outputs error messages to stderr.
        :2: Outputs error messages and debug messages to stderr.

    ``DVDREAD_NOKEYS``
        Skip retrieving all keys on startup. Currently disabled.

    ``HOME``
        FIXME: Document this.


EXIT CODES
==========

Normally **mpv** returns 0 as exit code after finishing playback successfully.
If errors happen, the following exit codes can be returned:

    :1: Error initializing mpv. This is also returned if unknown options are
        passed to mpv.
    :2: The file passed to mpv couldn't be played. This is somewhat fuzzy:
        currently, playback of a file is considered to be successful if
        initialization was mostly successful, even if playback fails
        immediately after initialization.
    :3: There were some files that could be played, and some files which
        couldn't (using the definition of success from above).
    :4: Quit due to a signal, Ctrl+c in a VO window (by default), or from the
        default quit key bindings in encoding mode.

Note that quitting the player manually will always lead to exit code 0,
overriding the exit code that would be returned normally. Also, the ``quit``
input command can take an exit code: in this case, that exit code is returned.

FILES
=====

For Windows-specifics, see `FILES ON WINDOWS`_ section.

``/usr/local/etc/mpv/mpv.conf``
    mpv system-wide settings (depends on ``--prefix`` passed to configure - mpv
    in default configuration will use ``/usr/local/etc/mpv/`` as config
    directory, while most Linux distributions will set it to ``/etc/mpv/``).

``~/.config/mpv/mpv.conf``
    mpv user settings (see `CONFIGURATION FILES`_ section)

``~/.config/mpv/input.conf``
    key bindings (see `INPUT.CONF`_ section)

``~/.config/mpv/fonts.conf``
    Fontconfig fonts.conf that is customized for mpv. You should include system
    fonts.conf in this file or mpv would not know about fonts that you already
    have in the system.

    Only available when libass is built with fontconfig.

``~/.config/mpv/subfont.ttf``
    fallback subtitle font

``~/.config/mpv/fonts/``
    Font files in this directory are used by mpv/libass for subtitles. Useful
    if you do not want to install fonts to your system. Note that files in this
    directory are loaded into memory before being used by mpv. If you have a
    lot of fonts, consider using fonts.conf (see above) to include additional
    fonts, which is more memory-efficient.

``~/.config/mpv/scripts/``
    All files in this directory are loaded as if they were passed to the
    ``--script`` option. They are loaded in alphabetical order, and sub-directories
    and files with no ``.lua`` extension are ignored. The ``--load-scripts=no``
    option disables loading these files.

``~/.config/mpv/watch_later/``
    Contains temporary config files needed for resuming playback of files with
    the watch later feature. See for example the ``Q`` key binding, or the
    ``quit-watch-later`` input command.

    Each file is a small config file which is loaded if the corresponding media
    file is loaded. It contains the playback position and some (not necessarily
    all) settings that were changed during playback. The filenames are hashed
    from the full paths of the media files. It's in general not possible to
    extract the media filename from this hash. However, you can set the
    ``--write-filename-in-watch-later-config`` option, and the player will
    add the media filename to the contents of the resume config file.

``~/.config/mpv/script-opts/osc.conf``
    This is loaded by the OSC script. See the `ON SCREEN CONTROLLER`_ docs
    for details.

    Other files in this directory are specific to the corresponding scripts
    as well, and the mpv core doesn't touch them.

Note that the environment variables ``$XDG_CONFIG_HOME`` and ``$MPV_HOME`` can
override the standard directory ``~/.config/mpv/``.

Also, the old config location at ``~/.mpv/`` is still read, and if the XDG
variant does not exist, will still be preferred.

FILES ON WINDOWS
================

On win32 (if compiled with MinGW, but not Cygwin), the default config file
locations are different. They are generally located under ``%APPDATA%/mpv/``.
For example, the path to mpv.conf is ``%APPDATA%/mpv/mpv.conf``, which maps to
a system and user-specific path, for example

    ``C:\users\USERNAME\AppData\Roaming\mpv\mpv.conf``

You can find the exact path by running ``echo %APPDATA%\mpv\mpv.conf`` in cmd.exe.

Other config files (such as ``input.conf``) are in the same directory. See the
`FILES`_ section above.

The environment variable ``$MPV_HOME`` completely overrides these, like on
UNIX.

If a directory named ``portable_config`` next to the mpv.exe exists, all
config will be loaded from this directory only. Watch later config files are
written to this directory as well. (This exists on Windows only and is redundant
with ``$MPV_HOME``. However, since Windows is very scripting unfriendly, a
wrapper script just setting ``$MPV_HOME``, like you could do it on other
systems, won't work. ``portable_config`` is provided for convenience to get
around this restriction.)

Config files located in the same directory as ``mpv.exe`` are loaded with
lower priority. Some config files are loaded only once, which means that
e.g. of 2 ``input.conf`` files located in two config directories, only the
one from the directory with higher priority will be loaded.

A third config directory with the lowest priority is the directory named ``mpv``
in the same directory as ``mpv.exe``. This used to be the directory with the
highest priority, but is now discouraged to use and might be removed in the
future.

Note that mpv likes to mix ``/`` and ``\`` path separators for simplicity.
kernel32.dll accepts this, but cmd.exe does not.
