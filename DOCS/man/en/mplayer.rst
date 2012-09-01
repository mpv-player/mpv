mplayer2 manual page
####################

Synopsis
========

| **mplayer** [options] [file|URL|-]
| **mplayer** [options] --playlist=PLAYLIST
| **mplayer** [options] files
| **mplayer** [options] {group of files and options}
| **mplayer** [br]://[title][/device] [options]
| **mplayer** dvd://[title|[start\_title]-end\_title][/device] [options]
| **mplayer** \vcd://track[/device]
| **mplayer** \tv://[channel][/input_id] [options]
| **mplayer** radio://[channel|frequency][/capture] [options]
| **mplayer** \pvr:// [options]
| **mplayer** \dvb://[card\_number@]channel [options]
| **mplayer** \mf://[filemask|\@listfile] [-mf options] [options]
| **mplayer** [cdda|cddb]://track[-endtrack][:speed][/device] [options]
| **mplayer** [file|mms[t]|http|http\_proxy|rt[s]p|ftp|udp|unsv|icyx|noicyx|smb]:// [user:pass\@]URL[:port] [options]
| **mplayer** \sdp://file [options]
| **mplayer** \mpst://host[:port]/URL [options]
| **mplayer** \tivo://host/[list|llist|fsid] [options]


DESCRIPTION
===========

**mplayer** is a movie player for Linux. It supports a wide variety of video
file formats, audio and video codecs, and subtitle types. Special input URL
types are available to read input from a variety of sources other than disk
files. Depending on platform, a variety of different video and audio output
methods are supported.

Usage examples to get you started quickly can be found at the end of this man
page.


INTERACTIVE CONTROL
===================

MPlayer has a fully configurable, command-driven control layer which allows you
to control MPlayer using keyboard, mouse, joystick or remote control (with
LIRC). See the ``--input`` option for ways to customize it.

keyboard control
----------------

LEFT and RIGHT
    Seek backward/forward 10 seconds. Shift+arrow does a 1 second exact seek
    (see ``--hr-seek``; currently modifier keys like shift only work if used in
    an X output window).

UP and DOWN
    Seek forward/backward 1 minute. Shift+arrow does a 5 second exact seek (see
    ``--hr-seek``; currently modifier keys like shift only work if used in an X
    output window).

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
    ``--framedrop`` and ``--hardframedrop``).

v
    Toggle subtitle visibility.

j and J
    Cycle through the available subtitles.

y and g
    Adjust subtitle delay to immediately display previous/next subtitle.

F
    Toggle displaying "forced subtitles".

a
    Toggle subtitle alignment: top / middle / bottom.

x and z
    Adjust subtitle delay by +/- 0.1 seconds.

V
    Toggle subtitle VSFilter aspect compatibility mode. See
    ``--ass-vsfilter-aspect-compat`` for more info.

C (``--capture`` only)
    Start/stop capturing the primary stream.

r and t
    Move subtitles up/down.

i (``--edlout`` mode only)
    Set start or end of an EDL skip and write it out to the given file.

s
    Take a screenshot.

S
    Start/stop taking screenshots.

I
    Show filename on the OSD.

P
    Show progression bar, elapsed time and total duration on the OSD.

! and @
    Seek to the beginning of the previous/next chapter.

D (``--vo=vdpau``, ``--vf=yadif``, ``--vf=kerndeint`` only)
    Activate/deactivate deinterlacer.

A
    Cycle through the available DVD angles.

c
    Change YUV colorspace.

(The following keys are valid only when using a video output that supports the
corresponding adjustment, the software equalizer (``--vf=eq`` or ``--vf=eq2``)
or hue filter (``--vf=hue``).)

1 and 2
    Adjust contrast.

3 and 4
    Adjust brightness.

5 and 6
    Adjust hue.

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
| `mplayer --ao pcm:file=%10%C:test.wav test.avi`
| Or in a script:
| `mplayer --ao pcm:file=%\`expr length "$NAME"\`%"$NAME" test.avi`


Per-file options
----------------

When playing multiple files, any option given on the command line usually
affects all files. Example:

`mplayer --a file1.mkv --b file2.mkv --c`

+-----------+-------------------------+
| File      | Active options          |
+===========+=========================+
| file1.mkv | --a --b --c             |
+-----------+-------------------------+
| file2.mkv | --a --b --c             |
+-----------+-------------------------+

Also, if any option is changed at runtime (via slave commands), they aren't
reset when a new file is played.

Sometimes, it's useful to change options per-file. This can be achieved by
adding the special per-file markers `--{` and `--}`. (Note that you must
escape these on some shells.) Example:

`mplayer --a file1.mkv --b --\\\{ --c file2.mkv --d file3.mkv --e --\\\} file4.mkv --f`

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

You can put all of the options in configuration files which will be read every
time MPlayer is run. The system-wide configuration file 'mplayer.conf' is in
your configuration directory (e.g. ``/etc/mplayer`` or
``/usr/local/etc/mplayer``), the user specific one is ``~/.mplayer/config``.
User specific options override system-wide options and options given on the
command line override either. The syntax of the configuration files is
``option=<value>``, everything after a *#* is considered a comment. Options
that work without values can be enabled by setting them to *yes* or *1* or
*true* and disabled by setting them to *no* or *0* or *false*. Even suboptions
can be specified in this way.

You can also write file-specific configuration files. If you wish to have a
configuration file for a file called 'movie.avi', create a file named
'movie.avi.conf' with the file-specific options in it and put it in
``~/.mplayer/``. You can also put the configuration file in the same directory
as the file to be played, as long as you give the ``--use-filedir-conf``
option (either on the command line or in your global config file). If a
file-specific configuration file is found in the same directory, no
file-specific configuration is loaded from ``~/.mplayer``. In addition, the
``--use-filedir-conf`` option enables directory-specific configuration files.
For this, MPlayer first tries to load a mplayer.conf from the same directory
as the file played and then tries to load any file-specific configuration.

*EXAMPLE MPLAYER CONFIGURATION FILE:*

| # Use gl3 video output by default.
| vo=gl3
| # I love practicing handstands while watching videos.
| flip=yes
| # Decode multiple files from PNG,
| # start with mf://filemask
| mf=type=png:fps=25
| # Eerie negative images are cool.
| vf=eq2=1.0:-0.8


PROFILES
========

To ease working with different configurations profiles can be defined in the
configuration files. A profile starts with its name between square brackets,
e.g. *[my-profile]*. All following options will be part of the profile. A
description (shown by ``--profile=help``) can be defined with the profile-desc
option. To end the profile, start another one or use the profile name
*default* to continue with normal options.

*EXAMPLE MPLAYER PROFILE:*

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

Taking screenshots
==================

Screenshots of the currently played file can be taken using the 'screenshot'
slave mode command, which is by default bound to the ``s`` key. Files named
``shotNNNN.png`` will be saved in the working directory, using the first
available number - no files will be overwritten.

A screenshot will usually contain the unscaled video contents at the end of the
video filter chain. Some video output drivers will include subtitles and OSD in
the video frame as well - this is because of technical restrictions.

The ``screenshot`` video filter is normally not required when using a
recommended GUI video output driver. The ``screenshot`` filter will be attempted
to be used if the video output doesn't support screenshots. Note that taking
screenshots with the video filter is not instant: the screenshot will be only
saved when the next video frame is displayed. This means attempting to take a
screenshot while the player is paused will do nothing, until the user unpauses
or seeks. Also, the screenshot filter is not compatible with hardware decoding,
and actually will cause initialization failure when use with hardware decoding
is attempted. Using the ``screenshot`` video filter is not recommended for
these reasons.

.. include:: changes.rst

ENVIRONMENT VARIABLES
=====================

There are a number of environment variables that can be used to control the
behavior of MPlayer.

``MPLAYER_CHARSET`` (see also ``--msgcharset``)
    Convert console messages to the specified charset (default: autodetect). A
    value of "noconv" means no conversion.

``MPLAYER_HOME``
    Directory where MPlayer looks for user settings.

``MPLAYER_LOCALEDIR``
    Directory where MPlayer looks for gettext translation files (if enabled).

``MPLAYER_VERBOSE`` (see also ``-v`` and ``--msglevel``)
    Set the initial verbosity level across all message modules (default: 0).
    The resulting verbosity corresponds to that of ``--msglevel=5`` plus the
    value of ``MPLAYER_VERBOSE``.

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

libao2:
    ``AUDIOSERVER``
        Specifies the Network Audio System server to which the nas audio
        output driver should connect and the transport that should be used. If
        unset DISPLAY is used instead. The transport can be one of tcp and
        unix. Syntax is ``tcp/<somehost>:<someport>``,
        ``<somehost>:<instancenumber>`` or ``[unix]:<instancenumber>``. The
        NAS base port is 8000 and <instancenumber> is added to that.

        *EXAMPLES*:

        ``AUDIOSERVER=somehost:0``
             Connect to NAS server on somehost using default port and
             transport.
        ``AUDIOSERVER=tcp/somehost:8000``
             Connect to NAS server on somehost listening on TCP port 8000.
        ``AUDIOSERVER=(unix)?:0``
             Connect to NAS server instance 0 on localhost using unix domain
             sockets.

    ``DISPLAY``
        FIXME: Document this.

osdep:
    ``TERM``
        FIXME: Document this.

libvo:
    ``DISPLAY``
        FIXME: Document this.

    ``FRAMEBUFFER``
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

``/usr/local/etc/mplayer/mplayer.conf``
    MPlayer system-wide settings

``~/.mplayer/config``
    MPlayer user settings

``~/.mplayer/input.conf``
    input bindings (see ``--input=keylist`` for the full list)

``~/.mplayer/DVDkeys/``
    cached CSS keys


EXAMPLES OF MPLAYER USAGE
=========================

Quickstart Blu-ray playing:
    - ``mplayer br:////path/to/disc``
    - ``mplayer br:// --bluray-device=/path/to/disc``

Quickstart DVD playing:
    ``mplayer dvd://1``

Play in Japanese with English subtitles:
    ``mplayer dvd://1 --alang=ja --slang=en``

Play only chapters 5, 6, 7:
    ``mplayer dvd://1 --chapter=5-7``

Play only titles 5, 6, 7:
    ``mplayer dvd://5-7``

Play a multiangle DVD:
    ``mplayer dvd://1 --dvdangle=2``

Play from a different DVD device:
    ``mplayer dvd://1 --dvd-device=/dev/dvd2``

Play DVD video from a directory with VOB files:
    ``mplayer dvd://1 --dvd-device=/path/to/directory/``

Stream from HTTP:
    ``mplayer http://mplayer.hq/example.avi``

Stream using RTSP:
    ``mplayer rtsp://server.example.com/streamName``

input from standard V4L:
    ``mplayer tv:// --tv=driver=v4l:width=640:height=480:outfmt=i420 --vc=rawi420 --vo=xv``

Play DTS-CD with passthrough:
    ``mplayer --ac=hwdts --rawaudio=format=0x2001 --cdrom-device=/dev/cdrom cdda://``

    You can also use ``--afm=hwac3`` instead of ``--ac=hwdts``. Adjust
    ``/dev/cdrom`` to match the CD-ROM device on your system. If your external
    receiver supports decoding raw DTS streams, you can directly play it via
    ``cdda://`` without setting format, hwac3 or hwdts.

Play a 6-channel AAC file with only two speakers:
    ``mplayer --rawaudio=format=0xff --demuxer=rawaudio --af=pan=2:.32:.32:.39:.06:.06:.39:.17:-.17:-.17:.17:.33:.33 adts_he-aac160_51.aac``

    You might want to play a bit with the pan values (e.g multiply with a
    value) to increase volume or avoid clipping.

checkerboard invert with geq filter:
    ``mplayer --vf=geq='128+(p(X\,Y)-128)*(0.5-gt(mod(X/SW\,128)\,64))*(0.5-gt(mod(Y/SH\,128)\,64))*4'``


AUTHORS
=======

MPlayer was initially written by Arpad Gereoffy. See the ``AUTHORS`` file for
a list of some of the many other contributors.

MPlayer is (C) 2000-2011 The MPlayer Team

This man page was written mainly by Gabucino, Jonas Jermann and Diego Biurrun.
