ENCODING
========

You can encode files from one format/codec to another using this facility.

``--o=<filename>``
    Enables encoding mode and specifies the output file name.

``--of=<format>``
    Specifies the output format (overrides autodetection by the file name
    extension of the file specified by ``-o``). This can be a comma separated
    list of possible formats to try. See ``--of=help`` for a full list of
    supported formats.

``--ofopts=<options>``
    Specifies the output format options for libavformat.
    See ``--ofopts=help`` for a full list of supported options.

    Options are managed in lists. There are a few commands to manage the
    options list.

    ``--ofopts-add=<options1[,options2,...]>``
        Appends the options given as arguments to the options list.

    ``--ofopts-pre=<options1[,options2,...]>``
        Prepends the options given as arguments to the options list.

    ``--ofopts-del=<index1[,index2,...]>``
        Deletes the options at the given indexes. Index numbers start at 0,
        negative numbers address the end of the list (-1 is the last).

    ``--ofopts-clr``
        Completely empties the options list.

``--ofps=<float value>``
    Specifies the output format time base (default: 24000). Low values like 25
    limit video fps by dropping frames.

``--oautofps``
    Sets the output format time base to the guessed frame rate of the input
    video (simulates MEncoder behavior, useful for AVI; may cause frame drops).
    Note that not all codecs and not all formats support VFR encoding, and some
    which do have bugs when a target bitrate is specified - use ``--ofps`` or
    ``--oautofps`` to force CFR encoding in these cases.

``--omaxfps=<float value>``
    Specifies the minimum distance of adjacent frames (default: 0, which means
    unset). Content of lower frame rate is not readjusted to this frame rate;
    content of higher frame rate is decimated to this frame rate.

``--oharddup``
    If set, the frame rate given by ``--ofps`` is attained not by skipping time
    codes, but by duplicating frames (constant frame rate mode).

``--oneverdrop``
    If set, frames are never dropped. Instead, time codes of video are
    readjusted to always increase. This may cause AV desync, though; to work
    around this, use a high-fps time base using ``--ofps`` and absolutely
    avoid ``--oautofps``.

``--oac=<codec>``
    Specifies the output audio codec. This can be a comma separated list of
    possible codecs to try. See ``--oac=help`` for a full list of supported
    codecs.

``--oaoffset=<value>``
    Shifts audio data by the given time (in seconds) by adding/removing
    samples at the start.

``--oacopts=<options>``
    Specifies the output audio codec options for libavcodec.
    See ``--oacopts=help`` for a full list of supported options.

    .. admonition:: Example

        "``--oac=libmp3lame --oacopts=b=128000``"
            selects 128 kbps MP3 encoding.

    Options are managed in lists. There are a few commands to manage the
    options list.

    ``--oacopts-add=<options1[,options2,...]>``
        Appends the options given as arguments to the options list.

    ``--oacopts-pre=<options1[,options2,...]>``
        Prepends the options given as arguments to the options list.

    ``--oacopts-del=<index1[,index2,...]>``
        Deletes the options at the given indexes. Index numbers start at 0,
        negative numbers address the end of the list (-1 is the last).

    ``--oacopts-clr``
        Completely empties the options list.

``--oafirst``
    Force the audio stream to become the first stream in the output. By default
    the order is unspecified.

``--ovc=<codec>``
    Specifies the output video codec. This can be a comma separated list of
    possible codecs to try. See ``--ovc=help`` for a full list of supported
    codecs.

``--ovoffset=<value>``
    Shifts video data by the given time (in seconds) by shifting the pts
    values.

``--ovcopts <options>``
    Specifies the output video codec options for libavcodec.
    See --ovcopts=help for a full list of supported options.

    .. admonition:: Examples

        ``"--ovc=mpeg4 --oacopts=qscale=5"``
            selects constant quantizer scale 5 for MPEG-4 encoding.

        ``"--ovc=libx264 --ovcopts=crf=23"``
            selects VBR quality factor 23 for H.264 encoding.

    Options are managed in lists. There are a few commands to manage the
    options list.

    ``--ovcopts-add=<options1[,options2,...]>``
        Appends the options given as arguments to the options list.

    ``--ovcopts-pre=<options1[,options2,...]>``
        Prepends the options given as arguments to the options list.

    ``--ovcopts-del=<index1[,index2,...]>``
        Deletes the options at the given indexes. Index numbers start at 0,
        negative numbers address the end of the list (-1 is the last).

    ``--ovcopts-clr``
        Completely empties the options list.

``--ovfirst``
    Force the video stream to become the first stream in the output. By default
    the order is unspecified.

``--ocopyts``
    Copies input pts to the output video (not supported by some output
    container formats, e.g. AVI). Discontinuities are still fixed.
    By default, audio pts are set to playback time and video pts are
    synchronized to match audio pts, as some output formats do not support
    anything else.

``--orawts``
    Copies input pts to the output video (not supported by some output
    container formats, e.g. AVI). In this mode, discontinuities are not fixed
    and all pts are passed through as-is. Never seek backwards or use multiple
    input files in this mode!

``--no-ometadata``
    Turns off copying of metadata from input files to output files when
    encoding (which is enabled by default).
