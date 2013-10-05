AUDIO OUTPUT DRIVERS
====================

Audio output drivers are interfaces to different audio output facilities. The
syntax is:

``--ao=<driver1[:suboption1[=value]:...],driver2,...[,]>``
    Specify a priority list of audio output drivers to be used.

If the list has a trailing ',', mpv will fall back on drivers not contained
in the list. Suboptions are optional and can mostly be omitted.

.. note::

    See ``--ao=help`` for a list of compiled-in audio output drivers.

.. admonition:: Examples

    - ``--ao=alsa,oss,`` Try the ALSA driver, then the OSS driver, then others.
    - ``--ao=alsa:no-block:device=[hw:0,3]`` Sets noblock-mode and the
      device-name as first card, fourth device.

Available audio output drivers are:

``alsa`` (Linux only)
    ALSA 0.9/1.x audio output driver

    ``no-block``
        Sets noblock-mode.
    ``device=<device>``
        Sets the device name. For ac3 output via S/PDIF, use an "iec958" or
        "spdif" device, unless you really know how to set it correctly.
    ``mixer-device=<device>``
        Set the mixer device used with ``--no-softvol`` (default: ``default``).
    ``mixer-name=<name>``
        Set the name of the mixer element (default: ``Master``). This is for
        example ``PCM`` or ``Master``.
    ``mixer-index=<number>``
        Set the index of the mixer channel (default: 0). Consider the output of
        "``amixer scontrols``", then the index is the number that follows the
        name of the element.

    .. note::

        MPlayer and mplayer2 required you to replace any ',' with '.' and
        any ':' with '=' in the ALSA device name. mpv does not do this anymore.
        Instead, quote the device name:

            ``--ao=alsa:device=[plug:surround50]``

        Note that the ``[`` and ``]`` simply quote the device name. With some
        shells (like zsh), you have to quote the option string to prevent the
        shell from interpreting the brackets instead of passing them to mpv.

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
    ``(no-)estimate``
        Estimate the audio delay, supposed to make the video playback smoother
        (default: enabled).
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
    Native Mac OS X audio output driver

    ``device_id=<id>``
        ID of output device to use (0 = default device)
    ``help``
        List all available output devices with their IDs.

``openal``
    Experimental OpenAL audio output driver

``pulse``
    PulseAudio audio output driver

    ``[<host>][:<output sink>]``
        Specify the host and optionally output sink to use. An empty <host>
        string uses a local connection, "localhost" uses network transfer
        (most likely not what you want).

``portaudio``
    PortAudio audio output driver. This works on all platforms, and has
    extensive MS Windows support.

    ``device``
        Specify the subdevice to use. Giving ``help`` as device name lists all
        devices found by PortAudio. Devices can be given as numeric values,
        starting from ``1``.

``dsound`` (Windows only)
    DirectX DirectSound audio output driver

    ``device=<devicenum>``
        Sets the device number to use. Playing a file with ``-v`` will show a
        list of available devices.

``sdl``
    SDL 1.2+ audio output driver. Should work on any platform supported by SDL
    1.2, but may require the ``SDL_AUDIODRIVER`` environment variable to be set
    appropriately for your system.

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
    ``--no-audio`` for benchmarking.

``pcm``
    Raw PCM/WAVE file writer audio output

    ``(no-)waveheader``
        Include or do not include the WAVE header (default: included). When
        not included, raw PCM will be generated.
    ``file=<filename>``
        Write the sound to ``<filename>`` instead of the default
        ``audiodump.wav``. If ``no-waveheader`` is specified, the default is
        ``audiodump.pcm``.

``rsound``
    Audio output to an RSound daemon

    ``host=<name/path>``
        Set the address of the server (default: localhost).  Can be either a
        network hostname for TCP connections or a Unix domain socket path
        starting with '/'.
    ``port=<number>``
        Set the TCP port used for connecting to the server (default: 12345).
        Not used if connecting to a Unix domain socket.

``sndio``
    Audio output to the OpenBSD sndio sound system

    (Note: only supports mono, stereo, 4.0, 5.1 and 7.1 channel
    layouts.)

    ``device=<device>``
        sndio device to use (default: ``$AUDIODEVICE``, resp. ``snd0``).

``wasapi``
    Audio output to the Windows Audio Session API.

    ``device=<id>``
        Uses the requested endpoint instead of the system's default audio
        endpoint. Both the number and the ID String are valid; the ID String
        is guaranteed to not change unless the driver is uninstalled.

        Also supports searching active devices by name. If more than one
        device matches the name, refuses loading it.

        To get a list of the valid devices, give ``help`` as the id. The
        list is the same as the ``list`` suboption, but stops the player
        initialization.
    ``exclusive``
        Requests exclusive, direct hardware access. By definition prevents
        sound playback of any other program until mpv exits.
    ``list``
        Lists all audio endpoints (output devices) present in the system.
