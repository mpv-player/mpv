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
ytdl requirements. Using ``no_clip`` with segments is not recommended and
probably breaks. ``mp4_dash`` already implicitly does a variant of ``no_clip``.

The ``mp4_dash`` and ``no_clip`` headers are not part of the core EDL format.
They may be changed or removed at any time, depending on mpv's internal
requirements.

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

The ``new_stream`` header is not part of the core EDL format. It may be changed
or removed at any time, depending on mpv's internal requirements.

If the first ``!new_stream`` is redundant, it is ignored. This is the same
example as above::

    # mpv EDL v0
    !new_stream
    video.mkv
    !new_stream
    audio.mkv

Note that ``!new_stream`` must be the first header. Whether the parser accepts
(i.e. ignores) or rejects other headers before that is implementation specific.

Track metadata
==============

The special ``track_meta`` header can set some specific metadata fields of the
current ``!new_stream`` partition. The tags are applied to all tracks within
the partition. It is not possible to set the metadata for individual tracks (the
feature was needed only for single-track media).

It provides following parameters change track metadata:

``lang``
    Set the language tag.

``title``
    Set the title tag.

``byterate``
    Number of bytes per second this stream uses. (Purely informational.)

``index``
    The numeric index of the track this should map to (default: -1). This is
    the 0-based index of the virtual stream as seen by the player, enumerating
    all audio/video/subtitle streams. If nothing matches, this is silently
    discarded. The special index -1 (the default) has two meanings: if there
    was a previous meta data entry (either ``!track_meta`` or ``!delay_open``
    element since the last ``!new_stream``), then this element manipulates
    the previous meta data entry. If there was no previous entry, a new meta
    data entry that matches all streams is created.

Example::

    # mpv EDL v0
    !track_meta,lang=bla,title=blabla
    file.mkv
    !new_stream
    !track_meta,title=ducks
    sub.srt

If ``file.mkv`` has an audio and a video stream, both will use ``blabla`` as
title. The subtitle stream will use ``ducks`` as title.

The ``track_meta`` header is not part of the core EDL format. It may be changed
or removed at any time, depending on mpv's internal requirements.

Global metadata
===============

The special ``global_tags`` header can set metadata fields (aka tags) of the EDL
file. This metadata is supposed to be informational, much like for example ID3
tags in audio files. Due to lack of separation of different kinds of metadata it
is unspecified what names are allowed, how they are interpreted, and whether
some of them affect playback functionally. (Much of this is unfortunately
inherited from FFmpeg. Another consequence of this is that FFmpeg "normalized"
tags are recognized, or stuff like replaygain tags.)

Example::

    !global_tags,title=bla,something_arbitrary=even_more_arbitrary

Any parameter names are allowed. Repeated use of this adds to the tag list. If
``!new_stream`` is used, the location doesn't matter.

May possibly be ignored in some cases, such as delayed media opening.

Delayed media opening
=====================

The special ``delay_open`` header can be used to open the media URL of the
stream only when the track is selected for the first time. This is supposed to
be an optimization to speed up opening of a remote stream if there are many
tracks for whatever reasons.

This has various tricky restrictions, and also will defer failure to open a
stream to "later". By design, it's supposed to be used for single-track streams.

Using multiple segments requires you to specify all offsets and durations (also
it was never tested whether it works at all). Interaction with ``mp4_dash`` may
be strange.

You can describe multiple sub-tracks by using multiple ``delay_open`` headers
before the same source URL. (If there are multiple sub-tracks of the same media
type, then the mapping to the real stream is probably rather arbitrary.) If the
source contains tracks not described, a warning is logged when the delayed
opening happens, and the track is hidden.

This has the following parameters:

``media_type``
    Required. Must be set to ``video``, ``audio``, or ``sub``. (Other tracks in
    the opened URL are ignored.)

``codec``
    The mpv codec name that is expected. Although mpv tries to initialize a
    decoder with it currently (and will fail track selection if it does not
    initialize successfully), it is not used for decoding - decoding still uses
    the information retrieved from opening the actual media information, and may
    be a different codec (you should try to avoid this, of course). Defaults to
    ``null``.

    Above also applies for similar fields such as ``w``.  These fields are
    mostly to help with user track pre-selection.

``flags``
    A ``+`` separated list of boolean flags. Currently defined flags:

        ``default``
            Set the default track flag.

        ``forced``
            Set the forced track flag.

    Other values are ignored after triggering a warning.

``w``, ``h``
    For video codecs: expected video size. See ``codec`` for details.

``fps``
    For video codecs: expected video framerate, as integer. (The rate is usually
    only crudely reported, and it makes no sense to expect exact values.)

``samplerate``
    For audio codecs: expected sample rate, as integer.

The ``delay_open`` header is not part of the core EDL format. It may be changed
or removed at any time, depending on mpv's internal requirements.

Timestamp format
================

Currently, time values are floating point values in seconds.

As an extension, you can set the ``timestamps=chapters`` option. If this option
is set, timestamps have to be integers, and refer to chapter numbers, starting
with 0. The default value for this parameter is ``seconds``, which means the
time is as described in the previous paragraph.

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

Entry which defines the track layout
====================================

Normally, you're supposed to put only files with compatible layouts into an EDL
file. However, at least the mpv implementation accepts entries that use
different codecs, or even have a different number of audio/video/subtitle
tracks. In this case, it's not obvious, which virtual tracks the EDL show should
expose when being played.

Currently, mpv will apply an arbitrary heuristic which tracks the EDL file
should expose. (Before mpv 0.30.0, it always used the first source file in the
segment list.)

You can set the ``layout`` option to ``this`` to make a specific entry define
the track layout.

Example::

    # mpv EDL v0
    file_with_2_streams.ts,5,240
    file_with_5_streams.mkv,0,90,layout=this

The way the different virtual EDL tracks are associated with the per-segment
ones is highly implementation-defined, and uses a heuristic. If a segment is
missing a track, there will be a "hole", and bad behavior may result. Improving
this is subject to further development (due to being fringe cases, they don't
have a high priority).

If future versions of mpv change this again, this option may be ignored.

Syntax of EDL URIs
==================

mpv accepts inline EDL data in form of ``edl://`` URIs. Other than the
header, the syntax is exactly the same. It's far more convenient to use ``;``
instead of line breaks, but that is orthogonal.

Example: ``edl://f1.mkv,length=5,start=10;f2.mkv,30,20;f3.mkv``
