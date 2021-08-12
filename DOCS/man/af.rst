AUDIO FILTERS
=============

Audio filters allow you to modify the audio stream and its properties. The
syntax is:

``--af=...``
    Setup a chain of audio filters. See ``--vf`` (`VIDEO FILTERS`_) for the
    full syntax.

.. note::

    To get a full list of available audio filters, see ``--af=help``.

    Also, keep in mind that most actual filters are available via the ``lavfi``
    wrapper, which gives you access to most of libavfilter's filters. This
    includes all filters that have been ported from MPlayer to libavfilter.

    The ``--vf`` description describes how libavfilter can be used and how to
    workaround deprecated mpv filters.

See ``--vf`` group of options for info on how ``--af-defaults``, ``--af-add``,
``--af-pre``, ``--af-del``, ``--af-clr``, and possibly others work.

Available filters are:

``lavcac3enc[=options]``
    Encode multi-channel audio to AC-3 at runtime using libavcodec. Supports
    16-bit native-endian input format, maximum 6 channels. The output is
    big-endian when outputting a raw AC-3 stream, native-endian when
    outputting to S/PDIF. If the input sample rate is not 48 kHz, 44.1 kHz or
    32 kHz, it will be resampled to 48 kHz.

    ``tospdif=<yes|no>``
        Output raw AC-3 stream if ``no``, output to S/PDIF for
        pass-through if ``yes`` (default).

    ``bitrate=<rate>``
        The bitrate use for the AC-3 stream. Set it to 384 to get 384 kbps.

        The default is 640. Some receivers might not be able to handle this.

        Valid values: 32, 40, 48, 56, 64, 80, 96, 112, 128,
        160, 192, 224, 256, 320, 384, 448, 512, 576, 640.

        The special value ``auto`` selects a default bitrate based on the
        input channel number:

        :1ch: 96
        :2ch: 192
        :3ch: 224
        :4ch: 384
        :5ch: 448
        :6ch: 448

    ``minch=<n>``
        If the input channel number is less than ``<minch>``, the filter will
        detach itself (default: 3).

    ``encoder=<name>``
        Select the libavcodec encoder used. Currently, this should be an AC-3
        encoder, and using another codec will fail horribly.

``format=format:srate:channels:out-srate:out-channels``
    Does not do any format conversion itself. Rather, it may cause the
    filter system to insert necessary conversion filters before or after this
    filter if needed. It is primarily useful for controlling the audio format
    going into other filters. To specify the format for audio output, see
    ``--audio-format``, ``--audio-samplerate``, and ``--audio-channels``. This
    filter is able to force a particular format, whereas ``--audio-*``
    may be overridden by the ao based on output compatibility.

    All parameters are optional. The first 3 parameters restrict what the filter
    accepts as input. They will therefore cause conversion filters to be
    inserted before this one.  The ``out-`` parameters tell the filters or audio
    outputs following this filter how to interpret the data without actually
    doing a conversion. Setting these will probably just break things unless you
    really know you want this for some reason, such as testing or dealing with
    broken media.

    ``<format>``
        Force conversion to this format. Use ``--af=format=format=help`` to get
        a list of valid formats.

    ``<srate>``
        Force conversion to a specific sample rate. The rate is an integer,
        48000 for example.

    ``<channels>``
        Force mixing to a specific channel layout. See ``--audio-channels`` option
        for possible values.

    ``<out-srate>``

    ``<out-channels>``

    *NOTE*: this filter used to be named ``force``. The old ``format`` filter
    used to do conversion itself, unlike this one which lets the filter system
    handle the conversion.

``scaletempo[=option1:option2:...]``
    Scales audio tempo without altering pitch, optionally synced to playback
    speed (default).

    This works by playing 'stride' ms of audio at normal speed then consuming
    'stride*scale' ms of input audio. It pieces the strides together by
    blending 'overlap'% of stride with audio following the previous stride. It
    optionally performs a short statistical analysis on the next 'search' ms
    of audio to determine the best overlap position.

    ``scale=<amount>``
        Nominal amount to scale tempo. Scales this amount in addition to
        speed. (default: 1.0)
    ``stride=<amount>``
        Length in milliseconds to output each stride. Too high of a value will
        cause noticeable skips at high scale amounts and an echo at low scale
        amounts. Very low values will alter pitch. Increasing improves
        performance. (default: 60)
    ``overlap=<percent>``
        Percentage of stride to overlap. Decreasing improves performance.
        (default: .20)
    ``search=<amount>``
        Length in milliseconds to search for best overlap position. Decreasing
        improves performance greatly. On slow systems, you will probably want
        to set this very low. (default: 14)
    ``speed=<tempo|pitch|both|none>``
        Set response to speed change.

        tempo
             Scale tempo in sync with speed (default).
        pitch
             Reverses effect of filter. Scales pitch without altering tempo.
             Add this to your ``input.conf`` to step by musical semi-tones::

                [ multiply speed 0.9438743126816935
                ] multiply speed 1.059463094352953

             .. warning::

                Loses sync with video.
        both
            Scale both tempo and pitch.
        none
            Ignore speed changes.

    .. admonition:: Examples

        ``mpv --af=scaletempo --speed=1.2 media.ogg``
            Would play media at 1.2x normal speed, with audio at normal
            pitch. Changing playback speed would change audio tempo to match.

        ``mpv --af=scaletempo=scale=1.2:speed=none --speed=1.2 media.ogg``
            Would play media at 1.2x normal speed, with audio at normal
            pitch, but changing playback speed would have no effect on audio
            tempo.

        ``mpv --af=scaletempo=stride=30:overlap=.50:search=10 media.ogg``
            Would tweak the quality and performance parameters.

        ``mpv --af=scaletempo=scale=1.2:speed=pitch audio.ogg``
            Would play media at 1.2x normal speed, with audio at normal pitch.
            Changing playback speed would change pitch, leaving audio tempo at
            1.2x.
    
``scaletempo2[=option1:option2:...]``
    Scales audio tempo without altering pitch.
    The algorithm is ported from chromium and uses the 
    Waveform Similarity Overlap-and-add (WSOLA) method.
    It seems to achieve a higher audio quality than scaletempo and rubberband.

    By default, the ``search-interval`` and ``window-size`` parameters 
    have the same values as in chromium.

    ``min-speed=<speed>``
        Mute audio if the playback speed is below ``<speed>``. (default: 0.25)

    ``max-speed=<speed>``
        Mute audio if the playback speed is above ``<speed>``
        and ``<speed> != 0``. (default: 4.0)

    ``search-interval=<amount>``
        Length in milliseconds to search for best overlap position. (default: 30)
    
    ``window-size=<amount>``
        Length in milliseconds of the overlap-and-add window. (default: 20)

``rubberband``
    High quality pitch correction with librubberband. This can be used in place
    of ``scaletempo``, and will be used to adjust audio pitch when playing
    at speed different from normal. It can also be used to adjust audio pitch
    without changing playback speed.

    ``<pitch-scale>``
        Sets the pitch scaling factor. Frequencies are multiplied by this value.

    This filter has a number of additional sub-options. You can list them with
    ``mpv --af=rubberband=help``. This will also show the default values
    for each option. The options are not documented here, because they are
    merely passed to librubberband. Look at the librubberband documentation
    to learn what each option does:
    https://breakfastquay.com/rubberband/code-doc/classRubberBand_1_1RubberBandStretcher.html
    (The mapping of the mpv rubberband filter sub-option names and values to
    those of librubberband follows a simple pattern: ``"Option" + Name + Value``.)

    This filter supports the following ``af-command`` commands:

    ``set-pitch``
        Set the ``<pitch-scale>`` argument dynamically. This can be used to
        change the playback pitch at runtime. Note that speed is controlled
        using the standard ``speed`` property, not ``af-command``.

    ``multiply-pitch <factor>``
        Multiply the current value of ``<pitch-scale>`` dynamically.  For
        example: 0.5 to go down by an octave, 1.5 to go up by a perfect fifth.
        If you want to go up or down by semi-tones, use 1.059463094352953 and
        0.9438743126816935

``lavfi=graph``
    Filter audio using FFmpeg's libavfilter.

    ``<graph>``
        Libavfilter graph. See ``lavfi`` video filter for details - the graph
        syntax is the same.

        .. warning::

            Don't forget to quote libavfilter graphs as described in the lavfi
            video filter section.

    ``o=<string>``
        AVOptions.

    ``fix-pts=<yes|no>``
        Determine PTS based on sample count (default: no). If this is enabled,
        the player won't rely on libavfilter passing through PTS accurately.
        Instead, it pass a sample count as PTS to libavfilter, and compute the
        PTS used by mpv based on that and the input PTS. This helps with filters
        which output a recomputed PTS instead of the original PTS (including
        filters which require the PTS to start at 0). mpv normally expects
        filters to not touch the PTS (or only to the extent of changing frame
        boundaries), so this is not the default, but it will be needed to use
        broken filters. In practice, these broken filters will either cause slow
        A/V desync over time (with some files), or break playback completely if
        you seek or start playback from the middle of a file.

``drop``
    This filter drops or repeats audio frames to adapt to playback speed. It
    always operates on full audio frames, because it was made to handle SPDIF
    (compressed audio passthrough). This is used automatically if the
    ``--video-sync=display-adrop`` option is used. Do not use this filter (or
    the given option); they are extremely low quality.
