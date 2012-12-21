Format of mplayer2 EDL files
============================

The first line in the file must be ``mplayer EDL file, version 2``.
The rest of the lines belong to one of these classes:

1) lines specifying source files
2) empty lines
3) lines specifying timeline segments.

Lines beginning with ``<`` specify source files. These lines first
contain an identifier used to refer to the source file later, then the
filename separated by whitespace. The identifier must start with a
letter. Filenames that start or end with whitespace or contain
newlines are not supported.

On other lines ``#`` characters delimit comments. Lines that contain
only whitespace after comments have been removed are ignored.

Timeline segments must appear in the file in chronological order. Each
segment has the following information associated with it:

- duration
- output start time
- output end time (= output start time + duration)
- source id (specifies the file the content of the segment comes from)
- source start time (timestamp in the source file)
- source end time (= source start time + duration)

The output timestamps must form a continuous timeline from 0 to the
end of the last segment, such that each new segment starts from the
time the previous one ends at. Source files and times may change
arbitrarily between segments.

The general format for lines specifying timeline segments is
[output time info] source_id [source time info]
source_id must be an identifier defined on a ``<`` line. Both the time
info parts consists of zero or more of the following elements:

1) ``timestamp``
2) ``-timestamp``
3) ``+duration``
4) ``*``
5) ``-*``

, where ``timestamp`` and ``duration`` are decimal numbers (computations
are done with nanosecond precision). Whitespace around ``+`` and ``-`` is
optional. 1) and 2) specify start and end time of the segment on
output or source side. 3) specifies duration; the semantics are the
same whether this appears on output or source side. 4) and 5) are
ignored on the output side (they're always implicitly assumed). On the
source side 4) specifies that the segment starts where the previous
segment *using this source* ended; if there was no previous segment
time 0 is used. 5) specifies that the segment ends where the next
segment using this source starts.

Redundant information may be omitted. It will be filled in using the
following rules:

- output start for first segment is 0
- two of [output start, output end, duration] imply third
- two of [source start, source end, duration] imply third
- output start = output end of previous segment
- output end = output start of next segment
- if ``*``, source start = source end of earlier segment
- if ``-*``, source end = source start of a later segment

As a special rule, a last zero-duration segment without a source
specification may appear. This will produce no corresponding segment
in the resulting timeline, but can be used as syntax to specify the
end time of the timeline (with effect equal to adding -time on the
previous line).

Examples::

    mplayer EDL file, version 2
    < id1 filename

    0 id1 123
    100 id1 456
    200 id1 789
    300

All segments come from the source file ``filename``. First segment
(output time 0-100) comes from time 123-223, second 456-556, third
789-889.

::

    mplayer EDL file, version 2
    < f filename
    f  60-120
    f 600-660
    f  30- 90

Play first seconds 60-120 from the file, then 600-660, then 30-90.

::

    mplayer EDL file, version 2
    < id1 filename1
    < id2 filename2

    +10 id1 *
    +10 id2 *
    +10 id1 *
    +10 id2 *
    +10 id1 *
    +10 id2 *

This plays time 0-10 from filename1, then 0-10 from filename1, then
10-20 from filename1, then 10-20 from filename2, then 20-30 from
filename1, then 20-30 from filename2.

::

    mplayer EDL file, version 2
    < t1 filename1
    < t2 filename2

    t1 * +2            # segment 1
    +2 t2 100          # segment 2
    t1 *               # segment 3
    t2 *-*             # segment 4
    t1 3 -*            # segment 5
    +0.111111 t2 102.5 # segment 6
    7.37 t1 5 +1       # segment 7

This rather pathological example illustrates the rules for filling in
implied data. All the values can be determined by recursively applying
the rules given above, and the full end result is this::

    +2         0-2                 t1  0-2              # segment 1
    +2         2-4                 t2  100-102          # segment 2
    +0.758889  4-4.758889          t1  2-2.758889       # segment 3
    +0.5       4.4758889-5.258889  t2  102-102.5        # segment 4
    +2         5.258889-7.258889   t1  3-5              # segment 5
    +0.111111  7.258889-7.37       t2  102.5-102.611111 # segment 6
    +1         7.37-8.37           t1  5-6              # segment 7
