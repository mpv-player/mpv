OPTIONS
=======

Track Selection
---------------

``--alang=<languagecode[,languagecode,...]>``
    Specify a priority list of audio languages to use. Different container
    formats employ different language codes. DVDs use ISO 639-1 two-letter
    language codes, Matroska, MPEG-TS and NUT use ISO 639-2 three-letter
    language codes, while OGM uses a free-form identifier. See also ``--aid``.

    .. admonition:: Examples

        ``mpv dvd://1 --alang=hu,en``
            Chooses the Hungarian language track on a DVD and falls back on
            English if Hungarian is not available.
        ``mpv --alang=jpn example.mkv``
            Plays a Matroska file in Japanese.

``--slang=<languagecode[,languagecode,...]>``
    Specify a priority list of subtitle languages to use. Different container
    formats employ different language codes. DVDs use ISO 639-1 two letter
    language codes, Matroska uses ISO 639-2 three letter language codes while
    OGM uses a free-form identifier. See also ``--sid``.

    .. admonition:: Examples

        - ``mpv dvd://1 --slang=hu,en`` chooses the Hungarian subtitle track on
          a DVD and falls back on English if Hungarian is not available.
        - ``mpv --slang=jpn example.mkv`` plays a Matroska file with Japanese
          subtitles.

``--aid=<ID|auto|no>``
    Select audio track. ``auto`` selects the default, ``no`` disables audio.
    See also ``--alang``. mpv normally prints available audio tracks on the
    terminal when starting playback of a file.

``--sid=<ID|auto|no>``
    Display the subtitle stream specified by ``<ID>``. ``auto`` selects
    the default, ``no`` disables subtitles.

    See also ``--slang``, ``--no-sub``.

``--vid=<ID|auto|no>``
    Select video channel. ``auto`` selects the default, ``no`` disables video.

``--ff-aid=<ID|auto|no>``, ``--ff-sid=<ID|auto|no>``, ``--ff-vid=<ID|auto|no>``
    Select audio/subtitle/video streams by the FFmpeg stream index. The FFmpeg
    stream index is relatively arbitrary, but useful when interacting with
    other software using FFmpeg (consider ``ffprobe``).

    Note that with external tracks (added with ``--sub-file`` and similar
    options), there will be streams with duplicate IDs. In this case, the
    first stream in order is selected.

``--edition=<ID|auto>``
    (Matroska files only)
    Specify the edition (set of chapters) to use, where 0 is the first. If set
    to ``auto`` (the default), mpv will choose the first edition declared as a
    default, or if there is no default, the first edition defined.


Playback Control
----------------

``--start=<relative time>``
    Seek to given time position.

    The general format for absolute times is ``[[hh:]mm:]ss[.ms]``. If the time
    is given with a prefix of ``+`` or ``-``, the seek is relative from the start
    or end of the file.

    ``pp%`` seeks to percent position pp (0-100).

    ``#c`` seeks to chapter number c. (Chapters start from 1.)

    .. admonition:: Examples

        ``--start=+56``, ``--start=+00:56``
            Seeks to the start time + 56 seconds.
        ``--start=-56``, ``--start=-00:56``
            Seeks to the end time - 56 seconds.
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

``--end=<time>``
    Stop at given absolute time. Use ``--length`` if the time should be relative
    to ``--start``. See ``--start`` for valid option values and examples.

``--length=<relative time>``
    Stop after a given time relative to the start time.
    See ``--start`` for valid option values and examples.

``--speed=<0.01-100>``
    Slow down or speed up playback by the factor given as parameter.

    If ``--audio-pitch-correction`` (on by default) is used, playing with a
    speed higher than normal automatically inserts the ``scaletempo`` audio
    filter.

``--loop=<N|inf|force|no>``
    Loops playback ``N`` times. A value of ``1`` plays it one time (default),
    ``2`` two times, etc. ``inf`` means forever. ``no`` is the same as ``1`` and
    disables looping. If several files are specified on command line, the
    entire playlist is looped.

    The ``force`` mode is like ``inf``, but does not skip playlist entries
    which have been marked as failing. This means the player might waste CPU
    time trying to loop a file that doesn't exist. But it might be useful for
    playing webradios under very bad network conditions.

``--pause``
    Start the player in paused state.

``--shuffle``
    Play files in random order.

``--chapter=<start[-end]>``
    Specify which chapter to start playing at. Optionally specify which
    chapter to end playing at. Also see ``--start``.

``--playlist=<filename>``
    Play files according to a playlist file (Supports some common formats. If
    no format is detected, it will be treated as list of files, separated by
    newline characters. Note that XML playlist formats are not supported.)

    You can play playlists directly and without this option, however, this
    option disables any security mechanisms that might be in place. You may
    also need this option to load plaintext files as playlist.

    .. warning::

        The way mpv uses playlist files via ``--playlist`` is not safe against
        maliciously constructed files. Such files may trigger harmful actions.
        This has been the case for all mpv and MPlayer versions, but
        unfortunately this fact was not well documented earlier, and some people
        have even misguidedly recommended use of ``--playlist`` with untrusted
        sources. Do NOT use ``--playlist`` with random internet sources or files
        you do not trust!

        Playlist can contain entries using other protocols, such as local files,
        or (most severely), special protocols like ``avdevice://``, which are
        inherently unsafe.

``--chapter-merge-threshold=<number>``
    Threshold for merging almost consecutive ordered chapter parts in
    milliseconds (default: 100). Some Matroska files with ordered chapters
    have inaccurate chapter end timestamps, causing a small gap between the
    end of one chapter and the start of the next one when they should match.
    If the end of one playback part is less than the given threshold away from
    the start of the next one then keep playing video normally over the
    chapter change instead of doing a seek.

``--chapter-seek-threshold=<seconds>``
    Distance in seconds from the beginning of a chapter within which a backward
    chapter seek will go to the previous chapter (default: 5.0). Past this
    threshold, a backward chapter seek will go to the beginning of the current
    chapter instead. A negative value means always go back to the previous
    chapter.

``--hr-seek=<no|absolute|yes>``
    Select when to use precise seeks that are not limited to keyframes. Such
    seeks require decoding video from the previous keyframe up to the target
    position and so can take some time depending on decoding performance. For
    some video formats, precise seeks are disabled. This option selects the
    default choice to use for seeks; it is possible to explicitly override that
    default in the definition of key bindings and in slave mode commands.

    :no:       Never use precise seeks.
    :absolute: Use precise seeks if the seek is to an absolute position in the
               file, such as a chapter seek, but not for relative seeks like
               the default behavior of arrow keys (default).
    :yes:      Use precise seeks whenever possible.
    :always:   Same as ``yes`` (for compatibility).

``--hr-seek-demuxer-offset=<seconds>``
    This option exists to work around failures to do precise seeks (as in
    ``--hr-seek``) caused by bugs or limitations in the demuxers for some file
    formats. Some demuxers fail to seek to a keyframe before the given target
    position, going to a later position instead. The value of this option is
    subtracted from the time stamp given to the demuxer. Thus, if you set this
    option to 1.5 and try to do a precise seek to 60 seconds, the demuxer is
    told to seek to time 58.5, which hopefully reduces the chance that it
    erroneously goes to some time later than 60 seconds. The downside of
    setting this option is that precise seeks become slower, as video between
    the earlier demuxer position and the real target may be unnecessarily
    decoded.

``--hr-seek-framedrop=<yes|no>``
    Allow the video decoder to drop frames during seek, if these frames are
    before the seek target. If this is enabled, precise seeking can be faster,
    but if you're using video filters which modify timestamps or add new
    frames, it can lead to precise seeking skipping the target frame. This
    e.g. can break frame backstepping when deinterlacing is enabled.

    Default: ``yes``

``--index=<mode>``
    Controls how to seek in files. Note that if the index is missing from a
    file, it will be built on the fly by default, so you don't need to change
    this. But it might help with some broken files.

    :default:   use an index if the file has one, or build it if missing
    :recreate:  don't read or use the file's index

    .. note::

        This option only works if the underlying media supports seeking
        (i.e. not with stdin, pipe, etc).

``--load-unsafe-playlists``
    Load URLs from playlists which are considered unsafe (default: no). This
    includes special protocols and anything that doesn't refer to normal files.
    Local files and HTTP links on the other hand are always considered safe.

    Note that ``--playlist`` always loads all entries, so you use that instead
    if you really have the need for this functionality.

``--loop-file=<N|inf|no>``
    Loop a single file N times. ``inf`` means forever, ``no`` means normal
    playback. For compatibility, ``--loop-file`` and ``--loop-file=yes`` are
    also accepted, and are the same as ``--loop-file=inf``.

    The difference to ``--loop`` is that this doesn't loop the playlist, just
    the file itself. If the playlist contains only a single file, the difference
    between the two option is that this option performs a seek on loop, instead
    of reloading the file.

``--ab-loop-a=<time>``, ``--ab-loop-b=<time>``
    Set loop points. If playback passes the ``b`` timestamp, it will seek to
    the ``a`` timestamp. Seeking past the ``b`` point doesn't loop (this is
    intentional). The loop-points can be adjusted at runtime with the
    corresponding properties. See also ``ab_loop`` command.

``--ordered-chapters``, ``--no-ordered-chapters``
    Enabled by default.
    Disable support for Matroska ordered chapters. mpv will not load or
    search for video segments from other files, and will also ignore any
    chapter order specified for the main file.

``--ordered-chapters-files=<playlist-file>``
    Loads the given file as playlist, and tries to use the files contained in
    it as reference files when opening a Matroska file that uses ordered
    chapters. This overrides the normal mechanism for loading referenced
    files by scanning the same directory the main file is located in.

    Useful for loading ordered chapter files that are not located on the local
    filesystem, or if the referenced files are in different directories.

    Note: a playlist can be as simple as a text file containing filenames
    separated by newlines.

``--chapters-file=<filename>``
    Load chapters from this file, instead of using the chapter metadata found
    in the main file.

``--sstep=<sec>``
    Skip <sec> seconds after every frame.

    .. note::

        Without ``--hr-seek``, skipping will snap to keyframes.

``--stop-playback-on-init-failure=<yes|no>``
    Stop playback if either audio or video fails to initialize. Currently,
    the default behavior is ``no`` for the command line player, but ``yes``
    for libmpv. With ``no``, playback will continue in video-only or audio-only
    mode if one of them fails. This doesn't affect playback of audio-only or
    video-only files.

Program Behavior
----------------

``--help``
    Show short summary of options.

``-v``
    Increment verbosity level, one level for each ``-v`` found on the command
    line.

``--version, -V``
    Print version string and exit.

``--no-config``
    Do not load default configuration files. This prevents loading of both the
    user-level and system-wide ``mpv.conf`` and ``input.conf`` files. Other
    configuration files are blocked as well, such as resume playback files.

    .. note::

        Files explicitly requested by command line options, like
        ``--include`` or ``--use-filedir-conf``, will still be loaded.

    Also see ``--config-dir``.

``--list-options``
    Prints all available options.

``--list-properties``
    Print a list of the available properties.

``--list-protocols``
    Print a list of the supported protocols.

``--log-file=<path>``
    Opens the given path for writing, and print log messages to it. Existing
    files will be truncated. The log level always corresponds to ``-v``,
    regardless of terminal verbosity levels.

``--config-dir=<path>``
    Force a different configuration directory. If this is set, the given
    directory is used to load configuration files, and all other configuration
    directories are ignored. This means the global mpv configuration directory
    as well as per-user directories are ignored, and overrides through
    environment variables (``MPV_HOME``) are also ignored.

    Note that the ``--no-config`` option takes precedence over this option.

``--save-position-on-quit``
    Always save the current playback position on quit. When this file is
    played again later, the player will seek to the old playback position on
    start. This does not happen if playback of a file is stopped in any other
    way than quitting. For example, going to the next file in the playlist
    will not save the position, and start playback at beginning the next time
    the file is played.

    This behavior is disabled by default, but is always available when quitting
    the player with Shift+Q.

``--dump-stats=<filename>``
    Write certain statistics to the given file. The file is truncated on
    opening. The file will contain raw samples, each with a timestamp. To
    make this file into a readable, the script ``TOOLS/stats-conv.py`` can be
    used (which currently displays it as a graph).

    This option is useful for debugging only.

``--idle=<no|yes|once>``
    Makes mpv wait idly instead of quitting when there is no file to play.
    Mostly useful in slave mode, where mpv can be controlled through input
    commands (see also ``--slave-broken``).

    ``once`` will only idle at start and let the player close once the
    first playlist has finished playing back.

``--include=<configuration-file>``
    Specify configuration file to be parsed after the default ones.

``--load-scripts=<yes|no>``
    If set to ``no``, don't auto-load scripts from the ``scripts``
    configuration subdirectory (usually ``~/.config/mpv/scripts/``).
    (Default: ``yes``)

``--script=<filename>``
    Load a Lua script. You can load multiple scripts by separating them with
    commas (``,``).

``--script-opts=key1=value1,key2=value2,...``
    Set options for scripts. A script can query an option by key. If an
    option is used and what semantics the option value has depends entirely on
    the loaded scripts. Values not claimed by any scripts are ignored.

``--merge-files``
    Pretend that all files passed to mpv are concatenated into a single, big
    file. This uses timeline/EDL support internally. Note that this won't work
    for ordered chapter files.

``--no-resume-playback``
    Do not restore playback position from the ``watch_later`` configuration
    subdirectory (usually ``~/.config/mpv/watch_later/``).
    See ``quit_watch_later`` input command.

``--profile=<profile1,profile2,...>``
    Use the given profile(s), ``--profile=help`` displays a list of the
    defined profiles.

``--reset-on-next-file=<all|option1,option2,...>``
    Normally, mpv will try to keep all settings when playing the next file on
    the playlist, even if they were changed by the user during playback. (This
    behavior is the opposite of MPlayer's, which tries to reset all settings
    when starting next file.)

    Default: Do not reset anything.

    This can be changed with this option. It accepts a list of options, and
    mpv will reset the value of these options on playback start to the initial
    value. The initial value is either the default value, or as set by the
    config file or command line.

    In some cases, this might not work as expected. For example, ``--volume``
    will only be reset if it is explicitly set in the config file or the
    command line.

    The special name ``all`` resets as many options as possible.

    .. admonition:: Examples

        - ``--reset-on-next-file=pause``
          Reset pause mode when switching to the next file.
        - ``--reset-on-next-file=fullscreen,speed``
          Reset fullscreen and playback speed settings if they were changed
          during playback.
        - ``--reset-on-next-file=all``
          Try to reset all settings that were changed during playback.

``--write-filename-in-watch-later-config``
    Prepend the watch later config files with the name of the file they refer
    to. This is simply written as comment on the top of the file.

    .. warning::

        This option may expose privacy-sensitive information and is thus
        disabled by default.

``--ignore-path-in-watch-later-config``
    Ignore path (i.e. use filename only) when using watch later feature.

``--show-profile=<profile>``
    Show the description and content of a profile.

``--use-filedir-conf``
    Look for a file-specific configuration file in the same directory as the
    file that is being played. See `File-specific Configuration Files`_.

    .. warning::

        May be dangerous if playing from untrusted media.

``--ytdl``, ``--no-ytdl``
    Enable the youtube-dl hook-script. It will look at the input URL, and will
    play the video located on the website. This works with many streaming sites,
    not just the one that the script is named after. This requires a recent
    version of youtube-dl to be installed on the system. (Enabled by default,
    except when the client API / libmpv is used.)

    If the script can't do anything with an URL, it will do nothing.

    (Note: this is the replacement for the now removed libquvi support.)

``--ytdl-format=<best|worst|mp4|webm|...>``
    Video format/quality that is directly passed to youtube-dl. The possible
    values are specific to the website and the video, for a given url the
    available formats can be found with the command
    ``youtube-dl --list-formats URL``. See youtube-dl's documentation for
    available aliases. To use experimental DASH support for youtube, use
    ``bestvideo+bestaudio``.
    (Default: ``best``)

``--ytdl-raw-options=<key>=<value>[,<key>=<value>[,...]]``
    Pass arbitraty options to youtube-dl. Parameter and argument should be
    passed as a key-value pair. Options without argument must include ``=``.

    There is no sanity checking so it's possible to break things (i.e.
    passing invalid parameters to youtube-dl).

    .. admonition:: Example

        ``--ytdl-raw-options=username=user,password=pass``
        ``--ytdl-raw-options=force-ipv6=``

Video
-----

``--vo=<driver1[:suboption1[=value]:...],driver2,...[,]>``
    Specify a priority list of video output drivers to be used. For
    interactive use, one would normally specify a single one to use, but in
    configuration files, specifying a list of fallbacks may make sense. See
    `VIDEO OUTPUT DRIVERS`_ for details and descriptions of available drivers.

``--vd=<[+|-]family1:(*|decoder1),[+|-]family2:(*|decoder2),...[-]>``
    Specify a priority list of video decoders to be used, according to their
    family and name. See ``--ad`` for further details. Both of these options
    use the same syntax and semantics; the only difference is that they
    operate on different codec lists.

    .. note::

        See ``--vd=help`` for a full list of available decoders.

``--vf=<filter1[=parameter1:parameter2:...],filter2,...>``
    Specify a list of video filters to apply to the video stream. See
    `VIDEO FILTERS`_ for details and descriptions of the available filters.
    The option variants ``--vf-add``, ``--vf-pre``, ``--vf-del`` and
    ``--vf-clr`` exist to modify a previously specified list, but you
    should not need these for typical use.

``--no-video``
    Do not play video. With some demuxers this may not work. In those cases
    you can try ``--vo=null`` instead.

    mpv will try to download the audio only if media is streamed with
    youtube-dl, because it saves bandwidth. This is done by setting the ytdl_format
    to "bestaudio/best" in the ytdl_hook.lua script.

``--untimed``
    Do not sleep when outputting video frames. Useful for benchmarks when used
    with ``--no-audio.``

``--framedrop=<mode>``
    Skip displaying some frames to maintain A/V sync on slow systems, or
    playing high framerate video on video outputs that have an upper framerate
    limit.

    The argument selects the drop methods, and can be one of the following:

    <no>
        Disable any framedropping.
    <vo>
        Drop late frames on video output (default). This still decodes and
        filters all frames, but doesn't render them on the VO. It tries to query
        the display FPS (X11 only, not correct on multi-monitor systems), or
        assumes infinite display FPS if that fails. Drops are indicated in
        the terminal status line as ``D:`` field. If the decoder is too slow,
        in theory all frames would have to be dropped (because all frames are
        too late) - to avoid this, frame dropping stops if the effective
        framerate is below 10 FPS.
    <decoder>
        Old, decoder-based framedrop mode. (This is the same as ``--framedrop=yes``
        in mpv 0.5.x and before.) This tells the decoder to skip frames (unless
        they are needed to decode future frames). May help with slow systems,
        but can produce unwatchably choppy output, or even freeze the display
        completely. Not recommended.
        The ``--vd-lavc-framedrop`` option controls what frames to drop.
    <decoder+vo>
        Enable both modes. Not recommended.

    .. note::

        ``--vo=vdpau`` has its own code for the ``vo`` framedrop mode. Slight
        differences to other VOs are possible.

``--display-fps=<fps>``
    Set the maximum assumed display FPS used with ``--framedrop``. By default
    a detected value is used (X11 only, not correct on multi-monitor systems),
    or infinite display FPS if that fails. Infinite FPS means only frames too
    late are dropped. If a correct FPS is provided, frames that are predicted
    to be too late are dropped too.

``--hwdec=<api>``
    Specify the hardware video decoding API that should be used if possible.
    Whether hardware decoding is actually done depends on the video codec. If
    hardware decoding is not possible, mpv will fall back on software decoding.

    ``<api>`` can be one of the following:

    :no:        always use software decoding (default)
    :auto:      see below
    :vdpau:     requires ``--vo=vdpau`` or ``--vo=opengl`` (Linux only)
    :vaapi:     requires ``--vo=opengl`` or ``--vo=vaapi`` (Linux with Intel GPUs only)
    :vaapi-copy: copies video back into system RAM (Linux with Intel GPUs only)
    :vda:       requires ``--vo=opengl`` (OS X only)
    :dxva2-copy: copies video back to system RAM (Windows only)
    :rpi:      requires ``--vo=rpi`` (Raspberry Pi only - default if available)

    ``auto`` tries to automatically enable hardware decoding using the first
    available method. This still depends what VO you are using. For example,
    if you are not using ``--vo=vdpau`` or ``--vo=opengl``, vdpau decoding will
    never be enabled. Also note that if the first found method doesn't actually
    work, it will always fall back to software decoding, instead of trying the
    next method (might matter on some Linux systems).

    The ``vaapi-copy`` mode allows you to use vaapi with any VO. Because
    this copies the decoded video back to system RAM, it's likely less efficient
    than the ``vaapi`` mode.

    .. note::

        When using this switch, hardware decoding is still only done for some
        codecs. See ``--hwdec-codecs`` to enable hardware decoding for more
        codecs.

``--panscan=<0.0-1.0>``
    Enables pan-and-scan functionality (cropping the sides of e.g. a 16:9
    video to make it fit a 4:3 display without black bands). The range
    controls how much of the image is cropped. May not work with all video
    output drivers.

``--video-aspect=<ratio>``
    Override video aspect ratio, in case aspect information is incorrect or
    missing in the file being played. See also ``--no-video-aspect``.

    Two values have special meaning:

    :0:  disable aspect ratio handling, pretend the video has square pixels
    :-1: use the video stream or container aspect (default)

    But note that handling of these special values might change in the future.

    .. admonition:: Examples

        - ``--video-aspect=4:3``  or ``--video-aspect=1.3333``
        - ``--video-aspect=16:9`` or ``--video-aspect=1.7777``

``--no-video-aspect``
    Ignore aspect ratio information from video file and assume the video has
    square pixels. See also ``--video-aspect``.

``--video-unscaled``
    Disable scaling of the video. If the window is larger than the video,
    black bars are added. Otherwise, the video is cropped. The video still
    can be influenced by the other ``--video-...`` options. (But not all; for
    example ``--video-zoom`` does nothing if this option is enabled.)

    The video and monitor aspects aspect will be ignored. Aspect correction
    would require to scale the video in the X or Y direction, but this option
    disables scaling, disabling all aspect correction.

    Note that the scaler algorithm may still be used, even if the video isn't
    scaled. For example, this can influence chroma conversion.

    This option is disabled if the ``--no-keepaspect`` option is used.

``--video-pan-x=<value>``, ``--video-pan-y=<value>``
    Moves the displayed video rectangle by the given value in the X or Y
    direction. The unit is in fractions of the size of the scaled video (the
    full size, even if parts of the video are not visible due to panscan or
    other options).

    For example, displaying a 1280x720 video fullscreen on a 1680x1050 screen
    with ``--video-pan-x=-0.1`` would move the video 168 pixels to the left
    (making 128 pixels of the source video invisible).

    This option is disabled if the ``--no-keepaspect`` option is used.

``--video-rotate=<0-360|no>``
    Rotate the video clockwise, in degrees. Currently supports 90° steps only.
    If ``no`` is given, the video is never rotated, even if the file has
    rotation metadata. (The rotation value is added to the rotation metadata,
    which means the value ``0`` would rotate the video according to the
    rotation metadata.)

``--video-stereo-mode=<mode>``
    Set the stereo 3D output mode (default: ``mono``). This is done by inserting
    the ``stereo3d`` conversion filter.

    The mode ``mono`` is an alias to ``ml``, which refers to the left frame in
    2D. This is the default, which means mpv will try to show 3D movies in 2D,
    instead of the mangled 3D image not intended for consumption (such as
    showing the left and right frame side by side, etc.).

    The pseudo-mode ``none`` disables automatic conversion completely.

    Use ``--video-stereo-mode=help`` to list all available modes. Check with
    the ``stereo3d`` filter documentation to see what the names mean. Note that
    some names refer to modes not supported by ``stereo3d`` - these modes can
    appear in files, but can't be handled properly by mpv.

``--video-zoom=<value>``
    Adjust the video display scale factor by the given value. The unit is in
    fractions of the (scaled) window video size.

    For example, given a 1280x720 video shown in a 1280x720 window,
    ``--video-zoom=-0.1`` would make the video by 128 pixels smaller in
    X direction, and 72 pixels in Y direction.

    This option is disabled if the ``--no-keepaspect`` option is used.

``--video-align-x=<-1-1>``, ``--video-align-y=<-1-1>``
    Moves the video rectangle within the black borders, which are usually added
    to pad the video to screen if video and screen aspect ratios are different.
    ``--video-align-y=-1`` would move the video to the top of the screen
    (leaving a border only on the bottom), a value of ``0`` centers it
    (default), and a value of ``1`` would put the video at the bottom of the
    screen.

    If video and screen aspect match perfectly, these options do nothing.

    This option is disabled if the ``--no-keepaspect`` option is used.

``--correct-pts``, ``--no-correct-pts``
    ``--no-correct-pts`` switches mpv to a mode where video timing is
    determined using a fixed framerate value (either using the ``--fps``
    option, or using file information). Sometimes, files with very broken
    timestamps can be played somewhat well in this mode. Note that video
    filters, subtitle rendering and audio synchronization can be completely
    broken in this mode.

``--fps=<float>``
    Override video framerate. Useful if the original value is wrong or missing.

    .. note::

        Works in ``--no-correct-pts`` mode only.

``--deinterlace=<yes|no|auto>``
    Enable or disable interlacing (default: auto, which usually means no).
    Interlaced video shows ugly comb-like artifacts, which are visible on
    fast movement. Enabling this typically inserts the yadif video filter in
    order to deinterlace the video, or lets the video output apply deinterlacing
    if supported.

    This behaves exactly like the ``deinterlace`` input property (usually
    mapped to ``Shift+D``).

    ``auto`` is a technicality. Strictly speaking, the default for this option
    is deinterlacing disabled, but the ``auto`` case is needed if ``yadif`` was
    added to the filter chain manually with ``--vf``. Then the core shouldn't
    disable deinterlacing just because the ``--deinterlace`` was not set.

``--field-dominance=<auto|top|bottom>``
    Set first field for interlaced content. Useful for deinterlacers that
    double the framerate: ``--vf=yadif=field`` and ``--vo=vdpau:deint``.

    :auto:    (default) If the decoder does not export the appropriate
              information, it falls back on ``top`` (top field first).
    :top:     top field first
    :bottom:  bottom field first

``--frames=<number>``
    Play/convert only first ``<number>`` video frames, then quit.

    ``--frames=0`` loads the file, but immediately quits before initializing
    playback. (Might be useful for scripts which just want to determine some
    file properties.)

    For audio-only playback, any value greater than 0 will quit playback
    immediately after initialization. The value 0 works as with video.

``--hwdec-codecs=<codec1,codec2,...|all>``
    Allow hardware decoding for a given list of codecs only. The special value
    ``all`` always allows all codecs.

    You can get the list of allowed codecs with ``mpv --vd=help``. Remove the
    prefix, e.g. instead of ``lavc:h264`` use ``h264``.

    By default this is set to ``h264,vc1,wmv3``. Note that the hardware
    acceleration special codecs like ``h264_vdpau`` are not relevant anymore,
    and in fact have been removed from Libav in this form.

    This is usually only needed with broken GPUs, where a codec is reported
    as supported, but decoding causes more problems than it solves.

    .. admonition:: Example

        ``mpv --hwdec=vdpau --vo=vdpau --hwdec-codecs=h264,mpeg2video``
            Enable vdpau decoding for h264 and mpeg2 only.

``--vd-lavc-check-hw-profile=<yes|no>``
    Check hardware decoder profile (default: yes). If ``no`` is set, the
    highest profile of the hardware decoder is unconditionally selected, and
    decoding is forced even if the profile of the video is higher than that.
    The result is most likely broken decoding, but may also help if the
    detected or reported profiles are somehow incorrect.

``--vd-lavc-bitexact``
    Only use bit-exact algorithms in all decoding steps (for codec testing).

``--vd-lavc-fast`` (MPEG-2, MPEG-4, and H.264 only)
    Enable optimizations which do not comply with the format specification and
    potentially cause problems, like simpler dequantization, simpler motion
    compensation, assuming use of the default quantization matrix, assuming YUV
    4:2:0 and skipping a few checks to detect damaged bitstreams.

``--vd-lavc-o=<key>=<value>[,<key>=<value>[,...]]``
    Pass AVOptions to libavcodec decoder. Note, a patch to make the ``o=``
    unneeded and pass all unknown options through the AVOption system is
    welcome. A full list of AVOptions can be found in the FFmpeg manual.

    Some options which used to be direct options can be set with this
    mechanism, like ``bug``, ``gray``, ``idct``, ``ec``, ``vismv``,
    ``skip_top`` (was ``st``), ``skip_bottom`` (was ``sb``), ``debug``.

    .. admonition:: Example

        ``--vd--lavc-o=debug=pict``

``--vd-lavc-show-all=<yes|no>``
    Show even broken/corrupt frames (default: no). If this option is set to
    no, libavcodec won't output frames that were either decoded before an
    initial keyframe was decoded, or frames that are recognized as corrupted.

``--vd-lavc-skiploopfilter=<skipvalue> (H.264 only)``
    Skips the loop filter (AKA deblocking) during H.264 decoding. Since
    the filtered frame is supposed to be used as reference for decoding
    dependent frames, this has a worse effect on quality than not doing
    deblocking on e.g. MPEG-2 video. But at least for high bitrate HDTV,
    this provides a big speedup with little visible quality loss.

    ``<skipvalue>`` can be one of the following:

    :none:    Never skip.
    :default: Skip useless processing steps (e.g. 0 size packets in AVI).
    :nonref:  Skip frames that are not referenced (i.e. not used for
              decoding other frames, the error cannot "build up").
    :bidir:   Skip B-Frames.
    :nonkey:  Skip all frames except keyframes.
    :all:     Skip all frames.

``--vd-lavc-skipidct=<skipvalue> (MPEG-1/2 only)``
    Skips the IDCT step. This degrades quality a lot in almost all cases
    (see skiploopfilter for available skip values).

``--vd-lavc-skipframe=<skipvalue>``
    Skips decoding of frames completely. Big speedup, but jerky motion and
    sometimes bad artifacts (see skiploopfilter for available skip values).

``--vd-lavc-framedrop=<skipvalue>``
    Set framedropping mode used with ``--framedrop`` (see skiploopfilter for
    available skip values).

``--vd-lavc-threads=<0-16>``
    Number of threads to use for decoding. Whether threading is actually
    supported depends on codec. 0 means autodetect number of cores on the
    machine and use that, up to the maximum of 16 (default: 0).



Audio
-----

``--audio-pitch-correction=<yes|no>``
    If this is enabled (default), playing with a speed different from normal
    automatically inserts the ``scaletempo`` audio filter. For details, see
    audio filter section.

``--audio-device=<name>``
    Use the given audio device. This consists of the audio output name, e.g.
    ``alsa``, followed by ``/``, followed by the audio output specific device
    name.

    You can list audio devices with ``--audio-device=help``. This outputs the
    device name in quotes, followed by a description. The device name is what
    you have to pass to the ``--audio-device`` option.

    The default value for this option is ``auto``, which tries every audio
    output in preference order with the default device.

    Note that many AOs have a ``device`` sub-option, which overrides the
    device selection of this option (but not the audio output selection).
    Likewise, forcing an AO with ``--ao`` will override the audio output
    selection of ``--audio-device`` (but not the device selecton).

    Currently not implemented for most AOs.

``--ao=<driver1[:suboption1[=value]:...],driver2,...[,]>``
    Specify a priority list of audio output drivers to be used. For
    interactive use one would normally specify a single one to use, but in
    configuration files specifying a list of fallbacks may make sense. See
    `AUDIO OUTPUT DRIVERS`_ for details and descriptions of available drivers.

``--af=<filter1[=parameter1:parameter2:...],filter2,...>``
    Specify a list of audio filters to apply to the audio stream. See
    `AUDIO FILTERS`_ for details and descriptions of the available filters.
    The option variants ``--af-add``, ``--af-pre``, ``--af-del`` and
    ``--af-clr`` exist to modify a previously specified list, but you
    should not need these for typical use.

``--ad=<[+|-]family1:(*|decoder1),[+|-]family2:(*|decoder2),...[-]>``
    Specify a priority list of audio decoders to be used, according to their
    family and decoder name. Entries like ``family:*`` prioritize all decoders
    of the given family. When determining which decoder to use, the first
    decoder that matches the audio format is selected. If that is unavailable,
    the next decoder is used. Finally, it tries all other decoders that are not
    explicitly selected or rejected by the option.

    ``-`` at the end of the list suppresses fallback on other available
    decoders not on the ``--ad`` list. ``+`` in front of an entry forces the
    decoder. Both of these should not normally be used, because they break
    normal decoder auto-selection!

    ``-`` in front of an entry disables selection of the decoder.

    .. admonition:: Examples

        ``--ad=lavc:mp3float``
            Prefer the FFmpeg/Libav ``mp3float`` decoder over all other MP3
            decoders.

        ``--ad=spdif:ac3,lavc:*``
            Always prefer spdif AC3 over FFmpeg/Libav over anything else.

        ``--ad=help``
            List all available decoders.

``--volume=<-1-100>``
    Set the startup volume. A value of -1 (the default) will not change the
    volume. See also ``--softvol``.

``--audio-delay=<sec>``
    Audio delay in seconds (positive or negative float value). Positive values
    delay the audio, and negative values delay the video.

``--no-audio``
    Do not play sound.

``--mute=<auto|yes|no>``
    Set startup audio mute status. ``auto`` (default) will not change the mute
    status. Also see ``--volume``.

``--softvol=<mode>``
    Control whether to use the volume controls of the audio output driver or
    the internal mpv volume filter.

    :no:    prefer audio driver controls, use the volume filter only if
            absolutely needed
    :yes:   always use the volume filter
    :auto:  prefer the volume filter if the audio driver uses the system mixer
            (default)

    The intention of ``auto`` is to avoid changing system mixer settings from
    within mpv with default settings. mpv is a video player, not a mixer panel.
    On the other hand, mixer controls are enabled for sound servers like
    PulseAudio, which provide per-application volume.

``--audio-demuxer=<[+]name>``
    Use this audio demuxer type when using ``--audio-file``. Use a '+' before
    the name to force it; this will skip some checks. Give the demuxer name as
    printed by ``--audio-demuxer=help``.

``--ad-lavc-ac3drc=<level>``
    Select the Dynamic Range Compression level for AC-3 audio streams.
    ``<level>`` is a float value ranging from 0 to 1, where 0 means no
    compression (which is the default) and 1 means full compression (make loud
    passages more silent and vice versa). Values up to 6 are also accepted, but
    are purely experimental. This option only shows an effect if the AC-3 stream
    contains the required range compression information.

    The standard mandates that DRC is enabled by default, but mpv (and some
    other players) ignore this for the sake of better audio quality.

``--ad-lavc-downmix=<yes|no>``
    Whether to request audio channel downmixing from the decoder (default: yes).
    Some decoders, like AC-3, AAC and DTS, can remix audio on decoding. The
    requested number of output channels is set with the ``--audio-channels`` option.
    Useful for playing surround audio on a stereo system.

``--ad-lavc-threads=<0-16>``
    Number of threads to use for decoding. Whether threading is actually
    supported depends on codec. As of this writing, it's supported for some
    lossless codecs only. 0 means autodetect number of cores on the
    machine and use that, up to the maximum of 16 (default: 1).

``--ad-lavc-o=<key>=<value>[,<key>=<value>[,...]]``
    Pass AVOptions to libavcodec decoder. Note, a patch to make the o=
    unneeded and pass all unknown options through the AVOption system is
    welcome. A full list of AVOptions can be found in the FFmpeg manual.

``--ad-spdif-dtshd=<yes|no>``, ``--dtshd``, ``--no-dtshd``
    When using DTS pass-through, output any DTS-HD track as-is.
    With ``ad-spdif-dtshd=no`` (the default), only the DTS Core parts will be
    output.

    DTS-HD tracks can be sent over HDMI but not over the original
    coax/TOSLINK S/PDIF system.

    Some receivers don't accept DTS core-only when ``--ad-spdif-dtshd=yes`` is
    used, even though they accept DTS-HD.

    ``--dtshd`` and ``--no-dtshd`` are deprecated aliases.

``--audio-channels=<number|layout>``
    Request a channel layout for audio output (default: auto). This  will ask
    the AO to open a device with the given channel layout. It's up to the AO
    to accept this layout, or to pick a fallback or to error out if the
    requested layout is not supported.

    The ``--audio-channels`` option either takes a channel number or an explicit
    channel layout. Channel numbers refer to default layouts, e.g. 2 channels
    refer to stereo, 6 refers to 5.1.

    See ``--audio-channels=help`` output for defined default layouts. This also
    lists speaker names, which can be used to express arbitrary channel
    layouts (e.g. ``fl-fr-lfe`` is 2.1).

    The default is ``--audio-channels=auto``, which tries to play audio using
    the input file's channel layout. (Or more precisely, the output of the
    audio filter chain.) (``empty`` is an accepted obsolete alias for ``auto``.)

    This will also request the channel layout from the decoder. If the decoder
    does not support the layout, it will fall back to its native channel layout.
    (You can use ``--ad-lavc-downmix=no`` to make the decoder always output
    its native layout.) Note that only some decoders support remixing audio.
    Some that do include AC-3, AAC or DTS audio.

    If the channel layout of the media file (i.e. the decoder) and the AO's
    channel layout don't match, mpv will attempt to insert a conversion filter.

``--audio-display=<no|attachment>``
    Setting this option to ``attachment`` (default) will display image
    attachments (e.g. album cover art) when playing audio files. It will
    display the first image found, and additional images are available as
    video tracks.

    Setting this option to ``no`` disables display of video entirely when
    playing audio files.

    This option has no influence on files with normal video tracks.

``--audio-file=<filename>``
    Play audio from an external file while viewing a video. Each use of this
    option will add a new audio track. The details are similar to how
    ``--sub-file`` works.

``--audio-format=<format>``
    Select the sample format used for output from the audio filter layer to
    the sound card. The values that ``<format>`` can adopt are listed below in
    the description of the ``format`` audio filter.

``--audio-samplerate=<Hz>``
    Select the output sample rate to be used (of course sound cards have
    limits on this). If the sample frequency selected is different from that
    of the current media, the lavrresample audio filter will be inserted into
    the audio filter layer to compensate for the difference.

``--gapless-audio=<no|yes|weak>``
    Try to play consecutive audio files with no silence or disruption at the
    point of file change. Default: ``weak``.

    :no:    Disable gapless audio.
    :yes:   The audio device is opened using parameters chosen according to the
            first file played and is then kept open for gapless playback. This
            means that if the first file for example has a low sample rate, then
            the following files may get resampled to the same low sample rate,
            resulting in reduced sound quality. If you play files with different
            parameters, consider using options such as ``--audio-samplerate``
            and ``--audio-format`` to explicitly select what the shared output
            format will be.
    :weak:  Normally, the audio device is kept open (using the format it was
            first initialized with). If the audio format the decoder output
            changes, the audio device is closed and reopened. This means that
            you will normally get gapless audio with files that were encoded
            using the same settings, but might not be gapless in other cases.
            (Unlike with ``yes``, you don't have to worry about corner cases
            like the first file setting a very low quality output format, and
            ruining the playback of higher quality files that follow.)

    .. note::

        This feature is implemented in a simple manner and relies on audio
        output device buffering to continue playback while moving from one file
        to another. If playback of the new file starts slowly, for example
        because it is played from a remote network location or because you have
        specified cache settings that require time for the initial cache fill,
        then the buffered audio may run out before playback of the new file
        can start.

``--initial-audio-sync``, ``--no-initial-audio-sync``
    When starting a video file or after events such as seeking, mpv will by
    default modify the audio stream to make it start from the same timestamp
    as video, by either inserting silence at the start or cutting away the
    first samples. Disabling this option makes the player behave like older
    mpv versions did: video and audio are both started immediately even if
    their start timestamps differ, and then video timing is gradually adjusted
    if necessary to reach correct synchronization later.

``--softvol-max=<10.0-10000.0>``
    Set the maximum amplification level in percent (default: 200). A value of
    200 will allow you to adjust the volume up to a maximum of double the
    current level. With values below 100 the initial volume (which is 100%)
    will be above the maximum, which e.g. the OSD cannot display correctly.

    .. admonition:: Note

        The maximum value of ``--volume`` as well as the ``volume`` property
        is always 100. Likewise, the volume OSD bar always goes from 0 to 100.
        This means that with ``--softvol-max=200``, ``--volume=100`` sets
        maximum amplification, i.e. amplify by 200%. The default volume (no
        change in volume) will be ``50`` in this case.

``--audio-file-auto=<no|exact|fuzzy|all>``, ``--no-audio-file-auto``
    Load additional audio files matching the video filename. The parameter
    specifies how external audio files are matched. ``exact`` is enabled by
    default.

    :no:    Don't automatically load external audio files.
    :exact: Load the media filename with audio file extension (default).
    :fuzzy: Load all audio files containing media filename.
    :all:   Load all audio files in the current directory.

``--audio-client-name=<name>``
    The application name the player reports to the audio API. Can be useful
    if you want to force a different audio profile (e.g. with PulseAudio),
    or to set your own application name when using libmpv.

``--volume-restore-data=<string>``
    Used internally for use by playback resume (e.g. with ``quit_watch_later``).
    Restoring value has to be done carefully, because different AOs as well as
    softvol can have different value ranges, and we don't want to restore
    volume if setting the volume changes it system wide. The normal options
    (like ``--volume``) would always set the volume. This option was added for
    restoring volume in a safer way (by storing the method used to set the
    volume), and is not generally useful. Its semantics are considered private
    to mpv.

    Do not use.

``--audio-buffer=<seconds>``
    Set the audio output minimum buffer. The audio device might actually create
    a larger buffer if it pleases. If the device creates a smaller buffer,
    additional audio is buffered in an additional software buffer.

    Making this larger will make soft-volume and other filters react slower,
    introduce additional issues on playback speed change, and block the
    player on audio format changes. A smaller buffer might lead to audio
    dropouts.

    This option should be used for testing only. If a non-default value helps
    significantly, the mpv developers should be contacted.

    Default: 0.2 (200 ms).

Subtitles
---------

``--no-sub``
    Do not select any subtitle when the file is loaded.

``--sub-demuxer=<[+]name>``
    Force subtitle demuxer type for ``--sub-file``. Give the demuxer name as
    printed by ``--sub-demuxer=help``.

``--sub-delay=<sec>``
    Delays subtitles by ``<sec>`` seconds. Can be negative.

``--sub-file=subtitlefile``
    Add a subtitle file to the list of external subtitles.

    If you use ``--sub-file`` only once, this subtitle file is displayed by
    default.

    If ``--sub-file`` is used multiple times, the subtitle to use can be
    switched at runtime by cycling subtitle tracks. It's possible to show
    two subtitles at once: use ``--sid`` to select the first subtitle index,
    and ``--secondary-sid`` to select the second index. (The index is printed
    on the terminal output after the ``--sid=`` in the list of streams.)

``--secondary-sid=<ID|auto|no>``
    Select a secondary subtitle stream. This is similar to ``--sid``. If a
    secondary subtitle is selected, it will be rendered as toptitle (i.e. on
    the top of the screen) alongside the normal subtitle, and provides a way
    to render two subtitles at once.

    there are some caveats associated with this feature. For example, bitmap
    subtitles will always be rendered in their usual position, so selecting a
    bitmap subtitle as secondary subtitle will result in overlapping subtitles.
    Secondary subtitles are never shown on the terminal if video is disabled.

    .. note::

        Styling and interpretation of any formatting tags is disabled for the
        secondary subtitle. Internally, the same mechanism as ``--no-sub-ass``
        is used to strip the styling.

    .. note::

        If the main subtitle stream contains formatting tags which display the
        subtitle at the top of the screen, it will overlap with the secondary
        subtitle. To prevent this, you could use ``--no-sub-ass`` to disable
        styling in the main subtitle stream.

``--sub-scale=<0-100>``
    Factor for the text subtitle font size (default: 1).

    .. note::

        This affects ASS subtitles as well, and may lead to incorrect subtitle
        rendering. Use with care, or use ``--sub-text-font-size`` instead.

``--sub-scale-by-window=<yes|no>``
    Whether to scale subtitles with the window size (default: yes). If this is
    disabled, changing the window size won't change the subtitle font size.

    Like ``--sub-scale``, this can break ASS subtitles.

``--sub-scale-with-window=<yes|no>``
    Make the subtitle font size relative to the window, instead of the video.
    This is useful if you always want the same font size, even if the video
    doesn't covert the window fully, e.g. because screen aspect and window
    aspect mismatch (and the player adds black bars).

    Default: yes.

    This option is misnamed. The difference to the confusingly similar sounding
    option ``--sub-scale-by-window`` is that ``--sub-scale-with-window`` still
    scales with the approximate window size, while the other option disables
    this scaling.

    Affects plain text subtitles only (or ASS if ``--ass-style-override`` is
    set high enough).

``--ass-scale-with-window=<yes|no>``
    Like ``--sub-scale-with-window``, but affects subtitles in ASS format only.
    Like ``--sub-scale``, this can break ASS subtitles.

    Default: no.

``--embeddedfonts``, ``--no-embeddedfonts``
    Use fonts embedded in Matroska container files and ASS scripts (default:
    enabled). These fonts can be used for SSA/ASS subtitle rendering.

``--sub-pos=<0-100>``
    Specify the position of subtitles on the screen. The value is the vertical
    position of the subtitle in % of the screen height.

    .. note::

        This affects ASS subtitles as well, and may lead to incorrect subtitle
        rendering. Use with care, or use ``--sub-text-margin-y`` instead.

``--sub-speed=<0.1-10.0>``
    Multiply the subtitle event timestamps with the given value. Can be used
    to fix the playback speed for frame-based subtitle formats. Works for
    external text subtitles only.

    .. admonition:: Example

        `--sub-speed=25/23.976`` plays frame based subtitles which have been
        loaded assuming a framerate of 23.976 at 25 FPS.

``--ass-force-style=<[Style.]Param=Value[,...]>``
    Override some style or script info parameters.

    .. admonition:: Examples

        - ``--ass-force-style=FontName=Arial,Default.Bold=1``
        - ``--ass-force-style=PlayResY=768``

    .. note::

        Using this option may lead to incorrect subtitle rendering.

``--ass-hinting=<none|light|normal|native>``
    Set font hinting type. <type> can be:

    :none:       no hinting (default)
    :light:      FreeType autohinter, light mode
    :normal:     FreeType autohinter, normal mode
    :native:     font native hinter

    .. admonition:: Warning

        Enabling hinting can lead to mispositioned text (in situations it's
        supposed to match up with video background), or reduce the smoothness
        of animations with some badly authored ASS scripts. It is recommended
        to not use this option, unless really needed.

``--ass-line-spacing=<value>``
    Set line spacing value for SSA/ASS renderer.

``--ass-shaper=<simple|complex>``
    Set the text layout engine used by libass.

    :simple:   uses Fribidi only, fast, doesn't render some languages correctly
    :complex:  uses HarfBuzz, slower, wider language support

    ``complex`` is the default. If libass hasn't been compiled against HarfBuzz,
    libass silently reverts to ``simple``.

``--ass-styles=<filename>``
    Load all SSA/ASS styles found in the specified file and use them for
    rendering text subtitles. The syntax of the file is exactly like the ``[V4
    Styles]`` / ``[V4+ Styles]`` section of SSA/ASS.

    .. note::

        Using this option may lead to incorrect subtitle rendering.

``--ass-style-override=<yes|no|force>``
    Control whether user style overrides should be applied.

    :yes:   Apply all the ``--ass-*`` style override options. Changing the default
            for any of these options can lead to incorrect subtitle rendering
            (default).
    :signfs: like ``yes``, but apply ``--sub-scale`` only to signs
    :no:    Render subtitles as forced by subtitle scripts.
    :force: Try to force the font style as defined by the ``--sub-text-*``
            options. Can break rendering easily.

``--ass-force-margins``
    Enables placing toptitles and subtitles in black borders when they are
    available, if the subtitles are in the ASS format.

    Default: no.

``--sub-use-margins``
    Enables placing toptitles and subtitles in black borders when they are
    available, if the subtitles are in a plain text format  (or ASS if
    ``--ass-style-override`` is set high enough).

    Default: yes.

    Renamed from ``--ass-use-margins``. To place ASS subtitles in the borders
    too (like the old option did), also add ``--ass-force-margins``.

``--ass-vsfilter-aspect-compat=<yes|no>``
    Stretch SSA/ASS subtitles when playing anamorphic videos for compatibility
    with traditional VSFilter behavior. This switch has no effect when the
    video is stored with square pixels.

    The renderer historically most commonly used for the SSA/ASS subtitle
    formats, VSFilter, had questionable behavior that resulted in subtitles
    being stretched too if the video was stored in anamorphic format that
    required scaling for display.  This behavior is usually undesirable and
    newer VSFilter versions may behave differently. However, many existing
    scripts compensate for the stretching by modifying things in the opposite
    direction.  Thus, if such scripts are displayed "correctly", they will not
    appear as intended.  This switch enables emulation of the old VSFilter
    behavior (undesirable but expected by many existing scripts).

    Enabled by default.

``--ass-vsfilter-blur-compat=<yes|no>``
    Scale ``\blur`` tags by video resolution instead of script resolution
    (enabled by default). This is bug in VSFilter, which according to some,
    can't be fixed anymore in the name of compatibility.

    Note that this uses the actual video resolution for calculating the
    offset scale factor, not what the video filter chain or the video output
    use.

``--ass-vsfilter-color-compat=<basic|full|force-601|no>``
    Mangle colors like (xy-)vsfilter do (default: basic). Historically, VSFilter
    was not color space aware. This was no problem as long as the color space
    used for SD video (BT.601) was used. But when everything switched to HD
    (BT.709), VSFilter was still converting RGB colors to BT.601, rendered
    them into the video frame, and handled the frame to the video output, which
    would use BT.709 for conversion to RGB. The result were mangled subtitle
    colors. Later on, bad hacks were added on top of the ASS format to control
    how colors are to be mangled.

    :basic: Handle only BT.601->BT.709 mangling, if the subtitles seem to
            indicate that this is required (default).
    :full:  Handle the full ``YCbCr Matrix`` header with all video color spaces
            supported by libass and mpv. This might lead to bad breakages in
            corner cases and is not strictly needed for compatibility
            (hopefully), which is why this is not default.
    :force-601: Force BT.601->BT.709 mangling, regardless of subtitle headers
            or video color space.
    :no:    Disable color mangling completely. All colors are RGB.

    Choosing anything other than ``no`` will make the subtitle color depend on
    the video color space, and it's for example in theory not possible to reuse
    a subtitle script with another video file. The ``--ass-style-override``
    option doesn't affect how this option is interpreted.

``--stretch-dvd-subs=<yes|no>``
    Stretch DVD subtitles when playing anamorphic videos for better looking
    fonts on badly mastered DVDs. This switch has no effect when the
    video is stored with square pixels - which for DVD input cannot be the case
    though.

    Many studios tend to use bitmap fonts designed for square pixels when
    authoring DVDs, causing the fonts to look stretched on playback on DVD
    players. This option fixes them, however at the price of possibly
    misaligning some subtitles (e.g. sign translations).

    Disabled by default.

``--sub-ass``, ``--no-sub-ass``
    Render ASS subtitles natively (enabled by default).

    If ``--no-sub-ass`` is specified, all tags and style declarations are
    stripped and ignored on display. The subtitle renderer uses the font style
    as specified by the ``--sub-text-`` options instead.

    .. note::

        Using ``--no-sub-ass`` may lead to incorrect or completely broken
        rendering of ASS/SSA subtitles. It can sometimes be useful to forcibly
        override the styling of ASS subtitles, but should be avoided in general.

    .. note::

        Try using ``--ass-style-override=force`` instead.

``--sub-auto=<no|exact|fuzzy|all>``, ``--no-sub-auto``
    Load additional subtitle files matching the video filename. The parameter
    specifies how external subtitle files are matched. ``exact`` is enabled by
    default.

    :no:    Don't automatically load external subtitle files.
    :exact: Load the media filename with subtitle file extension (default).
    :fuzzy: Load all subs containing media filename.
    :all:   Load all subs in the current and ``--sub-paths`` directories.

``--sub-codepage=<codepage>``
    If your system supports ``iconv(3)``, you can use this option to specify
    the subtitle codepage. By default, ENCA will be used to guess the charset.
    If mpv is not compiled with ENCA, ``UTF-8:UTF-8-BROKEN`` is the default,
    which means it will try to use UTF-8, otherwise the ``UTF-8-BROKEN``
    pseudo codepage (see below).

    The default value for this option is ``auto``, whose actual effect depends
    on whether ENCA is compiled.

    .. admonition:: Warning

        If you force the charset, even subtitles that are known to be
        UTF-8 will be recoded, which is perhaps not what you expect. Prefix
        codepages with ``utf8:`` if you want the codepage to be used only if the
        input is not valid UTF-8.

    .. admonition:: Examples

        - ``--sub-codepage=utf8:latin2`` Use Latin 2 if input is not UTF-8.
        - ``--sub-codepage=cp1250`` Always force recoding to cp1250.

    The pseudo codepage ``UTF-8-BROKEN`` is used internally. When it
    is the codepage, subtitles are interpreted as UTF-8 with "Latin 1" as
    fallback for bytes which are not valid UTF-8 sequences. iconv is
    never involved in this mode.

    If the player was compiled with ENCA support, you can control it with the
    following syntax:

    ``--sub-codepage=enca:<language>:<fallback codepage>``

    Language is specified using a two letter code to help ENCA detect
    the codepage automatically. If an invalid language code is
    entered, mpv will complain and list valid languages.  (Note
    however that this list will only be printed when the conversion code is actually
    called, for example when loading an external subtitle). The
    fallback codepage is used if autodetection fails.  If no fallback
    is specified, ``UTF-8-BROKEN`` is used.

    .. admonition:: Examples

        - ``--sub-codepage=enca:pl:cp1250`` guess the encoding, assuming the subtitles
          are Polish, fall back on cp1250
        - ``--sub-codepage=enca:pl`` guess the encoding for Polish, fall back on UTF-8.
        - ``--sub-codepage=enca`` try universal detection, fall back on UTF-8.

    If the player was compiled with libguess support, you can use it with:

    ``--sub-codepage=guess:<language>:<fallback codepage>``

    libguess always needs a language. There is no universal detection
    mode. Use ``--sub-codepage=guess:help`` to get a list of
    languages subject to the same caveat as with ENCA above.

``--sub-fix-timing``, ``--no-sub-fix-timing``
    By default, external text subtitles are preprocessed to remove minor gaps
    or overlaps between subtitles (if the difference is smaller than 200 ms,
    the gap or overlap is removed). This does not affect image subtitles,
    subtitles muxed with audio/video, or subtitles in the ASS format.

``--sub-forced-only``
    Display only forced subtitles for the DVD subtitle stream selected by e.g.
    ``--slang``.

``--sub-fps=<rate>``
    Specify the framerate of the subtitle file (default: video fps).

    .. note::

        ``<rate>`` > video fps speeds the subtitles up for frame-based
        subtitle files and slows them down for time-based ones.

    Also see ``--sub-speed`` option.

``--sub-gauss=<0.0-3.0>``
    Apply Gaussian blur to image subtitles (default: 0). This can help making
    pixelated DVD/Vobsubs look nicer. A value other than 0 also switches to
    software subtitle scaling. Might be slow.

    .. note::

        Never applied to text subtitles.

``--sub-gray``
    Convert image subtitles to grayscale. Can help making yellow DVD/Vobsubs
    look nicer.

    .. note::

        Never applied to text subtitles.

``--sub-paths=<path1:path2:...>``
    Specify extra directories to search for subtitles matching the video.
    Multiple directories can be separated by ":" (";" on Windows).
    Paths can be relative or absolute. Relative paths are interpreted relative
    to video file directory.

    .. admonition:: Example

        Assuming that ``/path/to/video/video.avi`` is played and
        ``--sub-paths=sub:subtitles:/tmp/subs`` is specified, mpv searches for
        subtitle files in these directories:

        - ``/path/to/video/``
        - ``/path/to/video/sub/``
        - ``/path/to/video/subtitles/``
        - ``/tmp/subs/``
        -  the ``sub`` configuration subdirectory (usually ``~/.config/mpv/sub/``)

``--sub-visibility``, ``--no-sub-visibility``
    Can be used to disable display of subtitles, but still select and decode
    them.

``--sub-clear-on-seek``
    (Obscure, rarely useful.) Can be used to play broken mkv files with
    duplicate ReadOrder fields. ReadOrder is the first field in a
    Matroska-style ASS subtitle packets. It should be unique, and libass
    uses it for fast elimination of duplicates. This option disables caching
    of subtitles across seeks, so after a seek libass can't eliminate subtitle
    packets with the same ReadOrder as earlier packets.

Window
------

``--title=<string>``
    Set the window title. This is used for the video window, and if possible,
    also sets the audio stream title.

    Properties are expanded. (See `Property Expansion`_.)

    .. warning::

        There is a danger of this causing significant CPU usage, depending on
        the properties used. Changing the window title is often a slow
        operation, and if the title changes every frame, playback can be ruined.

``--screen=<default|0-32>``
    In multi-monitor configurations (i.e. a single desktop that spans across
    multiple displays), this option tells mpv which screen to display the
    video on.

    .. admonition:: Note (X11)

        This option does not work properly with all window managers. In these
        cases, you can try to use ``--geometry`` to position the window
        explicitly. It's also possible that the window manager provides native
        features to control which screens application windows should use.

    See also ``--fs-screen``.

``--fullscreen``, ``--fs``
    Fullscreen playback.

``--fs-screen=<all|current|0-32>``
    In multi-monitor configurations (i.e. a single desktop that spans across
    multiple displays), this option tells mpv which screen to go fullscreen to.
    If ``default`` is provided mpv will fallback on using the behavior
    depending on what the user provided with the ``screen`` option.

    .. admonition:: Note (X11)

        This option does works properly only with window managers which
        understand the EWMH ``_NET_WM_FULLSCREEN_MONITORS`` hint.

    .. admonition:: Note (OS X)

        ``all`` does not work on OS X and will behave like ``current``.

    See also ``--screen``.

``--fs-black-out-screens``

    OS X only. Black out other displays when going fullscreen.

``--keep-open=<yes|no|always>``
    Do not terminate when playing or seeking beyond the end of the file, and
    there is not next file to be played (and ``--loop`` is not used).
    Instead, pause the player. When trying to seek beyond end of the file, the
    player will attempt to seek to the last frame.

    The following arguments can be given:

    :no:        If the current file ends, go to the next file or terminate.
                (Default.)
    :yes:       Don't terminate if the current file is the last playlist entry.
                Equivalent to ``--keep-open`` without arguments.
    :always:    Like ``yes``, but also applies to files before the last playlist
                entry. This means playback will never automatically advance to
                the next file.

    .. note::

        This option is not respected when using ``--frames``. Explicitly
        skipping to the next file if the binding uses ``force`` will terminate
        playback as well.

        Also, if errors or unusual circumstances happen, the player can quit
        anyway.

    Since mpv 0.6.0, this doesn't pause if there is a next file in the playlist,
    or the playlist is looped. Approximately, this will pause when the player
    would normally exit, but in practice there are corner cases in which this
    is not the case (e.g. ``mpv --keep-open file.mkv /dev/null`` will play
    file.mkv normally, then fail to open ``/dev/null``, then exit). (In
    mpv 0.8.0, ``always`` was introduced, which restores the old behavior.)

``--force-window``
    Create a video output window even if there is no video. This can be useful
    when pretending that mpv is a GUI application. Currently, the window
    always has the size 640x480, and is subject to ``--geometry``,
    ``--autofit``, and similar options.

    .. warning::

        The window is created only after initialization (to make sure default
        window placement still works if the video size is different from the
        ``--force-window`` default window size). This can be a problem if
        initialization doesn't work perfectly, such as when opening URLs with
        bad network connection, or opening broken video files.

``--ontop``
    Makes the player window stay on top of other windows.

``--border``, ``--no-border``
    Play video with window border and decorations. Since this is on by
    default, use ``--no-border`` to disable the standard window decorations.

``--on-all-workspaces``
    (X11 only)
    Show the video window on all virtual desktops.

``--geometry=<[W[xH]][+-x+-y]>``, ``--geometry=<x:y>``
    Adjust the initial window position or size. ``W`` and ``H`` set the window
    size in pixels. ``x`` and ``y`` set the window position, measured in pixels
    from the top-left corner of the screen to the top-left corner of the image
    being displayed. If a percentage sign (``%``) is given after the argument,
    it turns the value into a percentage of the screen size in that direction.
    Positions are specified similar to the standard X11 ``--geometry`` option
    format, in which e.g. +10-50 means "place 10 pixels from the left border and
    50 pixels from the lower border" and "--20+-10" means "place 20 pixels
    beyond the right and 10 pixels beyond the top border".

    If an external window is specified using the ``--wid`` option, this
    option is ignored.

    The coordinates are relative to the screen given with ``--screen`` for the
    video output drivers that fully support ``--screen``.

    .. note::

        Generally only supported by GUI VOs. Ignored for encoding.

    .. admonition: Note (OS X)

        On Mac OS X the origin of the screen coordinate system is located on the
        bottom-left corner. For instance, ``0:0`` will place the window at the
        bottom-left of the screen.

    .. admonition:: Note (X11)

        This option does not work properly with all window managers.

    .. admonition:: Examples

        ``50:40``
            Places the window at x=50, y=40.
        ``50%:50%``
            Places the window in the middle of the screen.
        ``100%:100%``
            Places the window at the bottom right corner of the screen.
        ``50%``
            Sets the window width to half the screen width. Window height is set
            so that the window has the video aspect ratio.
        ``50%x50%``
            Forces the window width and height to half the screen width and
            height. Will show black borders to compensate for the video aspect
            ration (with most VOs and without ``--no-keepaspect``).
        ``50%+10+10``
            Sets the window to half the screen widths, and positions it 10
            pixels below/left of the top left corner of the screen.

    See also ``--autofit`` and ``--autofit-larger`` for fitting the window into
    a given size without changing aspect ratio.

``--autofit=<[W[xH]]>``
    Set the initial window size to a maximum size specified by ``WxH``, without
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

    Use ``--autofit-larger`` instead if you just want to limit the maximum size
    of the window, rather than always forcing a window size.

    Use ``--geometry`` if you want to force both window width and height to a
    specific size.

    .. note::

        Generally only supported by GUI VOs. Ignored for encoding.

    .. admonition:: Examples

        ``70%``
            Make the window width 70% of the screen size, keeping aspect ratio.
        ``1000``
            Set the window width to 1000 pixels, keeping aspect ratio.
        ``70%:60%``
            Make the window as large as possible, without being wider than 70%
            of the screen width, or higher than 60% of the screen height.

``--autofit-larger=<[W[xH]]>``
    This option behaves exactly like ``--autofit``, except the window size is
    only changed if the window would be larger than the specified size.

    .. admonition:: Example

        ``90%x80%``
            If the video is larger than 90% of the screen width or 80% of the
            screen height, make the window smaller until either its width is 90%
            of the screen, or its height is 80% of the screen.

``--autofit-smaller=<[W[xH]]>``
    This option behaves exactly like ``--autofit``, except that it sets the
    minimum size of the window (just as ``--autofit-larger`` sets the maximum).

    .. admonition:: Example

        ``500x500``
            Make the window at least 500 pixels wide and 500 pixels high
            (depending on the video aspect ratio, the width or height will be
            larger than 500 in order to keep the aspect ratio the same).

``--window-scale=<factor>``
    Resize the video window to a multiple (or fraction) of the video size. This
    option is applied before ``--autofit`` and other options are applied (so
    they override this option).

    For example, ``--window-scale=0.5`` would show the window at half the
    video size.

``--cursor-autohide=<number|no|always>``
    Make mouse cursor automatically hide after given number of milliseconds.
    ``no`` will disable cursor autohide. ``always`` means the cursor will stay
    hidden.

``--cursor-autohide-fs-only``
    If this option is given, the cursor is always visible in windowed mode. In
    fullscreen mode, the cursor is shown or hidden according to
    ``--cursor-autohide``.

``--no-fixed-vo``, ``--fixed-vo``
    ``--no-fixed-vo`` enforces closing and reopening the video window for
    multiple files (one (un)initialization for each file).

``--force-rgba-osd-rendering``
    Change how some video outputs render the OSD and text subtitles. This
    does not change appearance of the subtitles and only has performance
    implications. For VOs which support native ASS rendering (like ``vdpau``,
    ``opengl``, ``direct3d``), this can be slightly faster or slower,
    depending on GPU drivers and hardware. For other VOs, this just makes
    rendering slower.

``--force-window-position``
    Forcefully move mpv's video output window to default location whenever
    there is a change in video parameters, video stream or file. This used to
    be the default behavior. Currently only affects X11 VOs.

``--heartbeat-cmd=<command>``
    Command that is executed every 30 seconds during playback via *system()* -
    i.e. using the shell. The time between the commands can be customized with
    the ``--heartbeat-interval`` option. The command is not run while playback
    is paused.

    .. note::

        mpv uses this command without any checking. It is your responsibility to
        ensure it does not cause security problems (e.g. make sure to use full
        paths if "." is in your path like on Windows). It also only works when
        playing video (i.e. not with ``--no-video`` but works with
        ``-vo=null``).

    This can be "misused" to disable screensavers that do not support the
    proper X API (see also ``--stop-screensaver``). If you think this is too
    complicated, ask the author of the screensaver program to support the
    proper X APIs. Note that the ``--stop-screensaver`` does not influence the
    heartbeat code at all.

    .. admonition:: Example for xscreensaver

        ``mpv --heartbeat-cmd="xscreensaver-command -deactivate" file``

    .. admonition:: Example for GNOME screensaver

        ``mpv --heartbeat-cmd="gnome-screensaver-command -p" file``


``--heartbeat-interval=<sec>``
    Time between ``--heartbeat-cmd`` invocations in seconds (default: 30).

    .. note::

        This does not affect the normal screensaver operation in any way.

``--no-keepaspect``, ``--keepaspect``
    ``--no-keepaspect`` will always stretch the video to window size, and will
    disable the window manager hints that force the window aspect ratio.
    (Ignored in fullscreen mode.)

``--no-keepaspect-window``, ``--keepaspect-window``
    ``--keepaspect-window`` (the default) will lock the window size to the
    video aspect. ``--no-keepaspect-window`` disables this behavior, and will
    instead add black bars if window aspect and video aspect mismatch. Whether
    this actually works depends on the VO backend.
    (Ignored in fullscreen mode.)

``--monitoraspect=<ratio>``
    Set the aspect ratio of your monitor or TV screen. A value of 0 disables a
    previous setting (e.g. in the config file). Overrides the
    ``--monitorpixelaspect`` setting if enabled.

    See also ``--monitorpixelaspect`` and ``--video-aspect``.

    .. admonition:: Examples

        - ``--monitoraspect=4:3``  or ``--monitoraspect=1.3333``
        - ``--monitoraspect=16:9`` or ``--monitoraspect=1.7777``

``--monitorpixelaspect=<ratio>``
    Set the aspect of a single pixel of your monitor or TV screen (default:
    1). A value of 1 means square pixels (correct for (almost?) all LCDs). See
    also ``--monitoraspect`` and ``--video-aspect``.

``--stop-screensaver``, ``--no-stop-screensaver``
    Turns off the screensaver (or screen blanker and similar mechanisms) at
    startup and turns it on again on exit (default: yes). The screensaver is
    always re-enabled when the player is paused.

    This is not supported on all video outputs or platforms. Sometimes it is
    implemented, but does not work (happens often on GNOME). You might be able
    to to work this around using ``--heartbeat-cmd`` instead.

``--wid=<ID>``
    This tells mpv to attach to an existing window. If a VO is selected that
    supports this option, it will use that window for video output. mpv will
    scale the video to the size of this window, and will add black bars to
    compensate if the aspect ratio of the video is different.

    On X11, the ID is interpreted as a ``Window`` on X11. Unlike
    MPlayer/mplayer2, mpv always creates its own window, and sets the wid
    window as parent. The window will always be resized to cover the parent
    window fully. The value ``0`` is interpreted specially, and mpv will
    draw directly on the root window.

    On win32, the ID is interpreted as ``HWND``. Pass it as value cast to
    ``intptr_t``. mpv will create its own window, and set the wid window as
    parent, like with X11.

    On OSX/Cocoa. the ID is interpreted as ``NSView*``. Pass it as value cast
    to ``intptr_t``. mpv will creates its own sub-view. Because OSX does not
    support window embedding of foreign processes, this works only with libmpv,
    and will crash when used from the command line.

``--no-window-dragging``
    Don't move the window when clicking on it and moving the mouse pointer.

``--x11-name``
    Set the window class name for X11-based video output methods.

``--x11-netwm=<yes|no|auto>``
    (X11 only)
    Control the use of NetWM protocol features.

    This may or may not help with broken window managers. This provides some
    functionality that was implemented by the now removed ``--fstype`` option.
    Actually, it is not known to the developers to which degree this option
    was needed, so feedback is welcome.

    Specifically, ``yes`` will force use of NetWM fullscreen support, even if
    not advertised by the WM. This can be useful for WMs that are broken on
    purpose, like XMonad. (XMonad supposedly doesn't advertise fullscreen
    support, because Flash uses it. Apparently, applications which want to
    use fullscreen anyway are supposed to either ignore the NetWM support hints,
    or provide a workaround. Shame on XMonad for deliberately breaking X
    protocols (as if X isn't bad enough already).

    By default, NetWM support is autodetected (``auto``).

    This option might be removed in the future.


Disc Devices
------------

``--cdrom-device=<path>``
    Specify the CD-ROM device (default: ``/dev/cdrom``).

``--dvd-device=<path>``
    Specify the DVD device or .iso filename (default: ``/dev/dvd``). You can
    also specify a directory that contains files previously copied directly
    from a DVD (with e.g. vobcopy).

    .. admonition:: Example

        ``mpv dvd:// --dvd-device=/path/to/dvd/``

``--bluray-device=<path>``
    (Blu-ray only)
    Specify the Blu-ray disc location. Must be a directory with Blu-ray
    structure.

    .. admonition:: Example

        ``mpv bd:// --bluray-device=/path/to/bd/``

``--bluray-angle=<ID>``
    Some Blu-ray discs contain scenes that can be viewed from multiple angles.
    This option tells mpv which angle to use (default: 1).

``--cdda-...``
    These options can be used to tune the CD Audio reading feature of mpv.

``--cdda-speed=<value>``
    Set CD spin speed.

``--cdda-paranoia=<0-2>``
    Set paranoia level. Values other than 0 seem to break playback of
    anything but the first track.

    :0: disable checking (default)
    :1: overlap checking only
    :2: full data correction and verification

``--cdda-sector-size=<value>``
    Set atomic read size.

``--cdda-overlap=<value>``
    Force minimum overlap search during verification to <value> sectors.

``--cdda-toc-bias``
    Assume that the beginning offset of track 1 as reported in the TOC
    will be addressed as LBA 0. Some discs need this for getting track
    boundaries correctly.

``--cdda-toc-offset=<value>``
    Add ``<value>`` sectors to the values reported when addressing tracks.
    May be negative.

``--cdda-skip=<yes|no>``
    (Never) accept imperfect data reconstruction.

``--cdda-cdtext=<yes|no>``
    Print CD text. This is disabled by default, because it ruins perfomance
    with CD-ROM drives for unknown reasons.

``--dvd-speed=<speed>``
    Try to limit DVD speed (default: 0, no change). DVD base speed is 1385
    kB/s, so an 8x drive can read at speeds up to 11080 kB/s. Slower speeds
    make the drive more quiet. For watching DVDs, 2700 kB/s should be quiet and
    fast enough. mpv resets the speed to the drive default value on close.
    Values of at least 100 mean speed in kB/s. Values less than 100 mean
    multiples of 1385 kB/s, i.e. ``--dvd-speed=8`` selects 11080 kB/s.

    .. note::

        You need write access to the DVD device to change the speed.

``--dvd-angle=<ID>``
    Some DVDs contain scenes that can be viewed from multiple angles.
    This option tells mpv which angle to use (default: 1).



Equalizer
---------

``--brightness=<-100-100>``
    Adjust the brightness of the video signal (default: 0). Not supported by
    all video output drivers.

``--contrast=<-100-100>``
    Adjust the contrast of the video signal (default: 0). Not supported by all
    video output drivers.

``--saturation=<-100-100>``
    Adjust the saturation of the video signal (default: 0). You can get
    grayscale output with this option. Not supported by all video output
    drivers.

``--gamma=<-100-100>``
    Adjust the gamma of the video signal (default: 0). Not supported by all
    video output drivers.

``--hue=<-100-100>``
    Adjust the hue of the video signal (default: 0). You can get a colored
    negative of the image with this option. Not supported by all video output
    drivers.

Demuxer
-------

``--demuxer=<[+]name>``
    Force demuxer type. Use a '+' before the name to force it; this will skip
    some checks. Give the demuxer name as printed by ``--demuxer=help``.

``--demuxer-lavf-analyzeduration=<value>``
    Maximum length in seconds to analyze the stream properties.

``--demuxer-lavf-probescore=<1-100>``
    Minimum required libavformat probe score. Lower values will require
    less data to be loaded (makes streams start faster), but makes file
    format detection less reliable. Can be used to force auto-detected
    libavformat demuxers, even if libavformat considers the detection not
    reliable enough. (Default: 26.)

``--demuxer-lavf-allow-mimetype=<yes|no>``
    Allow deriving the format from the HTTP MIME type (default: yes). Set
    this to no in case playing things from HTTP mysteriously fails, even
    though the same files work from local disk.

    This is default in order to reduce latency when opening HTTP streams.

``--demuxer-lavf-format=<name>``
    Force a specific libavformat demuxer.

``--demuxer-lavf-hacks=<yes|no>``
    By default, some formats will be handled differently from other formats
    by explicitly checking for them. Most of these compensate for weird or
    imperfect behavior from libavformat demuxers. Passing ``no`` disables
    these. For debugging and testing only.

``--demuxer-lavf-genpts-mode=<no|lavf>``
    Mode for deriving missing packet PTS values from packet DTS. ``lavf``
    enables libavformat's ``genpts`` option. ``no`` disables it. This used
    to be enabled by default, but then it was deemed as not needed anymore.
    Enabling this might help with timestamp problems, or make them worse.

``--demuxer-lavf-o=<key>=<value>[,<key>=<value>[,...]]``
    Pass AVOptions to libavformat demuxer.

    Note, a patch to make the *o=* unneeded and pass all unknown options
    through the AVOption system is welcome. A full list of AVOptions can
    be found in the FFmpeg manual. Note that some options may conflict
    with mpv options.

    .. admonition:: Example

        ``--demuxer-lavf-o=fflags=+ignidx``

``--demuxer-lavf-probesize=<value>``
    Maximum amount of data to probe during the detection phase. In the
    case of MPEG-TS this value identifies the maximum number of TS packets
    to scan.

``--demuxer-lavf-buffersize=<value>``
    Size of the stream read buffer allocated for libavformat in bytes
    (default: 32768). Lowering the size could lower latency. Note that
    libavformat might reallocate the buffer internally, or not fully use all
    of it.

``--demuxer-lavf-cryptokey=<hexstring>``
    Encryption key the demuxer should use. This is the raw binary data of
    the key converted to a hexadecimal string.

``--demuxer-mkv-subtitle-preroll``, ``--mkv-subtitle-preroll``
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

    You can use the ``--demuxer-mkv-subtitle-preroll-secs`` option to specify
    how mach data the demuxer should pre-read at most in order to find subtitle
    packets that may overlap. Setting this to 0 will effectively disable this
    preroll mechanism. Setting a very large value can make seeking very slow,
    and an extremely large value would completely reread the entire file from
    start to seek target on every seek - seeking can become slower towards the
    end of the file. The details are messy, and the value is actually rounded
    down to the cluster with the previous video keyframe.

    Some files, especially files muxed with newer mkvmerge versions, have
    information embedded that can be used to determine what subtitle packets
    overlap with a seek target. In these cases, mpv will reduce the amount
    of data read to a minimum. (Although it will still read *all* data between
    the cluster that contains the first wanted subtitle packet, and the seek
    target.)

    See also ``--hr-seek-demuxer-offset`` option. This option can achieve a
    similar effect, but only if hr-seek is active. It works with any demuxer,
    but makes seeking much slower, as it has to decode audio and video data
    instead of just skipping over it.

    ``--mkv-subtitle-preroll`` is a deprecated alias.

``--demuxer-mkv-subtitle-preroll-secs=<value>``
    See ``--demuxer-mkv-subtitle-preroll``.

``--demuxer-mkv-probe-video-duration``
    When opening the file, seek to the end of it, and check what timestamp the
    last video packet has, and report that as file duration. This is strictly
    for compatibility with Haali only. In this mode, it's possible that opening
    will be slower (especially when playing over http), or that behavior with
    broken files is much worse. So don't use this option.

``--demuxer-mkv-fix-timestamps=<yes|no>``
    Fix rounded Matroska timestamps (enabled by default). Matroska usually
    stores timestamps rounded to milliseconds. This means timestamps jitter
    by some amount around the intended timestamp. mpv can correct the timestamps
    based on the framerate value stored in the file: the timestamp is rounded
    to the next frame (according to the framerate), unless the new timestamp
    would deviate more than 1ms from the old one. This should undo the rounding
    done by the muxer.

    (The allowed deviation can be less than 1ms if the file uses a non-standard
    timecode scale.)

``--demuxer-rawaudio-channels=<value>``
    Number of channels (or channel layout) if ``--demuxer=rawaudio`` is used
    (default: stereo).

``--demuxer-rawaudio-format=<value>``
    Sample format for ``--demuxer=rawaudio`` (default: s16le).
    Use ``--demuxer-rawaudio-format=help`` to get a list of all formats.

``--demuxer-rawaudio-rate=<value>``
    Sample rate for ``--demuxer=rawaudio`` (default: 44 kHz).

``--demuxer-rawvideo-fps=<value>``
    Rate in frames per second for ``--demuxer=rawvideo`` (default: 25.0).

``--demuxer-rawvideo-w=<value>``, ``--demuxer-rawvideo-h=<value>``
    Image dimension in pixels for ``--demuxer=rawvideo``.

    .. admonition:: Example

        Play a raw YUV sample::

            mpv sample-720x576.yuv --demuxer=rawvideo \
            --demuxer-rawvideo-w=720 --demuxer-rawvideo-h=576

``--demuxer-rawvideo-format=<value>``
    Color space (fourcc) in hex or string for ``--demuxer=rawvideo``
    (default: ``YV12``).

``--demuxer-rawvideo-mp-format=<value>``
    Color space by internal video format for ``--demuxer=rawvideo``. Use
    ``--demuxer-rawvideo-mp-format=help`` for a list of possible formats.

``--demuxer-rawvideo-codec=<value>``
    Set the video codec instead of selecting the rawvideo codec when using
    ``--demuxer=rawvideo``. This uses the same values as codec names in
    ``--vd`` (but it does not accept decoder names).

``--demuxer-rawvideo-size=<value>``
    Frame size in bytes when using ``--demuxer=rawvideo``.

``--demuxer-thread=<yes|no>``
    Run the demuxer in a separate thread, and let it prefetch a certain amount
    of packets (default: yes). Having this enabled may lead to smoother
    playback, but on the other hand can add delays to seeking or track
    switching.

``--demuxer-readahead-secs=<seconds>``
    If ``--demuxer-thread`` is enabled, this controls how much the demuxer
    should buffer ahead in seconds (default: 1). As long as no packet has
    a timestamp difference higher than the readahead amount relative to the
    last packet returned to the decoder, the demuxer keeps reading.

    Note that the ``--cache-secs`` option will override this value if a cache
    is enabled, and the value is larger.

    (This value tends to be fuzzy, because many file formats don't store linear
    timestamps.)

``--demuxer-readahead-packets=<packets>``
    If ``--demuxer-thread`` is enabled, this controls how much the demuxer
    should buffer ahead. As long as the number of packets in the packet queue
    doesn't exceed ``--demuxer-readahead-packets``, and the total number of
    bytes doesn't exceed ``--demuxer-readahead-bytes``, the thread keeps
    reading ahead.

    Note that if you set these options near the maximum, you might get a
    packet queue overflow warning.

    See ``--list-options`` for defaults and value range.

``--demuxer-readahead-bytes=<bytes>``
    See ``--demuxer-readahead-packets``.


Input
-----

``--native-keyrepeat``
    Use system settings for keyrepeat delay and rate, instead of
    ``--input-ar-delay`` and ``--input-ar-rate``. (Whether this applies
    depends on the VO backend and how it handles keyboard input. Does not
    apply to terminal input.)

``--input-ar-delay``
    Delay in milliseconds before we start to autorepeat a key (0 to disable).

``--input-ar-rate``
    Number of key presses to generate per second on autorepeat.

``--input-conf=<filename>``
    Specify input configuration file other than the default location in the mpv
    configuration directory (usually ``~/.config/mpv/input.conf``).

``--no-input-default-bindings``
    Disable mpv default (built-in) key bindings.

``--input-cmdlist``
    Prints all commands that can be bound to keys.

``--input-doubleclick-time=<milliseconds>``
    Time in milliseconds to recognize two consecutive button presses as a
    double-click (default: 300).

``--input-keylist``
    Prints all keys that can be bound to commands.

``--input-key-fifo-size=<2-65000>``
    Specify the size of the FIFO that buffers key events (default: 7). If it
    is too small some events may be lost. The main disadvantage of setting it
    to a very large value is that if you hold down a key triggering some
    particularly slow command then the player may be unresponsive while it
    processes all the queued commands.

``--input-test``
    Input test mode. Instead of executing commands on key presses, mpv
    will show the keys and the bound commands on the OSD. Has to be used
    with a dummy video, and the normal ways to quit the player will not
    work (key bindings that normally quit will be shown on OSD only, just
    like any other binding). See `INPUT.CONF`_.

``--input-file=<filename>``
    Read commands from the given file. Mostly useful with a FIFO. Since
    mpv 0.7.0 also understands JSON commands (see `JSON IPC`_), but you can't
    get replies or events. Use ``--input-unix-socket`` for something
    bi-directional. On MS Windows, JSON commands are not available.

    This can also specify a direct file descriptor with ``fd://N`` (UNIX only).
    In this case, JSON replies will be written if the FD is writable.

    See also ``--slave-broken``.

    .. note::

        When the given file is a FIFO mpv opens both ends, so you can do several
        `echo "seek 10" > mp_pipe` and the pipe will stay valid.

``--input-terminal``, ``--no-input-terminal``
    ``--no-input-terminal`` prevents the player from reading key events from
    standard input. Useful when reading data from standard input. This is
    automatically enabled when ``-`` is found on the command line. There are
    situations where you have to set it manually, e.g. if you open
    ``/dev/stdin`` (or the equivalent on your system), use stdin in a playlist
    or intend to read from stdin later on via the loadfile or loadlist slave
    commands.

``--input-unix-socket=<filename>``
    Enable the IPC support and create the listening socket at the given path.

    See `JSON IPC`_ for details.

    Not available on MS Windows.

``--input-appleremote=<yes|no>``
    (OS X only)
    Enable/disable Apple Remote support. Enabled by default (except for libmpv).

``--input-cursor``, ``--no-input-cursor``
    Permit mpv to receive pointer events reported by the video output
    driver. Necessary to use the OSC, or to select the buttons in DVD menus.
    Support depends on the VO in use.

``--input-media-keys=<yes|no>``
    (OS X only)
    Enable/disable media keys support. Enabled by default (except for libmpv).

``--input-right-alt-gr``, ``--no-input-right-alt-gr``
    (Cocoa and Windows only)
    Use the right Alt key as Alt Gr to produce special characters. If disabled,
    count the right Alt as an Alt modifier key. Enabled by default.

``--input-vo-keyboard=<yes|no>``
    Disable all keyboard input on for VOs which can't participate in proper
    keyboard input dispatching. May not affect all VOs. Generally useful for
    embedding only.

    On X11, a sub-window with input enabled grabs all keyboard input as long
    as it is 1. a child of a focused window, and 2. the mouse is inside of
    the sub-window. The can steal away all keyboard input from the
    application embedding the mpv window, and on the other hand, the mpv
    window will receive no input if the mouse is outside of the mpv window,
    even though mpv has focus. Modern toolkits work around this weird X11
    behavior, but naively embedding foreign windows breaks it.

    The only way to handle this reasonably is using the XEmbed protocol, which
    was designed to solve these problems. GTK provides ``GtkSocket``, which
    supports XEmbed. Qt doesn't seem to provide anything working in newer
    versions.

    If the embedder supports XEmbed, input should work with default settings
    and with this option disabled. Note that ``input-default-bindings`` is
    disabled by default in libmpv as well - it should be enabled if you want
    the mpv default key bindings.

    (This option was renamed from ``--input-x11-keyboard``.)

``--input-app-events=<yes|no>``
    (OS X only)
    Enable/disable application wide keyboard events so that keyboard shortcuts
    can be processed without a window. Enabled by default (except for libmpv).

OSD
---

``--osc``, ``--no-osc``
    Whether to load the on-screen-controller (default: yes).

``--no-osd-bar``, ``--osd-bar``
    Disable display of the OSD bar. This will make some things (like seeking)
    use OSD text messages instead of the bar.

    You can configure this on a per-command basis in input.conf using ``osd-``
    prefixes, see ``Input command prefixes``. If you want to disable the OSD
    completely, use ``--osd-level=0``.

``--osd-duration=<time>``
    Set the duration of the OSD messages in ms (default: 1000).

``--osd-font=<pattern>``, ``--sub-text-font=<pattern>``
    Specify font to use for OSD and for subtitles that do not themselves
    specify a particular font. The default is ``sans-serif``.

    .. admonition:: Examples

        - ``--osd-font='Bitstream Vera Sans'``
        - ``--osd-font='Bitstream Vera Sans:style=Bold'`` (fontconfig pattern)

    .. note::

        The ``--sub-text-font`` option (and most other ``--sub-text-``
        options) are ignored when ASS-subtitles are rendered, unless the
        ``--no-sub-ass`` option is specified.

``--osd-font-size=<size>``, ``--sub-text-font-size=<size>``
    Specify the OSD/sub font size. The unit is the size in scaled pixels at a
    window height of 720. The actual pixel size is scaled with the window
    height: if the window height is larger or smaller than 720, the actual size
    of the text increases or decreases as well.

    Default: 55.

``--osd-msg1=<string>``
    Show this string as message on OSD with OSD level 1 (visible by default).
    The message will be visible by default, and as long no other message
    covers it, and the OSD level isn't changed (see ``--osd-level``).
    Expands properties; see `Property Expansion`_.

``--osd-msg2=<string>``
    Similar as ``--osd-msg1``, but for OSD level 2. If this is an empty string
    (default), then the playback time is shown.

``--osd-msg3=<string>``
    Similar as ``--osd-msg1``, but for OSD level 3. If this is an empty string
    (default), then the playback time, duration, and some more information is
    shown.

    This is also used for the ``show_progress`` command (by default mapped to
    ``P``), or in some non-default cases when seeking.

    ``--osd-status-msg`` is a legacy equivalent (but with a minor difference).

``--osd-status-msg=<string>``
    Show a custom string during playback instead of the standard status text.
    This overrides the status text used for ``--osd-level=3``, when using the
    ``show_progress`` command (by default mapped to ``P``), or in some
    non-default cases when seeking. Expands properties. See
    `Property Expansion`_.

    This option has been replaced with ``--osd-msg3``. The only difference is
    that this option implicitly includes ``${osd-sym-cc}``. This option is
    ignored if ``--osd-msg3`` is not empty.

``--osd-playing-msg=<string>``
    Show a message on OSD when playback starts. The string is expanded for
    properties, e.g. ``--osd-playing-msg='file: ${filename}'`` will show the
    message ``file:`` followed by a space and the currently played filename.

    See `Property Expansion`_.

``--osd-bar-align-x=<-1-1>``
    Position of the OSD bar. -1 is far left, 0 is centered, 1 is far right.
    Fractional values (like 0.5) are allowed.

``--osd-bar-align-y=<-1-1>``
    Position of the OSD bar. -1 is top, 0 is centered, 1 is bottom.
    Fractional values (like 0.5) are allowed.

``--osd-bar-w=<1-100>``
    Width of the OSD bar, in percentage of the screen width (default: 75).
    A value of 50 means the bar is half the screen wide.

``--osd-bar-h=<0.1-50>``
    Height of the OSD bar, in percentage of the screen height (default: 3.125).

``--osd-back-color=<color>``, ``--sub-text-back-color=<color>``
    See ``--osd-color``. Color used for OSD/sub text background.

``--osd-blur=<0..20.0>``, ``--sub-text-blur=<0..20.0>``
    Gaussian blur factor. 0 means no blur applied (default).

``--osd-bold=<yes|no>``, ``--sub-text-bold=<yes|no>``
    Format text on bold.

``--osd-border-color=<color>``, ``--sub-text-border-color=<color>``
    See ``--osd-color``. Color used for the OSD/sub font border.

    .. note::

        ignored when ``--osd-back-color``/``--sub-text-back-color`` is
        specified (or more exactly: when that option is not set to completely
        transparent).

``--osd-border-size=<size>``, ``--sub-text-border-size=<size>``
    Size of the OSD/sub font border in scaled pixels (see ``--osd-font-size``
    for details). A value of 0 disables borders.

    Default: 3.

``--osd-color=<color>``, ``--sub-text-color=<color>``
    Specify the color used for OSD/unstyled text subtitles.

    The color is specified in the form ``r/g/b``, where each color component
    is specified as number in the range 0.0 to 1.0. It's also possible to
    specify the transparency by using ``r/g/b/a``, where the alpha value 0
    means fully transparent, and 1.0 means opaque. If the alpha component is
    not given, the color is 100% opaque.

    Passing a single number to the option sets the OSD to gray, and the form
    ``gray/a`` lets you specify alpha additionally.

    .. admonition:: Examples

        - ``--osd-color=1.0/0.0/0.0`` set OSD to opaque red
        - ``--osd-color=1.0/0.0/0.0/0.75`` set OSD to opaque red with 75% alpha
        - ``--osd-color=0.5/0.75`` set OSD to 50% gray with 75% alpha

    Alternatively, the color can be specified as a RGB hex triplet in the form
    ``#RRGGBB``, where each 2-digit group expresses a color value in the
    range 0 (``00``) to 255 (``FF``). For example, ``#FF0000`` is red.
    This is similar to web colors. Alpha is given with ``#AARRGGBB``.

    .. admonition:: Examples

        - ``--osd-color='#FF0000'`` set OSD to opaque red
        - ``--osd-color='#C0808080'`` set OSD to 50% gray with 75% alpha

``--osd-fractions``
    Show OSD times with fractions of seconds (in millisecond precision). Useful
    to see the exact timestamp of a video frame.

``--osd-level=<0-3>``
    Specifies which mode the OSD should start in.

    :0: OSD completely disabled (subtitles only)
    :1: enabled (shows up only on user interaction)
    :2: enabled + current time visible by default
    :3: enabled + ``--osd-status-msg`` (current time and status by default)

``--osd-margin-x=<size>, --sub-text-margin-x=<size>``
    Left and right screen margin for the OSD/subs in scaled pixels (see
    ``--osd-font-size`` for details).

    This option specifies the distance of the OSD to the left, as well as at
    which distance from the right border long OSD text will be broken.

    Default: 25.

``--osd-margin-y=<size>, --sub-text-margin-y=<size>``
    Top and bottom screen margin for the OSD/subs in scaled pixels (see
    ``--osd-font-size`` for details).

    This option specifies the vertical margins of the OSD. This is also used
    for unstyled text subtitles. If you just want to raise the vertical
    subtitle position, use ``--sub-pos``.

    Default: 22.

``--osd-align-x=<left|center|right>``,  ``--sub-text-align-x=...``
    Control to which corner of the screen OSD or text subtitles should be
    aligned to (default: ``center`` for subs, ``left`` for OSD).

    Never applied to ASS subtitles, except in ``--no-sub-ass`` mode. Likewise,
    this does not apply to image subtitles.

``--osd-align-y=<top|center|bottom>`` ``--sub-text-align-y=...``
    Vertical position (default: ``bottom`` for subs, ``top`` for OSD).
    Details see ``--osd-align-x``.

``--osd-scale=<factor>``
    OSD font size multiplier, multiplied with ``--osd-font-size`` value.

``--osd-scale-by-window=<yes|no>``
    Whether to scale the OSD with the window size (default: yes). If this is
    disabled, ``--osd-font-size`` and other OSD options that use scaled pixels
    are always in actual pixels. The effect is that changing the window size
    won't change the OSD font size.

``--osd-shadow-color=<color>, --sub-text-shadow-color=<color>``
    See ``--osd-color``. Color used for OSD/sub text shadow.

``--osd-shadow-offset=<size>, --sub-text-shadow-offset=<size>``
    Displacement of the OSD/sub text shadow in scaled pixels (see
    ``--osd-font-size`` for details). A value of 0 disables shadows.

    Default: 0.

``--osd-spacing=<size>, --sub-text-spacing=<size>``
    Horizontal OSD/sub font spacing in scaled pixels (see ``--osd-font-size``
    for details). This value is added to the normal letter spacing. Negative
    values are allowed.

    Default: 0.

``--use-text-osd=<yes|no>``
    Disable text OSD rendering completely. (This includes the complete OSC as
    well.) This is mostly useful for avoiding loading fontconfig in situations
    where fontconfig does not behave well, and OSD is unused - this could for
    example allow GUI programs using libmpv to workaround fontconfig issues.

    Note that selecting subtitles of any kind still initializes fontconfig.

    Default: ``no``.


Screenshot
----------

``--screenshot-format=<type>``
    Set the image file type used for saving screenshots.

    Available choices:

    :png:       PNG
    :ppm:       PPM
    :pgm:       PGM
    :pgmyuv:    PGM with YV12 pixel format
    :tga:       TARGA
    :jpg:       JPEG (default)
    :jpeg:      JPEG (same as jpg, but with .jpeg file ending)

``--screenshot-tag-colorspace=<yes|no>``
    Tag screenshots with the appropriate colorspace.

    Note that not all formats are supported.

    Default: ``yes``.

``--screenshot-template=<template>``
    Specify the filename template used to save screenshots. The template
    specifies the filename without file extension, and can contain format
    specifiers, which will be substituted when taking a screenshot.
    By default the template is ``shot%n``, which results in filenames like
    ``shot0012.png`` for example.

    The template can start with a relative or absolute path, in order to
    specify a directory location where screenshots should be saved.

    If the final screenshot filename points to an already existing file, the
    file will not be overwritten. The screenshot will either not be saved, or if
    the template contains ``%n``, saved using different, newly generated
    filename.

    Allowed format specifiers:

    ``%[#][0X]n``
        A sequence number, padded with zeros to length X (default: 04). E.g.
        passing the format ``%04n`` will yield ``0012`` on the 12th screenshot.
        The number is incremented every time a screenshot is taken or if the
        file already exists. The length ``X`` must be in the range 0-9. With
        the optional # sign, mpv will use the lowest available number. For
        example, if you take three screenshots--0001, 0002, 0003--and delete
        the first two, the next two screenshots will not be 0004 and 0005, but
        0001 and 0002 again.
    ``%f``
        Filename of the currently played video.
    ``%F``
        Same as ``%f``, but strip the file extension, including the dot.
    ``%x``
        Directory path of the currently played video. If the video is not on
        the filesystem (but e.g. ``http://``), this expand to an empty string.
    ``%X{fallback}``
        Same as ``%x``, but if the video file is not on the filesystem, return
        the fallback string inside the ``{...}``.
    ``%p``
        Current playback time, in the same format as used in the OSD. The
        result is a string of the form "HH:MM:SS". For example, if the video is
        at the time position 5 minutes and 34 seconds, ``%p`` will be replaced
        with "00:05:34".
    ``%P``
        Similar to ``%p``, but extended with the playback time in milliseconds.
        It is formatted as "HH:MM:SS.mmm", with "mmm" being the millisecond
        part of the playback time.

        .. note::

            This is a simple way for getting unique per-frame timestamps. Frame
            numbers would be more intuitive, but are not easily implementable
            because container formats usually use time stamps for identifying
            frames.)
    ``%wX``
        Specify the current playback time using the format string ``X``.
        ``%p`` is like ``%wH:%wM:%wS``, and ``%P`` is like ``%wH:%wM:%wS.%wT``.

        Valid format specifiers:
            ``%wH``
                hour (padded with 0 to two digits)
            ``%wh``
                hour (not padded)
            ``%wM``
                minutes (00-59)
            ``%wm``
                total minutes (includes hours, unlike ``%wM``)
            ``%wS``
                seconds (00-59)
            ``%ws``
                total seconds (includes hours and minutes)
            ``%wf``
                like ``%ws``, but as float
            ``%wT``
                milliseconds (000-999)

    ``%tX``
        Specify the current local date/time using the format ``X``. This format
        specifier uses the UNIX ``strftime()`` function internally, and inserts
        the result of passing "%X" to ``strftime``. For example, ``%tm`` will
        insert the number of the current month as number. You have to use
        multiple ``%tX`` specifiers to build a full date/time string.
    ``%{prop[:fallback text]}``
        Insert the value of the slave property 'prop'. E.g. ``%{filename}`` is
        the same as ``%f``. If the property does not exist or is not available,
        an error text is inserted, unless a fallback is specified.
    ``%%``
        Replaced with the ``%`` character itself.

``--screenshot-jpeg-quality=<0-100>``
    Set the JPEG quality level. Higher means better quality. The default is 90.

``--screenshot-png-compression=<0-9>``
    Set the PNG compression level. Higher means better compression. This will
    affect the file size of the written screenshot file and the time it takes
    to write a screenshot. Too high compression might occupy enough CPU time to
    interrupt playback. The default is 7.

``--screenshot-png-filter=<0-5>``
    Set the filter applied prior to PNG compression. 0 is none, 1 is "sub", 2 is
    "up", 3 is "average", 4 is "Paeth", and 5 is "mixed". This affects the level
    of compression that can be achieved. For most images, "mixed" achieves the
    best compression ratio, hence it is the default.


Software Scaler
---------------

``--sws-scaler=<name>``
    Specify the software scaler algorithm to be used with ``--vf=scale``. This
    also affects video output drivers which lack hardware acceleration,
    e.g. ``x11``. See also ``--vf=scale``.

    To get a list of available scalers, run ``--sws-scaler=help``.

    Default: ``bicubic``.

``--sws-lgb=<0-100>``
    Software scaler Gaussian blur filter (luma). See ``--sws-scaler``.

``--sws-cgb=<0-100>``
    Software scaler Gaussian blur filter (chroma). See ``--sws-scaler``.

``--sws-ls=<-100-100>``
    Software scaler sharpen filter (luma). See ``--sws-scaler``.

``--sws-cs=<-100-100>``
    Software scaler sharpen filter (chroma). See ``--sws-scaler``.

``--sws-chs=<h>``
    Software scaler chroma horizontal shifting. See ``--sws-scaler``.

``--sws-cvs=<v>``
    Software scaler chroma vertical shifting. See ``--sws-scaler``.


Terminal
--------

``--quiet``
    Make console output less verbose; in particular, prevents the status line
    (i.e. AV: 3.4 (00:00:03.37) / 5320.6 ...) from being displayed.
    Particularly useful on slow terminals or broken ones which do not properly
    handle carriage return (i.e. ``\r``).

    Also see ``--really-quiet`` and ``--msg-level``.

``--really-quiet``
    Display even less output and status messages than with ``--quiet``.

``--no-terminal``, ``--terminal``
    Disable any use of the terminal and stdin/stdout/stderr. This completely
    silences any message output.

    Unlike ``--really-quiet``, this disables input and terminal initialization
    as well.

``--no-msg-color``
    Disable colorful console output on terminals.

``--msg-level=<module1=level1,module2=level2,...>``
    Control verbosity directly for each module. The ``all`` module changes the
    verbosity of all the modules not explicitly specified on the command line.

    Run mpv with ``--msg-level=all=trace`` to see all messages mpv outputs. You
    can use the module names printed in the output (prefixed to each line in
    ``[...]``) to limit the output to interesting modules.

    .. note::

        Some messages are printed before the command line is parsed and are
        therefore not affected by ``--msg-level``. To control these messages,
        you have to use the ``MPV_VERBOSE`` environment variable; see
        `ENVIRONMENT VARIABLES`_ for details.

    Available levels:

        :no:        complete silence
        :fatal:     fatal messages only
        :error:     error messages
        :warn:      warning messages
        :info:      informational messages
        :status:    status messages (default)
        :v:         verbose messages
        :debug:     debug messages
        :trace:     very noisy debug messages

``--term-osd, --no-term-osd``, ``--term-osd=force``
    Display OSD messages on the console when no video output is available.
    Enabled by default.

    ``force`` enables terminal OSD even if a video window is created.

``--term-osd-bar``, ``--no-term-osd-bar``
    Enable printing a progress bar under the status line on the terminal.
    (Disabled by default.)

``--term-osd-bar-chars=<string>``
    Customize the ``--term-osd-bar`` feature. The string is expected to
    consist of 5 characters (start, left space, position indicator,
    right space, end). You can use Unicode characters, but note that double-
    width characters will not be treated correctly.

    Default: ``[-+-]``.

``--term-playing-msg=<string>``
    Print out a string after starting playback. The string is expanded for
    properties, e.g. ``--term-playing-msg='file: ${filename}'`` will print the string
    ``file:`` followed by a space and the currently played filename.

    See `Property Expansion`_.

``--term-status-msg=<string>``
    Print out a custom string during playback instead of the standard status
    line. Expands properties. See `Property Expansion`_.

``--msg-module``
    Prepend module name to each console message.

``--msg-time``
    Prepend timing information to each console message.


TV
--

``--tv-...``
    These options tune various properties of the TV capture module. For
    watching TV with mpv, use ``tv://`` or ``tv://<channel_number>`` or
    even ``tv://<channel_name>`` (see option ``tv-channels`` for ``channel_name``
    below) as a media URL. You can also use ``tv:///<input_id>`` to start
    watching a video from a composite or S-Video input (see option ``input`` for
    details).

``--tv-device=<value>``
    Specify TV device (default: ``/dev/video0``).

``--tv-channel=<value>``
    Set tuner to <value> channel.

``--no-tv-audio``
    no sound

``--tv-automute=<0-255> (v4l and v4l2 only)``
    If signal strength reported by device is less than this value, audio
    and video will be muted. In most cases automute=100 will be enough.
    Default is 0 (automute disabled).

``--tv-driver=<value>``
    See ``--tv=driver=help`` for a list of compiled-in TV input drivers.
    available: dummy, v4l2 (default: autodetect)

``--tv-input=<value>``
    Specify input (default: 0 (TV), see console output for available
    inputs).

``--tv-freq=<value>``
    Specify the frequency to set the tuner to (e.g. 511.250). Not
    compatible with the channels parameter.

``--tv-outfmt=<value>``
    Specify the output format of the tuner with a preset value supported
    by the V4L driver (YV12, UYVY, YUY2, I420) or an arbitrary format given
    as hex value.

``--tv-width=<value>``
    output window width

``--tv-height=<value>``
    output window height

``--tv-fps=<value>``
    framerate at which to capture video (frames per second)

``--tv-buffersize=<value>``
    maximum size of the capture buffer in megabytes (default: dynamical)

``--tv-norm=<value>``
    See the console output for a list of all available norms, also see the
    ``normid`` option below.

``--tv-normid=<value> (v4l2 only)``
    Sets the TV norm to the given numeric ID. The TV norm depends on the
    capture card. See the console output for a list of available TV norms.

``--tv-chanlist=<value>``
    available: argentina, australia, china-bcast, europe-east,
    europe-west, france, ireland, italy, japan-bcast, japan-cable,
    newzealand, russia, southafrica, us-bcast, us-cable, us-cable-hrc

``--tv-channels=<chan>-<name>[=<norm>],<chan>-<name>[=<norm>],...``
    Set names for channels.

    .. note::

        If <chan> is an integer greater than 1000, it will be treated as
        frequency (in kHz) rather than channel name from frequency table.
        Use _ for spaces in names (or play with quoting ;-) ). The channel
        names will then be written using OSD, and the slave commands
        ``tv_step_channel``, ``tv_set_channel`` and ``tv_last_channel``
        will be usable for a remote control. Not compatible with
        the ``frequency`` parameter.

    .. note::

        The channel number will then be the position in the 'channels'
        list, beginning with 1.

    .. admonition:: Examples

        ``tv://1``, ``tv://TV1``, ``tv_set_channel 1``,
        ``tv_set_channel TV1``

``--tv-[brightness|contrast|hue|saturation]=<-100-100>``
    Set the image equalizer on the card.

``--tv-audiorate=<value>``
    Set input audio sample rate.

``--tv-forceaudio``
    Capture audio even if there are no audio sources reported by v4l.

``--tv-alsa``
    Capture from ALSA.

``--tv-amode=<0-3>``
    Choose an audio mode:

    :0: mono
    :1: stereo
    :2: language 1
    :3: language 2

``--tv-forcechan=<1-2>``
    By default, the count of recorded audio channels is determined
    automatically by querying the audio mode from the TV card. This option
    allows forcing stereo/mono recording regardless of the amode option
    and the values returned by v4l. This can be used for troubleshooting
    when the TV card is unable to report the current audio mode.

``--tv-adevice=<value>``
    Set an audio device. <value> should be ``/dev/xxx`` for OSS and a
    hardware ID for ALSA. You must replace any ':' by a '.' in the
    hardware ID for ALSA.

``--tv-audioid=<value>``
    Choose an audio output of the capture card, if it has more than one.

``--tv-[volume|bass|treble|balance]=<0-100>``
    These options set parameters of the mixer on the video capture card.
    They will have no effect, if your card does not have one. For v4l2 50
    maps to the default value of the control, as reported by the driver.

``--tv-gain=<0-100>``
    Set gain control for video devices (usually webcams) to the desired
    value and switch off automatic control. A value of 0 enables automatic
    control. If this option is omitted, gain control will not be modified.

``--tv-immediatemode=<bool>``
    A value of 0 means capture and buffer audio and video together. A
    value of 1 (default) means to do video capture only and let the audio
    go through a loopback cable from the TV card to the sound card.

``--tv-mjpeg``
    Use hardware MJPEG compression (if the card supports it). When using
    this option, you do not need to specify the width and height of the
    output window, because mpv will determine it automatically from
    the decimation value (see below).

``--tv-decimation=<1|2|4>``
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

``--tv-quality=<0-100>``
    Choose the quality of the JPEG compression (< 60 recommended for full
    size).

``--tv-scan-autostart``
    Begin channel scanning immediately after startup (default: disabled).

``--tv-scan-period=<0.1-2.0>``
    Specify delay in seconds before switching to next channel (default:
    0.5). Lower values will cause faster scanning, but can detect inactive
    TV channels as active.

``--tv-scan-threshold=<1-100>``
    Threshold value for the signal strength (in percent), as reported by
    the device (default: 50). A signal strength higher than this value will
    indicate that the currently scanning channel is active.


Cache
-----

``--cache=<kBytes|yes|no|auto>``
    Set the size of the cache in kilobytes, disable it with ``no``, or
    automatically enable it if needed with ``auto`` (default: ``auto``).
    With ``auto``, the cache will usually be enabled for network streams,
    using the size set by ``--cache-default``. With ``yes``, the cache will
    always be enabled with the size set by ``--cache-default`` (unless the
    stream can not be cached, or ``--cache-default`` disables caching).

    May be useful when playing files from slow media, but can also have
    negative effects, especially with file formats that require a lot of
    seeking, such as MP4.

    Note that half the cache size will be used to allow fast seeking back. This
    is also the reason why a full cache is usually reported as 50% full. The
    cache fill display does not include the part of the cache reserved for
    seeking back. Likewise, when starting a file the cache will be at 100%,
    because no space is reserved for seeking back yet.

``--cache-default=<kBytes|no>``
    Set the size of the cache in kilobytes (default: 150000 KB). Using ``no``
    will not automatically enable the cache e.g. when playing from a network
    stream. Note that using ``--cache`` will always override this option.

``--cache-initial=<kBytes>``
    Playback will start when the cache has been filled up with this many
    kilobytes of data (default: 0).

``--cache-seek-min=<kBytes>``
    If a seek is to be made to a position within ``<kBytes>`` of the cache
    size from the current position, mpv will wait for the cache to be
    filled to this position rather than performing a stream seek (default:
    500).

    This matters for small forward seeks. With slow streams (especially HTTP
    streams) there is a tradeoff between skipping the data between current
    position and seek destination, or performing an actual seek. Depending
    on the situation, either of these might be slower than the other method.
    This option allows control over this.

``--cache-file=<TMP|path>``
    Create a cache file on the filesystem.

    There are two ways of using this:

    1. Passing a path (a filename). The file will always be overwritten. When
       the general cache is enabled, this file cache will be used to store
       whatever is read from the source stream.

       This will always overwrite the cache file, and you can't use an existing
       cache file to resume playback of a stream. (Technically, mpv wouldn't
       even know which blocks in the file are valid and which not.)

       The resulting file will not necessarily contain all data of the source
       stream. For example, if you seek, the parts that were skipped over are
       never read and consequently are not written to the cache. The skipped over
       parts are filled with zeros. This means that the cache file doesn't
       necessarily correspond to a full download of the source stream.

       Both of these issues could be improved if there is any user interest.

       .. warning:: Causes random corruption when used with ordered chapters or
                    with ``--audio-file``.

    2. Passing the string ``TMP``. This will not be interpreted as filename.
       Instead, an invisible temporary file is created. It depends on your
       C library where this file is created (usually ``/tmp/``), and whether
       filename is visible (the ``tmpfile()`` function is used). On some
       systems, automatic deletion of the cache file might not be guaranteed.

       If you want to use a file cache, this mode is recommended, because it
       doesn't break ordered chapters or ``--audio-file``. These modes open
       multiple cache streams, and using the same file for them obviously
       clashes.

    Also see ``--cache-file-size``.

``--cache-file-size=<kBytes>``
    Maximum size of the file created with ``--cache-file``. For read accesses
    above this size, the cache is simply not used.

    Keep in mind that some use-cases, like playing ordered chapters with cache
    enabled, will actually create multiple cache files, each of which will
    use up to this much disk space.

    (Default: 1048576, 1 GB.)

``--no-cache``
    Turn off input stream caching. See ``--cache``.

``--cache-secs=<seconds>``
    How many seconds of audio/video to prefetch if the cache is active. This
    overrides the ``--demuxer-readahead-secs`` option if and only if the cache
    is enabled and the value is larger. (Default: 10.)

``--cache-pause``, ``--no-cache-pause``
    Whether the player should automatically pause when the cache runs low,
    and unpause once more data is available ("buffering").


Network
-------

``--user-agent=<string>``
    Use ``<string>`` as user agent for HTTP streaming.

``--cookies``, ``--no-cookies``
    Support cookies when making HTTP requests. Disabled by default.

``--cookies-file=<filename>``
    Read HTTP cookies from <filename>. The file is assumed to be in Netscape
    format.

``--http-header-fields=<field1,field2>``
    Set custom HTTP fields when accessing HTTP stream.

    .. admonition:: Example

        ::

            mpv --http-header-fields='Field1: value1','Field2: value2' \
            http://localhost:1234

        Will generate HTTP request::

            GET / HTTP/1.0
            Host: localhost:1234
            User-Agent: MPlayer
            Icy-MetaData: 1
            Field1: value1
            Field2: value2
            Connection: close

``--tls-ca-file=<filename>``
    Certificate authority database file for use with TLS. (Silently fails with
    older FFmpeg or Libav versions.)

``--tls-verify``
    Verify peer certificates when using TLS (e.g. with ``https://...``).
    (Silently fails with older FFmpeg or Libav versions.)

``--referrer=<string>``
    Specify a referrer path or URL for HTTP requests.

``--network-timeout=<seconds>``
    Specify the network timeout in seconds. This affects at least HTTP. The
    special value 0 (default) uses the FFmpeg/Libav defaults. If a protocol
    is used which does not support timeouts, this option is silently ignored.

``--rtsp-transport=<lavf|udp|tcp|http>``
    Select RTSP transport method (default: tcp). This selects the underlying
    network transport when playing ``rtsp://...`` URLs. The value ``lavf``
    leaves the decision to libavformat.

``--hls-bitrate=<no|min|max>``
    If HLS streams are played, this option controls what streams are selected
    by default. The option allows the following parameters:

    :no:        Don't do anything special. Typically, this will simply pick the
                first audio/video streams it can find.
    :min:       Pick the streams with the lowest bitrate.
    :max:       Same, but highest bitrate. (Default.)

    The bitrate as used is sent by the server, and there's no guarantee it's
    actually meaningful.

DVB
---

``--dvbin-card=<1-4>``
    Specifies using card number 1-4 (default: 1).

``--dvbin-file=<filename>``
    Instructs mpv to read the channels list from ``<filename>``. The default is
    in the mpv configuration directory (usually ``~/.config/mpv``) with the
    filename ``channels.conf.{sat,ter,cbl,atsc}`` (based on your card type) or
    ``channels.conf`` as a last resort.
    For DVB-S/2 cards, a VDR 1.7.x format channel list is recommended
    as it allows tuning to DVB-S2 channels, enabling subtitles and
    decoding the PMT (which largely improves the demuxing).
    Classic mplayer format channel lists are still supported (without
    these improvements), and for other card types, only limited VDR
    format channel list support is implemented (patches welcome).
    For channels with dynamic PID switching or incomplete
    ``channels.conf``, ``--dvbin-full-transponder`` or the magic PID
    ``8192`` are recommended.

``--dvbin-timeout=<1-30>``
    Maximum number of seconds to wait when trying to tune a frequency before
    giving up (default: 30).

``--dvbin-full-transponder=<yes|no>``
    Apply no filters on program PIDs, only tune to frequency and pass full
    transponder to demuxer. This is useful to record multiple programs
    on a single transponder, or to work around issues in the ``channels.conf``.
    It is also recommended to use this for channels which switch PIDs
    on-the-fly, e.g. for regional news.

    Default: ``no``

PVR
---

``--pvr-...``
    These options tune various encoding properties of the PVR capture module.
    It has to be used with any hardware MPEG encoder based card supported by
    the V4L2 driver. The Hauppauge WinTV PVR-150/250/350/500 and all IVTV
    based cards are known as PVR capture cards. Be aware that only Linux
    2.6.18 kernel and above is able to handle MPEG stream through V4L2 layer.
    For hardware capture of an MPEG stream and watching it with mpv, use
    ``pvr://`` as media URL.


``--pvr-aspect=<0-3>``
    Specify input aspect ratio:

    :0: 1:1
    :1: 4:3 (default)
    :2: 16:9
    :3: 2.21:1

``--pvr-arate=<32000-48000>``
    Specify encoding audio rate (default: 48000 Hz, available: 32000,
    44100 and 48000 Hz).

``--pvr-alayer=<1-3>``
    Specify MPEG audio layer encoding (default: 2).

``--pvr-abitrate=<32-448>``
    Specify audio encoding bitrate in kbps (default: 384).

``--pvr-amode=<value>``
    Specify audio encoding mode. Available preset values are 'stereo',
    'joint_stereo', 'dual' and 'mono' (default: stereo).

``--pvr-vbitrate=<value>``
    Specify average video bitrate encoding in Mbps (default: 6).

``--pvr-vmode=<value>``
    Specify video encoding mode:

    :vbr: Variable Bit Rate (default)
    :cbr: Constant Bit Rate

``--pvr-vpeak=<value>``
    Specify peak video bitrate encoding in Mbps (only useful for VBR
    encoding, default: 9.6).

``--pvr-fmt=<value>``
    Choose an MPEG format for encoding:

    :ps:    MPEG-2 Program Stream (default)
    :ts:    MPEG-2 Transport Stream
    :mpeg1: MPEG-1 System Stream
    :vcd:   Video CD compatible stream
    :svcd:  Super Video CD compatible stream
    :dvd:   DVD compatible stream


Miscellaneous
-------------

``--display-tags=tag1,tags2,...``
    Set the list of tags that should be displayed on the terminal. Tags that
    are in the list, but are not present in the played file, will not be shown.
    If a value ends with ``*``, all tags are matched by prefix (though there
    is no general globbing). Just passing ``*`` essentially filtering.

    The default includes a common list of tags, call mpv with ``--list-options``
    to see it.

``--mc=<seconds/frame>``
    Maximum A-V sync correction per frame (in seconds)

``--autosync=<factor>``
    Gradually adjusts the A/V sync based on audio delay measurements.
    Specifying ``--autosync=0``, the default, will cause frame timing to be
    based entirely on audio delay measurements. Specifying ``--autosync=1``
    will do the same, but will subtly change the A/V correction algorithm. An
    uneven video framerate in a video which plays fine with ``--no-audio`` can
    often be helped by setting this to an integer value greater than 1. The
    higher the value, the closer the timing will be to ``--no-audio``. Try
    ``--autosync=30`` to smooth out problems with sound drivers which do not
    implement a perfect audio delay measurement. With this value, if large A/V
    sync offsets occur, they will only take about 1 or 2 seconds to settle
    out. This delay in reaction time to sudden A/V offsets should be the only
    side-effect of turning this option on, for all sound drivers.

``--mf-fps=<value>``
    Framerate used when decoding from multiple PNG or JPEG files with ``mf://``
    (default: 1).

``--mf-type=<value>``
    Input file type for ``mf://`` (available: jpeg, png, tga, sgi). By default,
    this is guessed from the file extension.

``--stream-capture=<filename>``
    Allows capturing the primary stream (not additional audio tracks or other
    kind of streams) into the given file. Capturing can also be started and
    stopped by changing the filename with the ``stream-capture`` slave property.
    Generally this will not produce usable results for anything else than MPEG
    or raw streams, unless capturing includes the file headers and is not
    interrupted. Note that, due to cache latencies, captured data may begin and
    end somewhat delayed compared to what you see displayed.

    The destination file is always appended. (Before mpv 0.8.0, the file was
    overwritten.)

``--stream-dump=<filename>``
    Same as ``--stream-capture``, but do not start playback. Instead, the entire
    file is dumped.

``--stream-lavf-o=opt1=value1,opt2=value2,...``
    Set AVOptions on streams opened with libavformat. Unknown or misspelled
    options are silently ignored. (They are mentioned in the terminal output
    in verbose mode, i.e. ``--v``. In general we can't print errors, because
    other options such as e.g. user agent are not available with all protocols,
    and printing errors for unknown options would end up being too noisy.)

``--priority=<prio>``
    (Windows only.)
    Set process priority for mpv according to the predefined priorities
    available under Windows.

    Possible values of ``<prio>``:
    idle|belownormal|normal|abovenormal|high|realtime

    .. warning:: Using realtime priority can cause system lockup.

``--pts-association-mode=<decode|sort|auto>``
    Select the method used to determine which container packet timestamp
    corresponds to a particular output frame from the video decoder. Normally
    you should not need to change this option.

    :decoder: Use decoder reordering functionality. Unlike in classic MPlayer
              and mplayer2, this includes a dTS fallback. (Default.)
    :sort:    Maintain a buffer of unused pts values and use the lowest value
              for the frame.
    :auto:    Try to pick a working mode from the ones above automatically.

    You can also try to use ``--no-correct-pts`` for files with completely
    broken timestamps.

``--media-title=<string>``
    Force the contents of the ``media-title`` property to this value. Useful
    for scripts which want to set a title, without overriding the user's
    setting in ``--title``.

``--slave-broken``
    Switches on the old slave mode. This is for testing only, and incompatible
    to the removed ``--slave`` switch.

    .. attention::
        Changes incompatible to slave mode applications have been made. In
        particular, the status line output was changed, which is used by some
        applications to determine the current playback position. This switch
        has been renamed to prevent these applications from working with this
        version of mpv, because it would lead to buggy and confusing behavior
        only. Moreover, the slave mode protocol is so horribly bad that it
        should not be used for new programs, nor should existing programs
        attempt to adapt to the changed output and use the ``--slave-broken``
        switch. Instead, a new, saner protocol should be developed (and will be,
        if there is enough interest).

        This affects most third-party GUI frontends.
