AUDIO OUTPUT DRIVERS
====================

Audio output drivers are interfaces to different audio output facilities. The
syntax is:

``--ao=<driver1,driver2,...[,]>``
    Specify a priority list of audio output drivers to be used.

If the list has a trailing ',', mpv will fall back on drivers not contained
in the list.

.. note::

    See ``--ao=help`` for a list of compiled-in audio output drivers sorted by
    autoprobe order.

    Note that the default audio output driver is subject to change, and must
    not be relied upon. If a certain AO needs to be used, it must be
    explicitly specified.

Available audio output drivers are:

``alsa``
    ALSA audio output driver.

    The following global options are supported by this audio output:

    ``--alsa-resample=yes``
        Enable ALSA resampling plugin. (This is disabled by default, because
        some drivers report incorrect audio delay in some cases.)

    ``--alsa-mixer-device=<device>``
        Set the mixer device used with ``ao-volume`` (default: ``default``).

    ``--alsa-mixer-name=<name>``
        Set the name of the mixer element (default: ``Master``). This is for
        example ``PCM`` or ``Master``.

    ``--alsa-mixer-index=<number>``
        Set the index of the mixer channel (default: 0). Consider the output of
        "``amixer scontrols``", then the index is the number that follows the
        name of the element.

    ``--alsa-non-interleaved``
        Allow output of non-interleaved formats (if the audio decoder uses
        this format). Currently disabled by default, because some popular
        ALSA plugins are utterly broken with non-interleaved formats.

    ``--alsa-ignore-chmap``
        Don't read or set the channel map of the ALSA device - only request the
        required number of channels, and then pass the audio as-is to it. This
        option most likely should not be used. It can be useful for debugging,
        or for static setups with a specially engineered ALSA configuration (in
        this case you should always force the same layout with ``--audio-channels``,
        or it will work only for files which use the layout implicit to your
        ALSA device).

    ``--alsa-buffer-time=<microseconds>``
        Set the requested buffer time in microseconds. A value of 0 skips requesting
        anything from the ALSA API. This and the ``--alsa-periods`` option uses the
        ALSA ``near`` functions to set the requested parameters. If doing so results
        in an empty configuration set, setting these parameters is skipped.

        Both options control the buffer size. A low buffer size can lead to higher
        CPU usage and audio dropouts, while a high buffer size can lead to higher
        latency in volume changes and other filtering.

    ``--alsa-periods=<number>``
        Number of periods requested from the ALSA API. See ``--alsa-buffer-time``
        for further remarks.

    .. warning::

        To get multichannel/surround audio, use ``--audio-channels=auto``. The
        default for this option is ``auto-safe``, which makes this audio output
        explicitly reject multichannel output, as there is no way to detect
        whether a certain channel layout is actually supported.

        You can also try `using the upmix plugin
        <https://github.com/mpv-player/mpv/wiki/ALSA-Surround-Sound-and-Upmixing>`_.
        This setup enables multichannel audio on the ``default`` device
        with automatic upmixing with shared access, so playing stereo
        and multichannel audio at the same time will work as expected.

``oss``
    OSS audio output driver

``jack``
    JACK (Jack Audio Connection Kit) audio output driver.

    The following global options are supported by this audio output:

    ``--jack-port=<name>``
        Connects to the ports with the given name (default: physical ports).
    ``--jack-name=<client>``
        Client name that is passed to JACK (default: ``mpv``). Useful
        if you want to have certain connections established automatically.
    ``--jack-autostart=<yes|no>``
        Automatically start jackd if necessary (default: disabled). Note that
        this tends to be unreliable and will flood stdout with server messages.
    ``--jack-connect=<yes|no>``
        Automatically create connections to output ports (default: enabled).
        When enabled, the maximum number of output channels will be limited to
        the number of available output ports.
    ``--jack-std-channel-layout=<waveext|any>``
        Select the standard channel layout (default: waveext). JACK itself has no
        notion of channel layouts (i.e. assigning which speaker a given
        channel is supposed to map to) - it just takes whatever the application
        outputs, and reroutes it to whatever the user defines. This means the
        user and the application are in charge of dealing with the channel
        layout. ``waveext`` uses WAVE_FORMAT_EXTENSIBLE order, which, even
        though it was defined by Microsoft, is the standard on many systems.
        The value ``any`` makes JACK accept whatever comes from the audio
        filter chain, regardless of channel layout and without reordering. This
        mode is probably not very useful, other than for debugging or when used
        with fixed setups.

``coreaudio`` (macOS only)
    Native macOS audio output driver using AudioUnits and the CoreAudio
    sound server.

    Automatically redirects to ``coreaudio_exclusive`` when playing compressed
    formats.

    The following global options are supported by this audio output:

    ``--coreaudio-change-physical-format=<yes|no>``
        Change the physical format to one similar to the requested audio format
        (default: no). This has the advantage that multichannel audio output
        will actually work. The disadvantage is that it will change the
        system-wide audio settings. This is equivalent to changing the ``Format``
        setting in the ``Audio Devices`` dialog in the ``Audio MIDI Setup``
        utility. Note that this does not affect the selected speaker setup.

    ``--coreaudio-spdif-hack=<yes|no>``
        Try to pass through AC3/DTS data as PCM. This is useful for drivers
        which do not report AC3 support. It converts the AC3 data to float,
        and assumes the driver will do the inverse conversion, which means
        a typical A/V receiver will pick it up as compressed IEC framed AC3
        stream, ignoring that it's marked as PCM. This disables normal AC3
        passthrough (even if the device reports it as supported). Use with
        extreme care.

``coreaudio_exclusive`` (macOS only)
    Native macOS audio output driver using direct device access and
    exclusive mode (bypasses the sound server).

``avfoundation`` (macOS only)
    Native macOS audio output driver using ``AVSampleBufferAudioRenderer``
    in AVFoundation, which supports `spatial audio
    <https://support.apple.com/en-us/HT211775>`_.

    .. warning::

        Turning on spatial audio may hang the playback
        if mpv is not started out of the bundle,
        though playback with spatial audio off always works.

    Currently, due to the implementation of A/V compensation,
    setting ``--video-sync`` to ``display-resample`` or ``display-vdrop``
    can alleviate A/V drift on changing playback speed
    when using this audio output driver.

    ``--avfoundation-buffer=<1-2000>``
        Set the audio buffer size in milliseconds. A higher value buffers
        more data, and has a lower probability of buffer underruns. A smaller
        value makes the audio stream react faster, e.g. to playback speed
        changes, soft volume change, and muting/unmuting.
        The default is 2000ms, which is conservative.

``openal``
    OpenAL audio output driver.

    ``--openal-num-buffers=<2-128>``
        Specify the number of audio buffers to use. Lower values are better for
        lower CPU usage. Default: 4.

    ``--openal-num-samples=<256-32768>``
        Specify the number of complete samples to use for each buffer. Higher
        values are better for lower CPU usage. Default: 8192.

    ``--openal-direct-channels=<yes|no>``
        Enable OpenAL Soft's direct channel extension when available to avoid
        tinting the sound with ambisonics or HRTF. Default: yes.

``pulse``
    PulseAudio audio output driver

    The following global options are supported by this audio output:

    ``--pulse-host=<host>``
        Specify the host to use. An empty <host> string uses a local connection,
        "localhost" uses network transfer (most likely not what you want).

    ``--pulse-buffer=<1-2000|native>``
        Set the audio buffer size in milliseconds. A higher value buffers
        more data, and has a lower probability of buffer underruns. A smaller
        value makes the audio stream react faster, e.g. to playback speed
        changes, soft volume change, and muting/unmuting.
        "native" lets the sound server determine buffers.

    ``--pulse-latency-hacks=<yes|no>``
        Enable hacks to workaround PulseAudio timing bugs (default: yes). If
        enabled, mpv will do elaborate latency calculations on its own. If
        disabled, it will use PulseAudio automatically updated timing
        information. Disabling this might help with e.g. networked audio or
        some plugins, while enabling it might help in some unknown situations
        (it is currently enabled due to known bugs with PulseAudio 16.0).

    ``--pulse-allow-suspended=<yes|no>``
        Allow mpv to use PulseAudio even if the sink is suspended (default: no).
        Can be useful if PulseAudio is running as a bridge to jack and mpv has its sink-input set to the one jack is using.

``pipewire``
    PipeWire audio output driver

    The following global options are supported by this audio output:

    ``--pipewire-buffer=<1-2000|native>``
        Set the audio buffer size in milliseconds. A higher value buffers
        more data, and has a lower probability of buffer underruns. A smaller
        value makes the audio stream react faster, e.g. to playback speed
        changes. "native" lets the sound server determine buffers.

    ``--pipewire-remote=<remote>``
        Specify the PipeWire remote daemon name to connect to via local UNIX
        sockets.
        An empty <remote> string uses the default remote named ``pipewire-0``.

    ``--pipewire-volume-mode=<channel|global>``
        Specify if the ``ao-volume`` property should apply to the channel
        volumes or the global volume.
        By default the channel volumes are used.

``sdl``
    SDL 2.0+ audio output driver. Should work on any platform supported by SDL
    2.0, but may require the ``SDL_AUDIODRIVER`` environment variable to be set
    appropriately for your system.

    .. note:: This driver is for compatibility with extremely foreign
              environments, such as systems where none of the other drivers
              are available.

    The following global options are supported by this audio output:

    ``--sdl-buflen=<length>``
        Sets the audio buffer length in seconds. Is used only as a hint by the
        sound system. Playing a file with ``-v`` will show the requested and
        obtained exact buffer size. A value of 0 selects the sound system
        default.

``null``
    Produces no audio output but maintains video playback speed. You can use
    ``--ao=null --ao-null-untimed`` for benchmarking.

    The following global options are supported by this audio output:

    ``--ao-null-untimed``
        Do not simulate timing of a perfect audio device. This means audio
        decoding will go as fast as possible, instead of timing it to the
        system clock.

    ``--ao-null-buffer``
        Simulated buffer length in seconds.

    ``--ao-null-outburst``
        Simulated chunk size in samples.

    ``--ao-null-speed``
        Simulated audio playback speed as a multiplier. Usually, a real audio
        device will not go exactly as fast as the system clock. It will deviate
        just a little, and this option helps to simulate this.

    ``--ao-null-latency``
        Simulated device latency. This is additional to EOF.

    ``--ao-null-broken-eof``
        Simulate broken audio drivers, which always add the fixed device
        latency to the reported audio playback position.

    ``--ao-null-broken-delay``
        Simulate broken audio drivers, which don't report latency correctly.

    ``--ao-null-channel-layouts``
        If not empty, this is a ``,`` separated list of channel layouts the
        AO allows. This can be used to test channel layout selection.

    ``--ao-null-format``
        Force the audio output format the AO will accept. If unset accepts any.

``pcm``
    Raw PCM/WAVE file writer audio output

    The following global options are supported by this audio output:

    ``--ao-pcm-waveheader=<yes|no>``
        Include or do not include the WAVE header (default: included). When
        not included, raw PCM will be generated.
    ``--ao-pcm-file=<filename>``
        Write the sound to ``<filename>`` instead of the default
        ``audiodump.wav``. If ``no-waveheader`` is specified, the default is
        ``audiodump.pcm``.
    ``--ao-pcm-append=<yes|no>``
        Append to the file, instead of overwriting it. Always use this with the
        ``no-waveheader`` option - with ``waveheader`` it's broken, because
        it will write a WAVE header every time the file is opened.

``sndio``
    Audio output to the OpenBSD sndio sound system

    (Note: only supports mono, stereo, 4.0, 5.1 and 7.1 channel
    layouts.)

``wasapi``
    Audio output to the Windows Audio Session API.

    The following global options are supported by this audio output:

    ``--wasapi-exclusive-buffer=<default|min|1-2000000>``
        Set buffer duration in exclusive mode (i.e., with
        ``--audio-exclusive=yes``). ``default`` and ``min`` use the default and
        minimum device period reported by WASAPI, respectively. You can also
        directly specify the buffer duration in microseconds, in which case a
        duration shorter than the minimum device period will be rounded up to
        the minimum period.

        The default buffer duration should provide robust playback in most
        cases, but reportedly on some devices there are glitches following
        stream resets under the default setting. In such cases, specifying a
        shorter duration might help.
