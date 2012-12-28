.. _audio_outputs:

AUDIO OUTPUT DRIVERS
====================

Audio output drivers are interfaces to different audio output facilities. The
syntax is:

--ao=<driver1[:suboption1[=value]:...],driver2,...[,]>
    Specify a priority list of audio output drivers to be used.

If the list has a trailing ',' mpv will fall back on drivers not contained
in the list. Suboptions are optional and can mostly be omitted.

*NOTE*: See ``--ao=help`` for a list of compiled-in audio output drivers.

*EXAMPLE*:

    - ``--ao=alsa,oss,`` Try the ALSA driver, then the OSS driver, then others.
    - ``--ao=alsa:noblock:device=hw=0.3`` Sets noblock-mode and the device-name
      as first card, fourth device.

Available audio output drivers are:

alsa
    ALSA 0.9/1.x audio output driver

    noblock
        Sets noblock-mode.
    device=<device>
        Sets the device name. Replace any ',' with '.' and any ':' with '=' in
        the ALSA device name. For hwac3 output via S/PDIF, use an "iec958" or
        "spdif" device, unless you really know how to set it correctly.

oss
    OSS audio output driver

    <dsp-device>
        Sets the audio output device (default: ``/dev/dsp``).
    <mixer-device>
        Sets the audio mixer device (default: ``/dev/mixer``).
    <mixer-channel>
        Sets the audio mixer channel (default: pcm).

jack
    audio output through JACK (Jack Audio Connection Kit)

    port=<name>
        Connects to the ports with the given name (default: physical ports).
    name=<client>
        Client name that is passed to JACK (default: mpv [<PID>]). Useful
        if you want to have certain connections established automatically.
    (no-)estimate
        Estimate the audio delay, supposed to make the video playback smoother
        (default: enabled).
    (no-)autostart
        Automatically start jackd if necessary (default: disabled). Note that
        this seems unreliable and will spam stdout with server messages.

coreaudio (Mac OS X only)
    native Mac OS X audio output driver

    device_id=<id>
        ID of output device to use (0 = default device)
    help
        List all available output devices with their IDs.

openal
    Experimental OpenAL audio output driver

pulse
    PulseAudio audio output driver

    [<host>][:<output sink>]
        Specify the host and optionally output sink to use. An empty <host>
        string uses a local connection, "localhost" uses network transfer
        (most likely not what you want).

portaudio
    PortAudio audio output driver. This works on all platforms, and has extensive
    MS Windows support.

    device
        Specify the subdevice to use. Giving ``help`` as device name lists all
        devices found by PortAudio. Devices can be given as numeric values,
        starting from ``1``.

dsound (Windows only)
    DirectX DirectSound audio output driver

    device=<devicenum>
        Sets the device number to use. Playing a file with ``-v`` will show a
        list of available devices.

sdl
    SDL 1.2+ audio output driver. Should work everywhere where SDL 1.2 builds,
    but may require the SDL_AUDIODRIVER environment variable to be set
    appropriately for your system.

    buflen=<length>
        Sets the audio buffer length in seconds. Is used only approximately,
        or even disaregarded entirely by the sound system. Playing a file with
        ``-v`` will show the requested and obtained exact buffer size. A value
        of 0 selects the sound system default.

    bufcnt=<count>
        Sets the number of extra audio buffers in mpv. Usually needs not be
        changed.

null
    Produces no audio output but maintains video playback speed. Use
    ``--no-audio`` for benchmarking.

pcm
    raw PCM/wave file writer audio output

    (no-)waveheader
        Include or do not include the wave header (default: included). When
        not included, raw PCM will be generated.
    file=<filename>
        Write the sound to <filename> instead of the default
        ``audiodump.wav``. If nowaveheader is specified, the default is
        ``audiodump.pcm``.

rsound
    audio output to an RSound daemon

    host=<name/path>
        Set the address of the server (default: localhost).  Can be either a
        network hostname for TCP connections or a Unix domain socket path
        starting with '/'.
    port=<number>
        Set the TCP port used for connecting to the server (default: 12345).
        Not used if connecting to a Unix domain socket.
