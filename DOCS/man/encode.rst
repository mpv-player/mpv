ENCODING
========

You can encode files from one format/codec to another using this facility.

``--o=<filename>``
    Enables encoding mode and specifies the output file name.

``--of=<format>``
    Specifies the output format (overrides autodetection by the file name
    extension of the file specified by ``-o``). See ``--of=help`` for a full
    list of supported formats.

``--ofopts=<options>``
    Specifies the output format options for libavformat.
    See ``--ofopts=help`` for a full list of supported options.

    This is a key/value list option. See `List Options`_ for details.

    ``--ofopts-add=<option>``
        Appends the option given as an argument to the options list. (Passing
        multiple options is currently still possible, but deprecated.)

    ``--ofopts=""``
        Completely empties the options list.

``--oac=<codec>``
    Specifies the output audio codec. See ``--oac=help`` for a full list of
    supported codecs.

``--oaoffset=<value>``
    Shifts audio data by the given time (in seconds) by adding/removing
    samples at the start. Deprecated.

``--oacopts=<options>``
    Specifies the output audio codec options for libavcodec.
    See ``--oacopts=help`` for a full list of supported options.

    .. admonition:: Example

        "``--oac=libmp3lame --oacopts=b=128000``"
            selects 128 kbps MP3 encoding.

    This is a key/value list option. See `List Options`_ for details.

    ``--oacopts-add=<option>``
        Appends the option given as an argument to the options list. (Passing
        multiple options is currently still possible, but deprecated.)

    ``--oacopts=""``
        Completely empties the options list.

``--oafirst``
    Force the audio stream to become the first stream in the output.
    By default, the order is unspecified. Deprecated.

``--ovc=<codec>``
    Specifies the output video codec. See ``--ovc=help`` for a full list of
    supported codecs.

``--ovoffset=<value>``
    Shifts video data by the given time (in seconds) by shifting the pts
    values. Deprecated.

``--ovcopts=<options>``
    Specifies the output video codec options for libavcodec.
    See --ovcopts=help for a full list of supported options.

    .. admonition:: Examples

        ``"--ovc=mpeg4 --ovcopts=qscale=5"``
            selects constant quantizer scale 5 for MPEG-4 encoding.

        ``"--ovc=libx264 --ovcopts=crf=23"``
            selects VBR quality factor 23 for H.264 encoding.

    This is a key/value list option. See `List Options`_ for details.

    ``--ovcopts-add=<option>``
        Appends the option given as an argument to the options list. (Passing
        multiple options is currently still possible, but deprecated.)

    ``--ovcopts=""``
        Completely empties the options list.

``--ovfirst``
    Force the video stream to become the first stream in the output.
    By default, the order is unspecified. Deprecated.

``--orawts``
    Copies input pts to the output video (not supported by some output
    container formats, e.g. AVI). In this mode, discontinuities are not fixed
    and all pts are passed through as-is. Never seek backwards or use multiple
    input files in this mode!

``--no-ocopy-metadata``
    Turns off copying of metadata from input files to output files when
    encoding (which is enabled by default).

``--oset-metadata=<metadata-tag[,metadata-tag,...]>``
    Specifies metadata to include in the output file.
    Supported keys vary between output formats. For example, Matroska (MKV) and
    FLAC allow almost arbitrary keys, while support in MP4 and MP3 is more
    limited.

    This is a key/value list option. See `List Options`_ for details.

    .. admonition:: Example

        "``--oset-metadata=title="Output title",comment="Another tag"``"
            adds a title and a comment to the output file.

``--oremove-metadata=<metadata-tag[,metadata-tag,...]>``
    Specifies metadata to exclude from the output file when copying from the
    input file.

    This is a string list option. See `List Options`_ for details.

    .. admonition:: Example

        "``--oremove-metadata=comment,genre``"
            excludes copying of the the comment and genre tags to the output
            file.
