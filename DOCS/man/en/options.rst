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

--ac=<[-\|+]codec1,[-\|+]codec2,...[,]>
    Specify a priority list of audio codecs to be used, according to their
    codec name in codecs.conf. Use a '-' before the codec name to omit it.
    Use a '+' before the codec name to force it, this will likely crash! If
    the list has a trailing ',' MPlayer will fall back on codecs not contained
    in the list.

    *NOTE*: See ``--ac=help`` for a full list of available codecs.

    *EXAMPLE*:

    :``--ac=mp3acm``:     Force the l3codeca.acm MP3 codec.
    :``--ac=mad,``:       Try libmad first, then fall back on others.
    :``--ac=hwac3,a52,``: Try hardware AC-3 passthrough, software AC-3, then
                          others.
    :``--ac=hwdts,``:     Try hardware DTS passthrough, then fall back on
                          others.
    :``--ac=-ffmp3,``:    Skip FFmpeg's MP3 decoder.

--adapter=<value>
    Set the graphics card that will receive the image. You can get a list of
    available cards when you run this option with ``-v``. Currently only works
    with the directx video output driver.

--af=<filter1[=parameter1:parameter2:...],filter2,...>
    Specify a list of audio filters to apply to the audio stream. See
    :ref:`audio_filters` for details and descriptions of the available filters.
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
            *Warning*: It may be possible to crash MPlayer using this setting.
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

--afm=<driver1,driver2,...>
    Specify a priority list of audio codec families to be used, according to
    their codec name in codecs.conf. Falls back on the default codecs if none
    of the given codec families work.

    *NOTE*: See ``--afm=help`` for a full list of available codec families.

    *EXAMPLE*:

    :``--afm=ffmpeg``:    Try FFmpeg's libavcodec codecs first.
    :``--afm=acm,dshow``: Try Win32 codecs first.

--aid=<ID>
    Select audio channel (MPEG: 0-31, AVI/OGM: 1-99, ASF/RM: 0-127, VOB(AC-3):
    128-159, VOB(LPCM): 160-191, MPEG-TS 17-8190). MPlayer prints the
    available audio IDs when run in verbose (-v) mode. When playing an MPEG-TS
    stream, MPlayer will use the first program (if present) with the chosen
    audio stream. See also ``--alang``.

--alang=<languagecode[,languagecode,...]>
    Specify a priority list of audio languages to use. Different container
    formats employ different language codes. DVDs use ISO 639-1 two letter
    language codes, Matroska, MPEG-TS and NUT use ISO 639-2 three letter
    language codes while OGM uses a free-form identifier. MPlayer prints the
    available languages when run in verbose (``-v``) mode. See also ``--aid``.

    *EXAMPLE*:

    ``mplayer dvd://1 --alang=hu,en``
        Chooses the Hungarian language track on a DVD and falls back on
        English if Hungarian is not available.
    ``mplayer --alang=jpn example.mkv``
        Plays a Matroska file in Japanese.

--ao=<driver1[:suboption1[=value]:...],driver2,...[,]>
    Specify a priority list of audio output drivers to be used. For
    interactive use you'd normally specify a single one to use, but in
    configuration files specifying a list of fallbacks may make sense. See
    :ref:`audio_outputs` for details and descriptions of available drivers.

--ar, --no-ar
      Enable/disable AppleIR remote support. Enabled by default.

--aspect=<ratio>
    Override movie aspect ratio, in case aspect information is incorrect or
    missing in the file being played. See also ``--no-aspect``.

    *EXAMPLE*:

    - ``--aspect=4:3``  or ``--aspect=1.3333``
    - ``--aspect=16:9`` or ``--aspect=1.7777``

--ass, --no-ass
    Use libass to render all text subtitles. This enables support for the
    native styling of SSA/ASS subtitles, and also support for some styling
    features in other subtitle formats by conversion to ASS markup. Enabled by
    default if the player was compiled with libass support.

    *NOTE*: Some of the other subtitle options were written for the old
    non-libass subtitle rendering system and may not work the same way or at
    all with libass rendering enabled.

--ass-border-color=<value>
    Sets the border (outline) color for text subtitles. The color format is
    RRGGBBAA.

--ass-bottom-margin=<value>
    Adds a black band at the bottom of the frame. The SSA/ASS renderer can
    place subtitles there (with ``--ass-use-margins``).

--ass-color=<value>
    Sets the color for text subtitles. The color format is RRGGBBAA.

--ass-font-scale=<value>
    Set the scale coefficient to be used for fonts in the SSA/ASS renderer.

--ass-force-style=<[Style.]Param=Value[,...]>
    Override some style or script info parameters.

    *EXAMPLE*:

    - ``--ass-force-style=FontName=Arial,Default.Bold=1``
    - ``--ass-force-style=PlayResY=768``

--ass-hinting=<type>
    Set hinting type. <type> can be:

    :0:       no hinting
    :1:       FreeType autohinter, light mode
    :2:       FreeType autohinter, normal mode
    :3:       font native hinter
    :0-3 + 4: The same, but hinting will only be performed if the OSD is
              rendered at screen resolution and will therefore not be scaled.

    The default value is 0 (no hinting).

--ass-line-spacing=<value>
    Set line spacing value for SSA/ASS renderer.

--ass-styles=<filename>
    Load all SSA/ASS styles found in the specified file and use them for
    rendering text subtitles. The syntax of the file is exactly like the ``[V4
    Styles]`` / ``[V4+ Styles]`` section of SSA/ASS.

--ass-top-margin=<value>
    Adds a black band at the top of the frame. The SSA/ASS renderer can place
    toptitles there (with ``--ass-use-margins``).

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

--audiofile=<filename>
    Play audio from an external file (WAV, MP3 or Ogg Vorbis) while viewing a
    movie.

--audiofile-cache=<kBytes>
    Enables caching for the stream used by ``--audiofile``, using the
    specified amount of memory.

--autosub, --no-autosub
    Load additional subtitle files matching the video filename. Enabled by
    default. See also ``--sub-fuzziness``.

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

--untimed
    Do not sleep when outputting video frames. Useful for benchmarks when used
    with --no-audio.

--bluray-angle=<ID>
    Some Blu-ray discs contain scenes that can be viewed from multiple angles.
    Here you can tell MPlayer which angles to use (default: 1).

--bluray-chapter=<ID>
    (Blu-ray only)
    Tells MPlayer which Blu-ray chapter to start the current title from
    (default: 1).

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

--cache-min=<percentage>
    Playback will start when the cache has been filled up to <percentage> of
    the total.

--cache-seek-min=<percentage>
    If a seek is to be made to a position within <percentage> of the cache
    size from the current position, MPlayer will wait for the cache to be
    filled to this position rather than performing a stream seek (default:
    50).

--cdda=<option1:option2>
    This option can be used to tune the CD Audio reading feature of MPlayer.

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

    (no)skip
        (Never) accept imperfect data reconstruction.

--cdrom-device=<path>
    Specify the CD-ROM device (default: ``/dev/cdrom``).

--channels=<number>
    Request the number of playback channels (default: 2). MPlayer asks the
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
    chapter to end playing at (default: 1).

--chapter-merge-threshold=<number>
    Threshold for merging almost consecutive ordered chapter parts in
    milliseconds (default: 100). Some Matroska files with ordered chapters
    have inaccurate chapter end timestamps, causing a small gap between the
    end of one chapter and the start of the next one when they should match.
    If the end of one playback part is less than the given threshold away from
    the start of the next one then keep playing video normally over the
    chapter change instead of doing a seek.

--codecpath=<dir>
    Specify a directory for binary codecs.

--codecs-file=<filename>
    Override the standard search path and use the specified file instead of
    the builtin codecs.conf.

--colormatrix=<colorspace>
    Controls the YUV to RGB color space conversion when playing video. There
    are various standards. Normally, BT.601 should be used for SD video, and
    BT.709 for HD video. (This is done by default.) Using incorrect color space
    results in slightly under or over saturated and shifted colors.

    The color space conversion is additionally influenced by the related
    options --colormatrix-input-range and --colormatrix-output-range.

    These options are not always supported. Different video outputs provide
    varying degrees of support. The gl and vdpau video output drivers usually
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
    :sd:            alias for BT.601
    :hd:            alias for BT.709
    :0:             compatibility alias for auto (do not use)
    :1:             compatibility alias for BT.601 (do not use)
    :2:             compatibility alias for BT.709 (do not use)
    :3:             compatibility alias for SMPTE-240M (do not use)

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
    Read HTTP cookies from <filename> (default: ``~/.mozilla/`` and
    ``~/.netscape/``) and skip reading from default locations. The file is
    assumed to be in Netscape format.

--correct-pts, --no-correct-pts
    Switches MPlayer to a mode where timestamps for video frames are
    calculated differently and video filters which add new frames or modify
    timestamps of existing ones are supported. Now enabled automatically for
    most common file formats. The more accurate timestamps can be visible for
    example when playing subtitles timed to scene changes with the ``--ass``
    option. Without ``--correct-pts`` the subtitle timing will typically be
    off by some frames. This option does not work correctly with some demuxers
    and codecs.

--cursor-autohide-delay=<number>
    Make mouse cursor automatically hide after given number of milliseconds.
    A value of -1 will disable cursor autohide. A value of -2 means the cursor
    will stay hidden. Supported by video output drivers which use X11 or
    OS X Cocoa.

--delay=<sec>
    audio delay in seconds (positive or negative float value). Negative values
    delay the audio, and positive values delay the video.

--demuxer=<[+]name>
    Force demuxer type. Use a '+' before the name to force it, this will skip
    some checks! Give the demuxer name as printed by ``--demuxer=help``.

--display=<name>
    (X11 only)
    Specify the hostname and display number of the X server you want to
    display on.

    *EXAMPLE*:

    ``--display=xtest.localdomain:0``

--double, --no-double
    Double buffering. The option to disable this exists mostly for debugging
    purposes and should not normally be used.

--doubleclick-time
    Time in milliseconds to recognize two consecutive button presses as a
    double-click (default: 300).

--dvbin=<options>
    Pass the following parameters to the DVB input module, in order to
    override the default ones:

    :card=<1-4>:      Specifies using card number 1-4 (default: 1).
    :file=<filename>: Instructs MPlayer to read the channels list from
                      <filename>. Default is
                      ``~/.mplayer/channels.conf.{sat,ter,cbl,atsc}`` (based
                      on your card type) or ``~/.mplayer/channels.conf`` as a
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
    fast enough. MPlayer resets the speed to the drive default value on close.
    Values of at least 100 mean speed in kB/s. Values less than 100 mean
    multiples of 1385 kB/s, i.e. ``--dvd-speed=8`` selects 11080 kB/s.

    *NOTE*: You need write access to the DVD device to change the speed.

--dvdangle=<ID>
    Some DVD discs contain scenes that can be viewed from multiple angles.
    Here you can tell MPlayer which angles to use (default: 1).

--edition=<ID>
    (Matroska files only)
    Specify the edition (set of chapters) to use, where 0 is the first. If set
    to -1 (the default), MPlayer will choose the first edition declared as a
    default, or if there is no default, the first edition defined.

--edlout=<filename>
    Creates a new file and writes edit decision list (EDL) records to it.
    During playback, the user hits 'i' to mark the start or end of a skip
    block. This provides a starting point from which the user can fine-tune
    EDL entries later. See http://www.mplayerhq.hu/DOCS/HTML/en/edl.html for
    details.

--embeddedfonts, --no-embeddedfonts
    Use fonts embedded in Matroska container files and ASS scripts (default:
    enabled). These fonts can be used for SSA/ASS subtitle rendering
    (``--ass`` option).

--endpos=<[[hh:]mm:]ss[.ms]>
    Stop at given time.

    *NOTE*: When used in conjunction with ``--ss`` option, ``--endpos`` time
    will shift forward by seconds specified with ``--ss``.

    *EXAMPLE*:

    ``--endpos=56``
        Stop at 56 seconds.
    ``--endpos=01:10:00``
        Stop at 1 hour 10 minutes.
    ``--ss=10 --endpos=56``
        Stop at 1 minute 6 seconds.

--extbased, --no-extbased
    Enabled by default.
    Disables extension-based demuxer selection. By default, when the file type
    (demuxer) cannot be detected reliably (the file has no header or it is not
    reliable enough), the filename extension is used to select the demuxer.
    Always falls back on content-based demuxer selection.

--ffactor=<number>
    Resample the font alphamap. Can be:

    :0:    plain white fonts
    :0.75: very narrow black outline (default)
    :1:    narrow black outline
    :10:   bold black outline

--field-dominance=<-1-1>
    Set first field for interlaced content. Useful for deinterlacers that
    double the framerate: ``--vf=tfields=1``, ``--vf=yadif=1`` and
    ``--vo=vdpau:deint``.

    :-1: auto (default): If the decoder does not export the appropriate
         information, it falls back to 0 (top field first).
    :0:  top field first
    :1:  bottom field first

--fixed-vo, --no-fixed-vo
    ``--fixed-vo`` enforces a fixed video system for multiple files (one
    (un)initialization for all files). Therefore only one window will be
    opened for all files. Now enabled by default, use ``--no-fixed-vo`` to
    disable and create a new window whenever the video stream changes. Some of
    the older drivers may not be *fixed-vo* compliant.

--flip
    Flip image upside-down.

--flip-hebrew
    Turns on flipping subtitles using FriBiDi.

--flip-hebrew-commas, --no-flip-hebrew-commas
    Enabled by default.
    Change FriBiDi's assumptions about the placements of commas in subtitles.
    Use this if commas in subtitles are shown at the start of a sentence
    instead of at the end.

--font=<pattern-or-filename>
    Specify font to use for OSD and for subtitles that do not themselves
    specify a particular font. See also ``--subfont``. With fontconfig enabled
    the argument is a fontconfig pattern and the default is ``sans``. Without
    fontconfig the argument is a filename and the default is
    ``~/.mplayer/subfont.ttf``.

    *EXAMPLE*:

    - ``--font=~/.mplayer/arialuni.ttf`` (no fontconfig)
    - ``--font='Bitstream Vera Sans'`` (usual case with fontconfig)
    - ``--font='Bitstream Vera Sans:style=Bold'`` (usual case with fontconfig)

--force-window-position
    Forcefully move MPlayer's video output window to default location whenever
    there is a change in video parameters, video stream or file. This used to
    be the default behavior. Currently only affects X11 VOs.

--forcedsubsonly
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

--framedrop
    Skip displaying some frames to maintain A/V sync on slow systems. Video
    filters are not applied to such frames. For B-frames even decoding is
    skipped completely. May produce unwatchably choppy output. See also
    ``--hardframedrop``.

--frames=<number>
    Play/convert only first <number> frames, then quit.

--fribidi-charset=<name>
    Specifies the character set that will be passed to FriBiDi when decoding
    non-UTF-8 subtitles (default: ISO8859-8).

--fs
    Fullscreen playback (centers movie, and paints black bands around it).

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

--geometry=<x[%][:y[%]]>, --geometry=<[WxH][+-x+-y]>
    Adjust where the output is on the screen initially. The x and y
    specifications are in pixels measured from the top-left of the screen to
    the top-left of the image being displayed, however if a percentage sign is
    given after the argument it turns the value into a percentage of the
    screen size in that direction. It also supports the standard X11
    ``--geometry`` option format, in which e.g. +10-50 means "place 10 pixels
    from the left border and 50 pixels from the lower border" and "--20+-10"
    means "place 20 pixels beyond the right and 10 pixels beyond the top
    border". If an external window is specified using the ``--wid`` option,
    then the x and y coordinates are relative to the top-left corner of the
    window rather than the screen. The coordinates are relative to the screen
    given with ``--xineramascreen`` for the video output drivers that fully
    support ``--xineramascreen`` (direct3d, gl, vdpau, x11, xv, corevideo).

    *NOTE*: May not be supported by some of the older VO drivers.

    *EXAMPLE*:

    ``50:40``
        Places the window at x=50, y=40.
    ``50%:50%``
        Places the window in the middle of the screen.
    ``100%``
        Places the window at the middle of the right edge of the screen.
    ``100%:100%``
        Places the window at the bottom right corner of the screen.

--grabpointer, --no-grabpointer
    ``--no-grabpointer`` tells the player to not grab the mouse pointer after a
    video mode change (``--vm``). Useful for multihead setups.

--hardframedrop
    More intense frame dropping (breaks decoding). Leads to image distortion!

--heartbeat-cmd
    Command that is executed every 30 seconds during playback via *system()* -
    i.e. using the shell.

    *NOTE*: mplayer uses this command without any checking, it is your
    responsibility to ensure it does not cause security problems (e.g. make
    sure to use full paths if "." is in your path like on Windows). It also
    only works when playing video (i.e. not with ``--no-video`` but works with
    ``-vo=null``).

    This can be "misused" to disable screensavers that do not support the
    proper X API (see also ``--stop-xscreensaver``). If you think this is too
    complicated, ask the author of the screensaver program to support the
    proper X APIs.

    *EXAMPLE for xscreensaver*: ``mplayer --heartbeat-cmd="xscreensaver-command
    -deactivate" file``

    *EXAMPLE for GNOME screensaver*: ``mplayer
    --heartbeat-cmd="gnome-screensaver-command -p" file``

--help
    Show short summary of options and key bindings.

--hr-mp3-seek
    Only affects the internal ``audio`` demuxer, which is not used by default
    for mp3 files any more. The equivalent functionality is always enabled
    with the now default libavformat demuxer for mp3. Hi-res MP3 seeking.
    Enabled when playing from an external MP3 file, as we need to seek to the
    very exact position to keep A/V sync. Can be slow especially when seeking
    backwards since it has to rewind to the beginning to find an exact frame
    position.

--hr-seek=<off|absolute|always>
    Select when to use precise seeks that are not limited to keyframes. Such
    seeks require decoding video from the previous keyframe up to the target
    position and so can take some time depending on decoding performance. For
    some video formats precise seeks are disabled. This option selects the
    default choice to use for seeks; it's possible to explicitly override that
    default in the definition of key bindings and in slave mode commands.

    :off:      Never use precise seeks.
    :absolute: Use precise seeks if the seek is to an absolute position in the
               file, such as a chapter seek, but not for relative seeks like
               the default behavior of arrow keys (default).
    :always:   Use precise seeks whenever possible.

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

            ``mplayer --http-header-fields='Field1: value1','Field2: value2' http://localhost:1234``

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

--identify
    Shorthand for ``--msglevel=identify=4``. Show file parameters in an easily
    parseable format. Also prints more detailed information about subtitle and
    audio track languages and IDs. In some cases you can get more information
    by using ``--msglevel=identify=6``. For example, for a DVD or Blu-ray it
    will list the chapters and time length of each title, as well as a disk
    ID. Combine this with ``--frames=0`` to suppress all video output. The
    wrapper script ``TOOLS/midentify.sh`` suppresses the other MPlayer output
    and (hopefully) shellescapes the filenames.

--idle
    Makes MPlayer wait idly instead of quitting when there is no file to play.
    Mostly useful in slave mode where MPlayer can be controlled through input
    commands (see also ``--slave``).

--idx
    Rebuilds index of files if no index was found, allowing seeking. Useful
    with broken/incomplete downloads, or badly created files. Now this is done
    automatically by the demuxers used for most video formats, meaning that
    this switch has no effect in the typical case. See also ``--forceidx``.

    *NOTE*: This option only works if the underlying media supports seeking
    (i.e. not with stdin, pipe, etc).

--ifo=<file>
    Indicate the VOBsub IFO file that will be used to load palette and frame
    size for VOBsub subtitles.

--ignore-start
    Ignore the specified starting time for streams in AVI files. This
    nullifies stream delays.

--include=<configuration-file>
    Specify configuration file to be parsed after the default ones.

--initial-audio-sync, --no-initial-audio-sync
    When starting a video file or after events such as seeking MPlayer will by
    default modify the audio stream to make it start from the same timestamp
    as video, by either inserting silence at the start or cutting away the
    first samples. Disabling this option makes the player behave like older
    MPlayer versions did: video and audio are both started immediately even if
    their start timestamps differ, and then video timing is gradually adjusted
    if necessary to reach correct synchronization later.

--input=<commands>
    This option can be used to configure certain parts of the input system.
    Paths are relative to ``~/.mplayer/``.

    *NOTE*: Autorepeat is currently only supported by joysticks.

    Available commands are:

    conf=<filename>
        Specify input configuration file other than the default
        ``~/.mplayer/input.conf``. ``~/.mplayer/<filename>`` is assumed if no
        full path is given.

    ar-dev=<device>
        Device to be used for Apple IR Remote (default is autodetected, Linux
        only).

    ar-delay
        Delay in milliseconds before we start to autorepeat a key (0 to
        disable).

    ar-rate
        Number of key presses to generate per second on autorepeat.

    (no)default-bindings
        Use the key bindings that MPlayer ships with by default.

    keylist
        Prints all keys that can be bound to commands.

    cmdlist
        Prints all commands that can be bound to keys.

    js-dev
        Specifies the joystick device to use (default: ``/dev/input/js0``).

    file=<filename>
        Read commands from the given file. Mostly useful with a FIFO.
        See also ``--slave``.

        *NOTE*: When the given file is a FIFO MPlayer opens both ends so you
        can do several `echo "seek 10" > mp_pipe` and the pipe will stay
        valid.

--ipv4-only-proxy
    Skip any HTTP proxy for IPv6 addresses. It will still be used for IPv4
    connections.

--joystick, --no-joystick
    Enable/disable joystick support. Enabled by default.

--keepaspect, --no-keepaspect
    Keep window aspect ratio when resizing windows. Enabled by default. By
    default MPlayer tries to keep the correct video aspect ratio by
    instructing the window manager to maintain window aspect when resizing,
    and by adding black bars if the window manager nevertheless allows window
    shape to change. --no-keepaspect disables window manager aspect hints and
    scales the video to completely fill the window without regard for aspect
    ratio.

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
        :0x0040: motion vector visualization (use ``--no-slices``)
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

    lowres=<number>[,<w>]
        Decode at lower resolutions. Low resolution decoding is not supported
        by all codecs, and it will often result in ugly artifacts. This is not
        a bug, but a side effect of not decoding at full resolution.

        :0: disabled
        :1: 1/2 resolution
        :2: 1/4 resolution
        :3: 1/8 resolution

        If <w> is specified lowres decoding will be used only if the width of
        the video is major than or equal to <w>.

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

    vstats
        Prints some statistics and stores them in ``./vstats_*.log``.

--lavfdopts=<option1:option2:...>
    Specify parameters for libavformat demuxers (``--demuxer=lavf``). Separate
    multiple options with a colon.

    Available suboptions are:

    analyzeduration=<value>
        Maximum length in seconds to analyze the stream properties.
    format=<value>
        Force a specific libavformat demuxer.
    o=<key>=<value>[,<key>=<value>[,...]]
        Pass AVOptions to libavformat demuxer.

        Note, a patch to make the *o=* unneeded and pass all unknown options
        through the AVOption system is welcome. A full list of AVOptions can
        be found in the FFmpeg manual. Note that some options may conflict
        with MPlayer options.

        *EXAMPLE*: ``o=fflags=+ignidx``
    probesize=<value>
        Maximum amount of data to probe during the detection phase. In the
        case of MPEG-TS this value identifies the maximum number of TS packets
        to scan.
    cryptokey=<hexstring>
        Encryption key the demuxer should use. This is the raw binary data of
        the key converted to a hexadecimal string.

--lirc, --no-lirc
    Enable/disable LIRC support. Enabled by default.

--lircconf=<filename>
    (LIRC only)
    Specifies a configuration file for LIRC (default: ``~/.lircrc``).

--list-options
    Prints all available options.

--list-properties
    Print a list of the available properties.

--loadidx=<filename>
    The file from which to read the video index data saved by ``--saveidx``.
    This index will be used for seeking, overriding any index data contained
    in the AVI itself. MPlayer will not prevent you from loading an index file
    generated from a different AVI, but this is sure to cause unfavorable
    results.

    *NOTE*: This option is obsolete now that MPlayer has OpenDML support.

--loop=<number>
    Loops movie playback <number> times. 0 means forever.

--mc=<seconds/frame>
    Maximum A-V sync correction per frame (in seconds)

--mf=<option1:option2:...>
    Used when decoding from multiple PNG or JPEG files.

    Available options are:

    :w=<value>:    input file width (default: autodetect)
    :h=<value>:    input file height (default: autodetect)
    :fps=<value>:  output fps (default: 25)
    :type=<value>: input file type (available: jpeg, png, tga, sgi)

--mixer=<device>
    Use a mixer device different from the default ``/dev/mixer``. For ALSA
    this is the mixer name.

--mixer-channel=<name[,index]>
    (``--ao=oss`` and ``--ao=alsa`` only)
    This option will tell MPlayer to use a different channel for controlling
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
    Permit MPlayer to receive pointer events reported by the video output
    driver. Necessary to select the buttons in DVD menus. Supported for
    X11-based VOs (x11, xv, etc) and the gl, direct3d and corevideo VOs.

--mouseinput, --no-mouseinput
    Enabled by default. Disable mouse button press/release input
    (mozplayerxp's context menu relies on this option).

--msgcolor
    Enable colorful console output on terminals that support ANSI color.

--msglevel=<module1=level1:module2=level2:...>
    Control verbosity directly for each module. The *all* module changes the
    verbosity of all the modules not explicitly specified on the command line.

    See ``--msglevel=help`` for a list of all modules.

    *NOTE*: Some messages are printed before the command line is parsed and
    are therefore not affected by ``--msglevel``. To control these messages
    you have to use the ``MPLAYER_VERBOSE`` environment variable; see its
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

--name
    Set the window class name for X11-based video output methods.

--ni
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

--no-config=<options>
    Do not parse selected configuration files.

    *NOTE*: If ``--include`` or ``--use-filedir-conf`` options are specified
    at the command line, they will be honoured.

    Available options are:

    :all:    all configuration files
    :system: system configuration file
    :user:   user configuration file

--no-idx
    Do not use index present in the file even if one is present.

--no-audio
    Do not play sound. Useful for benchmarking.

--no-sub
    Disables any otherwise auto-selected internal subtitles (as e.g. the
    Matroska/mkv demuxer supports). Use ``--no-autosub`` to disable the
    loading of external subtitle files.

--no-video
    Do not play video. With some demuxers this may not work. In those cases
    you can try ``--vc=null --vo=null`` instead; but ``--vc=null`` is always
    unreliable.

--ontop
    Makes the player window stay on top of other windows. Supported by video
    output drivers which use X11, as well as corevideo.

--ordered-chapters, --no-ordered-chapters
    Enabled by default.
    Disable support for Matroska ordered chapters. MPlayer will not load or
    search for video segments from other files, and will also ignore any
    chapter order specified for the main file.

--osd-duration=<time>
    Set the duration of the OSD messages in ms (default: 1000).

--osd-fractions
    Show OSD times with fractions of seconds.

--osdlevel=<0-3>
    Specifies which mode the OSD should start in.

    :0: subtitles only
    :1: volume + seek (default)
    :2: volume + seek + timer + percentage
    :3: volume + seek + timer + percentage + total time

--overlapsub
    Allows the next subtitle to be displayed while the current one is still
    visible (default is to enable the support only for specific formats).

--panscan=<0.0-1.0>
    Enables pan-and-scan functionality (cropping the sides of e.g. a 16:9
    movie to make it fit a 4:3 display without black bands). The range
    controls how much of the image is cropped. May not work with all video
    output drivers.

    *NOTE*: Values between -1 and 0 are allowed as well, but highly
    experimental and may crash or worse. Use at your own risk!

--panscanrange=<-19.0-99.0>
    (experimental)
    Change the range of the pan-and-scan functionality (default: 1). Positive
    values mean multiples of the default range. Negative numbers mean you can
    zoom in up to a factor of ``--panscanrange=+1``. E.g. ``--panscanrange=-3``
    allows a zoom factor of up to 4. This feature is experimental. Do not
    report bugs unless you are using ``--vo=gl``.

--passwd=<password>
    Used with some network protocols. Specify password for HTTP authentication.
    See also ``--user``.

--playing-msg=<string>
    Print out a string before starting playback. The following expansions are
    supported:

    ${NAME}
        Expand to the value of the property ``NAME``.
    ?(NAME:TEXT)
        Expand ``TEXT`` only if the property ``NAME`` is available.
    ?(!NAME:TEXT)
        Expand ``TEXT`` only if the property ``NAME`` is not available.

--playlist=<filename>
    Play files according to a playlist file (ASX, Winamp, SMIL, or
    one-file-per-line format).

    *WARNING*: The way MPlayer parses and uses playlist files is not safe
    against maliciously constructed files. Such files may trigger harmful
    actions. This has been the case for all MPlayer versions, but
    unfortunately this fact was not well documented earlier, and some people
    have even misguidedly recommended use of ``--playlist`` with untrusted
    sources. Do NOT use ``--playlist`` with random internet sources or files
    you don't trust!

    *NOTE*: This option is considered an entry so options found after it will
    apply only to the elements of this playlist.

    FIXME: This needs to be clarified and documented thoroughly.

--pp=<quality>
    This option only works when decoding video with Win32 DirectShow DLLs with
    internal postprocessing routines. See also ``--vf=pp``. Set the DLL
    postprocess level. The valid range of ``--pp`` values varies by codec, it
    is mostly 0-6, where 0=disable, 6=slowest/best.

--pphelp
    Show a summary about the available postprocess filters and their usage.
    See also ``--vf=pp``.

--prefer-ipv4
    Use IPv4 on network connections. Falls back on IPv6 automatically.

--prefer-ipv6
    Use IPv6 on network connections. Falls back on IPv4 automatically.

--priority=<prio>
    (Windows only.)
    Set process priority for MPlayer according to the predefined priorities
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
    For hardware capture of an MPEG stream and watching it with MPlayer, use
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

--radio=<option1:option2:...>
    These options set various parameters of the radio capture module. For
    listening to radio with MPlayer use ``radio://<frequency>`` (if channels
    option is not given) or ``radio://<channel_number>`` (if channels option
    is given) as a movie URL. You can see allowed frequency range by running
    MPlayer with ``-v``. To start the grabbing subsystem, use
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

    freq_min=<value> (\*BSD BT848 only)
        minimum allowed frequency (default: 87.50)

    freq_max=<value> (\*BSD BT848 only)
        maximum allowed frequency (default: 108.00)

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
    which are not 44kHz 16-bit stereo. For playing raw AC-3 streams use
    ``--rawaudio=format=0x2000 --demuxer=rawaudio``.

    Available options are:

    :channels=<value>:   number of channels
    :rate=<value>:       rate in samples per second
    :samplesize=<value>: sample size in bytes
    :bitrate=<value>:    bitrate for rawaudio files
    :format=<value>:     fourcc in hex

--rawvideo=<option1:option2:...>
    This option lets you play raw video files. You have to use
    ``--demuxer=rawvideo`` as well.

    Available options are:

    :fps=<value>:                  rate in frames per second (default: 25.0)
    :sqcif|qcif|cif|4cif|pal|ntsc: set standard image size
    :w=<value>:                    image width in pixels
    :h=<value>:                    image height in pixels
    :i420|yv12|yuy2|y8:            set colorspace
    :format=<value>:               colorspace (fourcc) in hex or string
                                   constant. Use ``--rawvideo=format=help``
                                   for a list of possible strings.
    :size=<value>:                 frame size in Bytes

    *EXAMPLE*:

    - ``mplayer foreman.qcif --demuxer=rawvideo --rawvideo=qcif`` Play the
      famous "foreman" sample video.

    - ``mplayer sample-720x576.yuv --demuxer=rawvideo --rawvideo=w=720:h=576``
      Play a raw YUV sample.

--really-quiet
    Display even less output and status messages than with ``--quiet``.

--referrer=<string>
    Specify a referrer path or URL for HTTP requests.

--reuse-socket
    (udp:// only)
    Allows a socket to be reused by other processes as soon as it is closed.

--rootwin
    Play movie in the root window (desktop background). Desktop background
    images may cover the movie window, though. May not work with all video
    output drivers.

--rtsp-destination
    Used with ``rtsp://`` URLs to force the destination IP address to be
    bound. This option may be useful with some RTSP server which do not send
    RTP packets to the right interface. If the connection to the RTSP server
    fails, use ``-v`` to see which IP address MPlayer tries to bind to and try
    to force it to one assigned to your computer instead.

--rtsp-port
    Used with ``rtsp://`` URLs to force the client's port number. This option
    may be useful if you are behind a router and want to forward the RTSP
    stream from the server to a specific client.

--rtsp-stream-over-http
    (LIVE555 only)
    Used with ``http://`` URLs to specify that the resulting incoming RTP and
    RTCP packets be streamed over HTTP.

--rtsp-stream-over-tcp
    (LIVE555 and NEMESI only)
    Used with ``rtsp://`` URLs to specify that the resulting incoming RTP and
    RTCP packets be streamed over TCP (using the same TCP connection as RTSP).
    This option may be useful if you have a broken internet connection that
    does not pass incoming UDP packets (see http://www.live555.com/mplayer/).

--saturation=<-100-100>
    Adjust the saturation of the video signal (default: 0). You can get
    grayscale output with this option. Not supported by all video output
    drivers.

--saveidx=<filename>
    Force index rebuilding and dump the index to <filename>. Currently this
    only works with AVI files.

    *NOTE*: This option is obsolete now that MPlayer has OpenDML support.

--sb=<n>
    Seek to byte position. Useful for playback from CD-ROM images or VOB files
    with junk at the beginning. See also ``--ss``.

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
        the optional # sign mplayer will use the lowest available number. For
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
        Insert the value of the slave property 'prop'. E.g. %{filename} is the
        same as %f. If the property doesn't exist or is not available, nothing
        is inserted, unless a fallback is specified.
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

--sid=<ID>
    Display the subtitle stream specified by <ID> (0-31). MPlayer prints the
    available subtitle IDs when run in verbose (``-v``) mode. If you cannot
    select one of the subtitles on a DVD, try ``--vobsubid``.
    See also ``--slang``, ``--vobsubid``, ``--no-sub``.

--slang=<languagecode[,languagecode,...]>
    Specify a priority list of subtitle languages to use. Different container
    formats employ different language codes. DVDs use ISO 639-1 two letter
    language codes, Matroska uses ISO 639-2 three letter language codes while
    OGM uses a free-form identifier. MPlayer prints the available languages
    when run in verbose (``-v``) mode. See also ``--sid``.

    *EXAMPLE*:

    - ``mplayer dvd://1 --slang=hu,en`` chooses the Hungarian subtitle track on
      a DVD and falls back on English if Hungarian is not available.
    - ``mplayer --slang=jpn example.mkv`` plays a Matroska file with Japanese
      subtitles.

--slave-broken
    Switches on the old slave mode. This is for testing only, and incompatible
    to the removed --slave switch.

    *NOTE*: Changes incompatible to slave mode applications have been made. In
    particular, the status line output was changed, which is used by some
    applications to determine the current playback position. This switch has
    been renamed to prevent these applications from working with this version
    of mplayer, because it would lead to buggy and confusing behavior only.
    Moreover, the slave mode protocol is so horribly bad that it should not be
    used for new programs, nor should existing programs attempt to adapt to the
    changed output and use the --slave-broken switch. Instead, a new, saner
    protocol should be developed (and will, if there is enough interest).

    This affects smplayer, smplayer2, mplayerosx, and others.

--slices, --no-slices
    Drawing video by 16-pixel height slices/bands, instead draws the
    whole frame in a single run. May be faster or slower, depending on video
    card and available cache. It has effect only with libavcodec codecs.
    Enabled by default if applicable; usually disabled when threading is used.

--softsleep
    Time frames by repeatedly checking the current time instead of asking
    the kernel to wake up MPlayer at the correct time. Useful if your kernel
    timing is imprecise and you cannot use the RTC either. Comes at the
    price of higher CPU consumption.

--no-softvol
    Try to use the sound card mixer (if available), instead of using the volume
    audio filter.

--softvol-max=<10.0-10000.0>
    Set the maximum amplification level in percent (default: 200). A value of
    200 will allow you to adjust the volume up to a maximum of double the
    current level. With values below 100 the initial volume (which is 100%)
    will be above the maximum, which e.g. the OSD cannot display correctly.

--speed=<0.01-100>
    Slow down or speed up playback by the factor given as parameter.

--spuaa=<mode>
    Antialiasing/scaling mode for DVD/VOBsub. A value of 16 may be added to
    <mode> in order to force scaling even when original and scaled frame size
    already match. This can be employed to e.g. smooth subtitles with gaussian
    blur. Available modes are:

    :0: none (fastest, very ugly)
    :1: approximate (broken?)
    :2: full (slow)
    :3: bilinear (default, fast and not too bad)
    :4: uses swscaler gaussian blur (looks very good)

--spualign=<-1-2>
    Specify how SPU (DVD/VOBsub) subtitles should be aligned.

    :-1:  Original position
    :0:   Align at top (original behavior, default).
    :1:   Align at center.
    :2:   Align at bottom.

--spugauss=<0.0-3.0>
    Variance parameter of gaussian used by ``--spuaa=4``. Higher means more
    blur (default: 1.0).

--srate=<Hz>
    Select the output sample rate to be used (of course sound cards have
    limits on this). If the sample frequency selected is different from that
    of the current media, the resample or lavcresample audio filter will be
    inserted into the audio filter layer to compensate for the difference. The
    type of resampling can be controlled by the ``--af-adv`` option.

--ss=<time>
    Seek to given time position.

    *EXAMPLE*:

    ``--ss=56``
        Seeks to 56 seconds.
    ``--ss=01:10:00``
        Seeks to 1 hour 10 min.

--ssf=<mode>
    Specifies software scaler parameters.

    :lgb=<0-100>:   gaussian blur filter (luma)
    :cgb=<0-100>:   gaussian blur filter (chroma)
    :ls=<-100-100>: sharpen filter (luma)
    :cs=<-100-100>: sharpen filter (chroma)
    :chs=<h>:       chroma horizontal shifting
    :cvs=<v>:       chroma vertical shifting

    *EXAMPLE*: ``--vf=scale=-ssf=lgb=3.0``

--sstep=<sec>
    Skip <sec> seconds after every frame. Since MPlayer will only seek to
    the next keyframe unless you use ``--hr-seek`` this may be inexact.

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

--sub-bg-alpha=<0-255>
    Specify the alpha channel value for subtitles and OSD backgrounds. Big
    values mean more transparency. 0 means completely transparent.

--sub-bg-color=<0-255>
    Specify the color value for subtitles and OSD backgrounds. Currently
    subtitles are grayscale so this value is equivalent to the intensity of
    the color. 255 means white and 0 black.

--sub-demuxer=<[+]name>
    Force subtitle demuxer type for ``--subfile``. Using a '+' before the name
    will force it, this will skip some checks! Give the demuxer name as
    printed by ``--sub-demuxer=help``.

--sub-fuzziness=<mode>
    Adjust matching fuzziness when searching for subtitles:

    :0: exact match
    :1: Load all subs containing movie name.
    :2: Load all subs in the current and ``--sub-paths`` directories.

--sub-no-text-pp
    Disables any kind of text post processing done after loading the
    subtitles. Used for debug purposes.

--sub-paths=<path1:path2:...>
    Specify extra directories where to search for subtitles matching the
    video. Multiple directories can be separated by ":" (";" on Windows).
    Paths can be relative or absolute. Relative paths are interpreted relative
    to video file directory.

    *EXAMPLE*: Assuming that ``/path/to/movie/movie.avi`` is played and
    ``--sub-paths=sub:subtitles:/tmp/subs`` is specified, MPlayer searches for
    subtitle files in these directories:

    - ``/path/to/movie/``
    - ``/path/to/movie/sub/``
    - ``/path/to/movie/subtitles/``
    - ``/tmp/subs/``
    - ``~/.mplayer/sub/``

--subalign=<0-2>
    Specify which edge of the subtitles should be aligned at the height given
    by ``--subpos``.

    :0: Align subtitle top edge (original behavior).
    :1: Align subtitle center.
    :2: Align subtitle bottom edge (default).

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
    watch mplayer ``-v`` output for available languages. Fallback codepage
    specifies the codepage to use, when autodetection fails.

    *EXAMPLE*:

    - ``--subcp=enca:cs:latin2`` guess the encoding, assuming the subtitles
      are Czech, fall back on latin 2, if the detection fails.
    - ``--subcp=enca:pl:cp1250`` guess the encoding for Polish, fall back on
      cp1250.

--subdelay=<sec>
    Delays subtitles by <sec> seconds. Can be negative.

--subfile=<filename>
    (BETA CODE)
    Currently useless. Same as ``--audiofile``, but for subtitle streams
    (OggDS?).

--subfont=<pattern-or-filename>
    Sets the subtitle font (see ``--font``). If no ``--subfont`` is given,
    ``--font`` is used for subtitles too.

--subfont-autoscale=<0-3>
    Sets the autoscale mode.

    *NOTE*: 0 means that text scale and OSD scale are font heights in points.

    The mode can be:

    :0: no autoscale
    :1: proportional to movie height
    :2: proportional to movie width
    :3: proportional to movie diagonal (default)

--subfont-blur=<0-8>
    Sets the font blur radius (default: 2).

--subfont-encoding=<value>
    Sets the font encoding. When set to 'unicode', all the glyphs from the
    font file will be rendered and unicode will be used (default: unicode).

--subfont-osd-scale=<0-100>
    Sets the autoscale coefficient of the OSD elements (default: 4).

--subfont-outline=<0-8>
    Sets the font outline thickness (default: 2).

--subfont-text-scale=<0-100>
    Sets the subtitle text autoscale coefficient as percentage of the screen
    size (default: 3.5).

--subfps=<rate>
    Specify the framerate of the subtitle file (default: movie fps).

    *NOTE*: <rate> > movie fps speeds the subtitles up for frame-based
    subtitle files and slows them down for time-based ones.

--subpos=<0-100>
    Specify the position of subtitles on the screen. The value is the vertical
    position of the subtitle in % of the screen height.
    Can be useful with ``--vf=expand``.

--subwidth=<10-100>
    Specify the maximum width of subtitles on the screen. Useful for TV-out.
    The value is the width of the subtitle in % of the screen width.

--sws=<n>
    Specify the software scaler algorithm to be used with the ``--zoom``
    option. This affects video output drivers which lack hardware
    acceleration, e.g. x11. See also ``--vf=scale`` and ``--zoom``.

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

--title
    Set the window title. The string can contain property names.

--tv=<option1:option2:...>
    This option tunes various properties of the TV capture module. For
    watching TV with MPlayer, use ``tv://`` or ``tv://<channel_number>`` or
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
        available: dummy, v4l, v4l2, bsdbt848 (default: autodetect)

    device=<value>
        Specify TV device (default: ``/dev/video0``). NOTE: For the bsdbt848
        driver you can provide both bktr and tuner device names separating
        them with a comma, tuner after bktr (e.g. ``--tv
        device=/dev/bktr1,/dev/tuner1``).

    input=<value>
        Specify input (default: 0 (TV), see console output for available
        inputs).

    freq=<value>
        Specify the frequency to set the tuner to (e.g. 511.250). Not
        compatible with the channels parameter.

    outfmt=<value>
        Specify the output format of the tuner with a preset value supported
        by the V4L driver (yv12, rgb32, rgb24, rgb16, rgb15, uyvy, yuy2, i420)
        or an arbitrary format given as hex value. Try outfmt=help for a list
        of all available formats.

    width=<value>
        output window width

    height=<value>
        output window height

    fps=<value>
        framerate at which to capture video (frames per second)

    buffersize=<value>
        maximum size of the capture buffer in megabytes (default: dynamical)

    norm=<value>
        For bsdbt848 and v4l, PAL, SECAM, NTSC are available. For v4l2, see
        the console output for a list of all available norms, also see the
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
        output window, because MPlayer will determine it automatically from
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
    Tune the TV channel scanner. MPlayer will also print value for "-tv
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

--unicode
    Tells MPlayer to handle the subtitle file as unicode.

--use-filedir-conf
    Look for a file-specific configuration file in the same directory as the
    file that is being played.

    *WARNING*: May be dangerous if playing from untrusted media.

--user=<username>
    Used with some network protocols.
    Specify username for HTTP authentication. See also ``--passwd``.

--user-agent=<string>
    Use <string> as user agent for HTTP streaming.

--utf8
    Tells MPlayer to handle the subtitle file as UTF-8.

-v
    Increment verbosity level, one level for each ``-v`` found on the command
    line.

--vc=<[-\|+]codec1,[-\|+]codec2,...[,]>
    Specify a priority list of video codecs to be used, according to their
    codec name in ``codecs.conf``. Use a '-' before the codec name to omit it.
    Use a '+' before the codec name to force it, this will likely crash! If
    the list has a trailing ',' MPlayer will fall back on codecs not contained
    in the list.

    *NOTE*: See ``--vc=help`` for a full list of available codecs.

    *EXAMPLE*:

    :``--vc=divx``:             Force Win32/VfW DivX codec, no fallback.
    :``--vc=-divxds,-divx,``:   Skip Win32 DivX codecs.
    :``--vc=ffmpeg12,mpeg12,``: Try libavcodec's MPEG-1/2 codec, then
                                libmpeg2, then others.

--vf=<filter1[=parameter1:parameter2:...],filter2,...>
    Specify a list of video filters to apply to the video stream. See
    :ref:`video_filters` for details and descriptions of the available filters.
    The option variants ``--vf-add``, ``--vf-pre``, ``--vf-del`` and
    ``--vf-clr`` exist to modify a previously specified list, but you
    shouldn't need these for typical use.

--vfm=<driver1,driver2,...>
    Specify a priority list of video codec families to be used, according to
    their names in codecs.conf. Falls back on the default codecs if none of
    the given codec families work.

    *NOTE*: See ``--vfm=help`` for a full list of available codec families.

    *EXAMPLE*:

    :``--vfm=ffmpeg,dshow,vfw``:
        Try the libavcodec, then Directshow, then VfW codecs and fall back on
        others, if they do not work.
    :``--vfm=xanim``:
        Try XAnim codecs first.

--vid=<ID>
    Select video channel (MPG: 0-15, ASF: 0-255, MPEG-TS: 17-8190). When
    playing an MPEG-TS stream, MPlayer will use the first program (if present)
    with the chosen video stream.

--vm
    Try to change to a different video mode. Supported by the x11 and xv video
    output drivers.

--vo=<driver1[:suboption1[=value]:...],driver2,...[,]>
    Specify a priority list of video output drivers to be used. For
    interactive use you'd normally specify a single one to use, but in
    configuration files specifying a list of fallbacks may make sense. See
    :ref:`video_outputs` for details and descriptions of available drivers.

--vobsub=<file>
    Specify a VOBsub file to use for subtitles. Has to be the full pathname
    without extension, i.e. without the ``.idx``, ``.ifo`` or ``.sub``.

--vobsubid=<0-31>
    Specify the VOBsub subtitle ID.

--volstep=<0-100>
    Set the step size of mixer volume changes in percent of the whole range
    (default: 3).

--volume=<-1-100>
    Set the startup volume in the mixer, either hardware or software (if used
    with ``--softvol``). A value of -1 (the default) will not change the
    volume. See also ``--af=volume``.

--no-vsync
    Tries to disable vsync.

--wid=<ID>
    (X11, OpenGL and DirectX only)
    This tells MPlayer to attach to an existing window. Useful to embed
    MPlayer in a browser (e.g. the plugger extension). Earlier this option
    always filled the given window completely, thus aspect scaling, panscan,
    etc were no longer handled by MPlayer but had to be managed by the
    application that created the window. Now aspect is maintained by default.
    If you don't want that use ``--no-keepaspect``.

--x=<width>
    Scale image to width <width> (if software/hardware scaling is available).
    Disables aspect calculations.

--xineramascreen=<-2-...>
    In Xinerama configurations (i.e. a single desktop that spans across
    multiple displays) this option tells MPlayer which screen to display the
    movie on. A value of -2 means fullscreen across the whole virtual display
    (in this case Xinerama information is completely ignored), -1 means
    fullscreen on the display the window currently is on. The initial position
    set via the ``--geometry`` option is relative to the specified screen.
    Will usually only work with ``--fstype=-fullscreen`` or ``--fstype=none``.
    This option is not suitable to only set the startup screen (because it
    will always display on the given screen in fullscreen mode),
    ``--geometry`` is the best that is available for that purpose currently.
    Supported by at least the direct3d, gl, x11, xv and corevideo video output
    drivers.

--xvidopts=<option1:option2:...>
    Specify additional parameters when decoding with Xvid.

    *NOTE*: Since libavcodec is faster than Xvid you might want to use the
    libavcodec postprocessing filter (``--vf=pp``) and decoder
    (``--vfm=ffmpeg``) instead.

    Xvid's internal postprocessing filters:

    :deblock-chroma (see also ``--vf=pp``):    chroma deblock filter
    :deblock-luma   (see also ``--vf=pp``):    luma deblock filter
    :dering-luma    (see also ``--vf=pp``):    luma deringing filter
    :dering-chroma  (see also ``--vf=pp``):    chroma deringing filter
    :filmeffect     (see also ``--vf=noise``):
        Adds artificial film grain to the video. May increase perceived
        quality, while lowering true quality.

    rendering methods:

    :dr2:   Activate direct rendering method 2.
    :nodr2: Deactivate direct rendering method 2.

--xy=<value>

    :value<=8: Scale image by factor <value>.
    :value>8:  Set width to value and calculate height to keep correct aspect
               ratio.

--y=<height>
    Scale image to height <height> (if software/hardware scaling is available).
    Disables aspect calculations.

--zoom
    Allow software scaling, where available. This will allow scaling with
    output drivers (like x11) that do not support hardware scaling,
    where MPlayer disables scaling by default for performance reasons.
