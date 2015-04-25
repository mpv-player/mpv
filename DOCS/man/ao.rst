AUDIO OUTPUT DRIVERS
====================

Audio output drivers are interfaces to different audio output facilities. The
syntax is:

``--ao=<driver1[:suboption1[=value]:...],driver2,...[,]>``
    Specify a priority list of audio output drivers to be used.

If the list has a trailing ',', mpv will fall back on drivers not contained
in the list. Suboptions are optional and can mostly be omitted.

You can also set defaults for each driver. The defaults are applied before the
normal driver parameters.

``--ao-defaults=<driver1[:parameter1:parameter2:...],driver2,...>``
    Set defaults for each driver.

.. note::

    See ``--ao=help`` for a list of compiled-in audio output drivers. The
    driver ``--ao=alsa`` is preferred. ``--ao=pulse`` is preferred on systems
    where PulseAudio is used. On Windows, ``--ao=wasapi`` is preferred,
    though it might cause trouble sometimes, in which case ``--ao=dsound``
    should be used. On BSD systems, ``--ao=oss`` or `--ao=sndio`` may work
    (the latter being experimental). On OS X systems, use ``--ao=coreaudio``.

.. admonition:: Examples

    - ``--ao=alsa,oss,`` Try the ALSA driver, then the OSS driver, then others.
    - ``--ao=alsa:resample=yes:device=[plughw:0,3]`` Lets ALSA resample and
      sets the device-name as first card, fourth device.

Available audio output drivers are:

``alsa`` (Linux only)
    ALSA audio output driver

    ``device=<device>``
        Sets the device name. For ac3 output via S/PDIF, use an "iec958" or
        "spdif" device, unless you really know how to set it correctly.
    ``resample=yes``
        Enable ALSA resampling plugin. (This is disabled by default, because
        some drivers report incorrect audio delay in some cases.)
    ``mixer-device=<device>``
        Set the mixer device used with ``--no-softvol`` (default: ``default``).
    ``mixer-name=<name>``
        Set the name of the mixer element (default: ``Master``). This is for
        example ``PCM`` or ``Master``.
    ``mixer-index=<number>``
        Set the index of the mixer channel (default: 0). Consider the output of
        "``amixer scontrols``", then the index is the number that follows the
        name of the element.
    ``non-interleaved``
        Allow output of non-interleaved formats (if the audio decoder uses
        this format). Currently disabled by default, because some popular
        ALSA plugins are utterly broken with non-interleaved formats.
    ``ingore-chmap``
        Don't read or set the channel map of the ALSA device - only request the
        required number of channels, and then pass the audio as-is to it. This
        option most likely should not be used. It can be useful for debugging,
        or for static setups with a specially engineered ALSA configuration (in
        this case you should always force the same layout with ``--audio-channels``,
        or it will work only for files which use the layout implicit to your
        ALSA device).

    .. note::

        MPlayer and mplayer2 required you to replace any ',' with '.' and
        any ':' with '=' in the ALSA device name. mpv does not do this anymore.
        Instead, quote the device name:

            ``--ao=alsa:device=[plug:surround50]``

        Note that the ``[`` and ``]`` simply quote the device name. With some
        shells (like zsh), you have to quote the option string to prevent the
        shell from interpreting the brackets instead of passing them to mpv.

        Actually, you should use the ``--audio-device`` option, instead of
        setting the device directly.

    .. warning::

        Handling of multichannel/surround audio changed in mpv 0.8.0 from the
        behavior in MPlayer/mplayer2 and older versions of mpv.

        The old behavior is that the player always downmixed to stereo by
        default. The ``--audio-channels`` (or ``--channels`` before that) option
        had to be set to get multichannel audio. Then playing stereo would
        use the ``default`` device (which typically allows multiple programs
        to play audio at the same time via dmix), while playing anything with
        more channels would open one of the hardware devices, e.g. via the
        ``surround51`` alias (typically with exclusive access). Whether the
        player would use exclusive access or not would depend on the file
        being played.

        The new behavior since mpv 0.8.0 always enables multichannel audio,
        i.e. ``--audio-channels=auto`` is the default. However, since ALSA
        provides no good way to play multichannel audio in a non-exclusive
        way (without blocking other applications from using audio), the player
        is restricted to the capabilities of the ``default`` device by default,
        which means it supports only stereo and mono (at least with current
        typical ALSA configurations). But if a hardware device is selected,
        then multichannel audio will typically work.

        The short story is: if you want multichannel audio with ALSA, use
        ``--audio-device`` to select the device (use ``--audio-device=help``
        to get a list of all devices and their mpv name).

        You can also try `using the upmix plugin <http://git.io/vfuAy>`_.
        This setup enables multichannel audio on the ``default`` device
        with automatic upmixing with shared access, so playing stereo
        and multichannel audio at the same time will work as expected.

``oss``
    OSS audio output driver

    ``<dsp-device>``
        Sets the audio output device (default: ``/dev/dsp``).
    ``<mixer-device>``
        Sets the audio mixer device (default: ``/dev/mixer``).
    ``<mixer-channel>``
        Sets the audio mixer channel (default: ``pcm``). Other valid values
        include **vol, pcm, line**. For a complete list of options look for
        ``SOUND_DEVICE_NAMES`` in ``/usr/include/linux/soundcard.h``.

``jack``
    JACK (Jack Audio Connection Kit) audio output driver

    ``port=<name>``
        Connects to the ports with the given name (default: physical ports).
    ``name=<client>``
        Client name that is passed to JACK (default: ``mpv``). Useful
        if you want to have certain connections established automatically.
    ``(no-)autostart``
        Automatically start jackd if necessary (default: disabled). Note that
        this tends to be unreliable and will flood stdout with server messages.
    ``(no-)connect``
        Automatically create connections to output ports (default: enabled).
        When enabled, the maximum number of output channels will be limited to
        the number of available output ports.
    ``std-channel-layout=alsa|waveext|any``
        Select the standard channel layout (default: alsa). JACK itself has no
        notion of channel layouts (i.e. assigning which speaker a given
        channel is supposed to map to) - it just takes whatever the application
        outputs, and reroutes it to whatever the user defines. This means the
        user and the application are in charge of dealing with the channel
        layout. ``alsa`` uses the old MPlayer layout, which is inspired by
        ALSA's standard layouts. In this mode, ao_jack will refuse to play 3
        or 7 channels (because these do not really have a defined meaning in
        MPlayer). ``waveext`` uses WAVE_FORMAT_EXTENSIBLE order, which, even
        though it was defined by Microsoft, is the standard on many systems.
        The value ``any`` makes JACK accept whatever comes from the audio
        filter chain, regardless of channel layout and without reordering. This
        mode is probably not very useful, other than for debugging or when used
        with fixed setups.

``coreaudio`` (Mac OS X only)
    Native Mac OS X audio output driver using AudioUnits and the CoreAudio
    sound server.

    Automatically redirects to ``coreaudio_exclusive`` when playing compressed
    formats.

``coreaudio_exclusive`` (Mac OS X only)
    Native Mac OS X audio output driver using direct device access and
    exclusive mode (bypasses the sound server).

    Supports only compressed formats (AC3 and DTS).

``openal``
    Experimental OpenAL audio output driver

    .. note:: This driver is not very useful. Playing multi-channel audio with
              it is slow.

``pulse``
    PulseAudio audio output driver

    ``[<host>][:<output sink>]``
        Specify the host and optionally output sink to use. An empty <host>
        string uses a local connection, "localhost" uses network transfer
        (most likely not what you want).

    ``buffer=<1-2000|native>``
        Set the audio buffer size in milliseconds. A higher value buffers
        more data, and has a lower probability of buffer underruns. A smaller
        value makes the audio stream react faster, e.g. to playback speed
        changes. Default: 250.

    ``latency-hacks=<yes|no>``
        Enable hacks to workaround PulseAudio timing bugs (default: no). If
        enabled, mpv will do elaborate latency calculations on its own. If
        disabled, it will use PulseAudio automatically updated timing
        information. Disabling this might help with e.g. networked audio or
        some plugins, while enabling it might help in some unknown situations
        (it used to be required to get good behavior on old PulseAudio versions).

        If you have stuttering video when using pulse, try to enable this
        option. (Or alternatively, try to update PulseAudio.)

``dsound`` (Windows only)
    DirectX DirectSound audio output driver

    .. note:: This driver is for compatibility with old systems.

    ``device=<devicenum>``
        Sets the device number to use. Playing a file with ``-v`` will show a
        list of available devices.

    ``buffersize=<ms>``
        DirectSound buffer size in milliseconds (default: 200).

``sdl``
    SDL 1.2+ audio output driver. Should work on any platform supported by SDL
    1.2, but may require the ``SDL_AUDIODRIVER`` environment variable to be set
    appropriately for your system.

    .. note:: This driver is for compatibility with extremely foreign
              environments, such as systems where none of the other drivers
              are available.

    ``buflen=<length>``
        Sets the audio buffer length in seconds. Is used only as a hint by the
        sound system. Playing a file with ``-v`` will show the requested and
        obtained exact buffer size. A value of 0 selects the sound system
        default.

    ``bufcnt=<count>``
        Sets the number of extra audio buffers in mpv. Usually needs not be
        changed.

``null``
    Produces no audio output but maintains video playback speed. Use
    ``--ao=null:untimed`` for benchmarking.

    ``untimed``
        Do not simulate timing of a perfect audio device. This means audio
        decoding will go as fast as possible, instead of timing it to the
        system clock.

    ``buffer``
        Simulated buffer length in seconds.

    ``outburst``
        Simulated chunk size in samples.

    ``speed``
        Simulated audio playback speed as a multiplier. Usually, a real audio
        device will not go exactly as fast as the system clock. It will deviate
        just a little, and this option helps simulating this.

    ``latency``
        Simulated device latency. This is additional to EOF.

    ``broken-eof``
        Simulate broken audio drivers, which always add the fixed device
        latency to the reported audio playback position.

    ``broken-delay``
        Simulate broken audio drivers, which don't report latency correctly.

``pcm``
    Raw PCM/WAVE file writer audio output

    ``(no-)waveheader``
        Include or do not include the WAVE header (default: included). When
        not included, raw PCM will be generated.
    ``file=<filename>``
        Write the sound to ``<filename>`` instead of the default
        ``audiodump.wav``. If ``no-waveheader`` is specified, the default is
        ``audiodump.pcm``.
    ``(no-)append``
        Append to the file, instead of overwriting it. Always use this with the
        ``no-waveheader`` option - with ``waveheader`` it's broken, because
        it will write a WAVE header every time the file is opened.

``rsound``
    Audio output to an RSound daemon

    .. note:: Completely useless, unless you intend to run RSound. Not to be
              confused with RoarAudio, which is something completely
              different.

    ``host=<name/path>``
        Set the address of the server (default: localhost).  Can be either a
        network hostname for TCP connections or a Unix domain socket path
        starting with '/'.
    ``port=<number>``
        Set the TCP port used for connecting to the server (default: 12345).
        Not used if connecting to a Unix domain socket.

``sndio``
    Audio output to the OpenBSD sndio sound system

    .. note:: Experimental. There are known bugs and issues.

    (Note: only supports mono, stereo, 4.0, 5.1 and 7.1 channel
    layouts.)

    ``device=<device>``
        sndio device to use (default: ``$AUDIODEVICE``, resp. ``snd0``).

``wasapi``
    Audio output to the Windows Audio Session API.

    ``exclusive``
        Requests exclusive, direct hardware access. By definition prevents
        sound playback of any other program until mpv exits.
    ``device=<id>``
        Uses the requested endpoint instead of the system's default audio
        endpoint. Both an ordinal number (0,1,2,...) and the GUID
        String are valid; the GUID string is guaranteed to not change
        unless the driver is uninstalled.

        Also supports searching active devices by human readable name. If more
        than one device matches the name, refuses loading it.

        This option is mostly deprecated in favour of the more general
        ``--audio-device`` option. That said, ``--audio-device=help`` will give
        a list of valid device GUIDs (prefixed with ``wasapi/``), as well as
        their human readable names, which should work here.
