mpv
###

##############
a media player
##############

:Copyright: GPLv2+
:Manual section: 1
:Manual group: multimedia

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
    might not work always; see ``sub_seek`` command.

[ and ]
    Decrease/increase current playback speed by 10%.

{ and }
    Halve/double current playback speed.

BACKSPACE
    Reset playback speed to normal.

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

w and e
    Decrease/increase pan-and-scan range.

o (also P)
    Show progression bar, elapsed time and total duration on the OSD.

O
    Toggle OSD states: none / seek / seek + timer / seek + timer + total time.

d
    Toggle frame dropping states: none / skip display / skip decoding (see
    ``--framedrop``).

v
    Toggle subtitle visibility.

j and J
    Cycle through the available subtitles.

x and z
    Adjust subtitle delay by +/- 0.1 seconds.

l
    Set/clear A-B loop points. See ``ab_loop`` command for details.

Ctrl + and Ctrl -
    Adjust audio delay by +/- 0.1 seconds.

u
    Switch between applying no style overrides to SSA/ASS subtitles, and
    overriding them almost completely with the normal subtitle style. See
    ``--ass-style-override`` for more info.

V
    Toggle subtitle VSFilter aspect compatibility mode. See
    ``--ass-vsfilter-aspect-compat`` for more info.

r and t
    Move subtitles up/down.

s
    Take a screenshot.

S
    Take a screenshot, without subtitles. (Whether this works depends on VO
    driver support.)

I
    Show filename on the OSD.

PGUP and PGDWN
    Seek to the beginning of the previous/next chapter. In most cases,
    "previous" will actually go to the beginning of the current chapter; see
    ``--chapter-seek-threshold``.

Shift+PGUP and Shift+PGDWN
    Seek backward or forward by 10 minutes. (This used to be mapped to
    PGUP/PGDWN without Shift.)

D
    Activate/deactivate deinterlacer.

A
    Cycle aspect ratio override.

(The following keys are valid only when using a video output that supports the
corresponding adjustment, or the software equalizer (``--vf=eq``).)

1 and 2
    Adjust contrast.

3 and 4
    Adjust brightness.

5 and 6
    Adjust gamma.

7 and 8
    Adjust saturation.

(The following keys are valid only on OS X.)

command + 0
    Resize video window to half its original size.
    (On other platforms, you can bind keys to change the ``window-scale``
    property.)

command + 1
    Resize video window to its original size.

command + 2
    Resize video window to double its original size.

command + f
    Toggle fullscreen (see also ``--fs``).

command + [ and command + ]
    Set video window alpha.

(The following keys are valid if you have a keyboard with multimedia keys.)

PAUSE
    Pause.

STOP
    Stop playing and quit.

PREVIOUS and NEXT
    Seek backward/forward 1 minute.

(The following keys are only valid if you compiled with TV or DVB input
support.)

h and k
    Select previous/next channel.

Mouse Control
-------------

button 3 and button 4
    Seek backward/forward 1 minute.

button 5 and button 6
    Decrease/increase volume.


USAGE
=====

Every *flag* option has a *no-flag* counterpart, e.g. the opposite of the
``--fs`` option is ``--no-fs``. ``--fs=yes`` is same as ``--fs``, ``--fs=no``
is the same as ``--no-fs``.

If an option is marked as *(XXX only)*, it will only work in combination with
the *XXX* option or if *XXX* is compiled in.

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

The suboption parser can quote strings with ``"``, ``'``, and ``[...]``.
Additionally, there is a special form of quoting with ``%n%`` described below.

For example, the ``opengl`` VO can take multiple options:

    ``mpv test.mkv --vo=opengl:scale=lanczos:icc-profile=file.icc,xv``

This passes ``scale=lanczos`` and ``icc-profile=file.icc`` to ``opengl``,
and also specifies ``xv`` as fallback VO. If the icc-profile path contains
spaces or characters like ``,`` or ``:``, you need to quote them:

    ``mpv '--vo=opengl:icc-profile="file with spaces.icc",xv'``

Shells may actually strip some quotes from the string passed to the commandline,
so the example quotes the string twice, ensuring that mpv recieves the ``"``
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

    ``mpv --ao=pcm:file=%10%C:test.wav test.avi``

    Or in a script:

    ``mpv --ao=pcm:file=%`expr length "$NAME"`%"$NAME" test.avi``

Suboptions passed to the client API are also subject to escaping. Using
``mpv_set_option_string()`` is exactly like passing ``--name=data`` to the
command line (but without shell processing of the string). Some options
support passing values in a more structured way instead of flat strings, and
can avoid the suboption parsing mess. For example, ``--vf`` supports
``MPV_FORMAT_NODE``, which let's you pass suboptions as a nested data structure
of maps and arrays. (``--vo`` supports this in the same way, although this
fact is undocumented.)

Paths
-----

Some care must be taken when passing arbitrary paths and filenames to mpv. For
example, paths starting with ``-`` will be interpreted as options. Likewise,
if a path contains the sequence ``://``, the string before that might be
interpreted as protocol prefix, even though ``://`` can be part of a legal
UNIX path. To avoid problems with arbitrary paths, you should be sure that
absolute paths passed to mpv start with ``/``, and relative paths with ``./``.

The name ``-`` itself is interpreted as stdin, and will cause mpv to disable
console controls. (Which makes it suitable for playing data piped to stdin.)

For paths passed to suboptions, the situation is further complicated by the
need to escape special characters. To work this around, the path can be
additionally wrapped in the fixed-length syntax, e.g. ``%n%string_of_length_n``
(see above).

Some mpv options interpret paths starting with ``~``. Currently, the prefix
``~~/`` expands to the mpv configuration directory (usually ``~/.config/mpv/``).
``~/`` expands to the user's home directory. (The trailing ``/`` is always
required.)

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

CONFIGURATION FILES
===================

Location and Syntax
-------------------

You can put all of the options in configuration files which will be read every
time mpv is run. The system-wide configuration file 'mpv.conf' is in your
configuration directory (e.g. ``/etc/mpv`` or ``/usr/local/etc/mpv``), the
user-specific one is ``~/.config/mpv/mpv.conf``. For details and platform
specifics see the `FILES`_ section.
User-specific options override system-wide options and options given on the
command line override either. The syntax of the configuration files is
``option=<value>``; everything after a *#* is considered a comment. Options
that work without values can be enabled by setting them to *yes* and disabled by
setting them to *no*. Even suboptions can be specified in this way.

.. admonition:: Example configuration file

    ::

        # Use opengl video output by default.
        vo=opengl
        # Use quotes for text that can contain spaces:
        status-msg="Time: ${time-pos}"

Escaping spaces and special characters
--------------------------------------

This is done like with command line options. The shell is not involved here,
but option values still need to be quoted as a whole if it contains certain
characters like spaces. A config entry can be quoted with ``"`` and ``'``,
as well as with the fixed-length syntax (``%n%``) mentioned before. This is like
passing the exact contents of the quoted string as command line option. C-style
escapes are currently _not_ interpreted on this level, although some options to
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

.. admonition:: Example mpv profile

    ::

        [vo.vdpau]
        # Use hardware decoding
        hwdec=vdpau

        [protocol.dvd]
        profile-desc="profile for dvd:// streams"
        alang=en

        [extension.flv]
        profile-desc="profile for .flv files"
        vf=flip

        [ao.alsa]
        device=spdif


TAKING SCREENSHOTS
==================

Screenshots of the currently played file can be taken using the 'screenshot'
input mode command, which is by default bound to the ``s`` key. Files named
``shotNNNN.jpg`` will be saved in the working directory, using the first
available number - no files will be overwritten.

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
- Dropped frames, e.g. ``Dropped: 4``. Shows up only if the count is not 0. Can
  grow if the video framerate is higher than that of the display, or if video
  rendering is too slow. Also can be incremented on "hiccups" and when the video
  frame couldn't be displayed on time. (``vo-drop-frame-count`` property.)
  If the decoder drops frames, the number of decoder-dropped frames is appended
  to the display as well, e.g.: ``Dropped: 4/34``. This happens only if
  decoder-framedropping is enabled with the ``--framedrop`` options.
  (``drop-frame-count`` property.)
- Cache state, e.g. ``Cache:  2s+134KB``. Visible if the stream cache is enabled.
  The first value shows the amount of video buffered in the demuxer in seconds,
  the second value shows *additional* data buffered in the stream cache in
  kilobytes. (``demuxer-cache-duration`` and ``cache-used`` properties.)


PROTOCOLS
=========

``http://...``, ``https://``, ...
    Many network protocols are supported, but the protocol prefix must always
    be specified. mpv will never attempt to guess whether a filename is
    actually a network address. A protocol prefix is always required.

    Note that not all prefixes are documented here. Undocumented prefixes are
    either aliases to documented protocols, or are just redirections to
    protocols implemented and documented in FFmpeg.

``-``
    Play data from stdin.

``smb://PATH``
    Play a path from  Samba share.

``bd://[title][/device]`` ``--bluray-device=PATH``
    Play a Blu-Ray disc. Currently, this does not accept ISO files. Instead,
    you must mount the ISO file as filesystem, and point ``--bluray-device``
    to the mounted directory directly.

``bdnav://[title][/device]``
    Play a Blu-Ray disc, with navigation features enabled. This feature is
    permanently experimental.

``dvd://[title|[starttitle]-endtitle][/device]`` ``--dvd-device=PATH``
    Play a DVD. If you want dvdnav menus, use ``dvd://menu``. If no title
    is given, the longest title is auto-selected.

    ``dvdnav://`` is an old alias for ``dvd://`` and does exactly the same
    thing.

``dvdread://...:``
    Play a DVD using the old libdvdread code. This is what MPlayer and older
    mpv versions use for ``dvd://``. Use is discouraged. It's provided only
    for compatibility and for transition.

``tv://[channel][/input_id]`` ``--tv-...``
    Analogue TV via V4L. Also useful for webcams. (Linux only.)

``pvr://`` ``--pvr-...``
    PVR. (Linux only.)

``dvb://[cardnumber@]channel`` ``--dvbin-...``
    Digital TV via DVB. (Linux only.)

``mf://[filemask|@listfile]`` ``--mf-...``
    Play a series of images as video.

``cdda://track[-endtrack][:speed][/device]`` ``--cdrom-device=PATH`` ``--cdda-...``
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

``edl://[edl specification as in edl-mpv.rst]``
    Stitch together parts of multiple files and play them.

``null://``
    Simulate an empty file.

``memory://data``
    Use the ``data`` part as source data.

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
- manually adding ``--profile=pseudo-gui`` to the command line

This mode implicitly adds ``--profile=pseudo-gui`` to the command line, with
the ``pseudo-gui`` profile being predefined with the following contents:

::

    [pseudo-gui]
    terminal=no
    force-window=yes
    idle=once

This follows the mpv config file format. To customize pseudo-GUI mode, you can
put your own ``pseudo-gui`` profile into your ``mpv.conf``. This profile will
enhance the default profile, rather than overwrite it.

The profile always overrides other settings in ``mpv.conf``.


.. include:: options.rst

.. include:: ao.rst

.. include:: vo.rst

.. include:: af.rst

.. include:: vf.rst

.. include:: encode.rst

.. include:: input.rst

.. include:: osc.rst

.. include:: lua.rst

.. include:: ipc.rst

.. include:: changes.rst

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

``TERM``
    Used to determine terminal type.

``MPV_HOME``
    Directory where mpv looks for user settings. Overrides ``HOME``, and mpv
    will try to load the config file as ``$MPV_HOME/mpv.conf``.

``MPV_VERBOSE`` (see also ``-v`` and ``--msg-level``)
    Set the initial verbosity level across all message modules (default: 0).
    This is an integer, and the resulting verbosity corresponds to the number
    of ``--v`` options passed to the command line.

``MPV_LEAK_REPORT``
    If set to ``1``, enable internal talloc leak reporting. Note that this can
    cause trouble with multithreading, so only developers should use this.

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
           player keys to try and get the disc key. This can fail if the drive
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

``~/.config/mpv/scripts/``
    All files in this directory are loaded as if they were passed to the
    ``--script`` option. They are loaded in alphabetical order, and sub-directories
    and files with no ``.lua`` extension are ignored. The ``--load-scripts=no``
    option disables loading these files.

``~/.config/mpv/watch_later/``
    Contains temporary config files needed for resuming playback of files with
    the watch later feature. See for example the ``Q`` key binding, or the
    ``quit_watch_later`` input command.

    Each file is a small config file which is loaded if the corresponding media
    file is loaded. It contains the playback position and some (not necessarily
    all) settings that were changed during playback. The filenames are hashed
    from the full paths of the media files. It's in general not possible to
    extract the media filename from this hash. However, you can set the
    ``--write-filename-in-watch-later-config`` option, and the player will
    add the media filename to the contents of the resume config file.

``~/.config/mpv/lua-settings/osc.conf``
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

    ``C:\users\USERNAME\Application Data\mpv\mpv.conf``

You can find the exact path by running ``echo %APPDATA%\mpv\mpv.conf`` in cmd.exe.

Other config files (such as ``input.conf``) are in the same directory. See the
`FILES`_ section above.

The environment variable ``$MPV_HOME`` completely overrides these, like on
UNIX.

Config files located in the same directory as ``mpv.exe`` are loaded with
lower priority. Some config files are loaded only once, which means that
e.g. of 2 ``input.conf`` files located in two config directories, only the
one from the directory with higher priority will be loaded.

A third config directory with lowest priority is the directory named ``mpv``
in the same directory as ``mpv.exe``. This used to be the directory with
highest priority, but is now discouraged to use and might be removed in the
future.

Note that mpv likes to mix ``/`` and ``\`` path separators for simplicity.
kernel32.dll accepts this, but cmd.exe does not.

EXAMPLES OF MPV USAGE
=====================

Blu-ray playback:
    - ``mpv bd:////path/to/disc``
    - ``mpv bd:// --bluray-device=/path/to/disc``

Play in Japanese with English subtitles:
    ``mpv dvd://1 --alang=ja --slang=en``

Play only chapters 5, 6, 7:
    ``mpv dvd://1 --chapter=5-7``

Play only titles 5, 6, 7:
    ``mpv dvd://5-7``

Play a multi-angle DVD:
    ``mpv dvd://1 --dvd-angle=2``

Play from a different DVD device:
    ``mpv dvd://1 --dvd-device=/dev/dvd2``

Play DVD video from a directory with VOB files:
    ``mpv dvd://1 --dvd-device=/path/to/directory/``

Stream from HTTP:
    ``mpv http://example.com/example.avi``

Stream using RTSP:
    ``mpv rtsp://server.example.com/streamName``

Play a libavfilter graph:
    ``mpv avdevice://lavfi:mandelbrot``

AUTHORS
=======

mpv is a MPlayer fork based on mplayer2, which in turn is a fork of MPlayer.

MPlayer was initially written by Arpad Gereoffy. See the ``AUTHORS`` file for
a list of some of the many other contributors.

MPlayer is (C) 2000-2013 The MPlayer Team

This man page was written mainly by Gabucino, Jonas Jermann and Diego Biurrun.
