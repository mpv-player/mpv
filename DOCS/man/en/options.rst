--a52drc=<level>
    Select the Dynamic Range Compression level for AC-3 audio streams. <level>
    is a float value ranging from 0 to 1, where 0 means no compression and 1
    (which is the default) means full compression (make loud passages more
    silent and vice versa). Values up to 2 are also accepted, but are purely
    experimental. This option only shows an effect if the AC-3 stream contains
    the required range compression information.

--abs=<value>
    (``--ao=oss`` only) (OBSOLETE)
    Override audio driver/card buffer size detection.

--ad=<[+|-]family1:(*|decoder1),[+|-]family2:(*|decoder2),...[-]>
    Specify a priority list of audio decoders to be used, according to their
    family and decoder name. Entries like ``family:*`` prioritize all decoders
    of the given family. When determining which decoder to use, the first
    decoder that matches the audio format is selected. If that is unavailable,
    the next decoder is used. Finally, it tries all other decoders that are not
    explicitly selected or rejected by the option.

    ``-`` at the end of the list suppresses fallback to other available
    decoders not on the ``--ad`` list. ``+`` in front of an entry forces the
    decoder. Both of these shouldn't normally be used, because they break
    normal decoder auto-selection!

    ``-`` in front of an entry disables selection of the decoder.

    *EXAMPLE*:

    ``--ad=lavc:mp3float``
        Prefer the FFmpeg/Libav ``mp3float`` decoder over all other mp3
        decoders.

    ``--ad=spdif:ac3,lavc:*``
        Always prefer spdif AC3 over FFmpeg/Libav over anything else.

    ``--ad=help``
        List all available decoders.

--af=<filter1[=parameter1:parameter2:...],filter2,...>
    Specify a list of audio filters to apply to the audio stream. See
    `audio_filters` for details and descriptions of the available filters.
    The option variants ``--af-add``, ``--af-pre``, ``--af-del`` and
    ``--af-clr`` exist to modify a previously specified list, but you
    shouldn't need these for typical use.

--af-adv=<force=(0-7):list=(filters)>
    See also ``--af``.
    Specify advanced audio filter options:

    force=<0-7>
        Forces the insertion of audio filters to one of the following:

        0
            Use completely automatic filter insertion (currently identical to
            1).
        1
            Optimize for accuracy (default).
        2
            Optimize for speed. *Warning*: Some features in the audio filters
            may silently fail, and the sound quality may drop.
        3
            Use no automatic insertion of filters and no optimization.
            *Warning*: It may be possible to crash mpv using this setting.
        4
            Use automatic insertion of filters according to 0 above, but use
            floating point processing when possible.
        5
            Use automatic insertion of filters according to 1 above, but use
            floating point processing when possible.
        6
            Use automatic insertion of filters according to 2 above, but use
            floating point processing when possible.
        7
            Use no automatic insertion of filters according to 3 above, and
            use floating point processing when possible.

    list=<filters>
        Same as ``--af``.

--aid=<ID|auto|no>
    Select audio channel. ``auto`` selects the default, ``no`` disables audio.
    See also ``--alang``.

--alang=<languagecode[,languagecode,...]>
    Specify a priority list of audio languages to use. Different container
    formats employ different language codes. DVDs use ISO 639-1 two letter
    language codes, Matroska, MPEG-TS and NUT use ISO 639-2 three letter
    language codes while OGM uses a free-form identifier. mpv prints the
    available languages when run in verbose (``-v``) mode. See also ``--aid``.

    *EXAMPLE*:

    ``mpv dvd://1 --alang=hu,en``
        Chooses the Hungarian language track on a DVD and falls back on
        English if Hungarian is not available.
    ``mpv --alang=jpn example.mkv``
        Plays a Matroska file in Japanese.

--ao=<driver1[:suboption1[=value]:...],driver2,...[,]>
    Specify a priority list of audio output drivers to be used. For
    interactive use you'd normally specify a single one to use, but in
    configuration files specifying a list of fallbacks may make sense. See
    `audio_outputs` for details and descriptions of available drivers.

--aspect=<ratio>
    Override movie aspect ratio, in case aspect information is incorrect or
    missing in the file being played. See also ``--no-aspect``.

    *EXAMPLE*:

    - ``--aspect=4:3``  or ``--aspect=1.3333``
    - ``--aspect=16:9`` or ``--aspect=1.7777``

--ass, --no-ass
    Render ASS subtitles natively, and convert text subtitles in other formats
    to ASS internally (enabled by default).

    If ``--no-ass`` is specified, all subtitles are converted to plain text
    internally. All tags and style declarations are stripped and ignored. The
    subtitle renderer uses the font style as specified by the ``--sub-text-``
    options instead.

    *NOTE*: Using ``--no-ass`` may lead to incorrect or completely broken
    rendering of ASS/SSA subtitles. It can sometimes be useful to forcibly
    override the styling of ASS subtitles, but should be avoided in general.

--ass-force-style=<[Style.]Param=Value[,...]>
    Override some style or script info parameters.

    *EXAMPLE*:

    - ``--ass-force-style=FontName=Arial,Default.Bold=1``
    - ``--ass-force-style=PlayResY=768``

    *NOTE*: Using this option may lead to incorrect subtitle rendering.

--ass-hinting=<type>
    Set hinting type. <type> can be:

    :0:       no hinting
    :1:       FreeType autohinter, light mode
    :2:       FreeType autohinter, normal mode
    :3:       font native hinter

    The default value is 0 (no hinting).

--ass-line-spacing=<value>
    Set line spacing value for SSA/ASS renderer.

--ass-styles=<filename>
    Load all SSA/ASS styles found in the specified file and use them for
    rendering text subtitles. The syntax of the file is exactly like the ``[V4
    Styles]`` / ``[V4+ Styles]`` section of SSA/ASS.

    *NOTE*: Using this option may lead to incorrect subtitle rendering.

--ass-style-override=<yes|no>
    Control whether user style overrides should be applied.

    :yes: Apply all the ``--ass-*`` style override options. Changing the default
          for any of these options can lead to incorrect subtitle rendering.
          (Default.)
    :no:  Render subtitles as forced by subtitle scripts.

--ass-use-margins
    Enables placing toptitles and subtitles in black borders when they are
    available.

--ass-vsfilter-aspect-compat
    Stretch SSA/ASS subtitles when playing anamorphic videos for compatibility
    with traditional VSFilter behavior. This switch has no effect when the
    video is stored with square pixels.

    The renderer historically most commonly used for the SSA/ASS subtitle
    formats, VSFilter, had questionable behavior that resulted in subtitles
    being stretched too if the video was stored in anamorphic format that
    required scaling for display.  This behavior is usually undesirable and
    newer VSFilter versions may behave differently. However, many existing
    scripts compensate for the stretching by modifying things in the opposite
    direction.  Thus if such scripts are displayed "correctly" they will not
    appear as intended.  This switch enables emulation of the old VSFilter
    behavior (undesirable but expected by many existing scripts).

    Enabled by default.

--audio-demuxer=<[+]name>
    Force audio demuxer type when using ``--audiofile``. Use a '+' before the
    name to force it, this will skip some checks! Give the demuxer name as
    printed by ``--audio-demuxer=help``. ``--audio-demuxer=audio`` forces MP3.

--audio-display=<no|attachment>
    Setting this option to ``attachment`` (default) will display image
    attachments when playing audio files. It will display the first image
    found, and additional images are available as video streams.

    Setting this option to ``no`` disables display of video entirely when
    playing audio files.

    This option has no influence on files with normal video tracks.

--audiofile=<filename>
    Play audio from an external file (WAV, MP3 or Ogg Vorbis) while viewing a
    movie.

--audiofile-cache=<kBytes>
    Enables caching for the stream used by ``--audiofile``, using the
    specified amount of memory.

--autofit=<[W[xH]]>
    Set the initial window size to a maximum size specified by WxH, without
    changing the window's aspect ratio. The size is measured in pixels, or if
    a number is followed by a percentage sign (``%``), in percents of the
    screen size.

    This option never changes the aspect ratio of the window. If the aspect
    ratio mismatches, the window's size is reduced until it fits into the
    specified size.

    Window position is not taken into account, nor is it modified by this
    option (the window manager still may place the window differently depending
    on size). Use ``--geometry`` to change the window position. Its effects
    are applied after this option.

    See ``--geometry`` for details how this is handled with multi-monitor
    setups.

    Use ``--autofit-larger`` instead if you don't want the window to get larger.
    Use ``--geometry`` if you want to force both window width and height to a
    specific size.

    *NOTE*: Generally only supported by GUI VOs. Ignored for encoding.

    *EXAMPLE*:

    ``70%``
        Make the window width 70% of the screen size, keeping aspect ratio.
    ``1000``
        Set the window width to 1000 pixels, keeping aspect ratio.
    ``70%:60%``
        Make the window as large as possible, without being wider than 70% of
        the screen width, or higher than 60% of the screen height.

--autofit-larger=<[W[xH]]>
    This option behaves exactly like ``--autofit``, except the window size is
    only changed if the window would be larger than the specified size.

    *EXAMPLE*:

    ``90%x80%``
        If the video is larger than 90% of the screen width or 80% of the
        screen height, make the window smaller until either its width is 90%
        of the screen, or its height is 80% of the screen.

--autosub, --no-autosub
    Load additional subtitle files matching the video filename. Enabled by
    default. See also ``--autosub-match``.

--autosub-match=<exact|fuzzy|all>
    Adjust matching fuzziness when searching for subtitles:

    :exact: exact match
    :fuzzy: Load all subs containing movie name.
    :all:   Load all subs in the current and ``--sub-paths`` directories.

    (Default: exact.)

--autosync=<factor>
    Gradually adjusts the A/V sync based on audio delay measurements.
    Specifying ``--autosync=0``, the default, will cause frame timing to be
    based entirely on audio delay measurements. Specifying ``--autosync=1``
    will do the same, but will subtly change the A/V correction algorithm. An
    uneven video framerate in a movie which plays fine with ``--no-audio`` can
    often be helped by setting this to an integer value greater than 1. The
    higher the value, the closer the timing will be to ``--no-audio``. Try
    ``--autosync=30`` to smooth out problems with sound drivers which do not
    implement a perfect audio delay measurement. With this value, if large A/V
    sync offsets occur, they will only take about 1 or 2 seconds to settle
    out. This delay in reaction time to sudden A/V offsets should be the only
    side-effect of turning this option on, for all sound drivers.

--bandwidth=<Bytes>
    Specify the maximum bandwidth for network streaming (for servers that are
    able to send content in different bitrates). Useful if you want to watch
    live streamed media behind a slow connection. With Real RTSP streaming, it
    is also used to set the maximum delivery bandwidth allowing faster cache
    filling and stream dumping.

    *NOTE*: probably broken/useless.

--untimed
    Do not sleep when outputting video frames. Useful for benchmarks when used
    with --no-audio.

--bluray-angle=<ID>
    Some Blu-ray discs contain scenes that can be viewed from multiple angles.
    Here you can tell mpv which angles to use (default: 1).

--bluray-device=<path>
    (Blu-ray only)
    Specify the Blu-ray disc location. Must be a directory with Blu-ray
    structure.

--border, --no-border
    Play movie with window border and decorations. Since this is on by
    default, use ``--no-border`` to disable the standard window decorations.

--brightness=<-100-100>
    Adjust the brightness of the video signal (default: 0). Not supported by
    all video output drivers.

--cache=<kBytes>
    Enable caching of the input stream (if not already enabled) and set the
    size of the cache in kilobytes. Caching is enabled by default (with a
    default cache size) for network streams. May be useful when playing files
    from slow media, but can also have negative effects, especially with file
    formats that require a lot of seeking, such as mp4. See also ``--no-cache``.

    Note that half the cache size will be used to allow fast seeking back. This
    is also the reason why a full cache is usually reported as 50% full. The
    cache fill display does not include the part of the cache reserved for
    seeking back. Likewise, when starting a file the cache will be at 100%,
    because no space is reserved for seeking back yet.

--cache-pause=<no|percentage>
    If the cache percentage goes below the specified value, pause and wait
    until the percentage set by ``--cache-min`` is reached, then resume
    playback (default: 10). If ``no`` is specified, this behavior is disabled.

    When the player is paused this way, the status line shows ``Buffering``
    instead of ``Paused``, and the OSD uses a clock symbol instead of the
    normal paused symbol.

--cache-min=<percentage>
    Playback will start when the cache has been filled up to <percentage> of
    the total (default: 20).

--cache-seek-min=<percentage>
    If a seek is to be made to a position within <percentage> of the cache
    size from the current position, mpv will wait for the cache to be
    filled to this position rather than performing a stream seek (default:
    50).

--cdda=<option1:option2>
    This option can be used to tune the CD Audio reading feature of mpv.

    Available options are:

    speed=<value>
        Set CD spin speed.

    paranoia=<0-2>
        Set paranoia level. Values other than 0 seem to break playback of
        anything but the first track.

        :0: disable checking (default)
        :1: overlap checking only
        :2: full data correction and verification

    generic-dev=<value>
        Use specified generic SCSI device.

    sector-size=<value>
        Set atomic read size.

    overlap=<value>
        Force minimum overlap search during verification to <value> sectors.

    toc-bias
        Assume that the beginning offset of track 1 as reported in the TOC
        will be addressed as LBA 0. Some Toshiba drives need this for getting
        track boundaries correct.

    toc-offset=<value>
        Add <value> sectors to the values reported when addressing tracks. May
        be negative.

    (no-)skip
        (Never) accept imperfect data reconstruction.

--cdrom-device=<path>
    Specify the CD-ROM device (default: ``/dev/cdrom``).

--channels=<number>
    Request the number of playback channels (default: 2). mpv asks the
    decoder to decode the audio into as many channels as specified. Then it is
    up to the decoder to fulfill the requirement. This is usually only
    important when playing videos with AC-3 audio (like DVDs). In that case
    liba52 does the decoding by default and correctly downmixes the audio into
    the requested number of channels. To directly control the number of output
    channels independently of how many channels are decoded, use the channels
    filter (``--af=channels``).

    *NOTE*: This option is honored by codecs (AC-3 only), filters (surround)
    and audio output drivers (OSS at least).

    Available options are:

    :2: stereo
    :4: surround
    :6: full 5.1
    :8: full 7.1

--chapter=<start[-end]>
    Specify which chapter to start playing at. Optionally specify which
    chapter to end playing at. Also see ``--start``.

--chapter-merge-threshold=<number>
    Threshold for merging almost consecutive ordered chapter parts in
    milliseconds (default: 100). Some Matroska files with ordered chapters
    have inaccurate chapter end timestamps, causing a small gap between the
    end of one chapter and the start of the next one when they should match.
    If the end of one playback part is less than the given threshold away from
    the start of the next one then keep playing video normally over the
    chapter change instead of doing a seek.

--colormatrix=<colorspace>
    Controls the YUV to RGB color space conversion when playing video. There
    are various standards. Normally, BT.601 should be used for SD video, and
    BT.709 for HD video. (This is done by default.) Using incorrect color space
    results in slightly under or over saturated and shifted colors.

    The color space conversion is additionally influenced by the related
    options --colormatrix-input-range and --colormatrix-output-range.

    These options are not always supported. Different video outputs provide
    varying degrees of support. The opengl and vdpau video output drivers usually
    offer full support. The xv output can set the color space if the system
    video driver supports it, but not input and output levels. The scale video
    filter can configure color space and input levels, but only if the output
    format is RGB (if the video output driver supports RGB output, you can
    force this with ``-vf scale,format=rgba``).

    If this option is set to ``auto`` (which is the default), the video's
    color space flag will be used. If that flag is unset, the color space
    will be selected automatically. This is done using a simple heuristic that
    attempts to distinguish SD and HD video. If the video is larger than
    1279x576 pixels, BT.709 (HD) will be used; otherwise BT.601 (SD) is
    selected.

    Available color spaces are:

    :auto:          automatic selection (default)
    :BT.601:        ITU-R BT.601 (SD)
    :BT.709:        ITU-R BT.709 (HD)
    :SMPTE-240M:    SMPTE-240M

--colormatrix-input-range=<color-range>
    YUV color levels used with YUV to RGB conversion. This option is only
    necessary when playing broken files, which don't follow standard color
    levels or which are flagged wrong. If the video doesn't specify its
    color range, it is assumed to be limited range.

    The same limitations as with --colormatrix apply.

    Available color ranges are:

    :auto:      automatic selection (normally limited range) (default)
    :limited:   limited range (16-235 for luma, 16-240 for chroma)
    :full:      full range (0-255 for both luma and chroma)

--colormatrix-output-range=<color-range>
    RGB color levels used with YUV to RGB conversion. Normally, output devices
    such as PC monitors use full range color levels. However, some TVs and
    video monitors expect studio level RGB. Providing full range output to a
    device expecting studio level input results in crushed blacks and whites,
    the reverse in dim grey blacks and dim whites.

    The same limitations as with --colormatrix apply.

    Available color ranges are:

    :auto:      automatic selection (equals to full range) (default)
    :limited:   limited range (16-235 per component), studio levels
    :full:      full range (0-255 per component), PC levels

--colorkey=<number>
    Changes the colorkey to an RGB value of your choice. 0x000000 is black and
    0xffffff is white. Only supported by the xv (see ``--vo=xv:ck``) video
    output driver. See also ``--no-colorkey``.

--consolecontrols, --no-consolecontrols
    ``--no-consolecontrols`` prevents the player from reading key events from
    standard input. Useful when reading data from standard input. This is
    automatically enabled when ``-`` is found on the command line. There are
    situations where you have to set it manually, e.g. if you open
    ``/dev/stdin`` (or the equivalent on your system), use stdin in a playlist
    or intend to read from stdin later on via the loadfile or loadlist slave
    commands.

--contrast=<-100-100>
    Adjust the contrast of the video signal (default: 0). Not supported by all
    video output drivers.

--cookies, --no-cookies
    (network only)
    Support cookies when making HTTP requests. Disabled by default.

--cookies-file=<filename>
    (network only)
    Read HTTP cookies from <filename>. The file is
    assumed to be in Netscape format.

--correct-pts, --no-correct-pts
    Switches mpv to a mode where timestamps for video frames are
    calculated differently and video filters which add new frames or modify
    timestamps of existing ones are supported. Now enabled automatically for
    most common file formats. The more accurate timestamps can be visible for
    example when playing subtitles timed to scene changes with the ``--ass``
    option. Without ``--correct-pts`` the subtitle timing will typically be
    off by some frames. This option does not work correctly with some demuxers
    and codecs.

--cursor-autohide=<number|no|always>
    Make mouse cursor automatically hide after given number of milliseconds.
    ``no`` will disable cursor autohide. ``always`` means the cursor
    will stay hidden. Supported by video output drivers which use X11 or
    OS X Cocoa.

--audio-delay=<sec>
    audio delay in seconds (positive or negative float value). Negative values
    delay the audio, and positive values delay the video.

--demuxer=<[+]name>
    Force demuxer type. Use a '+' before the name to force it, this will skip
    some checks! Give the demuxer name as printed by ``--demuxer=help``.

--doubleclick-time=<milliseconds>
    Time in milliseconds to recognize two consecutive button presses as a
    double-click (default: 300).

--dtshd, --no-dtshd
    When using DTS passthrough, output any DTS-HD track as-is.
    With ``--no-dtshd`` (the default) only the DTS Core parts will be output.

    DTS-HD tracks can be sent over HDMI but not over the original
    coax/toslink S/PDIF system.

--dvbin=<options>
    Pass the following parameters to the DVB input module, in order to
    override the default ones:

    :card=<1-4>:      Specifies using card number 1-4 (default: 1).
    :file=<filename>: Instructs mpv to read the channels list from
                      <filename>. Default is
                      ``~/.mpv/channels.conf.{sat,ter,cbl,atsc}`` (based
                      on your card type) or ``~/.mpv/channels.conf`` as a
                      last resort.
    :timeout=<1-30>:  Maximum number of seconds to wait when trying to tune a
                      frequency before giving up (default: 30).

--dvd-device=<path>
    Specify the DVD device or .iso filename (default: ``/dev/dvd``). You can
    also specify a directory that contains files previously copied directly
    from a DVD (with e.g. vobcopy).

--dvd-speed=<speed>
    Try to limit DVD speed (default: 0, no change). DVD base speed is 1385
    kB/s, so a 8x drive can read at speeds up to 11080 kB/s. Slower speeds
    make the drive more quiet. For watching DVDs 2700 kB/s should be quiet and
    fast enough. mpv resets the speed to the drive default value on close.
    Values of at least 100 mean speed in kB/s. Values less than 100 mean
    multiples of 1385 kB/s, i.e. ``--dvd-speed=8`` selects 11080 kB/s.

    *NOTE*: You need write access to the DVD device to change the speed.

--dvdangle=<ID>
    Some DVD discs contain scenes that can be viewed from multiple angles.
    Here you can tell mpv which angles to use (default: 1).

--edition=<ID>
    (Matroska files only)
    Specify the edition (set of chapters) to use, where 0 is the first. If set
    to -1 (the default), mpv will choose the first edition declared as a
    default, or if there is no default, the first edition defined.

--embeddedfonts, --no-embeddedfonts
    Use fonts embedded in Matroska container files and ASS scripts (default:
    enabled). These fonts can be used for SSA/ASS subtitle rendering
    (``--ass`` option).

--end=<time>
    Stop at given absolute time. Use ``--length`` if the time should be relative
    to ``--start``. See ``--start`` for valid option values and examples.

--no-extbased, --extbased
    ``--no-extbased`` disables extension-based demuxer selection. By default, when the file type
    (demuxer) cannot be detected reliably (the file has no header or it is not
    reliable enough), the filename extension is used to select the demuxer.
    Always falls back on content-based demuxer selection.

--field-dominance=<auto|top|bottom>
    Set first field for interlaced content. Useful for deinterlacers that
    double the framerate: ``--vf=yadif=1`` and ``--vo=vdpau:deint``.

    :auto:    (default) If the decoder does not export the appropriate
              information, it falls back to ``top`` (top field first).
    :top:     top field first
    :bottom:  bottom field first

--no-fixed-vo, --fixed-vo
    ``--no-fixed-vo`` enforces closing and reopening the video window for
    multiple files (one (un)initialization for all files).

--flip
    Flip image upside-down.

--force-rgba-osd-rendering
    Change how some video outputs render the OSD and text subtitles. This
    does not change appearance of the subtitles and only has performance
    implications. For VOs which support native ASS rendering (like ``vdpau``,
    ``opengl``, ``direct3d``), this can be slightly faster or slower,
    depending on GPU drivers and hardware. For other VOs, this just makes
    rendering slower.

--force-window-position
    Forcefully move mpv's video output window to default location whenever
    there is a change in video parameters, video stream or file. This used to
    be the default behavior. Currently only affects X11 VOs.

--sub-forced-only
    Display only forced subtitles for the DVD subtitle stream selected by e.g.
    ``--slang``.

--forceidx
    Force index rebuilding. Useful for files with broken index (A/V desync,
    etc). This will enable seeking in files where seeking was not possible.

    *NOTE*: This option only works if the underlying media supports seeking
    (i.e. not with stdin, pipe, etc).

--format=<format>
    Select the sample format used for output from the audio filter layer to
    the sound card. The values that <format> can adopt are listed below in the
    description of the ``format`` audio filter.

--fps=<float>
    Override video framerate. Useful if the original value is wrong or missing.

    *NOTE*: Works in ``--no-correct-pts`` mode only.

--framedrop=<no|yes|hard>
    Skip displaying some frames to maintain A/V sync on slow systems. Video
    filters are not applied to such frames. For B-frames even decoding is
    skipped completely. May produce unwatchably choppy output. With ``hard``,
    decoding and output of any frame can be skipped, and will lead to an even
    worse playback experience.

    *NOTE*: Practical use of this feature is questionable. Disabled by default.

--frames=<number>
    Play/convert only first <number> video frames, then quit. For audio only,
    run <number> iteration of the playback loop, which is most likely not what
    you want. (This behavior also applies to the corner case when there are
    less video frames than <number>, and audio is longer than the video.)

--fullscreen, --fs
    Fullscreen playback (centers movie, and paints black bands around it).


--fs-screen=<all|current|0-32>
    In multi-monitor configurations (i.e. a single desktop that spans across
    multiple displays) this option tells mpv which screen to go fullscreen to.
    If ``default`` is provided mpv will fallback to using the behaviour
    depending on what the user provided with the ``screen`` option.

    *NOTE (X11)*: this option does not work properly with all window managers.
    ``all`` in particular will usually only work with ``--fstype=-fullscreen``
    or ``--fstype=none``, and even then only with some window managers.

    *NOTE (OSX)*: ``all`` doesn't work on OSX and will behave like ``current``.

    See also ``--screen``.

--fsmode-dontuse=<0-31>
    OBSOLETE, use the ``--fs`` option.
    Try this option if you still experience fullscreen problems.

--fstype=<type1,type2,...>
    (X11 only)
    Specify a priority list of fullscreen modes to be used. You can negate the
    modes by prefixing them with '-'. If you experience problems like the
    fullscreen window being covered by other windows try using a different
    order.

    *NOTE*: See ``--fstype=help`` for a full list of available modes.

    The available types are:

    above
        Use the ``_NETWM_STATE_ABOVE`` hint if available.
    below
        Use the ``_NETWM_STATE_BELOW`` hint if available.
    fullscreen
        Use the ``_NETWM_STATE_FULLSCREEN`` hint if available.
    layer
        Use the ``_WIN_LAYER`` hint with the default layer.
    layer=<0...15>
        Use the ``_WIN_LAYER`` hint with the given layer number.
    netwm
        Force NETWM style.
    none
        Clear the list of modes; you can add modes to enable afterward.
    stays_on_top
        Use ``_NETWM_STATE_STAYS_ON_TOP`` hint if available.

    *EXAMPLE*:

    ``--fstype=layer,stays_on_top,above,fullscreen``
         Default order, will be used as a fallback if incorrect or
         unsupported modes are specified.
    ``--fstype=fullscreen``
         Fixes fullscreen switching on OpenBox 1.x.

--gamma=<-100-100>
    Adjust the gamma of the video signal (default: 0). Not supported by all
    video output drivers.

--gapless-audio
    Try to play consecutive audio files with no silence or disruption at the
    point of file change. This feature is implemented in a simple manner and
    relies on audio output device buffering to continue playback while moving
    from one file to another. If playback of the new file starts slowly, for
    example because it's played from a remote network location or because you
    have specified cache settings that require time for the initial cache
    fill, then the buffered audio may run out before playback of the new file
    can start.

    *NOTE*: The audio device is opened using parameters chosen according to
    the first file played and is then kept open for gapless playback. This
    means that if the first file for example has a low samplerate then the
    following files may get resampled to the same low samplerate, resulting in
    reduced sound quality. If you play files with different parameters,
    consider using options such as ``--srate`` and ``--format`` to explicitly
    select what the shared output format will be.

--geometry=<[W[xH]][+-x+-y]>, --geometry=<x:y>
    Adjust the initial window position or size. W and H set the window size in
    pixels. x and y set the window position, measured in pixels from the
    top-left of the screen to the top-left of the image being displayed. If a
    percentage sign (``%``) is given after the argument it turns the value into
    a percentage of the screen size in that direction. Positions are specified
    similar to the standard X11 ``--geometry`` option format, in which e.g.
    +10-50 means "place 10 pixels from the left border and 50 pixels from the
    lower border" and "--20+-10" means "place 20 pixels beyond the right and
    10 pixels beyond the top border".

    If an external window is specified using the ``--wid`` option, this
    option is ignored.

    The coordinates are relative to the screen given with ``--screen`` for the
    video output drivers that fully support ``--screen``.

    *NOTE*: Generally only supported by GUI VOs. Ignored for encoding.

    *NOTE (OSX)*: On Mac OSX the origin of the screen coordinate system is
    located on the the bottom-left corner. For instance, ``0:0`` will place the
    window at the bottom-left of the screen.

    *NOTE (X11)*: this option does not work properly with all window managers.

    *EXAMPLE*:

    ``50:40``
        Places the window at x=50, y=40.
    ``50%:50%``
        Places the window in the middle of the screen.
    ``100%:100%``
        Places the window at the bottom right corner of the screen.
    ``50%``
        Sets the window width to half the screen width. Window height is set so
        that the window has the video aspect ratio.
    ``50%x50%``
        Forces the window width and height to half the screen width and height.
        Will show black borders to compensate for the video aspect ration (with
        most VOs and without ``--no-keepaspect``).
    ``50%+10+10``
        Sets the window to half the screen widths, and positions it 10 pixels
        below/left of the top left corner of the screen.

    See also ``--autofit`` and ``--autofit-larger`` for fitting the window into
    a given size without changing aspect ratio.

--grabpointer, --no-grabpointer
    ``--no-grabpointer`` tells the player to not grab the mouse pointer after a
    video mode change (``--vm``). Useful for multihead setups.

--heartbeat-cmd
    Command that is executed every 30 seconds during playback via *system()* -
    i.e. using the shell. The time between the commands can be customized with
    the ``--heartbeat-interval`` option.

    *NOTE*: mpv uses this command without any checking. It is your
    responsibility to ensure it does not cause security problems (e.g. make
    sure to use full paths if "." is in your path like on Windows). It also
    only works when playing video (i.e. not with ``--no-video`` but works with
    ``-vo=null``).

    This can be "misused" to disable screensavers that do not support the
    proper X API (see also ``--stop-xscreensaver``). If you think this is too
    complicated, ask the author of the screensaver program to support the
    proper X APIs.

    *EXAMPLE for xscreensaver*: ``mpv --heartbeat-cmd="xscreensaver-command
    -deactivate" file``

    *EXAMPLE for GNOME screensaver*: ``mpv
    --heartbeat-cmd="gnome-screensaver-command -p" file``

--heartbeat-interval=<sec>
    Time between ``--heartbeat-cmd`` invocations in seconds (default: 30).

--help
    Show short summary of options.

--hr-seek=<no|absolute|yes>
    Select when to use precise seeks that are not limited to keyframes. Such
    seeks require decoding video from the previous keyframe up to the target
    position and so can take some time depending on decoding performance. For
    some video formats precise seeks are disabled. This option selects the
    default choice to use for seeks; it's possible to explicitly override that
    default in the definition of key bindings and in slave mode commands.

    :no:       Never use precise seeks.
    :absolute: Use precise seeks if the seek is to an absolute position in the
               file, such as a chapter seek, but not for relative seeks like
               the default behavior of arrow keys (default).
    :yes:      Use precise seeks whenever possible.

--hr-seek-demuxer-offset=<seconds>
    This option exists to work around failures to do precise seeks (as in
    ``--hr-seek``) caused by bugs or limitations in the demuxers for some file
    formats. Some demuxers fail to seek to a keyframe before the given target
    position, going to a later position instead. The value of this option is
    subtracted from the time stamp given to the demuxer. Thus if you set this
    option to 1.5 and try to do a precise seek to 60 seconds, the demuxer is
    told to seek to time 58.5, which hopefully reduces the chance that it
    erroneously goes to some time later than 60 seconds. The downside of
    setting this option is that precise seeks become slower, as video between
    the earlier demuxer position and the real target may be unnecessarily
    decoded.

--http-header-fields=<field1,field2>
    Set custom HTTP fields when accessing HTTP stream.

    *EXAMPLE*:

            ``mpv --http-header-fields='Field1: value1','Field2: value2' http://localhost:1234``

        Will generate HTTP request:

            | GET / HTTP/1.0
            | Host: localhost:1234
            | User-Agent: MPlayer
            | Icy-MetaData: 1
            | Field1: value1
            | Field2: value2
            | Connection: close

--hue=<-100-100>
    Adjust the hue of the video signal (default: 0). You can get a colored
    negative of the image with this option. Not supported by all video output
    drivers.

--hwdec=<api>
    Specify the hardware video decoding API that should be used if possible.
    Whether hardware decoding is actually done depends on the video codec. If
    hardware decoding is not possible, mpv will fall back to software decoding.

    <api> can be one of the following:

    :no:        always use software decoding (default)
    :vdpau:     works with nvidia drivers only, requires ``--vo=vdpau``
    :vda:       OSX
    :crystalhd: Broadcom Crystal HD

--hwdec-codecs=<codec1,codec2,...|all>
    Allow hardware decoding for a given list of codecs only. The default is the
    special value ``all``, which always allows all codecs.

    This is usually only needed with broken GPUs, where fallback to software
    decoding doesn't work properly.

    *EXAMPLE*:

    - ``mpv --hwdec=vdpau --vo=vdpau --hwdec-codecs=h264,mpeg2video``
       Enable vdpau decoding for h264 and mpeg2 only.

--identify
    Deprecated. Use ``TOOLS/mpv_identify.sh``.

--idle
    Makes mpv wait idly instead of quitting when there is no file to play.
    Mostly useful in slave mode where mpv can be controlled through input
    commands (see also ``--slave-broken``).

--idx
    Rebuilds index of files if no index was found, allowing seeking. Useful
    with broken/incomplete downloads, or badly created files. Now this is done
    automatically by the demuxers used for most video formats, meaning that
    this switch has no effect in the typical case. See also ``--forceidx``.

    *NOTE*: This option only works if the underlying media supports seeking
    (i.e. not with stdin, pipe, etc).

--ignore-start
    Matters with the builtin AVI demuxer only, which is not enabled by default.
    Ignore the specified starting time for streams in AVI files. This
    nullifies stream delays.

--include=<configuration-file>
    Specify configuration file to be parsed after the default ones.

--initial-audio-sync, --no-initial-audio-sync
    When starting a video file or after events such as seeking mpv will by
    default modify the audio stream to make it start from the same timestamp
    as video, by either inserting silence at the start or cutting away the
    first samples. Disabling this option makes the player behave like older
    mpv versions did: video and audio are both started immediately even if
    their start timestamps differ, and then video timing is gradually adjusted
    if necessary to reach correct synchronization later.

--input-conf=<filename>
    Specify input configuration file other than the default
    ``~/.mpv/input.conf``.

--input-ar-delay
    Delay in milliseconds before we start to autorepeat a key (0 to
    disable).

--input-ar-rate
    Number of key presses to generate per second on autorepeat.

--no-input-default-bindings
    Disable mpv default (builtin) key bindings.

--input-keylist
    Prints all keys that can be bound to commands.

--input-cmdlist
    Prints all commands that can be bound to keys.

--input-js-dev
    Specifies the joystick device to use (default: ``/dev/input/js0``).

--input-file=<filename>
    Read commands from the given file. Mostly useful with a FIFO.
    See also ``--slave-broken``.

    *NOTE*: When the given file is a FIFO mpv opens both ends so you
    can do several `echo "seek 10" > mp_pipe` and the pipe will stay
    valid.

--input-test
    Input test mode. Instead of executing commands on key presses, mpv
    will show the keys and the bound commands on the OSD. Has to be used
    with a dummy video, and the normal ways to quit the player will not
    work (key bindings that normally quit will be shown on OSD only, just
    like any other binding).

--ipv4-only-proxy
    Skip any HTTP proxy for IPv6 addresses. It will still be used for IPv4
    connections.

    *WARNING*: works with the deprecated ``mp_http://`` protocol only.

--joystick, --no-joystick
    Enable/disable joystick support. Enabled by default.

--no-keepaspect, --keepaspect
    --no-keepaspect will always stretch the video to window size, and will
    disable the window manager hints that force the window aspect ratio.
    (Ignored in fullscreen mode.)

--keep-open
    Do not terminate when playing or seeking beyond the end of the file.
    Instead, pause the player. When trying to seek beyond end of the file, the
    player will pause at an arbitrary playback position (or, in corner cases,
    not redraw the window at all).

    *NOTE*: this option is not respected when using ``--frames``, ``--end``,
    ``--length``, or when passing a chapter range to ``--chapter``. Explicitly
    skipping to the next file or skipping beyond the last chapter will terminate
    playback as well, even if ``--keep-open`` is given.

--key-fifo-size=<2-65000>
    Specify the size of the FIFO that buffers key events (default: 7). If it
    is too small some events may be lost. The main disadvantage of setting it
    to a very large value is that if you hold down a key triggering some
    particularly slow command then the player may be unresponsive while it
    processes all the queued commands.

--lavdopts=<option1:option2:...>
    Specify libavcodec decoding parameters. Separate multiple options with a
    colon.

    *EXAMPLE*: ``--lavdopts=gray:skiploopfilter=all:skipframe=nonref``

    Available options are:

    bitexact
        Only use bit-exact algorithms in all decoding steps (for codec
        testing).

    bug=<value>
        Manually work around encoder bugs.

        :0:    nothing
        :1:    autodetect bugs (default)
        :2:    (msmpeg4v3): some old lavc generated msmpeg4v3 files (no
               autodetection)
        :4:    (mpeg4): Xvid interlacing bug (autodetected if fourcc==XVIX)
        :8:    (mpeg4): UMP4 (autodetected if fourcc==UMP4)
        :16:   (mpeg4): padding bug (autodetected)
        :32:   (mpeg4): illegal vlc bug (autodetected per fourcc)
        :64:   (mpeg4): Xvid and DivX qpel bug (autodetected per
               fourcc/version)
        :128:  (mpeg4): old standard qpel (autodetected per fourcc/version)
        :256:  (mpeg4): another qpel bug (autodetected per fourcc/version)
        :512:  (mpeg4): direct-qpel-blocksize bug (autodetected per
               fourcc/version)
        :1024: (mpeg4): edge padding bug (autodetected per fourcc/version)

    debug=<value>
        Display debugging information.

        :0:      disabled
        :1:      picture info
        :2:      rate control
        :4:      bitstream
        :8:      macroblock (MB) type
        :16:     per-block quantization parameter (QP)
        :32:     motion vector
        :0x0040: motion vector visualization
        :0x0080: macroblock (MB) skip
        :0x0100: startcode
        :0x0200: PTS
        :0x0400: error resilience
        :0x0800: memory management control operations (H.264)
        :0x1000: bugs
        :0x2000: Visualize quantization parameter (QP), lower QP are tinted
                 greener.
        :0x4000: Visualize block types.

    ec=<value>
        Set error concealment strategy.

        :1: Use strong deblock filter for damaged MBs.
        :2: iterative motion vector (MV) search (slow)
        :3: all (default)

    fast (MPEG-2, MPEG-4, and H.264 only)
        Enable optimizations which do not comply to the specification and
        might potentially cause problems, like simpler dequantization, simpler
        motion compensation, assuming use of the default quantization matrix,
        assuming YUV 4:2:0 and skipping a few checks to detect damaged
        bitstreams.

    gray
        grayscale only decoding (a bit faster than with color)

    idct=<0-99>
        For best decoding quality use the same IDCT algorithm for decoding and
        encoding. This may come at a price in accuracy, though.

    o=<key>=<value>[,<key>=<value>[,...]]
        Pass AVOptions to libavcodec decoder. Note, a patch to make the o=
        unneeded and pass all unknown options through the AVOption system is
        welcome. A full list of AVOptions can be found in the FFmpeg manual.

        *EXAMPLE*: ``o=debug=pict``

    sb=<number> (MPEG-2 only)
        Skip the given number of macroblock rows at the bottom.

    st=<number> (MPEG-2 only)
        Skip the given number of macroblock rows at the top.

    skiploopfilter=<skipvalue> (H.264 only)
        Skips the loop filter (AKA deblocking) during H.264 decoding. Since
        the filtered frame is supposed to be used as reference for decoding
        dependent frames this has a worse effect on quality than not doing
        deblocking on e.g. MPEG-2 video. But at least for high bitrate HDTV
        this provides a big speedup with no visible quality loss.

        <skipvalue> can be one of the following:

        :none:    Never skip.
        :default: Skip useless processing steps (e.g. 0 size packets in AVI).
        :nonref:  Skip frames that are not referenced (i.e. not used for
                  decoding other frames, the error cannot "build up").
        :bidir:   Skip B-Frames.
        :nonkey:  Skip all frames except keyframes.
        :all:     Skip all frames.

    skipidct=<skipvalue> (MPEG-1/2 only)
        Skips the IDCT step. This degrades quality a lot of in almost all
        cases (see skiploopfilter for available skip values).

    skipframe=<skipvalue>
        Skips decoding of frames completely. Big speedup, but jerky motion and
        sometimes bad artifacts (see skiploopfilter for available skip
        values).

    threads=<0-16>
        Number of threads to use for decoding. Whether threading is actually
        supported depends on codec. 0 means autodetect number of cores on the
        machine and use that, up to the maximum of 16. (default: 0)

    vismv=<value>
        Visualize motion vectors.

        :0: disabled
        :1: Visualize forward predicted MVs of P-frames.
        :2: Visualize forward predicted MVs of B-frames.
        :4: Visualize backward predicted MVs of B-frames.


--lavfdopts=<option1:option2:...>
    Specify parameters for libavformat demuxers (``--demuxer=lavf``). Separate
    multiple options with a colon.

    Available suboptions are:

    analyzeduration=<value>
        Maximum length in seconds to analyze the stream properties.
    probescore=<1-100>
        Minimum required libavformat probe score. Lower values will require
        less data to be loaded (makes streams start faster), but makes file
        format detection less reliable. Can be used to force auto-detected
        libavformat demuxers, even if libavformat considers the detection not
        reliable enough. (Default: 26.)
    format=<value>
        Force a specific libavformat demuxer.
    o=<key>=<value>[,<key>=<value>[,...]]
        Pass AVOptions to libavformat demuxer.

        Note, a patch to make the *o=* unneeded and pass all unknown options
        through the AVOption system is welcome. A full list of AVOptions can
        be found in the FFmpeg manual. Note that some options may conflict
        with mpv options.

        *EXAMPLE*: ``o=fflags=+ignidx``
    probesize=<value>
        Maximum amount of data to probe during the detection phase. In the
        case of MPEG-TS this value identifies the maximum number of TS packets
        to scan.
    cryptokey=<hexstring>
        Encryption key the demuxer should use. This is the raw binary data of
        the key converted to a hexadecimal string.

--length=<relative time>
    Stop after a given time relative to the start time.
    See ``--start`` for valid option values and examples.

--lirc, --no-lirc
    Enable/disable LIRC support. Enabled by default.

--lircconf=<filename>
    (LIRC only)
    Specifies a configuration file for LIRC (default: ``~/.lircrc``).

--list-options
    Prints all available options.

--list-properties
    Print a list of the available properties.

--loop=<number|inf|no>
    Loops playback <number> times. ``inf`` means forever and ``no`` disables
    looping. If several files are specified on command line, the whole playlist
    is looped.

--mc=<seconds/frame>
    Maximum A-V sync correction per frame (in seconds)

--mf=<option1:option2:...>
    Used when decoding from multiple PNG or JPEG files with ``mf://``.

    Available options are:

    :fps=<value>:  output fps (default: 25)
    :type=<value>: input file type (available: jpeg, png, tga, sgi)

--mkv-subtitle-preroll
    Try harder to show embedded soft subtitles when seeking somewhere. Normally,
    it can happen that the subtitle at the seek target is not shown due to how
    some container file formats are designed. The subtitles appear only if
    seeking before or exactly to the position a subtitle first appears. To
    make this worse, subtitles are often timed to appear a very small amount
    before the associated video frame, so that seeking to the video frame
    typically does not demux the subtitle at that position.

    Enabling this option makes the demuxer start reading data a bit before the
    seek target, so that subtitles appear correctly. Note that this makes
    seeking slower, and is not guaranteed to always work. It only works if the
    subtitle is close enough to the seek target.

    Works with the internal Matroska demuxer only. Always enabled for absolute
    and hr-seeks, and this option changes behavior with relative or imprecise
    seeks only.

    See also ``--hr-seek-demuxer-offset`` option. This option can achieve a
    similar effect, but only if hr-seek is active. It works with any demuxer,
    but makes seeking much slower, as it has to decode audio and video data,
    instead of just skipping over it.

--mixer=<device>
    Use a mixer device different from the default ``/dev/mixer``. For ALSA
    this is the mixer name.

--mixer-channel=<name[,index]>
    (``--ao=oss`` and ``--ao=alsa`` only)
    This option will tell mpv to use a different channel for controlling
    volume than the default PCM. Options for OSS include **vol, pcm, line**.
    For a complete list of options look for ``SOUND_DEVICE_NAMES`` in
    ``/usr/include/linux/soundcard.h``. For ALSA you can use the names e.g.
    alsamixer displays, like **Master, Line, PCM**.

    *NOTE*: ALSA mixer channel names followed by a number must be specified in
    the <name,number> format, i.e. a channel labeled 'PCM 1' in alsamixer must
    be converted to PCM,1.

--monitoraspect=<ratio>
    Set the aspect ratio of your monitor or TV screen. A value of 0 disables a
    previous setting (e.g. in the config file). Overrides the
    ``--monitorpixelaspect`` setting if enabled.
    See also ``--monitorpixelaspect`` and ``--aspect``.

    *EXAMPLE*:

    - ``--monitoraspect=4:3``  or ``--monitoraspect=1.3333``
    - ``--monitoraspect=16:9`` or ``--monitoraspect=1.7777``

--monitorpixelaspect=<ratio>
    Set the aspect of a single pixel of your monitor or TV screen (default:
    1). A value of 1 means square pixels (correct for (almost?) all LCDs). See
    also ``--monitoraspect`` and ``--aspect``.

--mouse-movements
    Permit mpv to receive pointer events reported by the video output
    driver. Necessary to select the buttons in DVD menus. Supported for
    X11-based VOs (x11, xv, etc) and the gl, direct3d and corevideo VOs.

--mouseinput, --no-mouseinput
    Enabled by default. Disable mouse button press/release input
    (mozplayerxp's context menu relies on this option).

--no-msgcolor
    Disable colorful console output on terminals.

--msglevel=<module1=level1:module2=level2:...>
    Control verbosity directly for each module. The *all* module changes the
    verbosity of all the modules not explicitly specified on the command line.

    See ``--msglevel=help`` for a list of all modules.

    *NOTE*: Some messages are printed before the command line is parsed and
    are therefore not affected by ``--msglevel``. To control these messages
    you have to use the ``MPV_VERBOSE`` environment variable; see its
    description below for details.

    Available levels:

    :-1: complete silence
    :0:  fatal messages only
    :1:  error messages
    :2:  warning messages
    :3:  short hints
    :4:  informational messages
    :5:  status messages (default)
    :6:  verbose messages
    :7:  debug level 2
    :8:  debug level 3
    :9:  debug level 4

--msgmodule
    Prepend module name in front of each console message.

--mute=<auto|yes|no>
    Set startup audio mute status. ``auto`` (default) will not change the mute
    status. Also see ``--volume``.

--name
    Set the window class name for X11-based video output methods.

--native-keyrepeat
    Use system settings for keyrepeat delay and rate, instead of
    ``--input-ar-delay`` and ``--input-ar-rate``. (Whether this applies
    depends on the VO backend and how it handles keyboard input. Does not
    apply to terminal input.)

--avi-ni
    (Internal AVI demuxer which is not used by default only)
    Force usage of non-interleaved AVI parser (fixes playback of some bad AVI
    files).

--no-aspect
    Ignore aspect ratio information from video file and assume the video has
    square pixels. See also ``--aspect``.

--no-bps
    (Internal AVI demuxer which is not used by default only)
    Do not use average byte/second value for A-V sync. Helps with some AVI
    files with broken header.

--no-cache
    Turn off input stream caching. See ``--cache``.

--no-colorkey
    Disables colorkeying. Only supported by the xv (see ``--vo=xv:ck``) video
    output driver.

--no-config
    Do not load default configuration files. This prevents loading of
    ``~/.mpv/config`` and ``~/.mpv/input.conf``, as well as loading the
    same files from system wide configuration directories.

    Loading of some configuration files is not affected by this option, such
    as configuration files for cddb, DVB code and fontconfig.

    *NOTE*: Files explicitly requested by command line options, like
    ``--include`` or ``--use-filedir-conf``, will still be loaded.

--no-idx
    Do not use index present in the file even if one is present.

--no-audio
    Do not play sound. With some demuxers this may not work. In those cases
    you can try ``--ao=null`` instead.

--no-resume-playback
    Do not restore playback position from ``~/.mpv/watch_later/``.
    See ``quit_watch_later`` input command.

--no-sub
    Disables display of internal and external subtitles.

--no-video
    Do not play video. With some demuxers this may not work. In those cases
    you can try ``--vo=null`` instead.

--ontop
    Makes the player window stay on top of other windows. Supported by video
    output drivers which use X11, as well as corevideo.

--ordered-chapters, --no-ordered-chapters
    Enabled by default.
    Disable support for Matroska ordered chapters. mpv will not load or
    search for video segments from other files, and will also ignore any
    chapter order specified for the main file.

--no-osd-bar, --osd-bar
    Disable display of the OSD bar. This will make some things (like seeking)
    use OSD text messages instead of the bar.

    You can configure this on a per-command basis in input.conf using ``osd-``
    prefixes, see ``Input command prefixes``. If you want to disable the OSD
    completely, use ``--osd-level=0``.

--osd-bar-align-x=<-1-1>
    Position of the OSD bar. -1 is far left, 0 is centered, 1 is far right.

--osd-bar-align-y=<-1-1>
    Position of the OSD bar. -1 is top, 0 is centered, 1 is bottom.

--osd-bar-w=<1-100>
    Width of the OSD bar, in percentage of the screen width (default: 75).
    A value of 0.5 means the bar is half the screen wide.

--osd-bar-h=<0.1-50>
    Height of the OSD bar, in percentage of the screen height (default: 3.125).

--osd-back-color=<#RRGGBB>, --sub-text-back-color=<#RRGGBB>
    See ``--osd-color``. Color used for OSD/sub text background.

--osd-blur=<0..20.0>, --sub-text-blur=<0..20.0>
    Gaussian blur factor. 0 means no blur applied (default).

--osd-border-color=<#RRGGBB>, --sub-text-border-color=<#RRGGBB>
    See ``--osd-color``. Color used for the OSD/sub font border.

    *NOTE*: ignored when ``--osd-back-color``/``--sub-text-back-color`` is
    specified (or more exactly: when that option is not set to completely
    transparent).

--osd-border-size=<size>, --sub-text-border-size=<size>
    Size of the OSD/sub font border in scaled pixels (see ``--osd-font-size``
    for details). A value of 0 disables borders.

    Default: 2.5.

--osd-color=<#RRGGBB|#AARRGGBB>, --sub-text-color=<#RRGGBB|#AARRGGBB>
    Specify the color used for OSD/unstyled text subtitles.

    The color is specified as a RGB hex triplet, and each 2-digit group
    expresses a color value in the range 0 (``00``) to 255 (`FF`).
    For example, ``#FF0000`` is red. This is similar to web colors.

    You can specify transparency by specifying an alpha value in the form
    ``#AARRGGBB``. 0 is fully transparent, while ``FF`` is opaque (opaque is
    default with the shorter color specification).

    *EXAMPLE*:

    - ``--osd-color='#FF0000'`` set OSD to opaque red
    - ``--osd-color='#C0808080'`` set OSD to 50% gray with 75% alpha

--osd-duration=<time>
    Set the duration of the OSD messages in ms (default: 1000).

--osd-font=<pattern>, --sub-text-font=<pattern>
    Specify font to use for OSD and for subtitles that do not themselves
    specify a particular font. The default is ``Sans``.

    *EXAMPLE*:

    - ``--osd-font='Bitstream Vera Sans'``
    - ``--osd-font='Bitstream Vera Sans:style=Bold'`` (fontconfig pattern)

    *NOTE*: the ``--sub-text-font`` option (and most other ``--sub-text-``
    options) are ignored when ASS-subtitles are rendered, unless the
    ``--no-ass`` option is specified.

--osd-font-size=<size>, --sub-text-font-size=<size>
    Specify the OSD/sub font size. The unit is the size in scaled pixels at a
    window height of 720. The actual pixel size is scaled with the window
    height: if the window height is larger or smaller than 720, the actual size
    of the text increases or decreases as well.

    Default: 45.

--osd-fractions
    Show OSD times with fractions of seconds.

--osd-level=<0-3>
    Specifies which mode the OSD should start in.

    :0: subtitles only
    :1: volume + seek (default)
    :2: volume + seek + timer + percentage
    :3: volume + seek + timer + percentage + total time

--osd-margin-x=<size>, --sub-text-margin-x=<size>
    Left and right screen margin for the OSD/subs in scaled pixels (see
    ``--osd-font-size`` for details).

    This option specifies the distance of the OSD to the left, as well as at
    which distance from the right border long OSD text will be broken.

    Default: 25.

--osd-margin-y=<size>, --sub-text-margin-y=<size>
    Top and bottom screen margin for the OSD/subs in scaled pixels (see
    ``--osd-font-size`` for details).

    This option specifies the vertical margins of the OSD. This is also used
    for unstyled text subtitles. If you just want to raise the vertical
    subtitle position, use ``--sub-pos``.

    Default: 10.

--osd-shadow-color=<#RRGGBB>, --sub-text-shadow-color=<#RRGGBB>
    See ``--osd-color``. Color used for OSD/sub text shadow.

--osd-shadow-offset=<size>, --sub-text-shadow-offset=<size>
    Displacement of the OSD/sub text shadow in scaled pixels (see
    ``--osd-font-size`` for details). A value of 0 disables shadows.

    Default: 0.

--osd-spacing=<size>, --sub-text-spacing=<size>
    Horizontal OSD/sub font spacing in scaled pixels (see ``--osd-font-size``
    for details). This value is added to the normal letter spacing. Negative
    values are allowed.

    Default: 0.

--osd-status-msg=<string>
    Show a custom string during playback instead of the standard status text.
    This overrides the status text used for ``--osd-level=3``, when using the
    ``show_progress`` command (by default mapped to ``P``), or in some
    non-default cases when seeking. Expands properties. See ``--playing-msg``.

--overlapsub
    Allows the next subtitle to be displayed while the current one is still
    visible (default is to enable the support only for specific formats). This
    only matters for subtitles loaded with ``-sub``.

--panscan=<0.0-1.0>
    Enables pan-and-scan functionality (cropping the sides of e.g. a 16:9
    movie to make it fit a 4:3 display without black bands). The range
    controls how much of the image is cropped. May not work with all video
    output drivers.

--panscanrange=<-19.0-99.0>
    (experimental)
    Change the range of the pan-and-scan functionality (default: 1). Positive
    values mean multiples of the default range. Negative numbers mean you can
    zoom in up to a factor of ``--panscanrange=+1``. E.g. ``--panscanrange=-3``
    allows a zoom factor of up to 4. This feature is experimental. Do not
    report bugs unless you are using ``--vo=opengl``.

--passwd=<password>
    Used with some network protocols. Specify password for HTTP authentication.
    See also ``--user``.

    *WARNING*: works with the deprecated ``mp_http://`` protocol only.

--playing-msg=<string>
    Print out a string after starting playback. The string is expanded for
    properties, e.g. ``--playing-msg=file: ${filename}`` will print the string
    ``file:`` followed by a space and the currently played filename.

    The following expansions are supported:

    \${NAME}
        Expands to the value of the property ``NAME``. If ``NAME`` starts with
        ``=``, use the raw value of the property. If retrieving the property
        fails, expand to an error string. (Use ``${NAME:}`` with a trailing
        ``:`` to expand to an empty string instead.)
    \${NAME:STR}
        Expands to the value of the property ``NAME``, or ``STR`` if the
        property can't be retrieved. ``STR`` is expanded recursively.
    \${!NAME:STR}
        Expands to ``STR`` (recursively) if the property ``NAME`` can't be
        retrieved.
    \${?NAME:STR}
        Expands to ``STR`` (recursively) if the property ``NAME`` is available.
    \$\$
        Expands to ``$``.
    \$}
        Expands to ``}``. (To produce this character inside recursive
        expansion.)
    \$>
        Disable property expansion and special handling of ``$`` for the rest
        of the string.

    This option also parses C-style escapes. Example:

    - ``\n`` becomes a newline character
    - ``\\`` expands to ``\``

--status-msg=<string>
    Print out a custom string during playback instead of the standard status
    line. Expands properties. See ``--playing-msg``.

--playlist=<filename>
    Play files according to a playlist file (ASX, Winamp, SMIL, or
    one-file-per-line format).

    *WARNING*: The way mpv parses and uses playlist files is not safe
    against maliciously constructed files. Such files may trigger harmful
    actions. This has been the case for all mpv and MPlayer versions, but
    unfortunately this fact was not well documented earlier, and some people
    have even misguidedly recommended use of ``--playlist`` with untrusted
    sources. Do NOT use ``--playlist`` with random internet sources or files
    you don't trust!

    FIXME: This needs to be clarified and documented thoroughly.

--pp=<quality>
    See also ``--vf=pp``.

--pphelp
    See also ``--vf=pp``.

--prefer-ipv4
    Use IPv4 on network connections. Falls back on IPv6 automatically.

    *WARNING*: works with the deprecated ``mp_http://`` protocol only.

--prefer-ipv6
    Use IPv6 on network connections. Falls back on IPv4 automatically.

    *WARNING*: works with the deprecated ``mp_http://`` protocol only.

--priority=<prio>
    (Windows only.)
    Set process priority for mpv according to the predefined priorities
    available under Windows.

    Possible values of <prio>:
    idle|belownormal|normal|abovenormal|high|realtime

    *WARNING*: Using realtime priority can cause system lockup.

--profile=<profile1,profile2,...>
    Use the given profile(s), ``--profile=help`` displays a list of the
    defined profiles.

--pts-association-mode=<auto|decode|sort>
    Select the method used to determine which container packet timestamp
    corresponds to a particular output frame from the video decoder. Normally
    you shouldn't need to change this option.

    :auto:    Try to pick a working mode from the ones below automatically
              (default)
    :decoder: Use decoder reordering functionality.
    :sort:    Maintain a buffer of unused pts values and use the lowest value
              for the frame.

--pvr=<option1:option2:...>
    This option tunes various encoding properties of the PVR capture module.
    It has to be used with any hardware MPEG encoder based card supported by
    the V4L2 driver. The Hauppauge WinTV PVR-150/250/350/500 and all IVTV
    based cards are known as PVR capture cards. Be aware that only Linux
    2.6.18 kernel and above is able to handle MPEG stream through V4L2 layer.
    For hardware capture of an MPEG stream and watching it with mpv, use
    ``pvr://`` as a movie URL.

    Available options are:

    aspect=<0-3>
        Specify input aspect ratio:

        :0: 1:1
        :1: 4:3 (default)
        :2: 16:9
        :3: 2.21:1

    arate=<32000-48000>
        Specify encoding audio rate (default: 48000 Hz, available: 32000,
        44100 and 48000 Hz).

    alayer=<1-3>
        Specify MPEG audio layer encoding (default: 2).

    abitrate=<32-448>
        Specify audio encoding bitrate in kbps (default: 384).

    amode=<value>
        Specify audio encoding mode. Available preset values are 'stereo',
        'joint_stereo', 'dual' and 'mono' (default: stereo).

    vbitrate=<value>
        Specify average video bitrate encoding in Mbps (default: 6).

    vmode=<value>
        Specify video encoding mode:

        :vbr: Variable BitRate (default)
        :cbr: Constant BitRate

    vpeak=<value>
        Specify peak video bitrate encoding in Mbps (only useful for VBR
        encoding, default: 9.6).

    fmt=<value>
        Choose an MPEG format for encoding:

        :ps:    MPEG-2 Program Stream (default)
        :ts:    MPEG-2 Transport Stream
        :mpeg1: MPEG-1 System Stream
        :vcd:   Video CD compatible stream
        :svcd:  Super Video CD compatible stream
        :dvd:   DVD compatible stream

--quiet
    Make console output less verbose; in particular, prevents the status line
    (i.e. AV: 3.4 (00:00:03.37) / 5320.6 ...) from being displayed.
    Particularly useful on slow terminals or broken ones which do not properly
    handle carriage return (i.e. \\r).

--quvi-format=<best|default|...>
    Video format/quality that is directly passed to libquvi (default: ``best``).
    This is used when opening links to streaming sites like YouTube. The
    interpretation of this value is highly specific to the streaming site and
    the video. The only well defined values that work on all sites are ``best``
    (best quality/highest bandwidth, default), and ``default`` (lowest quality).

    The quvi command line tool can be used to find out which formats are
    supported for a given URL: ``quvi --query-formats URL``.

--radio=<option1:option2:...>
    These options set various parameters of the radio capture module. For
    listening to radio with mpv use ``radio://<frequency>`` (if channels
    option is not given) or ``radio://<channel_number>`` (if channels option
    is given) as a movie URL. You can see allowed frequency range by running
    mpv with ``-v``. To start the grabbing subsystem, use
    ``radio://<frequency or channel>/capture``. If the capture keyword is not
    given you can listen to radio using the line-in cable only. Using capture
    to listen is not recommended due to synchronization problems, which makes
    this process uncomfortable.

    Available options are:

    device=<value>
        Radio device to use (default: ``/dev/radio0`` for Linux and
        ``/dev/tuner0`` for \*BSD).

    driver=<value>
        Radio driver to use (default: v4l2 if available, otherwise v4l).
        Currently, v4l and v4l2 drivers are supported.

    volume=<0..100>
        sound volume for radio device (default 100)

    channels=<frequency>-<name>,<frequency>-<name>,...
        Set channel list. Use _ for spaces in names (or play with quoting ;-).
        The channel names will then be written using OSD and the slave
        commands radio_step_channel and radio_set_channel will be usable for a
        remote control (see LIRC). If given, number in movie URL will be
        treated as channel position in channel list.

        *EXAMPLE*: ``radio://1``, ``radio://104.4``, ``radio_set_channel 1``

    adevice=<value> (radio capture only)
        Name of device to capture sound from. Without such a name capture will
        be disabled, even if the capture keyword appears in the URL. For ALSA
        devices use it in the form ``hw=<card>.<device>``. If the device name
        contains a '=', the module will use ALSA to capture, otherwise OSS.

    arate=<value> (radio capture only)
        Rate in samples per second (default: 44100).

        *NOTE*: When using audio capture set also ``--rawaudio=rate=<value>``
        option with the same value as arate. If you have problems with sound
        speed (runs too quickly), try to play with different rate values (e.g.
        48000, 44100, 32000,...).

    achannels=<value> (radio capture only)
        Number of audio channels to capture.

--rawaudio=<option1:option2:...>
    This option lets you play raw audio files. You have to use
    ``--demuxer=rawaudio`` as well. It may also be used to play audio CDs
    which are not 44kHz 16-bit stereo.

    Available options are:

    :channels=<value>:   number of channels
    :rate=<value>:       rate in samples per second
    :format=<value>:     mpv audio format (e.g. s16le)

--rawvideo=<option1:option2:...>
    This option lets you play raw video files. You have to use
    ``--demuxer=rawvideo`` as well.

    Available options are:

    :fps=<value>:                  rate in frames per second (default: 25.0)
    :w=<value>:                    image width in pixels
    :h=<value>:                    image height in pixels
    :format=<value>:               colorspace (fourcc) in hex or string
                                   constant.
    :mp-format=<value>:            colorspace by internal video format
                                   Use ``--rawvideo=mp-format=help``
                                   for a list of possible formats.
    :codec:                        set the video codec (instead of selecting
                                   the rawvideo codec)
    :size=<value>:                 frame size in Bytes

    *EXAMPLE*:

    - ``mpv sample-720x576.yuv --demuxer=rawvideo --rawvideo=w=720:h=576``
      Play a raw YUV sample.

--really-quiet
    Display even less output and status messages than with ``--quiet``.

--referrer=<string>
    Specify a referrer path or URL for HTTP requests.

--reset-on-next-file=<all|option1,option2,...>
    Normally, mpv will try to keep all settings when playing the next file on
    the playlist, even if they were changed by the user during playback. (This
    behavior is the opposite of MPlayer's, which tries to reset all settings
    when starting next file.)

    This can be changed with this option. It accepts a list of options, and
    mpv will reset the value of these options on playback start to the initial
    value. The initial value is either the default value, or as set by the
    config file or command line.

    In some cases, this might not work as expected. For example, ``--volume``
    will only be reset the volume if it's explicitly set in the config file
    or the command line.

    The special name ``all`` resets as many options as possible.

    *EXAMPLE*:

    - ``--reset-on-next-file=fullscreen,speed`` Reset fullscreen and playback
      speed settings if they were changed during playback.
    - ``--reset-on-next-file=all`` Try to reset all settings that were changed
      during playback.

--reuse-socket
    (udp:// only)
    Allows a socket to be reused by other processes as soon as it is closed.

--saturation=<-100-100>
    Adjust the saturation of the video signal (default: 0). You can get
    grayscale output with this option. Not supported by all video output
    drivers.

--save-position-on-quit
    Always save the current playback position on quit. When this file is
    played again later, the player will seek to the old playback position on
    start. This affects any form of stopping playback (quitting, going to the
    next file).

    This behavior is disabled by default, but is always available when quitting
    the player with Shift+Q.

--sb=<n>
    Seek to byte position. Useful for playback from CD-ROM images or VOB files
    with junk at the beginning. See also ``--start``.

--screen=<default|0-32>
    In multi-monitor configurations (i.e. a single desktop that spans across
    multiple displays) this option tells mpv which screen to display the
    movie on.

    This option doesn't always work. In these cases, try to use ``--geometry``
    to position the window explicitly.

    *NOTE (X11)*: this option does not work properly with all window managers.

    See also ``--fs-screen``.

--screenshot-format=<type>
    Set the image file type used for saving screenshots.

    Available choices:

    :png:   PNG
    :ppm:   PPM
    :pgm:   PGM
    :pgmyuv:   PGM with YV12 pixel format
    :tga:   TARGA
    :jpg:   JPEG (default)
    :jpeg:  JPEG (same as jpg, but with .jpeg file ending)

--screenshot-jpeg-quality=<0-100>
    Set the JPEG quality level. Higher means better quality. The default is 90.

--screenshot-png-compression=<0-9>
    Set the PNG compression level. Higher means better compression. This will
    affect the file size of the written screenshot file, and the time it takes
    to write a screenshot. Too high compression might occupy enough CPU time to
    interrupt playback. The default is 7.

--screenshot-template=<template>
    Specify the filename template used to save screenshots. The template
    specifies the filename without file extension, and can contain format
    specifiers, which will be substituted when taking a screeshot.
    By default the template is ``shot%n``, which results in filenames like
    ``shot0012.png`` for example.

    The template can start with a relative or absolute path, in order to
    specify a directory location where screenshots should be saved.

    If the final screenshot filename points to an already existing file, the
    file won't be overwritten. The screenshot will either not be saved, or if
    the template contains ``%n``, saved using different, newly generated
    filename.

    Allowed format specifiers:

    ``%[#][0X]n``
        A sequence number, padded with zeros to length X (default: 04). E.g.
        passing the format ``%04n`` will yield ``0012`` on the 12th screenshot.
        The number is incremented every time a screenshot is taken, or if the
        file already exists. The length ``X`` must be in the range 0-9. With
        the optional # sign mpv will use the lowest available number. For
        example, if you take three screenshots--0001, 0002, 0003--and delete
        the first two, the next two screenshots won't be 0004 and 0005, but
        0001 and 0002 again.
    ``%f``
        Filename of the currently played video.
    ``%F``
        Same as ``%f``, but strip the file extension, including the dot.
    ``%p``
        Current playback time, in the same format as used in the OSD. The
        result is a string of the form "HH:MM:SS". For example, if the video is
        at the time position 5 minutes and 34 seconds, ``%p`` will be replaced
        with "00:05:34".
    ``%P``
        Similar to ``%p``, but extended with the playback time in milliseconds.
        It is formatted as "HH:MM:SS.mmm", with "mmm" being the millisecond
        part of the playback time. (Note that this is a simple way for getting
        unique per-frame timestamps. Frame numbers would be more intuitive, but
        are not easily implementable, because container formats usually use
        time stamps for identifying frames.)
    ``%tX``
        Specify the current local date/time using the format ``X``. This format
        specifier uses the UNIX ``strftime()`` function internally, and inserts
        the result of passing "%X" to ``strftime``. For example, ``%tm`` will
        insert the number of the current month as number. You have to use
        multiple ``%tX`` specifiers to build a full date/time string.
    ``%{prop[:fallback text]}``
        Insert the value of the slave property 'prop'. E.g. ``%{filename}`` is
        the same as ``%f``. If the property doesn't exist or is not available,
        an error text is inserted, unless a fallback is specified.
    ``%%``
        Replaced with the ``%`` character itself.

--screenh=<pixels>
    Specify the screen height for video output drivers which do not know the
    screen resolution, like x11 and TV-out.

--screenw=<pixels>
    Specify the screen width for video output drivers which do not know the
    screen resolution, like x11 and TV-out.

--show-profile=<profile>
    Show the description and content of a profile.

--shuffle
    Play files in random order.

--sid=<ID|auto|no>
    Display the subtitle stream specified by <ID> (0-31). ``auto`` selects the
    default, ``no`` disables subtitles.
    See also ``--slang``, ``--no-sub``.

--slang=<languagecode[,languagecode,...]>
    Specify a priority list of subtitle languages to use. Different container
    formats employ different language codes. DVDs use ISO 639-1 two letter
    language codes, Matroska uses ISO 639-2 three letter language codes while
    OGM uses a free-form identifier. mpv prints the available languages
    when run in verbose (``-v``) mode. See also ``--sid``.

    *EXAMPLE*:

    - ``mpv dvd://1 --slang=hu,en`` chooses the Hungarian subtitle track on
      a DVD and falls back on English if Hungarian is not available.
    - ``mpv --slang=jpn example.mkv`` plays a Matroska file with Japanese
      subtitles.

--slave-broken
    Switches on the old slave mode. This is for testing only, and incompatible
    to the removed --slave switch.

    *NOTE*: Changes incompatible to slave mode applications have been made. In
    particular, the status line output was changed, which is used by some
    applications to determine the current playback position. This switch has
    been renamed to prevent these applications from working with this version
    of mpv, because it would lead to buggy and confusing behavior only.
    Moreover, the slave mode protocol is so horribly bad that it should not be
    used for new programs, nor should existing programs attempt to adapt to the
    changed output and use the --slave-broken switch. Instead, a new, saner
    protocol should be developed (and will, if there is enough interest).

    This affects smplayer, smplayer2, mplayerosx, and others.

--softsleep
    Time frames by repeatedly checking the current time instead of asking
    the kernel to wake up mpv at the correct time. Useful if your kernel
    timing is imprecise and you cannot use the RTC either. Comes at the
    price of higher CPU consumption.

--softvol=<mode>
    Control whether to use the volume controls of the audio output driver, or
    the internal mpv volume filter.

    :no:    prefer audio driver controls, use the volume filter only if
            absolutely needed
    :yes:   always use the volume filter
    :auto:  prefer the volume filter if the audio driver uses the system mixer (default)

    The intention of ``auto`` is to avoid changing system mixer settings from
    within mpv with default settings. mpv is a video player, not a mixer panel.
    On the other hand, mixer controls are enabled for sound servers like
    PulseAudio, which provide per-application volume.

--softvol-max=<10.0-10000.0>
    Set the maximum amplification level in percent (default: 200). A value of
    200 will allow you to adjust the volume up to a maximum of double the
    current level. With values below 100 the initial volume (which is 100%)
    will be above the maximum, which e.g. the OSD cannot display correctly.

--speed=<0.01-100>
    Slow down or speed up playback by the factor given as parameter.

--srate=<Hz>
    Select the output sample rate to be used (of course sound cards have
    limits on this). If the sample frequency selected is different from that
    of the current media, the resample or lavcresample audio filter will be
    inserted into the audio filter layer to compensate for the difference. The
    type of resampling can be controlled by the ``--af-adv`` option.

--start=<relative time>
    Seek to given time position.

    The general format for absolute times is ``[[hh:]mm:]ss[.ms]``. If the time
    is negated with ``-``, the seek is relative from the end of the file.

    ``pp%`` seeks to percent position pp (0-100).

    ``#c`` seeks to chapter number c. (Chapters start from 1.)

    *EXAMPLE*:

    ``--start=56``
        Seeks to 56 seconds.
    ``--start=01:10:00``
        Seeks to 1 hour 10 min.
    ``--start=50%``
        Seeks to the middle of the file.
    ``--start=30 --end=40``
        Seeks to 30 seconds, plays 10 seconds, and exits.
    ``--start=-3:20 --length=10``
        Seeks to 3 minutes and 20 seconds before the end of the file, plays
        10 seconds, and exits.
    ``--start='#2' --end='#4'``
        Plays chapters 2 and 3, and exits.

--ssf=<mode>
    Specifies software scaler parameters.

    :lgb=<0-100>:   gaussian blur filter (luma)
    :cgb=<0-100>:   gaussian blur filter (chroma)
    :ls=<-100-100>: sharpen filter (luma)
    :cs=<-100-100>: sharpen filter (chroma)
    :chs=<h>:       chroma horizontal shifting
    :cvs=<v>:       chroma vertical shifting

    *EXAMPLE*: ``--vf=scale --ssf=lgb=3.0``

--sstep=<sec>
    Skip <sec> seconds after every frame.

    *NOTE*: without ``--hr-seek``, skipping will snap to keyframes.

--stereo=<mode>
    Select type of MP2/MP3 stereo output.

    :0: stereo
    :1: left channel
    :2: right channel

--stop-xscreensaver
    (X11 only)
    Turns off xscreensaver at startup and turns it on again on exit. If your
    screensaver supports neither the XSS nor XResetScreenSaver API please use
    ``--heartbeat-cmd`` instead.

--sub=<subtitlefile1,subtitlefile2,...>
    Use/display these subtitle files. Only one file can be displayed at the
    same time.

--sub-demuxer=<[+]name>
    Force subtitle demuxer type for ``--subfile``. Using a '+' before the name
    will force it, this will skip some checks! Give the demuxer name as
    printed by ``--sub-demuxer=help``.

--sub-no-text-pp
    Disables any kind of text post processing done after loading the
    subtitles. Used for debug purposes.

--sub-paths=<path1:path2:...>
    Specify extra directories where to search for subtitles matching the
    video. Multiple directories can be separated by ":" (";" on Windows).
    Paths can be relative or absolute. Relative paths are interpreted relative
    to video file directory.

    *EXAMPLE*: Assuming that ``/path/to/movie/movie.avi`` is played and
    ``--sub-paths=sub:subtitles:/tmp/subs`` is specified, mpv searches for
    subtitle files in these directories:

    - ``/path/to/movie/``
    - ``/path/to/movie/sub/``
    - ``/path/to/movie/subtitles/``
    - ``/tmp/subs/``
    - ``~/.mpv/sub/``

--subcp=<codepage>
    If your system supports ``iconv(3)``, you can use this option to specify
    the subtitle codepage.

    *EXAMPLE*:
    - ``--subcp=latin2``
    - ``--subcp=cp1250``

    If the player was compiled with ENCA support you can use special syntax
    to use that.

    ``--subcp=enca:<language>:<fallback codepage>``

    You can specify your language using a two letter language code to make
    ENCA detect the codepage automatically. If unsure, enter anything and
    watch mpv ``-v`` output for available languages. Fallback codepage
    specifies the codepage to use, when autodetection fails.

    *EXAMPLE*:

    - ``--subcp=enca:cs:latin2`` guess the encoding, assuming the subtitles
      are Czech, fall back on latin 2, if the detection fails.
    - ``--subcp=enca:pl:cp1250`` guess the encoding for Polish, fall back on
      cp1250.

--sub-delay=<sec>
    Delays subtitles by <sec> seconds. Can be negative.

--subfile=<filename>
    Open the given file with a demuxer, and use its subtitle streams. Same as
    ``--audiofile``, but for subtitle streams.

    *NOTE*: use ``--sub`` for subtitle files. This option is useless, unless
    you want to force libavformat subtitle parsers instead of libass or
    internal subtitle parsers.

--subfps=<rate>
    Specify the framerate of the subtitle file (default: movie fps).

    *NOTE*: <rate> > movie fps speeds the subtitles up for frame-based
    subtitle files and slows them down for time-based ones.

--sub-gauss=<0.0-3.0>
    Apply gaussian blur to image subtitles (default: 0). This can help making
    pixelated DVD/Vobsubs look nicer. A value other than 0 also switches to
    software subtitle scaling. Might be slow.

    *NOTE*: never applied to text subtitles.

--sub-gray
    Convert image subtitles to grayscale. Can help making yellow DVD/Vobsubs
    look nicer.

    *NOTE*: never affects text subtitles.

--sub-pos=<0-100>
    Specify the position of subtitles on the screen. The value is the vertical
    position of the subtitle in % of the screen height.

    *NOTE*: this affects ASS subtitles as well, and may lead to incorrect
    subtitle rendering. Use with care, or use ``--sub-text-margin-y`` instead.

--sub-scale=<0-100>
    Factor for the text subtitle font size (default: 1).

    *NOTE*: this affects ASS subtitles as well, and may lead to incorrect
    subtitle rendering. Use with care, or use ``--sub-text-font-size`` instead.

--sws=<n>
    Specify the software scaler algorithm to be used with ``--vf=scale``. This
    also affects video output drivers which lack hardware acceleration,
    e.g. x11. See also ``--vf=scale``.

    Available types are:

    :0:  fast bilinear
    :1:  bilinear
    :2:  bicubic (good quality) (default)
    :3:  experimental
    :4:  nearest neighbor (bad quality)
    :5:  area
    :6:  luma bicubic / chroma bilinear
    :7:  gauss
    :8:  sincR
    :9:  lanczos
    :10: natural bicubic spline

    *NOTE*: Some ``--sws`` options are tunable. The description of the scale
    video filter has further information.

--term-osd, --no-term-osd
    Display OSD messages on the console when no video output is available.
    Enabled by default.

--term-osd-esc=<string>
    Specify the escape sequence to use before writing an OSD message on the
    console. The escape sequence should move the pointer to the beginning of
    the line used for the OSD and clear it (default: ``^[[A\r^[[K``).

--title=<string>
    Set the window title. Properties are expanded on playback start
    (see ``--playing-msg``).

--tv=<option1:option2:...>
    This option tunes various properties of the TV capture module. For
    watching TV with mpv, use ``tv://`` or ``tv://<channel_number>`` or
    even ``tv://<channel_name>`` (see option channels for channel_name below)
    as a movie URL. You can also use ``tv:///<input_id>`` to start watching a
    movie from a composite or S-Video input (see option input for details).

    Available options are:

    noaudio
        no sound

    automute=<0-255> (v4l and v4l2 only)
        If signal strength reported by device is less than this value, audio
        and video will be muted. In most cases automute=100 will be enough.
        Default is 0 (automute disabled).

    driver=<value>
        See ``--tv=driver=help`` for a list of compiled-in TV input drivers.
        available: dummy, v4l2 (default: autodetect)

    device=<value>
        Specify TV device (default: ``/dev/video0``).

    input=<value>
        Specify input (default: 0 (TV), see console output for available
        inputs).

    freq=<value>
        Specify the frequency to set the tuner to (e.g. 511.250). Not
        compatible with the channels parameter.

    outfmt=<value>
        Specify the output format of the tuner with a preset value supported
        by the V4L driver (YV12, UYVY, YUY2, I420)
        or an arbitrary format given as hex value.

    width=<value>
        output window width

    height=<value>
        output window height

    fps=<value>
        framerate at which to capture video (frames per second)

    buffersize=<value>
        maximum size of the capture buffer in megabytes (default: dynamical)

    norm=<value>
        See the console output for a list of all available norms, also see the
        normid option below.

    normid=<value> (v4l2 only)
        Sets the TV norm to the given numeric ID. The TV norm depends on the
        capture card. See the console output for a list of available TV norms.

    channel=<value>
        Set tuner to <value> channel.

    chanlist=<value>
        available: argentina, australia, china-bcast, europe-east,
        europe-west, france, ireland, italy, japan-bcast, japan-cable,
        newzealand, russia, southafrica, us-bcast, us-cable, us-cable-hrc

    channels=<chan>-<name>[=<norm>],<chan>-<name>[=<norm>],...
        Set names for channels.

        *NOTE*: If <chan> is an integer greater than 1000, it will be treated
        as frequency (in kHz) rather than channel name from frequency table.
        Use _ for spaces in names (or play with quoting ;-). The channel names
        will then be written using OSD, and the slave commands
        tv_step_channel, tv_set_channel and tv_last_channel will be usable for
        a remote control (see LIRC). Not compatible with the frequency
        parameter.

        *NOTE*: The channel number will then be the position in the 'channels'
        list, beginning with 1.

        *EXAMPLE*: ``tv://1``, ``tv://TV1``, ``tv_set_channel 1``,
        ``tv_set_channel TV1``

    [brightness|contrast|hue|saturation]=<-100-100>
        Set the image equalizer on the card.

    audiorate=<value>
        Set input audio sample rate.

    forceaudio
        Capture audio even if there are no audio sources reported by v4l.

    alsa
        Capture from ALSA.

    amode=<0-3>
        Choose an audio mode:

        :0: mono
        :1: stereo
        :2: language 1
        :3: language 2

    forcechan=<1-2>
        By default, the count of recorded audio channels is determined
        automatically by querying the audio mode from the TV card. This option
        allows forcing stereo/mono recording regardless of the amode option
        and the values returned by v4l. This can be used for troubleshooting
        when the TV card is unable to report the current audio mode.

    adevice=<value>
        Set an audio device. <value> should be ``/dev/xxx`` for OSS and a
        hardware ID for ALSA. You must replace any ':' by a '.' in the
        hardware ID for ALSA.

    audioid=<value>
        Choose an audio output of the capture card, if it has more than one.

    [volume|bass|treble|balance]=<0-65535> (v4l1)

    [volume|bass|treble|balance]=<0-100> (v4l2)
        These options set parameters of the mixer on the video capture card.
        They will have no effect, if your card does not have one. For v4l2 50
        maps to the default value of the control, as reported by the driver.

    gain=<0-100> (v4l2)
        Set gain control for video devices (usually webcams) to the desired
        value and switch off automatic control. A value of 0 enables automatic
        control. If this option is omitted, gain control will not be modified.

    immediatemode=<bool>
        A value of 0 means capture and buffer audio and video together. A
        value of 1 (default) means to do video capture only and let the audio
        go through a loopback cable from the TV card to the sound card.

    mjpeg
        Use hardware MJPEG compression (if the card supports it). When using
        this option, you do not need to specify the width and height of the
        output window, because mpv will determine it automatically from
        the decimation value (see below).

    decimation=<1|2|4>
        choose the size of the picture that will be compressed by hardware
        MJPEG compression:

        :1: full size

            - 704x576 PAL
            - 704x480 NTSC

        :2: medium size

            - 352x288 PAL
            - 352x240 NTSC

        :4: small size

            - 176x144 PAL
            - 176x120 NTSC

    quality=<0-100>
        Choose the quality of the JPEG compression (< 60 recommended for full
        size).

    hidden_video_renderer (dshow only)
        Terminate stream with video renderer instead of Null renderer
        (default: off). Will help if video freezes but audio does not.

        *NOTE*: May not work with ``--vo=directx`` and ``--vf=crop``
        combination.

    hidden_vp_renderer (dshow only)
        Terminate VideoPort pin stream with video renderer instead of removing
        it from the graph (default: off). Useful if your card has a VideoPort
        pin and video is choppy.

        *NOTE*: May not work with ``--vo=directx`` and ``--vf=crop``
        combination.

    system_clock (dshow only)
        Use the system clock as sync source instead of the default graph clock
        (usually the clock from one of the live sources in graph).

    normalize_audio_chunks (dshow only)
        Create audio chunks with a time length equal to video frame time
        length (default: off). Some audio cards create audio chunks about 0.5s
        in size, resulting in choppy video when using immediatemode=0.

--tvscan=<option1:option2:...>
    Tune the TV channel scanner. mpv will also print value for "-tv
    channels=" option, including existing and just found channels.

    Available suboptions are:

    autostart
        Begin channel scanning immediately after startup (default: disabled).

    period=<0.1-2.0>
        Specify delay in seconds before switching to next channel (default:
        0.5). Lower values will cause faster scanning, but can detect inactive
        TV channels as active.

    threshold=<1-100>
        Threshold value for the signal strength (in percent), as reported by
        the device (default: 50). A signal strength higher than this value will
        indicate that the currently scanning channel is active.

--use-filedir-conf
    Look for a file-specific configuration file in the same directory as the
    file that is being played.

    *WARNING*: May be dangerous if playing from untrusted media.

--user=<username>
    Used with some network protocols.
    Specify username for HTTP authentication. See also ``--passwd``.

    *WARNING*: works with the deprecated ``mp_http://`` protocol only.

--user-agent=<string>
    Use <string> as user agent for HTTP streaming.

-v
    Increment verbosity level, one level for each ``-v`` found on the command
    line.

--vd=<[+|-]family1:(*|decoder1),[+|-]family2:(*|decoder2),...[-]>
    Specify a priority list of video decoders to be used, according to their
    family and name. See ``--ad`` for further details. Both of these options
    use the same syntax and semantics, the only difference is that they
    operate on different codec lists.

    *NOTE*: See ``--vd=help`` for a full list of available decoders.

--vf=<filter1[=parameter1:parameter2:...],filter2,...>
    Specify a list of video filters to apply to the video stream. See
    `video_filters` for details and descriptions of the available filters.
    The option variants ``--vf-add``, ``--vf-pre``, ``--vf-del`` and
    ``--vf-clr`` exist to modify a previously specified list, but you
    shouldn't need these for typical use.

--vid=<ID|auto|no>
    Select video channel. ``auto`` selects the default, ``no`` disables video.

--vo=<driver1[:suboption1[=value]:...],driver2,...[,]>
    Specify a priority list of video output drivers to be used. For
    interactive use you'd normally specify a single one to use, but in
    configuration files specifying a list of fallbacks may make sense. See
    `video_outputs` for details and descriptions of available drivers.

--volstep=<0-100>
    Set the step size of mixer volume changes in percent of the whole range
    (default: 3).

--volume=<-1-100>
    Set the startup volume. A value of -1 (the default) will not change the
    volume. See also ``--softvol``.

--wid=<ID>
    (X11 and win32 only)
    This tells mpv to attach to an existing window. The ID is interpreted as
    "Window" on X11, and as HWND on win32. If a VO is selected that supports
    this option, a new window will be created and the given window will be set
    as parent. The window will always be resized to cover the parent window
    fully, and will add black bars to compensate for the video aspect ratio.

    See ``--slave-broken``.
