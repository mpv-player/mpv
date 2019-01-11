EDL files
=========

EDL files basically concatenate ranges of video/audio from multiple source
files into a single continuous virtual file. Each such range is called a
segment, and consists of source file, source offset, and segment length.

For example::

    # mpv EDL v0
    f1.mkv,10,20
    f2.mkv
    f1.mkv,40,10

This would skip the first 10 seconds of the file f1.mkv, then play the next
20 seconds, then switch to the file f2.mkv and play all of it, then switch
back to f1.mkv, skip to the 40 second mark, and play 10 seconds, and then
stop playback. The difference to specifying the files directly on command
line (and using ``--{ --start=10 --length=20 f1.mkv --}`` etc.) is that the
virtual EDL file appears as a virtual timeline (like a single file), instead
as a playlist.

The general simplified syntax is::

    # mpv EDL v0
    <filename>
    <filename>,<start in seconds>,<length in seconds>

If the start time is omitted, 0 is used. If the length is omitted, the
estimated remaining duration of the source file is used.

Note::

    Usage of relative or absolute paths as well as any protocol prefixes may be
    prevented for security reasons.


Syntax of mpv EDL files
=======================

Generally, the format is relatively strict. No superfluous whitespace (except
empty lines and commented lines) are allowed. You must use UNIX line breaks.

The first line in the file must be ``# mpv EDL v0``. This designates that the
file uses format version 0, which is not frozen yet and may change any time.
(If you need a stable EDL file format, make a feature request. Likewise, if
you have suggestions for improvements, it's not too late yet.)

The rest of the lines belong to one of these classes:

1) An empty or commented line. A comment starts with ``#``, which must be the
   first character in the line. The rest of the line (up until the next line
   break) is ignored. An empty line has 0 bytes between two line feed bytes.
2) A header entry if the line starts with ``!``.
3) A segment entry in all other cases.

Each segment entry consists of a list of named or unnamed parameters.
Parameters are separated with ``,``. Named parameters consist of a name,
followed by ``=``, followed by the value. Unnamed parameters have only a
value, and the name is implicit from the parameter position.

Syntax::

    segment_entry ::= <param> ( <param> ',' )*
    param         ::= [ <name> '=' ] ( <value> | '%' <number> '%' <valuebytes> )

The ``name`` string can consist of any characters, except ``=%,;\n!``. The
``value`` string can consist of any characters except of ``,;\n!``.

The construct starting with ``%`` allows defining any value with arbitrary
contents inline, where ``number`` is an integer giving the number of bytes in
``valuebytes``. If a parameter value contains disallowed characters, it has to
be guarded by a length specifier using this syntax.

The parameter name defines the meaning of the parameter:

1) ``file``, the source file to use for this segment.
2) ``start``, a time value that specifies the start offset into the source file.
3) ``length``, a time value that specifies the length of the segment.

See the section below for the format of timestamps.

Unnamed parameters carry implicit names. The parameter position determines
which of the parameters listed above is set. For example, the second parameter
implicitly uses the name ``start``.

Example::

    # mpv EDL v0
    %18%filename,with,.mkv,10,length=20,param3=%13%value,escaped,param4=value2

this sets ``file`` to ``filename,with,.mkv``, ``start`` to ``10``, ``length``
to ``20``, ``param3`` to ``value,escaped``, ``param4`` to ``value2``.

Instead of line breaks, the character ``;`` can be used. Line feed bytes and
``;`` are treated equally.

Header entries start with ``!`` as first character after a line break. Header
entries affect all other file entries in the EDL file. Their format is highly
implementation specific. They should generally follow the file header, and come
before any file entries.

Disabling chapter generation and copying
========================================

By default, chapters from the source ranges are copied to the virtual file's
chapters. Also, a chapter is inserted after each range. This can be disabled
with the ``no_chapters`` header.

Example::

    !no_chapters


MP4 DASH
========

This is a header that helps implementing DASH, although it only provides a low
level mechanism.

If this header is set, the given url designates an mp4 init fragment. It's
downloaded, and every URL in the EDL is prefixed with the init fragment on the
byte stream level. This is mostly for use by mpv's internal ytdl support. The
ytdl script will call youtube-dl, which in turn actually processes DASH
manifests. It may work only for this very specific purpose and fail to be
useful in other scenarios. It can be removed or changed in incompatible ways
at any times.

Example::

    !mp4_dash,init=url

The ``url`` is encoded as parameter value as defined in the general EDL syntax.
It's expected to point to an "initialization fragment", which will be prefixed
to every entry in the EDL on the byte stream level.

The current implementation will

- ignore stream start times
- use durations as hint for seeking only
- not adjust source timestamps
- open and close segments (i.e. fragments) as needed
- not add segment boundaries as chapter points
- require full compatibility between all segments (same codec etc.)

Another header part of this mechanism is ``no_clip``. This header is similar
to ``mp4_dash``, but does not include on-demand opening/closing of segments,
and does not support init segments. It also exists solely to support internal
ytdl requirements.

Separate files for tracks
=========================

The special ``new_stream`` header lets you specify separate parts and time
offsets for separate tracks. This can for example be used to source audio and
video track from separate files.

Example::

    # mpv EDL v0
    video.mkv
    !new_stream
    audio.mkv

This adds all tracks from both files to the virtual track list. Upon playback,
the tracks will be played at the same time, instead of appending them. The files
can contain more than 1 stream; the apparent effect is the same as if the second
part after the ``!new_stream`` part were in a separate ``.edl`` file and added
with ``--external-file``.

Note that all metadata between the stream sets created by ``new_stream`` is
disjoint. Global metadata is taken from the first part only.

In context of mpv, this is redundant to the ``--audio-file`` and
``--external-file`` options, but (as of this writing) has the advantage that
this will use a unified cache for all streams.

Timestamp format
================

Currently, time values are floating point values in seconds.

As an extension, you can set the ``timestamps=chapters`` option. If this option
is set, timestamps have to be integers, and refer to chapter numbers, starting
with 0.

Example::

    # mpv EDL v0
    file.mkv,2,4,timestamps=chapters

Plays chapter 3 and ends with the start of chapter 7 (4 chapters later).

Implicit chapters
=================

mpv will add one chapter per segment entry to the virtual timeline.

By default, the chapter's titles will match the entries' filenames.
You can override set the ``title`` option to override the chapter title for
that segment.

Example::

    # mpv EDL v0
    cap.ts,5,240
    OP.mkv,0,90,title=Show Opening

The virtual timeline will have two chapters, one called "cap.ts" from 0-240s
and a second one called "Show Opening" from 240-330s.

Syntax of EDL URIs
==================

mpv accepts inline EDL data in form of ``edl://`` URIs. Other than the
header, the syntax is exactly the same. It's far more convenient to use ``;``
instead of line breaks, but that is orthogonal.

Example: ``edl://f1.mkv,length=5,start=10;f2.mkv,30,20;f3.mkv``

As a quirks, mpv will accept arbitrary paths in EDLs originating from
``edl://``, while ``.edl`` does not. This makes no sense.
