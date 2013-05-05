mpv
###

##############
a movie player
##############

:Author: wm4
:Date: 2012-11-29
:Copyright: GPLv3
:Version: git
:Manual section: 1
:Manual group: multimedia

Synopsis
========

| **mpv** [options] [file|URL|-]
| **mpv** [options] --playlist=PLAYLIST
| **mpv** [options] files
| **mpv** [options] {group of files and options}
| **mpv** [bd]://[title][/device] [options]
| **mpv** dvd://[title|[start\_title]-end\_title][/device] [options]
| **mpv** \vcd://track[/device]
| **mpv** \tv://[channel][/input_id] [options]
| **mpv** radio://[channel|frequency][/capture] [options]
| **mpv** \pvr:// [options]
| **mpv** \dvb://[card\_number@]channel [options]
| **mpv** \mf://[filemask|\@listfile] [-mf options] [options]
| **mpv** [cdda|cddb]://track[-endtrack][:speed][/device] [options]
| **mpv** [file|mms[t]|http|httpproxy|rt[s]p|ftp|udp|unsv|icyx|noicyx|smb]:// [user:pass\@]URL[:port] [options]


DESCRIPTION
===========

**mpv** is a movie player based on MPlayer and mplayer2. It supports a wide variety of video
file formats, audio and video codecs, and subtitle types. Special input URL
types are available to read input from a variety of sources other than disk
files. Depending on platform, a variety of different video and audio output
methods are supported.

Usage examples to get you started quickly can be found at the end of this man
page.


INTERACTIVE CONTROL
===================

mpv has a fully configurable, command-driven control layer which allows you
to control mpv using keyboard, mouse, joystick or remote control (with
LIRC). See the ``--input-`` options for ways to customize it.

keyboard control
----------------

LEFT and RIGHT
    Seek backward/forward 10 seconds. Shift+arrow does a 1 second exact seek
    (see ``--hr-seek``).

UP and DOWN
    Seek forward/backward 1 minute. Shift+arrow does a 5 second exact seek (see
    ``--hr-seek``).

PGUP and PGDWN
    Seek forward/backward 10 minutes.

[ and ]
    Decrease/increase current playback speed by 10%.

{ and }
    Halve/double current playback speed.

BACKSPACE
    Reset playback speed to normal.

< and >
    Go backward/forward in the playlist.

ENTER
    Go forward in the playlist, even over the end.

p / SPACE
    Pause (pressing again unpauses).

.
    Step forward. Pressing once will pause movie, every consecutive press will
    play one frame and then go into pause mode again.

q / ESC
    Stop playing and quit.

Q
    Like ``q``, but store the current playback position. Playing the same file
    later will resume at the old playback position if possible.

U
    Stop playing (and quit if ``--idle`` is not used).

\+ and -
    Adjust audio delay by +/- 0.1 seconds.

/ and *
    Decrease/increase volume.

9 and 0
    Decrease/increase volume.

( and )
    Adjust audio balance in favor of left/right channel.

m
    Mute sound.

\_
    Cycle through the available video tracks.

\#
    Cycle through the available audio tracks.

TAB (MPEG-TS and libavformat only)
    Cycle through the available programs.

f
    Toggle fullscreen (see also ``--fs``).

T
    Toggle stay-on-top (see also ``--ontop``).

w and e
    Decrease/increase pan-and-scan range.

o
    Toggle OSD states: none / seek / seek + timer / seek + timer + total time.

d
    Toggle frame dropping states: none / skip display / skip decoding (see
    ``--framedrop``).

v
    Toggle subtitle visibility.

j and J
    Cycle through the available subtitles.

F
    Toggle displaying "forced subtitles".

x and z
    Adjust subtitle delay by +/- 0.1 seconds.

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

P
    Show progression bar, elapsed time and total duration on the OSD.

! and @
    Seek to the beginning of the previous/next chapter.

D (``--vo=vdpau``, ``--vf=yadif`` only)
    Activate/deactivate deinterlacer.

A
    Cycle through the available DVD angles.

c
    Change YUV colorspace.

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

(The following keys are valid only when using the corevideo video output
driver.)

command + 0
    Resize movie window to half its original size.

command + 1
    Resize movie window to its original size.

command + 2
    Resize movie window to double its original size.

command + f
    Toggle fullscreen (see also ``--fs``).

command + [ and command + ]
    Set movie window alpha.

(The following keys are valid if you have a keyboard with multimedia keys.)

PAUSE
    Pause.

STOP
    Stop playing and quit.

PREVIOUS and NEXT
    Seek backward/forward 1 minute.

(The following keys are only valid if you compiled with TV or DVB input
support and will take precedence over the keys defined above.)

h and k
    Select previous/next channel.

n
    Change norm.

u
    Change channel list.

mouse control
-------------

button 3 and button 4
    Seek backward/forward 1 minute.

button 5 and button 6
    Decrease/increase volume.

joystick control
----------------

left and right
    Seek backward/forward 10 seconds.

up and down
    Seek forward/backward 1 minute.

button 1
    Pause.

button 2
    Toggle OSD states: none / seek / seek + timer / seek + timer + total time.

button 3 and button 4
    Decrease/increase volume.


USAGE
=====

Every *flag* option has a *no-flag* counterpart, e.g. the opposite of the
``--fs`` option is ``--no-fs``. ``--fs=yes`` is same as ``--fs``, ``--fs=no``
is the same as ``--no-fs``.

If an option is marked as *(XXX only)*, it will only work in combination with
the *XXX* option or if *XXX* is compiled in.

| *NOTE*: The suboption parser (used for example for ``--ao=pcm`` suboptions)
  supports a special kind of string-escaping intended for use with external
  GUIs.
| It has the following format:
| %n%string\_of\_length\_n

| *EXAMPLES*:
| `mpv --ao=pcm:file=%10%C:test.wav test.avi`
| Or in a script:
| `mpv --ao=pcm:file=%\`expr length "$NAME"\`%"$NAME" test.avi`

Per-file options
----------------

When playing multiple files, any option given on the command line usually
affects all files. Example:

`mpv --a file1.mkv --b file2.mkv --c`

+-----------+-------------------------+
| File      | Active options          |
+===========+=========================+
| file1.mkv | --a --b --c             |
+-----------+-------------------------+
| file2.mkv | --a --b --c             |
+-----------+-------------------------+

Also, if any option is changed at runtime (via input commands), they aren't
reset when a new file is played.

Sometimes, it's useful to change options per-file. This can be achieved by
adding the special per-file markers `--{` and `--}`. (Note that you must
escape these on some shells.) Example:

`mpv --a file1.mkv --b --\\\{ --c file2.mkv --d file3.mkv --e --\\\} file4.mkv --f`

+-----------+-------------------------+
| File      | Active options          |
+===========+=========================+
| file1.mkv | --a --b --f             |
+-----------+-------------------------+
| file2.mkv | --a --b --f --c --d --e |
+-----------+-------------------------+
| file3.mkv | --a --b --f --c --d --e |
+-----------+-------------------------+
| file4.mkv | --a --b --f             |
+-----------+-------------------------+

Additionally, any file-local option changed at runtime is reset when the current
file stops playing. If option ``--c`` is changed during playback of `file2.mkv`,
it's reset when advancing to `file3.mkv`. This only affects file-local options.
The option ``--a`` is never reset here.

CONFIGURATION FILES
===================

Location and syntax
-------------------

You can put all of the options in configuration files which will be read every
time mpv is run. The system-wide configuration file 'mpv.conf' is in
your configuration directory (e.g. ``/etc/mpv`` or
``/usr/local/etc/mpv``), the user specific one is ``~/.mpv/config``.
User specific options override system-wide options and options given on the
command line override either. The syntax of the configuration files is
``option=<value>``, everything after a *#* is considered a comment. Options
that work without values can be enabled by setting them to *yes* and disabled by
setting them to *no*. Even suboptions can be specified in this way.

*EXAMPLE CONFIGURATION FILE:*

| # Use opengl video output by default.
| vo=opengl
| # Use quotes for text that can contain spaces:
| status-msg="Time: ${time-pos}"

Putting command line options into the configuration file
--------------------------------------------------------

Almost all command line options can be put into the configuration file. Here
is a small guide:

+----------------------+--------------------------+
| Option               | Configuration file entry |
+======================+==========================+
| --flag               | flag                     |
+----------------------+--------------------------+
| -opt val             | opt=val                  |
+----------------------+--------------------------+
| --opt=val            | opt=val                  |
+----------------------+--------------------------+
| -opt "has spaces"    | opt="has spaces"         |
+----------------------+--------------------------+

File specific configuration files
---------------------------------

You can also write file-specific configuration files. If you wish to have a
configuration file for a file called 'movie.avi', create a file named
'movie.avi.conf' with the file-specific options in it and put it in
``~/.mpv/``. You can also put the configuration file in the same directory
as the file to be played, as long as you give the ``--use-filedir-conf``
option (either on the command line or in your global config file). If a
file-specific configuration file is found in the same directory, no
file-specific configuration is loaded from ``~/.mpv``. In addition, the
``--use-filedir-conf`` option enables directory-specific configuration files.
For this, mpv first tries to load a mpv.conf from the same directory
as the file played and then tries to load any file-specific configuration.


Profiles
--------

To ease working with different configurations profiles can be defined in the
configuration files. A profile starts with its name between square brackets,
e.g. *[my-profile]*. All following options will be part of the profile. A
description (shown by ``--profile=help``) can be defined with the profile-desc
option. To end the profile, start another one or use the profile name
*default* to continue with normal options.

*EXAMPLE MPV PROFILE:*

| [vo.vdpau]
| # Use hardware decoding (this might break playback of some h264 files)
| hwdec=vdpau
|
| [protocol.dvd]
| profile-desc="profile for dvd:// streams"
| vf=pp=hb/vb/dr/al/fd
| alang=en
|
| [extension.flv]
| profile-desc="profile for .flv files"
| flip=yes
|
| [ao.alsa]
| device=spdif


OPTIONS
=======

.. include:: options.rst

.. include:: ao.rst

.. include:: vo.rst

.. include:: af.rst

.. include:: vf.rst

.. include:: encode.rst

.. include:: input.rst

Taking screenshots
==================

Screenshots of the currently played file can be taken using the 'screenshot'
input mode command, which is by default bound to the ``s`` key. Files named
``shotNNNN.jpg`` will be saved in the working directory, using the first
available number - no files will be overwritten.

A screenshot will usually contain the unscaled video contents at the end of the
video filter chain and subtitles. By default the ``S`` takes screenshots without
subtitles, while ``s`` includes subtitles.

The ``screenshot`` video filter is not required when using a recommended GUI
video output driver. It should normally not be added to the config file, as
taking screenshots is handled by the VOs, and adding the screenshot filter will
break hardware decoding. (The filter may still be useful for taking screenshots
at a certain point within the video chain when using multiple video filters.)

.. include:: changes.rst

ENVIRONMENT VARIABLES
=====================

There are a number of environment variables that can be used to control the
behavior of mpv.

``MPV_HOME``
    Directory where mpv looks for user settings.

``MPV_LOCALEDIR``
    Directory where mpv looks for gettext translation files (if enabled).

``MPV_VERBOSE`` (see also ``-v`` and ``--msglevel``)
    Set the initial verbosity level across all message modules (default: 0).
    The resulting verbosity corresponds to that of ``--msglevel=5`` plus the
    value of ``MPV_VERBOSE``.

libaf:
    ``LADSPA_PATH``
        If ``LADSPA_PATH`` is set, it searches for the specified file. If it
        is not set, you must supply a fully specified pathname.

        FIXME: This is also mentioned in the ladspa section.

libdvdcss:
    ``DVDCSS_CACHE``
        Specify a directory in which to store title key values. This will
        speed up descrambling of DVDs which are in the cache. The
        ``DVDCSS_CACHE`` directory is created if it does not exist, and a
        subdirectory is created named after the DVD's title or manufacturing
        date. If ``DVDCSS_CACHE`` is not set or is empty, libdvdcss will use
        the default value which is ``${HOME}/.dvdcss/`` under Unix and
        ``C:\Documents and Settings\$USER\Application Data\dvdcss\`` under
        Win32. The special value "off" disables caching.

    ``DVDCSS_METHOD``
        Sets the authentication and decryption method that libdvdcss will use
        to read scrambled discs. Can be one of title, key or disc.

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

osdep:
    ``TERM``
        FIXME: Document this.

libvo:
    ``DISPLAY``
        FIXME: Document this.

    ``HOME``
        FIXME: Document this.

libmpdemux:

    ``HOME``
        FIXME: Document this.

    ``HOMEPATH``
        FIXME: Document this.

    ``http_proxy``
        FIXME: Document this.

    ``LOGNAME``
        FIXME: Document this.

    ``USERPROFILE``
        FIXME: Document this.

libavformat:

    ``AUDIO_FLIP_LEFT``
        FIXME: Document this.

    ``BKTR_DEV``
        FIXME: Document this.

    ``BKTR_FORMAT``
        FIXME: Document this.

    ``BKTR_FREQUENCY``
        FIXME: Document this.

    ``http_proxy``
        FIXME: Document this.

    ``no_proxy``
        FIXME: Document this.


FILES
=====

``/usr/local/etc/mpv/mpv.conf``
    mpv system-wide settings

``~/.mpv/config``
    mpv user settings

``~/.mpv/input.conf``
    input bindings (see ``--input-keylist`` for the full list)

``~/.mpv/DVDkeys/``
    cached CSS keys


EXAMPLES OF MPV USAGE
=========================

Quickstart Blu-ray playing:
    - ``mpv bd:////path/to/disc``
    - ``mpv bd:// --bluray-device=/path/to/disc``

Quickstart DVD playing:
    ``mpv dvd://1``

Play in Japanese with English subtitles:
    ``mpv dvd://1 --alang=ja --slang=en``

Play only chapters 5, 6, 7:
    ``mpv dvd://1 --chapter=5-7``

Play only titles 5, 6, 7:
    ``mpv dvd://5-7``

Play a multiangle DVD:
    ``mpv dvd://1 --dvdangle=2``

Play from a different DVD device:
    ``mpv dvd://1 --dvd-device=/dev/dvd2``

Play DVD video from a directory with VOB files:
    ``mpv dvd://1 --dvd-device=/path/to/directory/``

Stream from HTTP:
    ``mpv http://example.com/example.avi``

Stream using RTSP:
    ``mpv rtsp://server.example.com/streamName``

Play a libavfilter graph:
    ``mpv avdevice://lavfi:mandlebrot``

AUTHORS
=======

mpv is a MPlayer fork based on mplayer2, which in turn is a fork of MPlayer.

MPlayer was initially written by Arpad Gereoffy. See the ``AUTHORS`` file for
a list of some of the many other contributors.

MPlayer is (C) 2000-2012 The MPlayer Team

This man page was written mainly by Gabucino, Jonas Jermann and Diego Biurrun.
