AUDIO FILTERS
=============

Audio filters allow you to modify the audio stream and its properties. The
syntax is:

``--af=<filter1[=parameter1:parameter2:...],filter2,...>``
    Setup a chain of audio filters.

.. note::

    To get a full list of available audio filters, see ``--af=help``.

You can also set defaults for each filter. The defaults are applied before the
normal filter parameters.

``--af-defaults=<filter1[=parameter1:parameter2:...],filter2,...>``
    Set defaults for each filter.

Audio filters are managed in lists. There are a few commands to manage the
filter list:

``--af-add=<filter1[,filter2,...]>``
    Appends the filters given as arguments to the filter list.

``--af-pre=<filter1[,filter2,...]>``
    Prepends the filters given as arguments to the filter list.

``--af-del=<index1[,index2,...]>``
    Deletes the filters at the given indexes. Index numbers start at 0,
    negative numbers address the end of the list (-1 is the last).

``--af-clr``
    Completely empties the filter list.

Available filters are:

``lavrresample[=option1:option2:...]``
    This filter uses libavresample (or libswresample, depending on the build)
    to change sample rate, sample format, or channel layout of the audio stream.
    This filter is automatically enabled if the audio output does not support
    the audio configuration of the file being played.

    It supports only the following sample formats: u8, s16, s32, float.

    ``filter-size=<length>``
        Length of the filter with respect to the lower sampling rate. (default:
        16)
    ``phase-shift=<count>``
        Log2 of the number of polyphase entries. (..., 10->1024, 11->2048,
        12->4096, ...) (default: 10->1024)
    ``cutoff=<cutoff>``
        Cutoff frequency (0.0-1.0), default set depending upon filter length.
    ``linear``
        If set then filters will be linearly interpolated between polyphase
        entries. (default: no)
    ``no-detach``
        Do not detach if input and output audio format/rate/channels match.
        (If you just want to set defaults for this filter that will be used
        even by automatically inserted lavrresample instances, you should
        prefer setting them with ``--af-defaults=lavrresample:...``.)
    ``o=<string>``
        Set AVOptions on the SwrContext or AVAudioResampleContext. These should
        be documented by FFmpeg or Libav.

``lavcac3enc[=tospdif[:bitrate[:minch]]]``
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

``sweep[=speed]``
    Produces a sine sweep.

    ``<0.0-1.0>``
        Sine function delta, use very low values to hear the sweep.

``sinesuppress[=freq:decay]``
    Remove a sine at the specified frequency. Useful to get rid of the 50/60 Hz
    noise on low quality audio equipment. It only works on mono input.

    ``<freq>``
        The frequency of the sine which should be removed (in Hz) (default:
        50)
    ``<decay>``
        Controls the adaptivity (a larger value will make the filter adapt to
        amplitude and phase changes quicker, a smaller value will make the
        adaptation slower) (default: 0.0001). Reasonable values are around
        0.001.

``bs2b[=option1:option2:...]``
    Bauer stereophonic to binaural transformation using libbs2b. Improves the
    headphone listening experience by making the sound similar to that from
    loudspeakers, allowing each ear to hear both channels and taking into
    account the distance difference and the head shadowing effect. It is
    applicable only to 2-channel audio.

    ``fcut=<300-1000>``
        Set cut frequency in Hz.
    ``feed=<10-150>``
        Set feed level for low frequencies in 0.1*dB.
    ``profile=<value>``
        Several profiles are available for convenience:

        :default: will be used if nothing else was specified (fcut=700,
                  feed=45)
        :cmoy:    Chu Moy circuit implementation (fcut=700, feed=60)
        :jmeier:  Jan Meier circuit implementation (fcut=650, feed=95)

    If ``fcut`` or ``feed`` options are specified together with a profile, they
    will be applied on top of the selected profile.

``hrtf[=flag]``
    Head-related transfer function: Converts multichannel audio to 2-channel
    output for headphones, preserving the spatiality of the sound.

    ==== ===================================
    Flag Meaning
    ==== ===================================
    m    matrix decoding of the rear channel
    s    2-channel matrix decoding
    0    no matrix decoding (default)
    ==== ===================================

``equalizer=g1:g2:g3:...:g10``
    10 octave band graphic equalizer, implemented using 10 IIR band-pass
    filters. This means that it works regardless of what type of audio is
    being played back. The center frequencies for the 10 bands are:

    === ==========
    No. frequency
    === ==========
    0    31.25  Hz
    1    62.50  Hz
    2   125.00  Hz
    3   250.00  Hz
    4   500.00  Hz
    5     1.00 kHz
    6     2.00 kHz
    7     4.00 kHz
    8     8.00 kHz
    9    16.00 kHz
    === ==========

    If the sample rate of the sound being played is lower than the center
    frequency for a frequency band, then that band will be disabled. A known
    bug with this filter is that the characteristics for the uppermost band
    are not completely symmetric if the sample rate is close to the center
    frequency of that band. This problem can be worked around by upsampling
    the sound using a resampling filter before it reaches this filter.

    ``<g1>:<g2>:<g3>:...:<g10>``
        floating point numbers representing the gain in dB for each frequency
        band (-12-12)

    .. admonition:: Example

        ``mpv --af=equalizer=11:11:10:5:0:-12:0:5:12:12 media.avi``
            Would amplify the sound in the upper and lower frequency region
            while canceling it almost completely around 1 kHz.

``channels=nch[:routes]``
    Can be used for adding, removing, routing and copying audio channels. If
    only ``<nch>`` is given, the default routing is used. It works as follows:
    If the number of output channels is greater than the number of input
    channels, empty channels are inserted (except when mixing from mono to
    stereo; then the mono channel is duplicated). If the number of output
    channels is less than the number of input channels, the exceeding
    channels are truncated.

    ``<nch>``
        number of output channels (1-8)
    ``<routes>``
        List of ``,`` separated routes, in the form ``from1-to1,from2-to2,...``.
        Each pair defines where to route each channel. There can be at most
        8 routes. Without this argument, the default routing is used. Since
        ``,`` is also used to separate filters, you must quote this argument
        with ``[...]`` or similar.

    .. admonition:: Examples

        ``mpv --af=channels=4:[0-1,1-0,0-2,1-3] media.avi``
            Would change the number of channels to 4 and set up 4 routes that
            swap channel 0 and channel 1 and leave channel 2 and 3 intact.
            Observe that if media containing two channels were played back,
            channels 2 and 3 would contain silence but 0 and 1 would still be
            swapped.

        ``mpv --af=channels=6:[0-0,0-1,0-2,0-3] media.avi``
            Would change the number of channels to 6 and set up 4 routes that
            copy channel 0 to channels 0 to 3. Channel 4 and 5 will contain
            silence.

    .. note::

        You should probably not use this filter. If you want to change the
        output channel layout, try the ``format`` filter, which can make mpv
        automatically up- and downmix standard channel layouts.

``format=format:srate:channels:out-format:out-srate:out-channels``
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

    ``<out-format>``

    ``<out-srate>``

    ``<out-channels>``

    *NOTE*: this filter used to be named ``force``. The old ``format`` filter
    used to do conversion itself, unlike this one which lets the filter system
    handle the conversion.

``convert24``
    Filter for internal use only. Converts between 24-bit and 32-bit sample
    formats.

``convertsign``
    Filter for internal use only. Converts between signed/unsigned formats.

``volume[=<volumedb>[:...]]``
    Implements software volume control. Use this filter with caution since it
    can reduce the signal to noise ratio of the sound. In most cases it is
    best to use the *Master* volume control of your sound card or the volume
    knob on your amplifier.

    *NOTE*: This filter is not reentrant and can therefore only be enabled
    once for every audio stream.

    ``<volumedb>``
        Sets the desired gain in dB for all channels in the stream from -200 dB
        to +60 dB, where -200 dB mutes the sound completely and +60 dB equals a
        gain of 1000 (default: 0).
    ``replaygain-track``
        Adjust volume gain according to the track-gain replaygain value stored
        in the file metadata.
    ``replaygain-album``
        Like replaygain-track, but using the album-gain value instead.
    ``replaygain-preamp``
        Pre-amplification gain in dB to apply to the selected replaygain gain
        (default: 0).
    ``replaygain-clip=yes|no``
        Prevent clipping caused by replaygain by automatically lowering the
        gain (default). Use ``replaygain-clip=no`` to disable this.
    ``softclip``
        Turns soft clipping on. Soft-clipping can make the
        sound more smooth if very high volume levels are used. Enable this
        option if the dynamic range of the loudspeakers is very low.

        *WARNING*: This feature creates distortion and should be considered a
        last resort.
    ``s16``
        Force S16 sample format if set. Lower quality, but might be faster
        in some situations.
    ``detach``
        Remove the filter if the volume is not changed at audio filter config
        time. Useful with replaygain: if the current file has no replaygain
        tags, then the filter will be removed if this option is enabled.
        (If ``--softvol=yes`` is used and the player volume controls are used
        during playback, a different volume filter will be inserted.)

    .. admonition:: Example

        ``mpv --af=volume=10.1 media.avi``
            Would amplify the sound by 10.1 dB and hard-clip if the sound level
            is too high.

``pan=n:[<matrix>]``
    Mixes channels arbitrarily. Basically a combination of the volume and the
    channels filter that can be used to down-mix many channels to only a few,
    e.g. stereo to mono, or vary the "width" of the center speaker in a
    surround sound system. This filter is hard to use, and will require some
    tinkering before the desired result is obtained. The number of options for
    this filter depends on the number of output channels. An example how to
    downmix a six-channel file to two channels with this filter can be found
    in the examples section near the end.

    ``<n>``
        Number of output channels (1-8).
    ``<matrix>``
        A list of values ``[L00,L01,L02,...,L10,L11,L12,...,Ln0,Ln1,Ln2,...]``,
        where each element ``Lij`` means how much of input channel i is mixed
        into output channel j (range 0-1). So in principle you first have n
        numbers saying what to do with the first input channel, then n numbers
        that act on the second input channel etc. If you do not specify any
        numbers for some input channels, 0 is assumed.
        Note that the values are separated by ``,``, which is already used
        by the option parser to separate filters. This is why you must quote
        the value list with ``[...]`` or similar.

    .. admonition:: Examples

        ``mpv --af=pan=1:[0.5,0.5] media.avi``
            Would downmix from stereo to mono.

        ``mpv --af=pan=3:[1,0,0.5,0,1,0.5] media.avi``
            Would give 3 channel output leaving channels 0 and 1 intact, and mix
            channels 0 and 1 into output channel 2 (which could be sent to a
            subwoofer for example).

    .. note::

        If you just want to force remixing to a certain output channel layout,
        it is easier to use the ``format`` filter. For example,
        ``mpv '--af=format=channels=5.1' '--audio-channels=5.1'`` would always force
        remixing audio to 5.1 and output it like this.

``sub[=fc:ch]``
    Adds a subwoofer channel to the audio stream. The audio data used for
    creating the subwoofer channel is an average of the sound in channel 0 and
    channel 1. The resulting sound is then low-pass filtered by a 4th order
    Butterworth filter with a default cutoff frequency of 60Hz and added to a
    separate channel in the audio stream.

    .. warning::

        Disable this filter when you are playing media with an LFE channel
        (e.g. 5.1 surround sound), otherwise this filter will disrupt the sound
        to the subwoofer.

    ``<fc>``
        cutoff frequency in Hz for the low-pass filter (20 Hz to 300 Hz)
        (default: 60 Hz) For the best result try setting the cutoff frequency
        as low as possible. This will improve the stereo or surround sound
        experience.
    ``<ch>``
        Determines the channel number in which to insert the sub-channel
        audio. Channel number can be between 0 and 7 (default: 5). Observe
        that the number of channels will automatically be increased to <ch> if
        necessary.

    .. admonition:: Example

        ``mpv --af=sub=100:4 --audio-channels=5 media.avi``
            Would add a subwoofer channel with a cutoff frequency of 100 Hz to
            output channel 4.

``center``
    Creates a center channel from the front channels. May currently be low
    quality as it does not implement a high-pass filter for proper extraction
    yet, but averages and halves the channels instead.

    ``<ch>``
        Determines the channel number in which to insert the center channel.
        Channel number can be between 0 and 7 (default: 5). Observe that the
        number of channels will automatically be increased to ``<ch>`` if
        necessary.

``surround[=delay]``
    Decoder for matrix encoded surround sound like Dolby Surround. Some files
    with 2-channel audio actually contain matrix encoded surround sound.

    ``<delay>``
        delay time in ms for the rear speakers (0 to 1000) (default: 20) This
        delay should be set as follows: If d1 is the distance from the
        listening position to the front speakers and d2 is the distance from
        the listening position to the rear speakers, then the delay should be
        set to 15ms if d1 <= d2 and to 15 + 5*(d1-d2) if d1 > d2.

    .. admonition:: Example

        ``mpv --af=surround=15 --audio-channels=4 media.avi``
            Would add surround sound decoding with 15 ms delay for the sound to
            the rear speakers.

``delay[=[ch1,ch2,...]]``
    Delays the sound to the loudspeakers such that the sound from the
    different channels arrives at the listening position simultaneously. It is
    only useful if you have more than 2 loudspeakers.

    ``[ch1,ch2,...]``
        The delay in ms that should be imposed on each channel (floating point
        number between 0 and 1000).

    To calculate the required delay for the different channels, do as follows:

    1. Measure the distance to the loudspeakers in meters in relation to your
       listening position, giving you the distances s1 to s5 (for a 5.1
       system). There is no point in compensating for the subwoofer (you will
       not hear the difference anyway).

    2. Subtract the distances s1 to s5 from the maximum distance, i.e.
       ``s[i] = max(s) - s[i]; i = 1...5``.

    3. Calculate the required delays in ms as ``d[i] = 1000*s[i]/342; i =
       1...5``.

    .. admonition:: Example

        ``mpv --af=delay=[10.5,10.5,0,0,7,0] media.avi``
            Would delay front left and right by 10.5 ms, the two rear channels
            and the subwoofer by 0 ms and the center channel by 7 ms.

``export=mmapped_file:nsamples]``
    Exports the incoming signal to other processes using memory mapping
    (``mmap()``). Memory mapped areas contain a header::

        int nch                      /* number of channels */
        int size                     /* buffer size */
        unsigned long long counter   /* Used to keep sync, updated every time
                                        new data is exported. */

    The rest is payload (non-interleaved) 16-bit data.

    ``<mmapped_file>``
        File to map data to (required)
    ``<nsamples>``
        number of samples per channel (default: 512).

    .. admonition:: Example

        ``mpv --af=export=/tmp/mpv-af_export:1024 media.avi``
            Would export 1024 samples per channel to ``/tmp/mpv-af_export``.

``extrastereo[=mul]``
    (Linearly) increases the difference between left and right channels which
    adds some sort of "live" effect to playback.

    ``<mul>``
        Sets the difference coefficient (default: 2.5). 0.0 means mono sound
        (average of both channels), with 1.0 sound will be unchanged, with
        -1.0 left and right channels will be swapped.

``drc[=method:target]``
    Applies dynamic range compression. This maximizes the volume by compressing
    the audio signal's dynamic range. (Formerly called ``volnorm``.)

    ``<method>``
        Sets the used method.

        1
            Use a single sample to smooth the variations via the standard
            weighted mean over past samples (default).
        2
            Use several samples to smooth the variations via the standard
            weighted mean over past samples.

    ``<target>``
        Sets the target amplitude as a fraction of the maximum for the sample
        type (default: 0.25).

    .. note::

        This filter can cause distortion with audio signals that have a very
        large dynamic range.

``ladspa=file:label:[<control0>,<control1>,...]``
    Load a LADSPA (Linux Audio Developer's Simple Plugin API) plugin. This
    filter is reentrant, so multiple LADSPA plugins can be used at once.

    ``<file>``
        Specifies the LADSPA plugin library file.

        .. note::

            See also the note about the ``LADSPA_PATH`` variable in the
            `ENVIRONMENT VARIABLES`_ section.
    ``<label>``
        Specifies the filter within the library. Some libraries contain only
        one filter, but others contain many of them. Entering 'help' here
        will list all available filters within the specified library, which
        eliminates the use of 'listplugins' from the LADSPA SDK.
    ``[<control0>,<control1>,...]``
        Controls are zero or more ``,`` separated floating point values that
        determine the behavior of the loaded plugin (for example delay,
        threshold or gain).
        In verbose mode (add ``-v`` to the mpv command line), all
        available controls and their valid ranges are printed. This eliminates
        the use of 'analyseplugin' from the LADSPA SDK.
        Note that ``,`` is already used by the option parser to separate
        filters, so you must quote the list of values with ``[...]`` or
        similar.

    .. admonition:: Example

        ``mpv --af=ladspa='/usr/lib/ladspa/delay.so':delay_5s:[0.5,0.2] media.avi``
            Does something.

``karaoke``
    Simple voice removal filter exploiting the fact that voice is usually
    recorded with mono gear and later 'center' mixed onto the final audio
    stream. Beware that this filter will turn your signal into mono. Works
    well for 2 channel tracks; do not bother trying it on anything but 2
    channel stereo.

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

        ``mpv --af=format=float,scaletempo media.ogg``
            Would make scaletempo use float code. Maybe faster on some
            platforms.

        ``mpv --af=scaletempo=scale=1.2:speed=pitch audio.ogg``
            Would play media at 1.2x normal speed, with audio at normal pitch.
            Changing playback speed would change pitch, leaving audio tempo at
            1.2x.

``rubberband``
    High quality pitch correction with librubberband. This can be used in place
    of ``scaletempo``, and will be used to adjust audio pitch when playing
    at speed different from normal.

    This filter has a number of sub-options. You can list them with
    ``mpv --af=rubberband=help``. This will also show the default values
    for each option. The options are not documented here, because they are
    merely passed to librubberband. Look at the librubberband documentation
    to learn what each option does:
    http://breakfastquay.com/rubberband/code-doc/classRubberBand_1_1RubberBandStretcher.html
    (The mapping of the mpv rubberband filter sub-option names and values to
    those of librubberband follows a simple pattern: ``"Option" + Name + Value``.)

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
