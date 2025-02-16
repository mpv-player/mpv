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

The following listings are not necessarily complete. See ``etc/input.conf``
in the mpv source files for a list of default bindings. User ``input.conf``
files and Lua scripts can define additional key bindings.

See `COMMAND INTERFACE`_ and `Key names`_ sections for more details on
configuring keybindings.

See also ``--input-test`` for interactive binding details by key, and the
`stats`_ built-in script for key bindings list (including print to terminal). By
default, the ? key toggles the display of this list.

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

Ctrl+Shift+LEFT and Ctrl+Shift+RIGHT
    Adjust subtitle delay so that the previous or next subtitle is displayed
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

p and SPACE
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
    later will resume at the old playback position if possible. See
    `RESUMING PLAYBACK`_.

/ and *
    Decrease/increase volume.

KP_DIVIDE and KP_MULTIPLY
    Decrease/increase volume.

9 and 0
    Decrease/increase volume.

m
    Mute sound.

\_
    Cycle through the available video tracks.

\#
    Cycle through the available audio tracks.

E
    Cycle through the available Editions.

f
    Toggle fullscreen (see also ``--fs``).

ESC
    Exit fullscreen mode.

T
    Toggle stay-on-top (see also ``--ontop``).

w and W
    Decrease/increase pan-and-scan range. The ``e`` key does the same as
    ``W`` currently, but use is discouraged. See ``--panscan`` for more
    information.

o and P
    Show progression bar, elapsed time and total duration on the OSD.

O
    Toggle OSD states between normal and playback time/duration.

v
    Toggle subtitle visibility.

j and J
    Cycle through the available subtitles.

z and Z
    Adjust subtitle delay by -/+ 0.1 seconds. The ``x`` key does the same as
    ``Z`` currently, but use is discouraged.

l
    Set/clear A-B loop points. See ``ab-loop`` command for details.

L
    Toggle infinite looping.

Ctrl++ and Ctrl+-
    Adjust audio delay (A/V sync) by +/- 0.1 seconds.

Ctrl+KP_ADD and Ctrl+KP_SUBTRACT
    Adjust audio delay (A/V sync) by +/- 0.1 seconds.

G and F
    Adjust subtitle font size by +/- 10%.

u
    Switch between applying only ``--sub-ass-*`` overrides (default) to SSA/ASS
    subtitles, and overriding them almost completely with the normal subtitle
    style. See ``--sub-ass-override`` for more info.

V
    Cycle through which video data gets used for ASS rendering.
    See ``--sub-ass-use-video-data`` for more info.

r and R
    Move subtitles up/down. The ``t`` key does the same as ``R`` currently, but
    use is discouraged.

s
    Take a screenshot.

S
    Take a screenshot, without subtitles. (Whether this works depends on VO
    driver support.)

Ctrl+s
    Take a screenshot, as the window shows it (with subtitles, OSD, and scaled
    video).

PGUP and PGDWN
    Seek to the beginning of the previous/next chapter. In most cases,
    "previous" will actually go to the beginning of the current chapter; see
    ``--chapter-seek-threshold``.

Shift+PGUP and Shift+PGDWN
    Seek backward or forward by 10 minutes. (This used to be mapped to
    PGUP/PGDWN without Shift.)

b
    Activate/deactivate debanding.

d
    Cycle the deinterlacing filter.

A
    Cycle aspect ratio override.

Ctrl+h
    Toggle hardware video decoding on/off.

Alt+LEFT, Alt+RIGHT, Alt+UP, Alt+DOWN
    Move the video rectangle (panning).

Alt++ and Alt+-
    Change video zoom.

Alt+KP_ADD and Alt+KP_SUBTRACT
    Change video zoom.

Alt+BACKSPACE
    Reset the pan/zoom settings.

F8
    Show the playlist and the current position in it.

F9
    Show the list of audio and subtitle streams.

Ctrl+v
    Append the file or URL in the clipboard to the playlist. If nothing is
    currently playing, it is played immediately. Only works on platforms that
    support the ``clipboard`` property.

i and I
    Show/toggle an overlay displaying statistics about the currently playing
    file such as codec, framerate, number of dropped frames and so on. See
    `STATS`_ for more information.

?
    Toggle an overlay displaying the active key bindings. See `STATS`_ for more
    information.

DEL
    Cycle OSC visibility between never / auto (mouse-move) / always

\`
    Show the console. (ESC closes it again. See `CONSOLE`_.)

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

Alt+0 (and Command+0 on macOS)
    Resize video window to half its original size.

Alt+1 (and Command+1 on macOS)
    Resize video window to its original size.

Alt+2 (and Command+2 on macOS)
    Resize video window to double its original size.

Command + f (macOS only)
    Toggle fullscreen (see also ``--fs``).

(The following keybindings open a selector in the console that lets you choose
from a list of items by typing part of the desired item, by clicking the desired
item, or by navigating them with keybindings: ``Down`` and ``Ctrl+n`` go down,
``Up`` and ``Ctrl+p`` go up, ``Page down`` and ``Ctrl+f`` scroll down one page,
and ``Page up`` and ``Ctrl+b`` scroll up one page.)

In track selectors, selecting the current tracks disables it.

g-p
    Select a playlist entry.

g-s
    Select a subtitle track.

g-S
    Select a secondary subtitle track.

g-a
    Select an audio track.

g-v
    Select a video track.

g-t
    Select a track of any type.

g-c
    Select a chapter.

g-e
    Select an MKV edition or DVD/Blu-ray title.

g-l
    Select a subtitle line to seek to. This currently requires ``ffmpeg`` in
    ``PATH``, or in the same folder as mpv on Windows.

g-d
    Select an audio device.

g-h
    Select a file from the watch history. Requires ``--save-watch-history``.

g-w
    Select a file from watch later config files (see `RESUMING PLAYBACK`_) to
    resume playing. Requires ``--write-filename-in-watch-later-config``.

g-b
    Select a defined input binding.

g-r
    Show the values of all properties.

MENU
    Show a menu with miscellaneous entries.

See `SELECT`_ for more information.

(The following keys are valid if you have a keyboard with multimedia keys.)

PAUSE
    Pause.

STOP
    Stop playing and quit.

PREVIOUS and NEXT
    Seek backward/forward 1 minute.

ZOOMIN and ZOOMOUT
    Change video zoom.

If you miss some older key bindings, look at ``etc/restore-old-bindings.conf``
in the mpv git repository.

Mouse Control
-------------

Left double click
    Toggle fullscreen on/off.

Right click
    Toggle pause on/off.

Forward/Back button
    Skip to next/previous entry in playlist.

Wheel up/down
    Decrease/increase volume.

Wheel left/right
    Seek forward/backward 10 seconds.

Ctrl+Wheel up/down
    Change video zoom.

Context Menu
-------------

.. warning::

    This feature is experimental. It may not work with all VOs. A libass based
    fallback may be implemented in the future.

Context Menu is a menu that pops up on the video window on user interaction
(mouse right click, etc.).

To use this feature, you need to fill the ``menu-data`` property with menu
definition data, and add a keybinding to run the ``context-menu`` command,
which can be done with a user script.

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
legacy syntax ``-option value`` and ``-option=value`` will also work. This is
mostly  for compatibility with MPlayer. Using these should be avoided. Their
semantics can change any time in the future.

For example, the alternative syntax will consider an argument following the
option a filename. ``mpv -fs no`` will attempt to play a file named ``no``,
because ``--fs`` is a flag option that requires no parameter. If an option
changes and its parameter becomes optional, then a command line using the
alternative syntax will break.

Until mpv 0.31.0, there was no difference whether an option started with ``--``
or a single ``-``. Newer mpv releases strictly expect that you pass the option
value after a ``=``. For example, before ``mpv --log-file f.txt`` would write
a log to ``f.txt``, but now this command line fails, as ``--log-file`` expects
an option value, and ``f.txt`` is simply considered a normal file to be played
(as in ``mpv f.txt``).

The future plan is that ``-option value`` will not work anymore, and options
with a single ``-`` behave the same as ``--`` options.

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

Note: where applicable with JSON-IPC, ``%n%`` is the length in UTF-8 bytes,
after decoding the JSON data.

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

For paths passed to mpv suboptions (options that have multiple `:` and
`,`-separated values), the situation is further complicated by the need to
escape special characters. To work around this, the path can instead be wrapped
in the "fixed-length" syntax, e.g. ``%n%string_of_length_n`` (see above).

When using the libmpv API, you should strictly avoid using ``mpv_command_string``
for invoking the ``loadfile`` command, and instead prefer e.g. ``mpv_command``
to avoid the need for filename escaping.

The same applies when you're using the scripting API, where you should avoid using
``mp.command``, and instead prefer using "separate parameter" APIs, such as
``mp.commandv`` and ``mp.command_native``.

Some mpv options will interpret special meanings for paths starting with ``~``,
making it easy to dynamically find special directories, such as referring to the
current user's home directory or the mpv configuration directory.

When using the special ``~`` prefix, there must always be a trailing ``/`` after
the special path prefix. In other words, ``~`` doesn't work, but ``~/`` will work.

The following special paths/keywords are currently recognized:

.. warning::

    Beware that if ``--no-config`` is used, all of the "config directory"-based
    paths (``~~/``, ``~~home/`` and ``~~global/``) will be empty strings.

    This means that ``~~home/`` would expand to an empty string, and that
    sub-paths such as ``~~home/foo/bar"`` would expand to a relative path
    (``foo/bar``), which may not be what you expected.

    Furthermore, any commands that search in config directories will fail
    to find anything, since there won't be any directories to search in.

    Be sure that your scripts can handle these "no config" scenarios.

================ ===============================================================
Name             Meaning
================ ===============================================================
``~/``           The current user's home directory (equivalent to ``~/`` and
                 ``$HOME/`` in terminal environments).
``~~/``          If the sub-path exists in any of mpv's config directories, then
                 the path of the existing file/dir is returned. Otherwise this
                 is equivalent to ``~~home/``.
``~~home/``      mpv's config dir (for example ``~/.config/mpv/``).
``~~global/``    The global config path (such as ``/etc/mpv``), if available
                 (not on win32).
``~~osxbundle/`` The macOS bundle resource path (macOS only).
``~~desktop/``   The path to the desktop (win32, macOS).
``~~exe_dir/``   The path to the directory containing ``mpv.exe`` (for config
                 file purposes, ``$MPV_HOME`` will override this) (win32 only).
``~~cache/``     The path to application cache data (``~/.cache/mpv/``).
                 On some platforms, this will be the same as ``~~home/``.
``~~state/``     The path to application state data (``~/.local/state/mpv/``).
                 On some platforms, this will be the same as ``~~home/``.
``~~old_home/``  Do not use.
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
example, the ``--display-tags`` option takes a ``,``-separated list of tags, but
the option also allows you to append a single tag with ``--display-tags-append``,
and the tag name can for example contain a literal ``,`` without the need for
escaping.

String list and path list options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

String lists are separated by ``,``. The strings are not parsed or interpreted
by the option system itself. However, most path or file list options use ``:``
(Unix) or ``;`` (Windows) as separator, instead of ``,``.

They support the following operations:

============= ===============================================
Suffix        Meaning
============= ===============================================
-set          Set a list of items (using the list separator, escaped with backslash)
-append       Append single item (does not interpret escapes)
-add          Append 1 or more items (same syntax as -set)
-pre          Prepend 1 or more items (same syntax as -set)
-clr          Clear the option (remove all items)
-del          Delete 1 or more items if present (same syntax as -set)
-remove       Delete item if present (does not interpret escapes)
-toggle       Append an item, or remove it if it already exists (no escapes)
============= ===============================================

``-append`` is meant as a simple way to append a single item without having
to escape the argument (you may still need to escape on the shell level).

Key/value list options
~~~~~~~~~~~~~~~~~~~~~~

A key/value list is a list of key/value string pairs. In programming languages,
this type of data structure is often called a map or a dictionary. The order
normally does not matter, although in some cases the order might matter.

They support the following operations:

============= ===============================================
Suffix        Meaning
============= ===============================================
-set          Set a list of items (using ``,`` as separator)
-append       Append a single item (escapes for the key, no escapes for the value)
-add          Append 1 or more items (same syntax as -set)
-clr          Clear the option (remove all items)
-del          Delete 1 or more keys if present (same syntax as -set)
-remove       Delete item by key if present (does not interpret escapes)
============= ===============================================

Keys are unique within the list. If an already present key is set, the existing
key is removed before the new value is appended.

If you want to pass a value without interpreting it for escapes or ``,``, it is
recommended to use the ``-append`` variant. When using libmpv, prefer using
``MPV_FORMAT_NODE_MAP``; when using a scripting backend or the JSON IPC, use an
appropriate structured data type.

Prior to mpv 0.33, ``:`` was also recognized as separator by ``-set``.

Object settings list options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is a very complex option type for some options, such as ``--af`` and ``--vf``.
They often require complicated escaping. See `VIDEO FILTERS`_ for details.

They support the following operations:

============= ===============================================
Suffix        Meaning
============= ===============================================
-set          Set a list of items (using ``,`` as separator)
-append       Append single item
-add          Append 1 or more items (same syntax as -set)
-pre          Prepend 1 or more items (same syntax as -set)
-clr          Clear the option (remove all items)
-remove       Delete 1 or items if present (same syntax as -set)
-toggle       Append an item, or remove it if it already exists
-help         Pseudo operation that prints a help text to the terminal
============= ===============================================

General
~~~~~~~

Without suffix, the operation used is normally ``-set``.

Some operations like ``-add`` and ``-pre`` specify multiple items, but be
aware that you may need to escape the arguments. ``-append`` accepts a single,
unescaped item only (so the ``,`` separator will not be interpreted and
is passed on as part of the value).

Some options (like ``--sub-file``, ``--audio-file``, ``--glsl-shader``) are
aliases for the proper option with ``-append`` action. For example,
``--sub-file`` is an alias for ``--sub-files-append``.

Options of this type can be changed at runtime using the ``change-list``
command, which takes the suffix (without the ``-``) as separate operation
parameter.

An object settings list can hold up to 100 elements.

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
command line override both. The syntax of the configuration files is
``option=value``. Everything after a *#* is considered a comment. Options that
work without values can be enabled by setting them to *yes* and disabled by
setting them to *no*, and if the value is omitted, *yes* is implied. Even
suboptions can be specified in this way.

.. admonition:: Example configuration file

    ::

        # Don't allow new windows to be larger than the screen.
        autofit-larger=100%x100%
        # Enable hardware decoding if available, =yes is implied.
        hwdec
        # Spaces don't have to be escaped.
        osd-playing-msg=File: ${filename}

Escaping special characters
--------------------------------------

This is done like with command line options. A config entry can be quoted with
``"``, ``'``, as well as with the fixed-length syntax (``%n%``) mentioned
before. This is like passing the exact contents of the quoted string as a
command line option. C-style escapes are currently _not_ interpreted on this
level, although some options do this manually (this is a mess and should
probably be changed at some point). The shell is not involved here, so option
values only need to be quoted to escape ``#`` anywhere in the value, ``"``,
``'`` or ``%`` at the beginning of the value, and leading and trailing
whitespace.

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
``-opt "has spaces"``   ``opt=has spaces``
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
        cache=yes
        demuxer-max-bytes=512MiB
        demuxer-readahead-secs=20

        [network]
        profile-desc="profile for content over network"
        force-window=immediate
        # you can also include other profiles
        profile=big-cache

        [reduce-judder]
        video-sync=display-resample
        interpolation=yes

        # using a profile again extends it
        [network]
        demuxer-max-back-bytes=512MiB
        # reference a builtin profile
        profile=fast

Runtime profiles
----------------

Profiles can be set at runtime with ``apply-profile`` command. Since this
operation is "destructive" (every item in a profile is simply set as an
option, overwriting the previous value), you can't just enable and disable
profiles again.

As a partial remedy, there is a way to make profiles save old option values
before overwriting them with the profile values, and then restoring the old
values at a later point using ``apply-profile <profile-name> restore``.

This can be enabled with the ``profile-restore`` option, which takes one of
the following options:

    ``default``
        Does nothing, and nothing can be restored (default).

    ``copy``
        When applying a profile, copy the old values of all profile options to a
        backup before setting them from the profile. These options are reset to
        their old values using the backup when restoring.

        Every profile has its own list of backed up values. If the backup
        already exists (e.g. if ``apply-profile name`` was called more than
        once in a row), the existing backup is no changed. The restore operation
        will remove the backup.

        It's important to know that restoring does not "undo" setting an option,
        but simply copies the old option value. Consider for example ``vf-add``,
        appends an entry to ``vf``. This mechanism will simply copy the entire
        ``vf`` list, and does _not_ execute the inverse of ``vf-add`` (that
        would be ``vf-remove``) on restoring.

        Note that if a profile contains recursive profiles (via the ``profile``
        option), the options in these recursive profiles are treated as if they
        were part of this profile. The referenced profile's backup list is not
        used when creating or using the backup. Restoring a profile does not
        restore referenced profiles, only the options of referenced profiles (as
        if they were part of the main profile).

    ``copy-equal``
        Similar to ``copy``, but restore an option only if it has the same value
        as the value effectively set by the profile. This tries to deal with
        the situation when the user does not want the option to be reset after
        interactively changing it.

.. admonition:: Example

    ::

        [something]
        profile-restore=copy-equal
        vf-add=rotate=PI/2  # rotate by 90 degrees

    Then running these commands will result in behavior as commented:

    ::

        set vf vflip
        apply-profile something
        vf add hflip
        apply-profile something
        # vf == vflip,rotate=PI/2,hflip,rotate=PI/2
        apply-profile something restore
        # vf == vflip

Conditional auto profiles
-------------------------

Profiles which have the ``profile-cond`` option set are applied automatically
if the associated condition matches (unless auto profiles are disabled). The
option takes a string, which is interpreted as Lua expression. If the
expression evaluates as truthy, the profile is applied. If the expression
errors or evaluates as falsy, the profile is not applied. This Lua code
execution is not sandboxed.

Any variables in condition expressions can reference properties. If an
identifier is not already defined by Lua or mpv, it is interpreted as property.
For example, ``pause`` would return the current pause status. You cannot
reference properties with ``-`` this way since that would denote a subtraction,
but if the variable name contains any ``_`` characters, they are turned into
``-``. For example, ``playback_time`` would return the property
``playback-time``.

A more robust way to access properties is using ``p.property_name`` or
``get("property-name", default_value)``. The automatic variable to property
magic will break if a new identifier with the same name is introduced (for
example, if a function named ``pause()`` were added, ``pause`` would return a
function value instead of the value of the ``pause`` property).

Note that if a property is not available, it will return ``nil``, which can
cause errors if used in expressions. These are logged in verbose mode, and the
expression is considered to be false.

Whenever a property referenced by a profile condition changes, the condition
is re-evaluated. If the return value of the condition changes from falsy or
error to truthy, the profile is applied.

This mechanism tries to "unapply" profiles once the condition changes from
truthy to falsy or error. If you want to use this, you need to set
``profile-restore`` for the profile. Another possibility it to create another
profile with an inverse condition to undo the other profile.

Recursive profiles can be used. But it is discouraged to reference other
conditional profiles in a conditional profile, since this can lead to tricky
and unintuitive behavior.

.. admonition:: Example

    Make only HD video look funny:

    ::

        [something]
        profile-desc=HD video sucks
        profile-cond=width >= 1280
        hue=-50

    Make only videos containing "youtube" or "youtu.be" in their path brighter:

    ::

        [youtube]
        profile-cond=path:find('youtu%.?be')
        gamma=20

    If you want the profile to be reverted if the condition goes to false again,
    you can set ``profile-restore``:

    ::

        [something]
        profile-desc=Mess up video when entering fullscreen
        profile-cond=fullscreen
        profile-restore=copy
        vf-add=rotate=PI/2  # rotate by 90 degrees

    This appends the ``rotate`` filter to the video filter chain when entering
    fullscreen. When leaving fullscreen, the ``vf`` option is set to the value
    it had before entering fullscreen. Note that this would also remove any
    other filters that were added during fullscreen mode by the user. Avoiding
    this is trickier, and could for example be solved by adding a second profile
    with an inverse condition and operation:

    ::

        [something]
        profile-cond=fullscreen
        vf-add=@rot:rotate=PI/2

        [something-inv]
        profile-cond=not fullscreen
        vf-remove=@rot

.. warning::

    Every time an involved property changes, the condition is evaluated again.
    If your condition uses ``p.playback_time`` for example, the condition is
    re-evaluated approximately on every video frame. This is probably slow.

This feature is managed by an internal Lua script. Conditions are executed as
Lua code within this script. Its environment contains at least the following
things:

``(function environment table)``
    Every Lua function has an environment table. This is used for identifier
    access. There is no named Lua symbol for it; it is implicit.

    The environment does "magic" accesses to mpv properties. If an identifier
    is not already defined in ``_G``, it retrieves the mpv property of the same
    name. Any occurrences of ``_`` in the name are replaced with ``-`` before
    reading the property. The returned value is as retrieved by
    ``mp.get_property_native(name)``. Internally, a cache of property values,
    updated by observing the property is used instead, so properties that are
    not observable will be stuck at the initial value forever.

    If you want to access properties, that actually contain ``_`` in the name,
    use ``get()`` (which does not perform transliteration).

    Internally, the environment table has a ``__index`` meta method set, which
    performs the access logic.

``p``
    A "magic" table similar to the environment table. Unlike the latter, this
    does not prefer accessing variables defined in ``_G`` - it always accesses
    properties.

``get(name [, def])``
    Read a property and return its value. If the property value is ``nil`` (e.g.
    if the property does not exist), ``def`` is returned.

    This is superficially similar to ``mp.get_property_native(name)``. An
    important difference is that this accesses the property cache, and enables
    the change detection logic (which is essential to the dynamic runtime
    behavior of auto profiles). Also, it does not return an error value as
    second return value.

    The "magic" tables mentioned above use this function as backend. It does not
    perform the ``_`` transliteration.

In addition, the same environment as in a blank mpv Lua script is present. For
example, ``math`` is defined and gives access to the Lua standard math library.

.. warning::

    This feature is subject to change indefinitely. You might be forced to
    adjust your profiles on mpv updates.

Legacy auto profiles
--------------------

Some profiles are loaded automatically using a legacy mechanism. The following
example demonstrates this:

.. admonition:: Auto profile loading

    ::

        [extension.mkv]
        profile-desc="profile for .mkv files"
        vf=vflip

The profile name follows the schema ``type.name``, where type can be
``protocol`` for the input/output protocol in use (see ``--list-protocols``),
and ``extension`` for the extension of the path of the currently played file
(*not* the file format).

This feature is very limited, and is considered soft-deprecated. Use conditional
auto profiles.

Using mpv from other programs or scripts
========================================

There are three choices for using mpv from other programs or scripts:

    1. Calling it as UNIX process. If you do this, *do not parse terminal output*.
       The terminal output is intended for humans, and may change any time. In
       addition, terminal behavior itself may change any time. Compatibility
       cannot be guaranteed.

       Your code should work even if you pass ``--terminal=no``. Do not attempt
       to simulate user input by sending terminal control codes to mpv's stdin.
       If you need interactive control, using ``--input-ipc-server`` or
       ``--input-ipc-client`` is recommended. This gives you access to the
       `JSON IPC`_  over unix domain sockets (or named pipes on Windows).

       Depending on what you do, passing ``--no-config`` or ``--config-dir`` may
       be a good idea to avoid conflicts with the normal mpv user configuration
       intended for CLI playback.

       Using ``--input-ipc-server`` or ``--input-ipc-client`` is also suitable for
       purposes like remote control (however, the IPC protocol itself is not
       "secure" and not intended to be so).

    2. Using libmpv. This is generally recommended when mpv is used as playback
       backend for a completely different application. The provided C API is
       very close to CLI mechanisms and the scripting API.

       Note that even though libmpv has different defaults, it can be configured
       to work exactly like the CLI player (except command line parsing is
       unavailable).

       See `EMBEDDING INTO OTHER PROGRAMS (LIBMPV)`_.

    3. As a user script (`LUA SCRIPTING`_, `JAVASCRIPT`_, `C PLUGINS`_). This is
       recommended when the goal is to "enhance" the CLI player. Scripts get
       access to the entire client API of mpv.

       This is the standard way to create third-party extensions for the player.

All these access the client API, which is the sum of the various mechanisms
provided by the player core, as documented here: `OPTIONS`_,
`List of Input Commands`_, `Properties`_, `List of events`_ (also see C API),
`Hooks`_.

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
- The total file duration (absent if unknown) (``duration`` property)
- Playback speed, e.g. ``x2.0``. Only visible if the speed is not normal. This
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
  frame couldn't be displayed on time. (``frame-drop-count`` property.)
  If the decoder drops frames, the number of decoder-dropped frames is appended
  to the display as well, e.g.: ``Dropped: 4/34``. This happens only if
  decoder frame dropping is enabled with the ``--framedrop`` options.
  (``decoder-frame-drop-count`` property.)
- Cache state, e.g. ``Cache:  2s/134KB``. Visible if the stream cache is enabled.
  The first value shows the amount of video buffered in the demuxer in seconds,
  the second value shows the estimated size of the buffered amount in kilobytes.
  (``demuxer-cache-duration`` and ``demuxer-cache-state`` properties.)


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
framerate. Using ``--untimed`` or ``--correct-pts=no --container-fps-override=60``
might help.

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

RESUMING PLAYBACK
=================

mpv is capable of storing the playback position of the currently playing file
and resume from there the next time that file is played. This is done with the
commands ``quit-watch-later`` (bound to Shift+Q by default) and
``write-watch-later-config``, and with the ``--save-position-on-quit`` option.

The difference between always quitting with a key bound to ``quit-watch-later``
and using ``--save-position-on-quit`` is that the latter will save the playback
position even when mpv is closed with a method other than a keybinding, such as
clicking the close button in the window title bar. However if mpv is terminated
abruptly and doesn't have the time to save, then the position will not be saved.
For example, if you shutdown your system without closing mpv beforehand.

mpv also stores options other than the playback position when they have been
modified after playback began, for example the volume and selected audio/subtitles,
and restores their values the next time the file is played. Which options are
saved can be configured with the ``--watch-later-options`` option.

When playing multiple playlist entries, mpv checks if one them has a resume
config file associated, and if it finds one it restarts playback from it. For
example, if you use ``quit-watch-later`` on the 5th episode of a show, and
later play all the episodes, mpv will automatically resume playback from
episode 5.

More options to configure this functionality are listed in `Watch Later`_.

PROTOCOLS
=========

``http://...``, ``https://``, ...

    Many network protocols are supported, but the protocol prefix must always
    be specified. mpv will never attempt to guess whether a filename is
    actually a network address. A protocol prefix is always required.

    Note that not all prefixes are documented here. Undocumented prefixes are
    either aliases to documented protocols, or are just redirections to
    protocols implemented and documented in FFmpeg.

    ``data:`` is supported, but needs to be in the format ``data://``.
    This is done to avoid ambiguity with filenames. You
    can also prefix it with ``lavf://`` or ``ffmpeg://``.

``ytdl://...``

    By default, the youtube-dl hook script only looks at http(s) URLs. Prefixing
    an URL with ``ytdl://`` forces it to be always processed by the script. This
    can also be used to invoke special youtube-dl functionality like playing a
    video by ID or invoking search.

    Keep in mind that you can't pass youtube-dl command line options by this,
    and you have to use ``--ytdl-raw-options`` instead.

``-``

    Play data from stdin.

``smb://PATH``

    Play a path from  Samba share. (Requires FFmpeg support.)

``bd://[title][/device]`` ``--bluray-device=PATH``

    Play a Blu-ray disc. Since libbluray 1.0.1, you can read from ISO files
    by passing them to ``--bluray-device``.

    ``title`` can be: ``longest`` or ``first`` (selects the default
    playlist); ``mpls/<number>`` (selects <number>.mpls playlist);
    ``<number>`` (select playlist with the same index). mpv will list
    the available playlists on loading.

    ``bluray://`` is an alias.

``dvd://[title][/device]`` ``--dvd-device=PATH``

    Play a DVD. DVD menus are not supported. If no title is given, the longest
    title is auto-selected. Without ``--dvd-device``, it will probably try
    to open an actual optical drive, if available and implemented for the OS.

    ``dvdnav://`` is an old alias for ``dvd://`` and does exactly the same
    thing.

``dvb://[cardnumber@]channel`` ``--dvbin-...``

    Digital TV via DVB. (Linux only.)

``mf://[@listfile|filemask|glob|printf-format]`` ``--mf-...``

    Play a series of images as video.

    If the URL path begins with ``@``, it is interpreted as the path to a file
    containing a list of image paths separated by newlines. If the URL path
    contains ``,``, it is interpreted as a list of image paths separated by
    ``,``. If the URL path does not contain ``%`` and if on POSIX platforms, is
    interpreted as a glob, and ``*`` is automatically appended if it was not
    specified. Otherwise, the printf sequences ``%[.][NUM]d``, where ``NUM`` is
    one, two, or three decimal digits, and ``%%`` and are interpreted. For
    example, ``mf://image-%d.jpg`` plays files like ``image-1.jpg``,
    ``image-2.jpg`` and ``image-10.jpg``, provided that there are no big gaps
    between the files.

``cdda://[device]`` ``--cdrom-device=PATH`` ``--cdda-...``

    Play CD.

``lavf://...``

    Access any FFmpeg libavformat protocol. Basically, this passed the
    string after the ``//`` directly to libavformat.

``av://type:options``

    This is intended for using libavdevice inputs. ``type`` is the libavdevice
    demuxer name, and ``options`` is the (pseudo-)filename passed to the
    demuxer.

    .. admonition:: Example

        ::

            mpv av://v4l2:/dev/video0 --profile=low-latency --untimed

        This plays video from the first v4l input with nearly the lowest latency
        possible. It's a good replacement for the removed ``tv://`` input.
        Using ``--untimed`` is a hack to output a captured frame immediately,
        instead of respecting the input framerate. (There may be better ways to
        handle this in the future.)

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

``slice://start[-end]@URL``

    Read a slice of a stream.

    ``start`` and ``end`` represent a byte range and accept
    suffixes such as ``KiB`` and ``MiB``. ``end`` is optional.

    if ``end`` starts with ``+``, it is considered as offset from ``start``.

    Only works with seekable streams.

    Examples::

      mpv slice://1g-2g@cap.ts

      This starts reading from cap.ts after seeking 1 GiB, then
      reads until reaching 2 GiB or end of file.

      mpv slice://1g-+2g@cap.ts

      This starts reading from cap.ts after seeking 1 GiB, then
      reads until reaching 3 GiB or end of file.

      mpv slice://100m@appending://cap.ts

      This starts reading from cap.ts after seeking 100MiB, then
      reads until end of file.

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
- started out of the bundle on macOS
- if you manually use ``--player-operation-mode=pseudo-gui`` on the command line

This mode applies options from the builtin profile ``builtin-pseudo-gui``, but
only if these haven't been set in the user's config file or on the command line,
which is the main difference to using ``--profile=builtin-pseudo-gui``.

The profile is currently defined as follows:

::

    [builtin-pseudo-gui]
    terminal=no
    force-window=yes
    idle=once
    screenshot-directory=~~desktop/

The ``pseudo-gui`` profile exists for compatibility. The options in the
``pseudo-gui`` profile are applied unconditionally. In addition, the profile
makes sure to enable the pseudo-GUI mode, so that ``--profile=pseudo-gui``
works like in older mpv releases:

::

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

.. include:: console.rst

.. include:: select.rst

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

``MPV_HOME``
    Directory where mpv looks for user settings. Overrides ``HOME``, and mpv
    will try to load the config file as ``$MPV_HOME/mpv.conf``.

``MPV_VERBOSE`` (see also ``-v`` and ``--msg-level``)
    Set the initial verbosity level across all message modules (default: 0).
    This is an integer, and the resulting verbosity corresponds to the number
    of ``--v`` options passed to the command line.

``MPV_LEAK_REPORT``
    If set to ``1``, enable internal talloc leak reporting. If set to another
    value, disable leak reporting.

``LADSPA_PATH``
    Specifies the search path for LADSPA plugins. If it is unset, fully
    qualified path names must be used.

``DISPLAY``
    Standard X11 display name to use.

FFmpeg:
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

Note that this section assumes Linux/BSD. On other platforms the paths may be different.
For Windows-specifics, see `FILES ON WINDOWS`_ section.

All configuration files should be encoded in UTF-8.

``/usr/local/etc/mpv/mpv.conf``
    mpv system-wide settings (depends on ``--prefix`` passed to configure - mpv
    in default configuration will use ``/usr/local/etc/mpv/`` as config
    directory, while most Linux distributions will set it to ``/etc/mpv/``).

``~/.cache/mpv``
    The standard cache directory. Certain options within mpv may cause it to write
    cache files to disk. This can be overridden by environment variables, in
    ascending order:

    :1: If ``$XDG_CACHE_HOME`` is set, then the derived cache directory
        will be ``$XDG_CACHE_HOME/mpv``.
    :2: If ``$MPV_HOME`` is set, then the derived cache directory will be
       ``$MPV_HOME``.

    If the directory does not exist, mpv will try to create it automatically.

``~/.config/mpv``
    The standard configuration directory. This can be overridden by environment
    variables, in ascending order:

    :1: If ``$XDG_CONFIG_HOME`` is set, then the derived configuration directory
        will be ``$XDG_CONFIG_HOME/mpv``.
    :2: If ``$MPV_HOME`` is set, then the derived configuration directory will be
       ``$MPV_HOME``.

    If this directory, nor the original configuration directory (see below) do
    not exist, mpv tries to create this directory automatically.

``~/.mpv/``
    The original (pre 0.5.0) configuration directory. It will continue to be
    read if present. If this directory is present and the standard configuration
    directory is not present, then cache files and watch later config files will
    also be written to this directory.

    If both this directory and the standard configuration directory are
    present, configuration will be read from both with the standard
    configuration directory content taking precedence. However, you should
    fully migrate to the standard directory and a warning will be shown in
    this situation.

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
    Default location for ``--sub-fonts-dir`` (see `Subtitles`_) and
    ``--osd-fonts-dir`` (see `OSD`_).

``~/.config/mpv/scripts/``
    All files in this directory are loaded as if they were passed to the
    ``--script`` option. They are loaded in alphabetical order.

    The ``--load-scripts=no`` option disables loading these files.

    See `Script location`_ for details.

``~/.local/state/mpv/watch_later/``
    Contains temporary config files needed for resuming playback of files with
    the watch later feature. See for example the ``Q`` key binding, or the
    ``quit-watch-later`` input command.

    This can be overridden by environment variables, in ascending order:

    :1: If ``$XDG_STATE_HOME`` is set, then the derived watch later directory
        will be ``$XDG_STATE_HOME/mpv/watch_later``.
    :2: If ``$MPV_HOME`` is set, then the derived watch later directory will be
       ``$MPV_HOME/watch_later``.

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

The cache directory is located at ``%LOCALAPPDATA%/mpv/cache``.

The watch_later directory is located at ``%LOCALAPPDATA%/mpv/watch_later``.

The environment variable ``$MPV_HOME`` completely overrides these, like on
UNIX.

If a directory named ``portable_config`` next to the mpv.exe exists, all
config will be loaded from this directory only. Watch later config files and
cache files are written to this directory as well. (This exists on Windows
only and is redundant with ``$MPV_HOME``. However, since Windows is very
scripting unfriendly, a wrapper script just setting ``$MPV_HOME``, like you
could do it on other systems, won't work. ``portable_config`` is provided for
convenience to get around this restriction.)

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

FILES ON MACOS
==============

On macOS the watch later directory is located at ``~/.config/mpv/watch_later/``
and the cache directory is set to ``~/Library/Caches/io.mpv/``. These directories
can't be overwritten by environment variables.
Everything else is the same as `FILES`_.
