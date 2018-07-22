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

        - ``mpv dvd://1 --alang=hu,en`` chooses the Hungarian language track
          on a DVD and falls back on English if Hungarian is not available.
        - ``mpv --alang=jpn example.mkv`` plays a Matroska file with Japanese
          audio.

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

``--vlang=<...>``
    Equivalent to ``--alang`` and ``--slang``, for video tracks.

``--aid=<ID|auto|no>``
    Select audio track. ``auto`` selects the default, ``no`` disables audio.
    See also ``--alang``. mpv normally prints available audio tracks on the
    terminal when starting playback of a file.

    ``--audio`` is an alias for ``--aid``.

    ``--aid=no`` or ``--audio=no`` or ``--no-audio`` disables audio playback.
    (The latter variant does not work with the client API.)

``--sid=<ID|auto|no>``
    Display the subtitle stream specified by ``<ID>``. ``auto`` selects
    the default, ``no`` disables subtitles.

    ``--sub`` is an alias for ``--sid``.

    ``--sid=no`` or ``--sub=no`` or ``--no-sub`` disables subtitle decoding.
    (The latter variant does not work with the client API.)

``--vid=<ID|auto|no>``
    Select video channel. ``auto`` selects the default, ``no`` disables video.

    ``--video`` is an alias for ``--vid``.

    ``--vid=no`` or ``--video=no`` or ``--no-video`` disables video playback.
    (The latter variant does not work with the client API.)

    If video is disabled, mpv will try to download the audio only if media is
    streamed with youtube-dl, because it saves bandwidth. This is done by
    setting the ytdl_format to "bestaudio/best" in the ytdl_hook.lua script.

``--edition=<ID|auto>``
    (Matroska files only)
    Specify the edition (set of chapters) to use, where 0 is the first. If set
    to ``auto`` (the default), mpv will choose the first edition declared as a
    default, or if there is no default, the first edition defined.

``--track-auto-selection=<yes|no>``
    Enable the default track auto-selection (default: yes). Enabling this will
    make the player select streams according to ``--aid``, ``--alang``, and
    others. If it is disabled, no tracks are selected. In addition, the player
    will not exit if no tracks are selected, and wait instead (this wait mode
    is similar to pausing, but the pause option is not set).

    This is useful with ``--lavfi-complex``: you can start playback in this
    mode, and then set select tracks at runtime by setting the filter graph.
    Note that if ``--lavfi-complex`` is set before playback is started, the
    referenced tracks are always selected.


Playback Control
----------------

``--start=<relative time>``
    Seek to given time position.

    The general format for absolute times is ``[[hh:]mm:]ss[.ms]``. If the time
    is given with a prefix of ``+`` or ``-``, the seek is relative from the start
    or end of the file. (Since mpv 0.14, the start of the file is always
    considered 0.)

    ``pp%`` seeks to percent position pp (0-100).

    ``#c`` seeks to chapter number c. (Chapters start from 1.)

    ``none`` resets any previously set option (useful for libmpv).

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

``--end=<relative time>``
    Stop at given time. Use ``--length`` if the time should be relative
    to ``--start``. See ``--start`` for valid option values and examples.

``--length=<relative time>``
    Stop after a given time relative to the start time.
    See ``--start`` for valid option values and examples.

    If both ``--end`` and ``--length`` are provided, playback will stop when it
    reaches either of the two endpoints.

``--rebase-start-time=<yes|no>``
    Whether to move the file start time to ``00:00:00`` (default: yes). This
    is less awkward for files which start at a random timestamp, such as
    transport streams. On the other hand, if there are timestamp resets, the
    resulting behavior can be rather weird. For this reason, and in case you
    are actually interested in the real timestamps, this behavior can be
    disabled with ``no``.

``--speed=<0.01-100>``
    Slow down or speed up playback by the factor given as parameter.

    If ``--audio-pitch-correction`` (on by default) is used, playing with a
    speed higher than normal automatically inserts the ``scaletempo`` audio
    filter.

``--pause``
    Start the player in paused state.

``--shuffle``
    Play files in random order.

``--chapter=<start[-end]>``
    Specify which chapter to start playing at. Optionally specify which
    chapter to end playing at.

    See also: ``--start``.

``--playlist-start=<auto|index>``
    Set which file on the internal playlist to start playback with. The index
    is an integer, with 0 meaning the first file. The value ``auto`` means that
    the selection of the entry to play is left to the playback resume mechanism
    (default). If an entry with the given index doesn't exist, the behavior is
    unspecified and might change in future mpv versions. The same applies if
    the playlist contains further playlists (don't expect any reasonable
    behavior). Passing a playlist file to mpv should work with this option,
    though. E.g. ``mpv playlist.m3u --playlist-start=123`` will work as expected,
    as long as ``playlist.m3u`` does not link to further playlists.

    The value ``no`` is a deprecated alias for ``auto``.

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
    default in the definition of key bindings and in input commands.

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

``--access-references=<yes|no>``
    Follow any references in the file being opened (default: yes). Disabling
    this is helpful if the file is automatically scanned (e.g. thumbnail
    generation). If the thumbnail scanner for example encounters a playlist
    file, which contains network URLs, and the scanner should not open these,
    enabling this option will prevent it. This option also disables ordered
    chapters, mov reference files, opening of archives, and a number of other
    features.

    On older FFmpeg versions, this will not work in some cases. Some FFmpeg
    demuxers might not respect this option.

    This option does not prevent opening of paired subtitle files and such. Use
    ``--autoload-files=no`` to prevent this.

    This option does not always work if you open non-files (for example using
    ``dvd://directory`` would open a whole bunch of files in the given
    directory). Prefixing the filename with ``./`` if it doesn't start with
    a ``/`` will avoid this.

``--loop-playlist=<N|inf|force|no>``, ``--loop-playlist``
    Loops playback ``N`` times. A value of ``1`` plays it one time (default),
    ``2`` two times, etc. ``inf`` means forever. ``no`` is the same as ``1`` and
    disables looping. If several files are specified on command line, the
    entire playlist is looped. ``--loop-playlist`` is the same as
    ``--loop-playlist=inf``.

    The ``force`` mode is like ``inf``, but does not skip playlist entries
    which have been marked as failing. This means the player might waste CPU
    time trying to loop a file that doesn't exist. But it might be useful for
    playing webradios under very bad network conditions.

``--loop-file=<N|inf|no>``, ``--loop=<N|inf|no>``
    Loop a single file N times. ``inf`` means forever, ``no`` means normal
    playback. For compatibility, ``--loop-file`` and ``--loop-file=yes`` are
    also accepted, and are the same as ``--loop-file=inf``.

    The difference to ``--loop-playlist`` is that this doesn't loop the playlist,
    just the file itself. If the playlist contains only a single file, the
    difference between the two option is that this option performs a seek on
    loop, instead of reloading the file.

    ``--loop`` is an alias for this option.

``--ab-loop-a=<time>``, ``--ab-loop-b=<time>``
    Set loop points. If playback passes the ``b`` timestamp, it will seek to
    the ``a`` timestamp. Seeking past the ``b`` point doesn't loop (this is
    intentional).

    If both options are set to ``no`` or unset, looping is disabled.
    Otherwise, the start/end of playback is used if one of the options
    is set to ``no`` or unset.

    The loop-points can be adjusted at runtime with the corresponding
    properties. See also ``ab-loop`` command.

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

    This accepts a media file (like mkv) or even a pseudo-format like ffmetadata
    and uses its chapters to replace the current file's chapters. This doesn't
    work with OGM or XML chapters directly.

``--sstep=<sec>``
    Skip <sec> seconds after every frame.

    .. note::

        Without ``--hr-seek``, skipping will snap to keyframes.

``--stop-playback-on-init-failure=<yes|no>``
    Stop playback if either audio or video fails to initialize (default: no).
    With ``no``, playback will continue in video-only or audio-only mode if one
    of them fails. This doesn't affect playback of audio-only or video-only
    files.

Program Behavior
----------------

``--help``, ``--h``
    Show short summary of options.

    You can also pass a string to this option, which will list all top-level
    options which contain the string in the name, e.g. ``--h=scale`` for all
    options that contain the word ``scale``. The special string ``*`` lists
    all top-level options.

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

    See also: ``--config-dir``.

``--list-options``
    Prints all available options.

``--list-properties``
    Print a list of the available properties.

``--list-protocols``
    Print a list of the supported protocols.

``--log-file=<path>``
    Opens the given path for writing, and print log messages to it. Existing
    files will be truncated. The log level is at least ``-v -v``, but
    can be raised via ``--msg-level`` (the option cannot lower it below the
    forced minimum log level).

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

``--watch-later-directory=<path>``

    The directory in which to store the "watch later" temporary files.

    The default is a subdirectory named "watch_later" underneath the
    config directory (usually ``~/.config/mpv/``).

``--dump-stats=<filename>``
    Write certain statistics to the given file. The file is truncated on
    opening. The file will contain raw samples, each with a timestamp. To
    make this file into a readable, the script ``TOOLS/stats-conv.py`` can be
    used (which currently displays it as a graph).

    This option is useful for debugging only.

``--idle=<no|yes|once>``
    Makes mpv wait idly instead of quitting when there is no file to play.
    Mostly useful in input mode, where mpv can be controlled through input
    commands. (Default: ``no``)

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
    file. This uses timeline/EDL support internally.

``--no-resume-playback``
    Do not restore playback position from the ``watch_later`` configuration
    subdirectory (usually ``~/.config/mpv/watch_later/``).
    See ``quit-watch-later`` input command.

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
    (Default: disabled)

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
    version of youtube-dl to be installed on the system. (Enabled by default.)

    If the script can't do anything with an URL, it will do nothing.

    The ``try_ytdl_first`` script option accepts a boolean 'yes' or 'no', and if
    'yes' will try parsing the URL with youtube-dl first, instead of the default
    where it's only after mpv failed to open it. This mostly depends on whether
    most of your URLs need youtube-dl parsing.

    The ``exclude`` script option accepts a ``|``-separated list of URL patterns
    which mpv should not use with youtube-dl. The patterns are matched after
    the ``http(s)://`` part of the URL.

    ``^`` matches the beginning of the URL, ``$`` matches its end, and you
    should use ``%`` before any of the characters ``^$()%|,.[]*+-?`` to match
    that character.

    .. admonition:: Examples

        - ``--script-opts=ytdl_hook-exclude='^youtube%.com'``
          will exclude any URL that starts with ``http://youtube.com`` or
          ``https://youtube.com``.
        - ``--script-opts=ytdl_hook-exclude='%.mkv$|%.mp4$'``
          will exclude any URL that ends with ``.mkv`` or ``.mp4``.

    See more lua patterns here: https://www.lua.org/manual/5.1/manual.html#5.4.1

    The ``use_manifests`` script option makes mpv use the master manifest URL for
    formats like HLS and DASH, if available, allowing for video/audio selection
    in runtime. It's disabled ("no") by default for performance reasons.

``--ytdl-format=<best|worst|mp4|webm|...>``
    Video format/quality that is directly passed to youtube-dl. The possible
    values are specific to the website and the video, for a given url the
    available formats can be found with the command
    ``youtube-dl --list-formats URL``. See youtube-dl's documentation for
    available aliases.
    (Default: youtube-dl's default, currently ``bestvideo+bestaudio/best``)

``--ytdl-raw-options=<key>=<value>[,<key>=<value>[,...]]``
    Pass arbitrary options to youtube-dl. Parameter and argument should be
    passed as a key-value pair. Options without argument must include ``=``.

    There is no sanity checking so it's possible to break things (i.e.
    passing invalid parameters to youtube-dl).

    A proxy URL can be passed for youtube-dl to use it in parsing the website.
    This is useful for geo-restricted URLs. After youtube-dl parsing, some
    URLs also require a proxy for playback, so this can pass that proxy
    information to mpv. Take note that SOCKS proxies aren't supported and
    https URLs also bypass the proxy. This is a limitation in FFmpeg.

    .. admonition:: Example

        - ``--ytdl-raw-options=username=user,password=pass``
        - ``--ytdl-raw-options=force-ipv6=``
        - ``--ytdl-raw-options=proxy=[http://127.0.0.1:3128]``
        - ``--ytdl-raw-options-append=proxy=http://127.0.0.1:3128``

``--load-stats-overlay=<yes|no>``
    Enable the builtin script that shows useful playback information on a key
    binding (default: yes). By default, the ``i`` key is used (``I`` to make
    the overlay permanent).

``--player-operation-mode=<cplayer|pseudo-gui>``
    For enabling "pseudo GUI mode", which means that the defaults for some
    options are changed. This option should not normally be used directly, but
    only by mpv internally, or mpv-provided scripts, config files, or .desktop
    files.

Video
-----

``--vo=<driver>``
    Specify the video output backend to be used. See `VIDEO OUTPUT DRIVERS`_ for
    details and descriptions of available drivers.

``--vd=<...>``
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
        the terminal status line as ``Dropped:`` field. If the decoder is too slow,
        in theory all frames would have to be dropped (because all frames are
        too late) - to avoid this, frame dropping stops if the effective
        framerate is below 10 FPS.
    <decoder>
        Old, decoder-based framedrop mode. (This is the same as ``--framedrop=yes``
        in mpv 0.5.x and before.) This tells the decoder to skip frames (unless
        they are needed to decode future frames). May help with slow systems,
        but can produce unwatchable choppy output, or even freeze the display
        completely. Not recommended.
        The ``--vd-lavc-framedrop`` option controls what frames to drop.
    <decoder+vo>
        Enable both modes. Not recommended.

    .. note::

        ``--vo=vdpau`` has its own code for the ``vo`` framedrop mode. Slight
        differences to other VOs are possible.

``--video-latency-hacks=<yes|no>``
    Enable some things which tend to reduce video latency by 1 or 2 frames
    (default: no). Note that this option might be removed without notice once
    the player's timing code does not inherently need to do these things
    anymore.

    This does:

    - Use the demuxer reported FPS for frame dropping. This avoids that the
      player needs to decode 1 frame in advance, lowering total latency in
      effect. This also means that if the demuxer reported FPS is wrong, or
      the video filter chain changes FPS (e.g. deinterlacing), then it could
      drop too many or not enough frames.
    - Disable waiting for the first video frame. Normally the player waits for
      the first video frame to be fully rendered before starting playback
      properly. Some VOs will lazily initialize stuff when rendering the first
      frame, so if this is not done, there is some likeliness that the VO has
      to drop some frames if rendering the first frame takes longer than needed.


``--display-fps=<fps>``
    Set the display FPS used with the ``--video-sync=display-*`` modes. By
    default, a detected value is used. Keep in mind that setting an incorrect
    value (even if slightly incorrect) can ruin video playback. On multi-monitor
    systems, there is a chance that the detected value is from the wrong
    monitor.

    Set this option only if you have reason to believe the automatically
    determined value is wrong.

``--hwdec=<api>``
    Specify the hardware video decoding API that should be used if possible.
    Whether hardware decoding is actually done depends on the video codec. If
    hardware decoding is not possible, mpv will fall back on software decoding.

    ``<api>`` can be one of the following:

    :no:        always use software decoding (default)
    :auto:      enable best hw decoder (see below)
    :yes:       exactly the same as ``auto``
    :auto-copy: enable best hw decoder with copy-back (see below)
    :vdpau:     requires ``--vo=gpu`` or ``--vo=vdpau`` (Linux only)
    :vdpau-copy: copies video back into system RAM (Linux with some GPUs only)
    :vaapi:     requires ``--vo=gpu`` or ``--vo=vaapi`` (Linux only)
    :vaapi-copy: copies video back into system RAM (Linux with some GPUs only)
    :videotoolbox: requires ``--vo=gpu`` (OS X 10.8 and up),
                   or ``--vo=opengl-cb`` (iOS 9.0 and up)
    :videotoolbox-copy: copies video back into system RAM (OS X 10.8 or iOS 9.0 and up)
    :dxva2:     requires ``--vo=gpu`` with ``--gpu-context=d3d11``,
                ``--gpu-context=angle`` or ``--gpu-context=dxinterop``
                (Windows only)
    :dxva2-copy: copies video back to system RAM (Windows only)
    :d3d11va:   requires ``--vo=gpu`` with ``--gpu-context=d3d11`` or
                ``--gpu-context=angle`` (Windows 8+ only)
    :d3d11va-copy: copies video back to system RAM (Windows 8+ only)
    :mediacodec: requires ``--vo=mediacodec_embed`` (Android only)
    :mediacodec-copy: copies video back to system RAM (Android only)
    :mmal:      requires ``--vo=gpu`` (Raspberry Pi only - default if available)
    :mmal-copy: copies video back to system RAM (Raspberry Pi only)
    :cuda:      requires ``--vo=gpu`` (Any platform CUDA is available)
    :cuda-copy: copies video back to system RAM (Any platform CUDA is available)
    :nvdec:     requires ``--vo=gpu`` (Any platform CUDA is available)
    :nvdec-copy: copies video back to system RAM (Any platform CUDA is available)
    :crystalhd: copies video back to system RAM (Any platform supported by hardware)
    :rkmpp:     requires ``--vo=gpu`` (some RockChip devices only)

    ``auto`` tries to automatically enable hardware decoding using the first
    available method. This still depends what VO you are using. For example,
    if you are not using ``--vo=gpu`` or ``--vo=vdpau``, vdpau decoding will
    never be enabled. Also note that if the first found method doesn't actually
    work, it will always fall back to software decoding, instead of trying the
    next method (might matter on some Linux systems).

    ``auto-copy`` selects only modes that copy the video data back to system
    memory after decoding. This selects modes like ``vaapi-copy`` (and so on).
    If none of these work, hardware decoding is disabled. This mode is always
    guaranteed to incur no additional loss compared to software decoding, and
    will allow CPU processing with video filters.

    The ``vaapi`` mode, if used with ``--vo=gpu``, requires Mesa 11 and most
    likely works with Intel GPUs only. It also requires the opengl EGL backend.

    The ``cuda`` and ``cuda-copy`` modes provides deinterlacing in the decoder
    which is useful as there is no other deinterlacing mechanism in the opengl
    output path. To use this deinterlacing you must pass the option:
    ``vd-lavc-o=deint=[weave|bob|adaptive]``.
    Pass ``weave`` (or leave the option unset) to not attempt any
    deinterlacing. ``cuda`` should always be preferred unless the ``gpu``
    vo is not being used or filters are required.

    ``nvdec`` is a newer implementation of CUVID/CUDA decoding, which uses the
    FFmpeg decoders for file parsing. Experimental, is known not to correctly
    check whether decoding is supported by the hardware at all. Deinterlacing
    is not supported. Since this uses FFmpeg's codec parsers, it is expected
    that this generally causes fewer issues than ``cuda``.

    Most video filters will not work with hardware decoding as they are
    primarily implemented on the CPU. Some exceptions are ``vdpaupp``,
    ``vdpaurb`` and ``vavpp``. See `VIDEO FILTERS`_ for more details.

    The ``...-copy`` modes (e.g. ``dxva2-copy``) allow you to use hardware
    decoding with any VO, backend or filter. Because these copy the decoded
    video back to system RAM, they're likely less efficient than the direct
    modes (like e.g. ``dxva2``), and probably not more efficient than software
    decoding except for some codecs (e.g. HEVC).

    .. note::

        When using this switch, hardware decoding is still only done for some
        codecs. See ``--hwdec-codecs`` to enable hardware decoding for more
        codecs.

    .. admonition:: Quality reduction with hardware decoding

        In theory, hardware decoding does not reduce video quality (at least
        for the codecs h264 and HEVC). However, due to restrictions in video
        output APIs, as well as bugs in the actual hardware decoders, there can
        be some loss, or even blatantly incorrect results.

        In some cases, RGB conversion is forced, which means the RGB conversion
        is performed by the hardware decoding API, instead of the shaders
        used by ``--vo=gpu``. This means certain colorspaces may not display
        correctly, and certain filtering (such as debanding) cannot be applied
        in an ideal way. This will also usually force the use of low quality
        chroma scalers instead of the one specified by ``--cscale``. In other
        cases, hardware decoding can also reduce the bit depth of the decoded
        image, which can introduce banding or precision loss for 10-bit files.

        ``vdpau`` is usually safe. If deinterlacing enabled (or the ``vdpaupp``
        video filter is active in general), it forces RGB conversion. The latter
        currently does not treat certain colorspaces like BT.2020 correctly
        (which is mostly a mpv-specific restriction). The ``vdpauprb`` video
        filter retrieves image data without RGB conversion and is safe (but
        precludes use of vdpau postprocessing).

        ``vaapi`` is safe if the ``vaapi-egl`` backend is indicated in the
        logs. If ``vaapi-glx`` is indicated, and the video colorspace is either
        BT.601 or BT.709, a forced, low-quality but correct RGB conversion is
        performed. Otherwise, the result will be totally incorrect.

        ``d3d11va`` is safe when used with the ``d3d11`` backend. If used with
        ``angle`` is it usually safe, except that 10 bit input (HEVC main 10
        profiles) will be rounded down to 8 bits, which will result in reduced
        quality. Also note that with very old ANGLE builds (without
        ``EGL_KHR_stream path``,) all input will be converted to RGB.

        ``dxva2`` is not safe. It appears to always use BT.601 for forced RGB
        conversion, but actual behavior depends on the GPU drivers. Some drivers
        appear to convert to limited range RGB, which gives a faded appearance.
        In addition to driver-specific behavior, global system settings might
        affect this additionally. This can give incorrect results even with
        completely ordinary video sources.

        ``rpi`` always uses the hardware overlay renderer, even with
        ``--vo=gpu``.

        ``cuda`` should be safe, but it has been reported to corrupt the
        timestamps causing glitched, flashing frames on some files. It can also
        sometimes cause massive framedrops for unknown reasons. Caution is
        advised.

        ``crystalhd`` is not safe. It always converts to 4:2:2 YUV, which
        may be lossy, depending on how chroma sub-sampling is done during
        conversion. It also discards the top left pixel of each frame for
        some reason.

        All other methods, in particular the copy-back methods (like
        ``dxva2-copy`` etc.) should hopefully be safe, although they can still
        cause random decoding issues. At the very least, they shouldn't affect
        the colors of the image.

        In particular, ``auto-copy`` will only select "safe" modes
        (although potentially slower than other methods), but there's still no
        guarantee the chosen hardware decoder will actually work correctly.

        In general, it's very strongly advised to avoid hardware decoding
        unless **absolutely** necessary, i.e. if your CPU is insufficient to
        decode the file in questions. If you run into any weird decoding issues,
        frame glitches or discoloration, and you have ``--hwdec`` turned on,
        the first thing you should try is disabling it.

``--gpu-hwdec-interop=<auto|all|no|name>``
    This option is for troubleshooting hwdec interop issues. Since it's a
    debugging option, its semantics may change at any time.

    This is useful for the ``gpu`` and ``opengl-cb`` VOs for selecting which
    hwdec interop context to use exactly. Effectively it also can be used
    to block loading of certain backends.

    If set to ``auto`` (default), the behavior depends on the VO: for ``gpu``,
    it does nothing, and the interop context is loaded on demand (when the
    decoder probes for ``--hwdec`` support). For ``opengl-cb``, which has
    has no on-demand loading, this is equivalent to ``all``.

    The empty string is equivalent to ``auto``.

    If set to ``all``, it attempts to load all interop contexts at GL context
    creation time.

    Other than that, a specific backend can be set, and the list of them can
    be queried with ``help`` (mpv CLI only).

    Runtime changes to this are ignored (the current option value is used
    whenever the renderer is created).

    The old aliases ``--opengl-hwdec-interop`` and ``--hwdec-preload`` are
    barely related to this anymore, but will be somewhat compatible in some
    cases.

``--hwdec-image-format=<name>``
    Set the internal pixel format used by hardware decoding via ``--hwdec``
    (default ``no``). The special value ``no`` selects an implementation
    specific standard format. Most decoder implementations support only one
    format, and will fail to initialize if the format is not supported.

    Some implementations might support multiple formats. In particular,
    videotoolbox is known to require ``uyvy422`` for good performance on some
    older hardware. d3d11va can always use ``yuv420p``, which uses an opaque
    format, with likely no advantages.

``--cuda-decode-device=<auto|0..>``
    Choose the GPU device used for decoding when using the ``cuda`` hwdec.

    By default, the device that is being used to provide OpenGL output will
    also be used for decoding (and in the vast majority of cases, only one
    GPU will be present).

    Note that when using the ``cuda-copy`` hwdec, a different option must be
    passed: ``--vd-lavc-o=gpu=<0..>``.

``--vaapi-device=<device file>``
    Choose the DRM device for ``vaapi-copy``. This should be the path to a
    DRM device file. (Default: ``/dev/dri/renderD128``)

``--panscan=<0.0-1.0>``
    Enables pan-and-scan functionality (cropping the sides of e.g. a 16:9
    video to make it fit a 4:3 display without black bands). The range
    controls how much of the image is cropped. May not work with all video
    output drivers.

    This option has no effect if ``--video-unscaled`` option is used.

``--video-aspect=<ratio|no>``
    Override video aspect ratio, in case aspect information is incorrect or
    missing in the file being played. See also ``--no-video-aspect``.

    These values have special meaning:

    :0:  disable aspect ratio handling, pretend the video has square pixels
    :no: same as ``0``
    :-1: use the video stream or container aspect (default)

    But note that handling of these special values might change in the future.

    .. admonition:: Examples

        - ``--video-aspect=4:3``  or ``--video-aspect=1.3333``
        - ``--video-aspect=16:9`` or ``--video-aspect=1.7777``
        - ``--no-video-aspect`` or ``--video-aspect=no``

``--video-aspect-method=<bitstream|container>``
    This sets the default video aspect determination method (if the aspect is
    _not_ overridden by the user with ``--video-aspect`` or others).

    :container: Strictly prefer the container aspect ratio. This is apparently
                the default behavior with VLC, at least with Matroska. Note that
                if the container has no aspect ratio set, the behavior is the
                same as with bitstream.
    :bitstream: Strictly prefer the bitstream aspect ratio, unless the bitstream
                aspect ratio is not set. This is apparently the default behavior
                with XBMC/kodi, at least with Matroska.

    The current default for mpv is ``container``.

    Normally you should not set this. Try the various choices if you encounter
    video that has the wrong aspect ratio in mpv, but seems to be correct in
    other players.

``--video-unscaled=<no|yes|downscale-big>``
    Disable scaling of the video. If the window is larger than the video,
    black bars are added. Otherwise, the video is cropped, unless the option
    is set to ``downscale-big``, in which case the video is fit to window. The
    video still can be influenced by the other ``--video-...`` options. This
    option disables the effect of ``--panscan``.

    Note that the scaler algorithm may still be used, even if the video isn't
    scaled. For example, this can influence chroma conversion. The video will
    also still be scaled in one dimension if the source uses non-square pixels
    (e.g. anamorphic widescreen DVDs).

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

``--video-rotate=<0-359|no>``
    Rotate the video clockwise, in degrees. Currently supports 90° steps only.
    If ``no`` is given, the video is never rotated, even if the file has
    rotation metadata. (The rotation value is added to the rotation metadata,
    which means the value ``0`` would rotate the video according to the
    rotation metadata.)

``--video-stereo-mode=<no|mode>``
    Set the stereo 3D output mode (default: ``mono``). This is mostly broken and
    thus deprecated.

    The pseudo-mode ``no`` disables automatic conversion completely.

    The mode ``mono`` is an alias to ``ml``, which refers to the left frame in
    2D. This is the default, which means mpv will try to show 3D movies in 2D,
    instead of the mangled 3D image not intended for consumption (such as
    showing the left and right frame side by side, etc.).

    Use ``--video-stereo-mode=help`` to list all available modes. Check with
    the ``stereo3d`` filter documentation to see what the names mean. Note that
    some names refer to modes not supported by ``stereo3d`` - these modes can
    appear in files, but can't be handled properly by mpv.

``--video-zoom=<value>``
    Adjust the video display scale factor by the given value. The parameter is
    given log 2. For example, ``--video-zoom=0`` is unscaled,
    ``--video-zoom=1`` is twice the size, ``--video-zoom=-2`` is one fourth of
    the size, and so on.

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
    filters, subtitle rendering, seeking (including hr-seeks and backstepping),
    and audio synchronization can be completely broken in this mode.

``--fps=<float>``
    Override video framerate. Useful if the original value is wrong or missing.

    .. note::

        Works in ``--no-correct-pts`` mode only.

``--deinterlace=<yes|no>``
    Enable or disable interlacing (default: no).
    Interlaced video shows ugly comb-like artifacts, which are visible on
    fast movement. Enabling this typically inserts the yadif video filter in
    order to deinterlace the video, or lets the video output apply deinterlacing
    if supported.

    This behaves exactly like the ``deinterlace`` input property (usually
    mapped to ``d``).

    Keep in mind that this **will** conflict with manually inserted
    deinterlacing filters, unless you take care. (Since mpv 0.27.0, even the
    hardware deinterlace filters will conflict. Also since that version,
    ``--deinterlace=auto`` was removed, which used to mean that the default
    interlacing option of possibly inserted video filters was used.)

    Note that this will make video look worse if it's not actually interlaced.

``--frames=<number>``
    Play/convert only first ``<number>`` video frames, then quit.

    ``--frames=0`` loads the file, but immediately quits before initializing
    playback. (Might be useful for scripts which just want to determine some
    file properties.)

    For audio-only playback, any value greater than 0 will quit playback
    immediately after initialization. The value 0 works as with video.

``--video-output-levels=<outputlevels>``
    RGB color levels used with YUV to RGB conversion. Normally, output devices
    such as PC monitors use full range color levels. However, some TVs and
    video monitors expect studio RGB levels. Providing full range output to a
    device expecting studio level input results in crushed blacks and whites,
    the reverse in dim gray blacks and dim whites.

    Not all VOs support this option. Some will silently ignore it.

    Available color ranges are:

    :auto:      automatic selection (equals to full range) (default)
    :limited:   limited range (16-235 per component), studio levels
    :full:      full range (0-255 per component), PC levels

    .. note::

        It is advisable to use your graphics driver's color range option
        instead, if available.

``--hwdec-codecs=<codec1,codec2,...|all>``
    Allow hardware decoding for a given list of codecs only. The special value
    ``all`` always allows all codecs.

    You can get the list of allowed codecs with ``mpv --vd=help``. Remove the
    prefix, e.g. instead of ``lavc:h264`` use ``h264``.

    By default, this is set to ``h264,vc1,wmv3,hevc,mpeg2video,vp9``. Note that
    the hardware acceleration special codecs like ``h264_vdpau`` are not
    relevant anymore, and in fact have been removed from Libav in this form.

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

``--vd-lavc-software-fallback=<yes|no|N>``
    Fallback to software decoding if the hardware-accelerated decoder fails
    (default: 3). If this is a number, then fallback will be triggered if
    N frames fail to decode in a row. 1 is equivalent to ``yes``.

``--vd-lavc-dr=<yes|no>``
    Enable direct rendering (default: yes). If this is set to ``yes``, the
    video will be decoded directly to GPU video memory (or staging buffers).
    This can speed up video upload, and may help with large resolutions or
    slow hardware. This works only with the following VOs:

        - ``gpu``: requires at least OpenGL 4.4 or Vulkan.

    (In particular, this can't be made work with ``opengl-cb``, but the libmpv
    render API has optional support.)

    Using video filters of any kind that write to the image data (or output
    newly allocated frames) will silently disable the DR code path.

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

        ``--vd-lavc-o=debug=pict``

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

``--vd-lavc-threads=<N>``
    Number of threads to use for decoding. Whether threading is actually
    supported depends on codec (default: 0). 0 means autodetect number of cores
    on the machine and use that, up to the maximum of 16. You can set more than
    16 threads manually.

``--vd-lavc-assume-old-x264=<yes|no>``
    Assume the video was encoded by an old, buggy x264 version (default: no).
    Normally, this is autodetected by libavcodec. But if the bitstream contains
    no x264 version info (or it was somehow skipped), and the stream was in fact
    encoded by an old x264 version (build 150 or earlier), and if the stream
    uses ``4:4:4`` chroma, then libavcodec will by default show corrupted video.
    This option sets the libavcodec ``x264_build`` option to ``150``, which
    means that if the stream contains no version info, or was not encoded by
    x264 at all, it assumes it was encoded by the old version. Enabling this
    option is pretty safe if you want your broken files to work, but in theory
    this can break on streams not encoded by x264, or if a stream encoded by a
    newer x264 version contains no version info.


Audio
-----

``--audio-pitch-correction=<yes|no>``
    If this is enabled (default), playing with a speed different from normal
    automatically inserts the ``scaletempo`` audio filter. For details, see
    audio filter section.

``--audio-device=<name>``
    Use the given audio device. This consists of the audio output name, e.g.
    ``alsa``, followed by ``/``, followed by the audio output specific device
    name. The default value for this option is ``auto``, which tries every audio
    output in preference order with the default device.

    You can list audio devices with ``--audio-device=help``. This outputs the
    device name in quotes, followed by a description. The device name is what
    you have to pass to the ``--audio-device`` option. The list of audio devices
    can be retrieved by API by using the ``audio-device-list`` property.

    While the option normally takes one of the strings as indicated by the
    methods above, you can also force the device for most AOs by building it
    manually. For example ``name/foobar`` forces the AO ``name`` to use the
    device ``foobar``. However, the ``--ao`` option will strictly force a
    specific AO. To avoid confusion, don't use ``--ao`` and ``--audio-device``
    together.

    .. admonition:: Example for ALSA

        MPlayer and mplayer2 required you to replace any ',' with '.' and
        any ':' with '=' in the ALSA device name. For example, to use the
        device named ``dmix:default``, you had to do:

            ``-ao alsa:device=dmix=default``

        In mpv you could instead use:

            ``--audio-device=alsa/dmix:default``


``--audio-exclusive=<yes|no>``
    Enable exclusive output mode. In this mode, the system is usually locked
    out, and only mpv will be able to output audio.

    This only works for some audio outputs, such as ``wasapi`` and
    ``coreaudio``. Other audio outputs silently ignore this options. They either
    have no concept of exclusive mode, or the mpv side of the implementation is
    missing.

``--audio-fallback-to-null=<yes|no>``
    If no audio device can be opened, behave as if ``--ao=null`` was given. This
    is useful in combination with ``--audio-device``: instead of causing an
    error if the selected device does not exist, the client API user (or a
    Lua script) could let playback continue normally, and check the
    ``current-ao`` and ``audio-device-list`` properties to make high-level
    decisions about how to continue.

``--ao=<driver>``
    Specify the audio output drivers to be used. See `AUDIO OUTPUT DRIVERS`_ for
    details and descriptions of available drivers.

``--af=<filter1[=parameter1:parameter2:...],filter2,...>``
    Specify a list of audio filters to apply to the audio stream. See
    `AUDIO FILTERS`_ for details and descriptions of the available filters.
    The option variants ``--af-add``, ``--af-pre``, ``--af-del`` and
    ``--af-clr`` exist to modify a previously specified list, but you
    should not need these for typical use.

``--audio-spdif=<codecs>``
    List of codecs for which compressed audio passthrough should be used. This
    works for both classic S/PDIF and HDMI.

    Possible codecs are ``ac3``, ``dts``, ``dts-hd``, ``eac3``, ``truehd``.
    Multiple codecs can be specified by separating them with ``,``. ``dts``
    refers to low bitrate DTS core, while ``dts-hd`` refers to DTS MA (receiver
    and OS support varies). If both ``dts`` and ``dts-hd`` are specified, it
    behaves equivalent to specifying ``dts-hd`` only.

    In earlier mpv versions you could use ``--ad`` to force the spdif wrapper.
    This does not work anymore.

    .. admonition:: Warning

        There is not much reason to use this. HDMI supports uncompressed
        multichannel PCM, and mpv supports lossless DTS-HD decoding via
        FFmpeg's new DCA decoder (based on libdcadec).

``--ad=<decoder1,decoder2,...[-]>``
    Specify a priority list of audio decoders to be used, according to their
    decoder name. When determining which decoder to use, the first decoder that
    matches the audio format is selected. If that is unavailable, the next
    decoder is used. Finally, it tries all other decoders that are not
    explicitly selected or rejected by the option.

    ``-`` at the end of the list suppresses fallback on other available
    decoders not on the ``--ad`` list. ``+`` in front of an entry forces the
    decoder. Both of these should not normally be used, because they break
    normal decoder auto-selection! Both of these methods are deprecated.

    .. admonition:: Examples

        ``--ad=mp3float``
            Prefer the FFmpeg/Libav ``mp3float`` decoder over all other MP3
            decoders.

        ``--ad=help``
            List all available decoders.

    .. admonition:: Warning

        Enabling compressed audio passthrough (AC3 and DTS via SPDIF/HDMI) with
        this option is not possible. Use ``--audio-spdif`` instead.

``--volume=<value>``
    Set the startup volume. 0 means silence, 100 means no volume reduction or
    amplification. Negative values can be passed for compatibility, but are
    treated as 0.

    Since mpv 0.18.1, this always controls the internal mixer (aka "softvol").

``--replaygain=<no|track|album>``
    Adjust volume gain according to the track-gain or album-gain replaygain
    value stored in the file metadata (default: no replaygain).

``--replaygain-preamp=<db>``
    Pre-amplification gain in dB to apply to the selected replaygain gain
    (default: 0).

``--replaygain-clip=<yes|no>``
    Prevent clipping caused by replaygain by automatically lowering the
    gain (default). Use ``--replaygain-clip=no`` to disable this.

``--replaygain-fallback=<db>``
    Gain in dB to apply if the file has no replay gain tags. This option
    is always applied if the replaygain logic is somehow inactive. If this
    is applied, no other replaygain options are applied.

``--audio-delay=<sec>``
    Audio delay in seconds (positive or negative float value). Positive values
    delay the audio, and negative values delay the video.

``--mute=<yes|no|auto>``
    Set startup audio mute status (default: no).

    ``auto`` is a deprecated possible value that is equivalent to ``no``.

    See also: ``--volume``.

``--softvol=<no|yes|auto>``
    Deprecated/unfunctional. Before mpv 0.18.1, this used to control whether
    to use the volume controls of the audio output driver or the internal mpv
    volume filter.

    The current behavior is that softvol is always enabled, i.e. as if this
    option is set to ``yes``. The other behaviors are not available anymore,
    although ``auto`` almost matches current behavior in most cases.

    The ``no`` behavior is still partially available through the ``ao-volume``
    and ``ao-mute`` properties. But there are no options to reset these.

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
    If DTS is passed through, use DTS-HD.

    .. admonition:: Warning

        This and enabling passthrough via ``--ad`` are deprecated in favor of
        using ``--audio-spdif=dts-hd``.

``--audio-channels=<auto-safe|auto|layouts>``
    Control which audio channels are output (e.g. surround vs. stereo). There
    are the following possibilities:

    - ``--audio-channels=auto-safe``
        Use the system's preferred channel layout. If there is none (such
        as when accessing a hardware device instead of the system mixer),
        force stereo. Some audio outputs might simply accept any layout and
        do downmixing on their own.

        This is the default.
    - ``--audio-channels=auto``
        Send the audio device whatever it accepts, preferring the audio's
        original channel layout. Can cause issues with HDMI (see the warning
        below).
    - ``--audio-channels=layout1,layout2,...``
        List of ``,``-separated channel layouts which should be allowed.
        Technically, this only adjusts the filter chain output to the best
        matching layout in the list, and passes the result to the audio API.
        It's possible that the audio API will select a different channel
        layout.

        Using this mode is recommended for direct hardware output, especially
        over HDMI (see HDMI warning below).
    - ``--audio-channels=stereo``
        Force  a plain stereo downmix. This is a special-case of the previous
        item. (See paragraphs below for implications.)

    If a list of layouts is given, each item can be either an explicit channel
    layout name (like ``5.1``), or a channel number. Channel numbers refer to
    default layouts, e.g. 2 channels refer to stereo, 6 refers to 5.1.

    See ``--audio-channels=help`` output for defined default layouts. This also
    lists speaker names, which can be used to express arbitrary channel
    layouts (e.g. ``fl-fr-lfe`` is 2.1).

    If the list of channel layouts has only 1 item, the decoder is asked to
    produce according output. This sometimes triggers decoder-downmix, which
    might be different from the normal mpv downmix. (Only some decoders support
    remixing audio, like AC-3, AAC or DTS. You can use ``--ad-lavc-downmix=no``
    to make the decoder always output its native layout.) One consequence is
    that ``--audio-channels=stereo`` triggers decoder downmix, while ``auto``
    or ``auto-safe`` never will, even if they end up selecting stereo. This
    happens because the decision whether to use decoder downmix happens long
    before the audio device is opened.

    If the channel layout of the media file (i.e. the decoder) and the AO's
    channel layout don't match, mpv will attempt to insert a conversion filter.
    You may need to change the channel layout of the system mixer to achieve
    your desired output as mpv does not have control over it. Another
    work-around for this on some AOs is to use ``--audio-exclusive=yes`` to
    circumvent the system mixer entirely.

    .. admonition:: Warning

        Using ``auto`` can cause issues when using audio over HDMI. The OS will
        typically report all channel layouts that _can_ go over HDMI, even if
        the receiver does not support them. If a receiver gets an unsupported
        channel layout, random things can happen, such as dropping the
        additional channels, or adding noise.

        You are recommended to set an explicit whitelist of the layouts you
        want. For example, most A/V receivers connected via HDMI and that can
        do 7.1 would  be served by: ``--audio-channels=7.1,5.1,stereo``

``--audio-display=<no|attachment>``
    Setting this option to ``attachment`` (default) will display image
    attachments (e.g. album cover art) when playing audio files. It will
    display the first image found, and additional images are available as
    video tracks.

    Setting this option to ``no`` disables display of video entirely when
    playing audio files.

    This option has no influence on files with normal video tracks.

``--audio-files=<files>``
    Play audio from an external file while viewing a video.

    This is a list option. See `List Options`_ for details.

``--audio-file=<file>``
    CLI/config file only alias for ``--audio-files-append``. Each use of this
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
    :yes:   The audio device is opened using parameters chosen for the first
            file played and is then kept open for gapless playback. This
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
            The exact conditions under which the audio device is kept open is
            an implementation detail, and can change from version to version.
            Currently, the device is kept even if the sample format changes,
            but the sample formats are convertible.

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

``--volume-max=<100.0-1000.0>``, ``--softvol-max=<...>``
    Set the maximum amplification level in percent (default: 130). A value of
    130 will allow you to adjust the volume up to about double the normal level.

    ``--softvol-max`` is a deprecated alias and should not be used.

``--audio-file-auto=<no|exact|fuzzy|all>``, ``--no-audio-file-auto``
    Load additional audio files matching the video filename. The parameter
    specifies how external audio files are matched.

    :no:    Don't automatically load external audio files (default).
    :exact: Load the media filename with audio file extension.
    :fuzzy: Load all audio files containing media filename.
    :all:   Load all audio files in the current and ``--audio-file-paths``
            directories.

``--audio-file-paths=<path1:path2:...>``
    Equivalent to ``--sub-file-paths`` option, but for auto-loaded audio files.

``--audio-client-name=<name>``
    The application name the player reports to the audio API. Can be useful
    if you want to force a different audio profile (e.g. with PulseAudio),
    or to set your own application name when using libmpv.

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

``--audio-stream-silence=<yes|no>``
    Cash-grab consumer audio hardware (such as A/V receivers) often ignore
    initial audio sent over HDMI. This can happen every time audio over HDMI
    is stopped and resumed. In order to compensate for this, you can enable
    this option to not to stop and restart audio on seeks, and fill the gaps
    with silence. Likewise, when pausing playback, audio is not stopped, and
    silence is played while paused. Note that if no audio track is selected,
    the audio device will still be closed immediately.

    Not all AOs support this.

``--audio-wait-open=<secs>``
    This makes sense for use with ``--audio-stream-silence=yes``. If this option
    is given, the player will wait for the given amount of seconds after opening
    the audio device before sending actual audio data to it. Useful if your
    expensive hardware discards the first 1 or 2 seconds of audio data sent to
    it. If ``--audio-stream-silence=yes`` is not set, this option will likely
    just waste time.

Subtitles
---------

.. note::

    Changing styling and position does not work with all subtitles. Image-based
    subtitles (DVD, Bluray/PGS, DVB) cannot changed for fundamental reasons.
    Subtitles in ASS format are normally not changed intentionally, but
    overriding them can be controlled with ``--sub-ass-override``.

    Previously some options working on text subtitles were called
    ``--sub-text-*``, they are now named ``--sub-*``, and those specifically
    for ASS have been renamed from ``--ass-*`` to ``--sub-ass-*``.
    They are now all in this section.

``--sub-demuxer=<[+]name>``
    Force subtitle demuxer type for ``--sub-file``. Give the demuxer name as
    printed by ``--sub-demuxer=help``.

``--sub-delay=<sec>``
    Delays subtitles by ``<sec>`` seconds. Can be negative.

``--sub-files=<file-list>``, ``--sub-file=<filename>``
    Add a subtitle file to the list of external subtitles.

    If you use ``--sub-file`` only once, this subtitle file is displayed by
    default.

    If ``--sub-file`` is used multiple times, the subtitle to use can be
    switched at runtime by cycling subtitle tracks. It's possible to show
    two subtitles at once: use ``--sid`` to select the first subtitle index,
    and ``--secondary-sid`` to select the second index. (The index is printed
    on the terminal output after the ``--sid=`` in the list of streams.)

    ``--sub-files`` is a list option (see `List Options`_  for details), and
    can take multiple file names separated by ``:`` (Unix) or ``;`` (Windows),
    while  ``--sub-file`` takes a single filename, but can be used multiple
    times to add multiple files. Technically, ``--sub-file`` is a CLI/config
    file only alias for  ``--sub-files-append``.

``--secondary-sid=<ID|auto|no>``
    Select a secondary subtitle stream. This is similar to ``--sid``. If a
    secondary subtitle is selected, it will be rendered as toptitle (i.e. on
    the top of the screen) alongside the normal subtitle, and provides a way
    to render two subtitles at once.

    There are some caveats associated with this feature. For example, bitmap
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
        rendering. Use with care, or use ``--sub-font-size`` instead.

``--sub-scale-by-window=<yes|no>``
    Whether to scale subtitles with the window size (default: yes). If this is
    disabled, changing the window size won't change the subtitle font size.

    Like ``--sub-scale``, this can break ASS subtitles.

``--sub-scale-with-window=<yes|no>``
    Make the subtitle font size relative to the window, instead of the video.
    This is useful if you always want the same font size, even if the video
    doesn't cover the window fully, e.g. because screen aspect and window
    aspect mismatch (and the player adds black bars).

    Default: yes.

    This option is misnamed. The difference to the confusingly similar sounding
    option ``--sub-scale-by-window`` is that ``--sub-scale-with-window`` still
    scales with the approximate window size, while the other option disables
    this scaling.

    Affects plain text subtitles only (or ASS if ``--sub-ass-override`` is set
    high enough).

``--sub-ass-scale-with-window=<yes|no>``
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
        rendering. Use with care, or use ``--sub-margin-y`` instead.

``--sub-speed=<0.1-10.0>``
    Multiply the subtitle event timestamps with the given value. Can be used
    to fix the playback speed for frame-based subtitle formats. Affects text
    subtitles only.

    .. admonition:: Example

        ``--sub-speed=25/23.976`` plays frame based subtitles which have been
        loaded assuming a framerate of 23.976 at 25 FPS.

``--sub-ass-force-style=<[Style.]Param=Value[,...]>``
    Override some style or script info parameters.

    .. admonition:: Examples

        - ``--sub-ass-force-style=FontName=Arial,Default.Bold=1``
        - ``--sub-ass-force-style=PlayResY=768``

    .. note::

        Using this option may lead to incorrect subtitle rendering.

``--sub-ass-hinting=<none|light|normal|native>``
    Set font hinting type. <type> can be:

    :none:       no hinting (default)
    :light:      FreeType autohinter, light mode
    :normal:     FreeType autohinter, normal mode
    :native:     font native hinter

    .. admonition:: Warning

        Enabling hinting can lead to mispositioned text (in situations it's
        supposed to match up video background), or reduce the smoothness
        of animations with some badly authored ASS scripts. It is recommended
        to not use this option, unless really needed.

``--sub-ass-line-spacing=<value>``
    Set line spacing value for SSA/ASS renderer.

``--sub-ass-shaper=<simple|complex>``
    Set the text layout engine used by libass.

    :simple:   uses Fribidi only, fast, doesn't render some languages correctly
    :complex:  uses HarfBuzz, slower, wider language support

    ``complex`` is the default. If libass hasn't been compiled against HarfBuzz,
    libass silently reverts to ``simple``.

``--sub-ass-styles=<filename>``
    Load all SSA/ASS styles found in the specified file and use them for
    rendering text subtitles. The syntax of the file is exactly like the ``[V4
    Styles]`` / ``[V4+ Styles]`` section of SSA/ASS.

    .. note::

        Using this option may lead to incorrect subtitle rendering.

``--sub-ass-override=<yes|no|force|scale|strip>``
    Control whether user style overrides should be applied. Note that all of
    these overrides try to be somewhat smart about figuring out whether or not
    a subtitle is considered a "sign".

    :no:    Render subtitles as specified by the subtitle scripts, without
            overrides.
    :yes:   Apply all the ``--sub-ass-*`` style override options. Changing the
            default for any of these options can lead to incorrect subtitle
            rendering (default).
    :force: Like ``yes``, but also force all ``--sub-*`` options. Can break
            rendering easily.
    :scale: Like ``yes``, but also apply ``--sub-scale``.
    :strip: Radically strip all ASS tags and styles from the subtitle. This
            is equivalent to the old ``--no-ass`` / ``--no-sub-ass`` options.

``--sub-ass-force-margins``
    Enables placing toptitles and subtitles in black borders when they are
    available, if the subtitles are in the ASS format.

    Default: no.

``--sub-use-margins``
    Enables placing toptitles and subtitles in black borders when they are
    available, if the subtitles are in a plain text format  (or ASS if
    ``--sub-ass-override`` is set high enough).

    Default: yes.

    Renamed from ``--sub-ass-use-margins``. To place ASS subtitles in the borders
    too (like the old option did), also add ``--sub-ass-force-margins``.

``--sub-ass-vsfilter-aspect-compat=<yes|no>``
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

``--sub-ass-vsfilter-blur-compat=<yes|no>``
    Scale ``\blur`` tags by video resolution instead of script resolution
    (enabled by default). This is bug in VSFilter, which according to some,
    can't be fixed anymore in the name of compatibility.

    Note that this uses the actual video resolution for calculating the
    offset scale factor, not what the video filter chain or the video output
    use.

``--sub-ass-vsfilter-color-compat=<basic|full|force-601|no>``
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
    a subtitle script with another video file. The ``--sub-ass-override``
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

``--stretch-image-subs-to-screen=<yes|no>``
    Stretch DVD and other image subtitles to the screen, ignoring the video
    margins. This has a similar effect as ``--sub-use-margins`` for text
    subtitles, except that the text itself will be stretched, not only just
    repositioned. (At least in general it is unavoidable, as an image bitmap
    can in theory consist of a single bitmap covering the whole screen, and
    the player won't know where exactly the text parts are located.)

    This option does not display subtitles correctly. Use with care.

    Disabled by default.

``--image-subs-video-resolution=<yes|no>``
    Override the image subtitle resolution with the video resolution
    (default: no). Normally, the subtitle canvas is fit into the video canvas
    (e.g. letterboxed). Setting this option uses the video size as subtitle
    canvas size. Can be useful to test broken subtitles, which often happen
    when the video was trancoded, while attempting to keep the old subtitles.

``--sub-ass``, ``--no-sub-ass``
    Render ASS subtitles natively (enabled by default).

    .. note::

        This has been deprecated by ``--sub-ass-override=strip``. You also
        may need ``--embeddedfonts=no`` to get the same behavior. Also,
        using ``--sub-ass-override=style`` should give better results
        without breaking subtitles too much.

    If ``--no-sub-ass`` is specified, all tags and style declarations are
    stripped and ignored on display. The subtitle renderer uses the font style
    as specified by the ``--sub-`` options instead.

    .. note::

        Using ``--no-sub-ass`` may lead to incorrect or completely broken
        rendering of ASS/SSA subtitles. It can sometimes be useful to forcibly
        override the styling of ASS subtitles, but should be avoided in general.

``--sub-auto=<no|exact|fuzzy|all>``, ``--no-sub-auto``
    Load additional subtitle files matching the video filename. The parameter
    specifies how external subtitle files are matched. ``exact`` is enabled by
    default.

    :no:    Don't automatically load external subtitle files.
    :exact: Load the media filename with subtitle file extension (default).
    :fuzzy: Load all subs containing media filename.
    :all:   Load all subs in the current and ``--sub-file-paths`` directories.

``--sub-codepage=<codepage>``
    You can use this option to specify the subtitle codepage. uchardet will be
    used to guess the charset. (If mpv was not compiled with uchardet, then
    ``utf-8`` is the effective default.)

    The default value for this option is ``auto``, which enables autodetection.

    The following steps are taken to determine the final codepage, in order:

    - if the specific codepage has a ``+``, use that codepage
    - if the data looks like UTF-8, assume it is UTF-8
    - if ``--sub-codepage`` is set to a specific codepage, use that
    - run uchardet, and if successful, use that
    - otherwise, use ``UTF-8-BROKEN``

    .. admonition:: Examples

        - ``--sub-codepage=latin2`` Use Latin 2 if input is not UTF-8.
        - ``--sub-codepage=+cp1250`` Always force recoding to cp1250.

    The pseudo codepage ``UTF-8-BROKEN`` is used internally. If it's set,
    subtitles are interpreted as UTF-8 with "Latin 1" as fallback for bytes
    which are not valid UTF-8 sequences. iconv is never involved in this mode.

    This option changed in mpv 0.23.0. Support for the old syntax was fully
    removed in mpv 0.24.0.


``--sub-fix-timing=<yes|no>``
    Adjust subtitle timing is to remove minor gaps or overlaps between
    subtitles (if the difference is smaller than 210 ms, the gap or overlap
    is removed).

``--sub-forced-only``
    Display only forced subtitles for the DVD subtitle stream selected by e.g.
    ``--slang``.

``--sub-fps=<rate>``
    Specify the framerate of the subtitle file (default: video fps). Affects
    text subtitles only.

    .. note::

        ``<rate>`` > video fps speeds the subtitles up for frame-based
        subtitle files and slows them down for time-based ones.

    See also: ``--sub-speed``.

``--sub-gauss=<0.0-3.0>``
    Apply Gaussian blur to image subtitles (default: 0). This can help to make
    pixelated DVD/Vobsubs look nicer. A value other than 0 also switches to
    software subtitle scaling. Might be slow.

    .. note::

        Never applied to text subtitles.

``--sub-gray``
    Convert image subtitles to grayscale. Can help to make yellow DVD/Vobsubs
    look nicer.

    .. note::

        Never applied to text subtitles.

``--sub-paths=<path1:path2:...>``
    Deprecated, use ``--sub-file-paths``.

``--sub-file-paths=<path-list>``
    Specify extra directories to search for subtitles matching the video.
    Multiple directories can be separated by ":" (";" on Windows).
    Paths can be relative or absolute. Relative paths are interpreted relative
    to video file directory.
    If the file is a URL, only absolute paths and ``sub`` configuration
    subdirectory will be scanned.

    .. admonition:: Example

        Assuming that ``/path/to/video/video.avi`` is played and
        ``--sub-file-paths=sub:subtitles`` is specified, mpv
        searches for subtitle files in these directories:

        - ``/path/to/video/``
        - ``/path/to/video/sub/``
        - ``/path/to/video/subtitles/``
        -  the ``sub`` configuration subdirectory (usually ``~/.config/mpv/sub/``)

    This is a list option. See `List Options`_ for details.

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

``--teletext-page=<1-999>``
    This works for ``dvb_teletext`` subtitle streams, and if FFmpeg has been
    compiled with support for it.

``--sub-font=<name>``
    Specify font to use for subtitles that do not themselves
    specify a particular font. The default is ``sans-serif``.

    .. admonition:: Examples

        - ``--sub-font='Bitstream Vera Sans'``
        - ``--sub-font='Comic Sans MS'``

    .. note::

        The ``--sub-font`` option (and many other style related ``--sub-``
        options) are ignored when ASS-subtitles are rendered, unless the
        ``--no-sub-ass`` option is specified.

        This used to support fontconfig patterns. Starting with libass 0.13.0,
        this stopped working.

``--sub-font-size=<size>``
    Specify the sub font size. The unit is the size in scaled pixels at a
    window height of 720. The actual pixel size is scaled with the window
    height: if the window height is larger or smaller than 720, the actual size
    of the text increases or decreases as well.

    Default: 55.

``--sub-back-color=<color>``
    See ``--sub-color``. Color used for sub text background. You can use
    ``--sub-shadow-offset`` to change its size relative to the text.

``--sub-blur=<0..20.0>``
    Gaussian blur factor. 0 means no blur applied (default).

``--sub-bold=<yes|no>``
    Format text on bold.

``--sub-italic=<yes|no>``
    Format text on italic.

``--sub-border-color=<color>``
    See ``--sub-color``. Color used for the sub font border.

    .. note::

        ignored when ``--sub-back-color`` is
        specified (or more exactly: when that option is not set to completely
        transparent).

``--sub-border-size=<size>``
    Size of the sub font border in scaled pixels (see ``--sub-font-size``
    for details). A value of 0 disables borders.

    Default: 3.

``--sub-color=<color>``
    Specify the color used for unstyled text subtitles.

    The color is specified in the form ``r/g/b``, where each color component
    is specified as number in the range 0.0 to 1.0. It's also possible to
    specify the transparency by using ``r/g/b/a``, where the alpha value 0
    means fully transparent, and 1.0 means opaque. If the alpha component is
    not given, the color is 100% opaque.

    Passing a single number to the option sets the sub to gray, and the form
    ``gray/a`` lets you specify alpha additionally.

    .. admonition:: Examples

        - ``--sub-color=1.0/0.0/0.0`` set sub to opaque red
        - ``--sub-color=1.0/0.0/0.0/0.75`` set sub to opaque red with 75% alpha
        - ``--sub-color=0.5/0.75`` set sub to 50% gray with 75% alpha

    Alternatively, the color can be specified as a RGB hex triplet in the form
    ``#RRGGBB``, where each 2-digit group expresses a color value in the
    range 0 (``00``) to 255 (``FF``). For example, ``#FF0000`` is red.
    This is similar to web colors. Alpha is given with ``#AARRGGBB``.

    .. admonition:: Examples

        - ``--sub-color='#FF0000'`` set sub to opaque red
        - ``--sub-color='#C0808080'`` set sub to 50% gray with 75% alpha

``--sub-margin-x=<size>``
    Left and right screen margin for the subs in scaled pixels (see
    ``--sub-font-size`` for details).

    This option specifies the distance of the sub to the left, as well as at
    which distance from the right border long sub text will be broken.

    Default: 25.

``--sub-margin-y=<size>``
    Top and bottom screen margin for the subs in scaled pixels (see
    ``--sub-font-size`` for details).

    This option specifies the vertical margins of unstyled text subtitles.
    If you just want to raise the vertical subtitle position, use ``--sub-pos``.

    Default: 22.

``--sub-align-x=<left|center|right>``
    Control to which corner of the screen text subtitles should be
    aligned to (default: ``center``).

    Never applied to ASS subtitles, except in ``--no-sub-ass`` mode. Likewise,
    this does not apply to image subtitles.

``--sub-align-y=<top|center|bottom>``
    Vertical position (default: ``bottom``).
    Details see ``--sub-align-x``.

``--sub-justify=<auto|left|center|right>``
    Control how multi line subs are justified irrespective of where they
    are aligned (default: ``auto`` which justifies as defined by
    ``--sub-align-y``).
    Left justification is recommended to make the subs easier to read
    as it is easier for the eyes.

``--sub-ass-justify=<yes|no>``
    Applies justification as defined by ``--sub-justify`` on ASS subtitles
    if ``--sub-ass-override`` is not set to ``no``.
    Default: ``no``.

``--sub-shadow-color=<color>``
    See ``--sub-color``. Color used for sub text shadow.

``--sub-shadow-offset=<size>``
    Displacement of the sub text shadow in scaled pixels (see
    ``--sub-font-size`` for details). A value of 0 disables shadows.

    Default: 0.

``--sub-spacing=<size>``
    Horizontal sub font spacing in scaled pixels (see ``--sub-font-size``
    for details). This value is added to the normal letter spacing. Negative
    values are allowed.

    Default: 0.

``--sub-filter-sdh=<yes|no>``
    Applies filter removing subtitle additions for the deaf or hard-of-hearing (SDH).
    This is intended for English, but may in part work for other languages too.
    The intention is that it can be always enabled so may not remove
    all parts added.
    It removes speaker labels (like MAN:), upper case text in parentheses and
    any text in brackets.

    Default: ``no``.

``--sub-filter-sdh-harder=<yes|no>``
    Do harder SDH filtering (if enabled by ``--sub-filter-sdh``).
    Will also remove speaker labels and text within parentheses using both
    lower and upper case letters.

    Default: ``no``.

``--sub-create-cc-track=<yes|no>``
    For every video stream, create a closed captions track (default: no). The
    only purpose is to make the track available for selection at the start of
    playback, instead of creating it lazily. This applies only to
    ``ATSC A53 Part 4 Closed Captions`` (displayed by mpv as subtitle tracks
    using the codec ``eia_608``). The CC track is marked "default" and selected
    according to the normal subtitle track selection rules. You can then use
    ``--sid`` to explicitly select the correct track too.

    If the video stream contains no closed captions, or if no video is being
    decoded, the CC track will remain empty and will not show any text.

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

        This option works properly only with window managers which
        understand the EWMH ``_NET_WM_FULLSCREEN_MONITORS`` hint.

    .. admonition:: Note (OS X)

        ``all`` does not work on OS X and will behave like ``current``.

    See also ``--screen``.

``--keep-open=<yes|no|always>``
    Do not terminate when playing or seeking beyond the end of the file, and
    there is not next file to be played (and ``--loop`` is not used).
    Instead, pause the player. When trying to seek beyond end of the file, the
    player will attempt to seek to the last frame.

    Normally, this will act like ``set pause yes`` on EOF, unless the
    ``--keep-open-pause=no`` option is set.

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

``--keep-open-pause=<yes|no>``
    If set to ``no``, instead of pausing when ``--keep-open`` is active, just
    stop at end of file and continue playing forward when you seek backwards
    until end where it stops again. Default: ``yes``.

``--image-display-duration=<seconds|inf>``
    If the current file is an image, play the image for the given amount of
    seconds (default: 1). ``inf`` means the file is kept open forever (until
    the user stops playback manually).

    Unlike ``--keep-open``, the player is not paused, but simply continues
    playback until the time has elapsed. (It should not use any resources
    during "playback".)

    This affects image files, which are defined as having only 1 video frame
    and no audio. The player may recognize certain non-images as images, for
    example if ``--length`` is used to reduce the length to 1 frame, or if
    you seek to the last frame.

    This option does not affect the framerate used for ``mf://`` or
    ``--merge-files``. For that, use ``--mf-fps`` instead.

    Setting ``--image-display-duration`` hides the OSC and does not track
    playback time on the command-line output, and also does not duplicate
    the image frame when encoding. To force the player into "dumb mode"
    and actually count out seconds, or to duplicate the image when
    encoding, you need to use ``--demuxer=lavf --demuxer-lavf-o=loop=1``,
    and use ``--length`` or ``--frames`` to stop after a particular time.

``--force-window=<yes|no|immediate>``
    Create a video output window even if there is no video. This can be useful
    when pretending that mpv is a GUI application. Currently, the window
    always has the size 640x480, and is subject to ``--geometry``,
    ``--autofit``, and similar options.

    .. warning::

        The window is created only after initialization (to make sure default
        window placement still works if the video size is different from the
        ``--force-window`` default window size). This can be a problem if
        initialization doesn't work perfectly, such as when opening URLs with
        bad network connection, or opening broken video files. The ``immediate``
        mode can be used to create the window always on program start, but this
        may cause other issues.

``--taskbar-progress``, ``--no-taskbar-progress``
    (Windows only)
    Enable/disable playback progress rendering in taskbar (Windows 7 and above).

    Enabled by default.

``--snap-window``
    (Windows only) Snap the player window to screen edges.

``--ontop``
    Makes the player window stay on top of other windows.

    On Windows, if combined with fullscreen mode, this causes mpv to be
    treated as exclusive fullscreen window that bypasses the Desktop Window
    Manager.

``--ontop-level=<window|system|level>``
    (OS X only)
    Sets the level of an ontop window (default: window).

    :window:  On top of all other windows.
    :system:  On top of system elements like Taskbar, Menubar and Dock.
    :level:   A level as integer.

``--border``, ``--no-border``
    Play video with window border and decorations. Since this is on by
    default, use ``--no-border`` to disable the standard window decorations.

``--fit-border``, ``--no-fit-border``
    (Windows only) Fit the whole window with border and decorations on the
    screen. Since this is on by default, use ``--no-fit-border`` to make mpv
    try to only fit client area with video on the screen. This behavior only
    applied to window/video with size exceeding size of the screen.

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
            ratio (with most VOs and without ``--no-keepaspect``).
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
        ``70%x60%``
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
    implications. For VOs which support native ASS rendering (like ``gpu``,
    ``vdpau``, ``direct3d``), this can be slightly faster or slower,
    depending on GPU drivers and hardware. For other VOs, this just makes
    rendering slower.

``--force-window-position``
    Forcefully move mpv's video output window to default location whenever
    there is a change in video parameters, video stream or file. This used to
    be the default behavior. Currently only affects X11 VOs.

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

``--hidpi-window-scale``, ``--no-hidpi-window-scale``
    (OS X and X11 only)
    Scale the window size according to the backing scale factor (default: yes).
    On regular HiDPI resolutions the window opens with double the size but appears
    as having the same size as on none-HiDPI resolutions. This is the default OS X
    behavior.

``--native-fs``, ``--no-native-fs``
    (OS X only)
    Uses the native fullscreen mechanism of the OS (default: yes).

``--monitorpixelaspect=<ratio>``
    Set the aspect of a single pixel of your monitor or TV screen (default:
    1). A value of 1 means square pixels (correct for (almost?) all LCDs). See
    also ``--monitoraspect`` and ``--video-aspect``.

``--stop-screensaver``, ``--no-stop-screensaver``
    Turns off the screensaver (or screen blanker and similar mechanisms) at
    startup and turns it on again on exit (default: yes). The screensaver is
    always re-enabled when the player is paused.

    This is not supported on all video outputs or platforms. Sometimes it is
    implemented, but does not work (especially with Linux "desktops").

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

    On OSX/Cocoa, the ID is interpreted as ``NSView*``. Pass it as value cast
    to ``intptr_t``. mpv will create its own sub-view. Because OSX does not
    support window embedding of foreign processes, this works only with libmpv,
    and will crash when used from the command line.

    On Android, the ID is interpreted as ``android.view.Surface``. Pass it as a
    value cast to ``intptr_t``. Use with ``--vo=mediacodec_embed`` and
    ``--hwdec=mediacodec`` for direct rendering using MediaCodec, or with
    ``--vo=gpu --gpu-context=android`` (with or without ``--hwdec=mediacodec-copy``).

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

``--x11-bypass-compositor=<yes|no|fs-only|never>``
    If set to ``yes``, then ask the compositor to unredirect the mpv window
    (default: ``fs-only``). This uses the ``_NET_WM_BYPASS_COMPOSITOR`` hint.

    ``fs-only`` asks the window manager to disable the compositor only in
    fullscreen mode.

    ``no`` sets ``_NET_WM_BYPASS_COMPOSITOR`` to 0, which is the default value
    as declared by the EWMH specification, i.e. no change is done.

    ``never`` asks the window manager to never disable the compositor.


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
    Print CD text. This is disabled by default, because it ruins performance
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

``--demuxer-lavf-probe-info=<yes|no|auto|nostreams>``
    Whether to probe stream information (default: auto). Technically, this
    controls whether libavformat's ``avformat_find_stream_info()`` function
    is called. Usually it's safer to call it, but it can also make startup
    slower.

    The ``auto`` choice (the default) tries to skip this for a few know-safe
    whitelisted formats, while calling it for everything else.

    The ``nostreams`` choice only calls it if and only if the file seems to
    contain no streams after opening (helpful in cases when calling the function
    is needed to detect streams at all, such as with FLV files).

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

``--demuxer-mkv-subtitle-preroll=<yes|index|no>``, ``--mkv-subtitle-preroll``
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
    how much data the demuxer should pre-read at most in order to find subtitle
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
    target.) If the ``index`` choice (which is the default) is specified, then
    prerolling will be done only if this information is actually available. If
    this method is used, the maximum amount of data to skip can be additionally
    controlled by ``--demuxer-mkv-subtitle-preroll-secs-index`` (it still uses
    the value of the option without ``-index`` if that is higher).

    See also ``--hr-seek-demuxer-offset`` option. This option can achieve a
    similar effect, but only if hr-seek is active. It works with any demuxer,
    but makes seeking much slower, as it has to decode audio and video data
    instead of just skipping over it.

    ``--mkv-subtitle-preroll`` is a deprecated alias.

``--demuxer-mkv-subtitle-preroll-secs=<value>``
    See ``--demuxer-mkv-subtitle-preroll``.

``--demuxer-mkv-subtitle-preroll-secs-index=<value>``
    See ``--demuxer-mkv-subtitle-preroll``.

``--demuxer-mkv-probe-video-duration=<yes|no|full>``
    When opening the file, seek to the end of it, and check what timestamp the
    last video packet has, and report that as file duration. This is strictly
    for compatibility with Haali only. In this mode, it's possible that opening
    will be slower (especially when playing over http), or that behavior with
    broken files is much worse. So don't use this option.

    The ``yes`` mode merely uses the index and reads a small number of blocks
    from the end of the file. The ``full`` mode actually traverses the entire
    file and can make a reliable estimate even without an index present (such
    as partial files).

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

``--demuxer-max-bytes=<bytesize>``
    This controls how much the demuxer is allowed to buffer ahead. The demuxer
    will normally try to read ahead as much as necessary, or as much is
    requested with ``--demuxer-readahead-secs``. The option can be used to
    restrict the maximum readahead. This limits excessive readahead in case of
    broken files or desynced playback. The demuxer will stop reading additional
    packets as soon as one of the limits is reached. (The limits still can be
    slightly overstepped due to technical reasons.)

    Set these limits higher if you get a packet queue overflow warning, and
    you think normal playback would be possible with a larger packet queue.

    See ``--list-options`` for defaults and value range. ``<bytesize>`` options
    accept suffixes such as ``KiB`` and ``MiB``.

``--demuxer-max-back-bytes=<bytesize>``
    This controls how much past data the demuxer is allowed to preserve. This
    is useful only if the ``--demuxer-seekable-cache`` option is enabled.
    Unlike the forward cache, there is no control how many seconds are actually
    cached - it will simply use as much memory this option allows. Setting this
    option to 0 will strictly disable any back buffer, but this will lead to
    the situation that the forward seek range starts after the current playback
    position (as it removes past packets that are seek points).

    Keep in mind that other buffers in the player (like decoders) will cause the
    demuxer to cache "future" frames in the back buffer, which can skew the
    impression about how much data the backbuffer contains.

    See ``--list-options`` for defaults and value range.

``--demuxer-seekable-cache=<yes|no|auto>``
    This controls whether seeking can use the demuxer cache (default: auto). If
    enabled, short seek offsets will not trigger a low level demuxer seek
    (which means for example that slow network round trips or FFmpeg seek bugs
    can be avoided). If a seek cannot happen within the cached range, a low
    level seek will be triggered. Seeking outside of the cache will start a new
    cached range, but can discard the old cache range if the demuxer exhibits
    certain unsupported behavior.

    Keep in mind that some events can flush the cache or force a low level
    seek anyway, such as switching tracks, or attempting to seek before the
    start or after the end of the file.

    The special value ``auto`` means ``yes`` in the same situation as
    ``--cache-secs`` is used (i.e. when the stream appears to be a network
    stream or the stream cache is enabled).

``--demuxer-thread=<yes|no>``
    Run the demuxer in a separate thread, and let it prefetch a certain amount
    of packets (default: yes). Having this enabled leads to smoother playback,
    enables features like prefetching, and prevents that stuck network freezes
    the player. On the other hand, it can add overhead, or the background
    prefetching can hog CPU resources.

    Disabling this option is not recommended. Use it for debugging only.

``--demuxer-readahead-secs=<seconds>``
    If ``--demuxer-thread`` is enabled, this controls how much the demuxer
    should buffer ahead in seconds (default: 1). As long as no packet has
    a timestamp difference higher than the readahead amount relative to the
    last packet returned to the decoder, the demuxer keeps reading.

    Note that the ``--cache-secs`` option will override this value if a cache
    is enabled, and the value is larger.

    (This value tends to be fuzzy, because many file formats don't store linear
    timestamps.)

``--prefetch-playlist=<yes|no>``
    Prefetch next playlist entry while playback of the current entry is ending
    (default: no). This merely opens the URL of the next playlist entry as soon
    as the current URL is fully read.

    This does **not** work with URLs resolved by the ``youtube-dl`` wrapper,
    and it won't.

    This does not affect HLS (``.m3u8`` URLs) - HLS prefetching depends on the
    demuxer cache settings and is on by default.

    This can give subtly wrong results if per-file options are used, or if
    options are changed in the time window between prefetching start and next
    file played.

    This can occasionally make wrong prefetching decisions. For example, it
    can't predict whether you go backwards in the playlist, and assumes you
    won't edit the playlist.

    Highly experimental.

``--force-seekable=<yes|no>``
    If the player thinks that the media is not seekable (e.g. playing from a
    pipe, or it's an http stream with a server that doesn't support range
    requests), seeking will be disabled. This option can forcibly enable it.
    For seeks within the cache, there's a good chance of success.

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
    is too small, some events may be lost. The main disadvantage of setting it
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
    get replies or events. Use ``--input-ipc-server`` for something
    bi-directional. On MS Windows, JSON commands are not available.

    This can also specify a direct file descriptor with ``fd://N`` (UNIX only).
    In this case, JSON replies will be written if the FD is writable.

    .. note::

        When the given file is a FIFO mpv opens both ends, so you can do several
        `echo "seek 10" > mp_pipe` and the pipe will stay valid.

``--input-terminal``, ``--no-input-terminal``
    ``--no-input-terminal`` prevents the player from reading key events from
    standard input. Useful when reading data from standard input. This is
    automatically enabled when ``-`` is found on the command line. There are
    situations where you have to set it manually, e.g. if you open
    ``/dev/stdin`` (or the equivalent on your system), use stdin in a playlist
    or intend to read from stdin later on via the loadfile or loadlist input
    commands.

``--input-ipc-server=<filename>``
    Enable the IPC support and create the listening socket at the given path.

    On Linux and Unix, the given path is a regular filesystem path. On Windows,
    named pipes are used, so the path refers to the pipe namespace
    (``\\.\pipe\<name>``). If the ``\\.\pipe\`` prefix is missing, mpv will add
    it automatically before creating the pipe, so
    ``--input-ipc-server=/tmp/mpv-socket`` and
    ``--input-ipc-server=\\.\pipe\tmp\mpv-socket`` are equivalent for IPC on
    Windows.

    See `JSON IPC`_ for details.

``--input-appleremote=<yes|no>``
    (OS X only)
    Enable/disable Apple Remote support. Enabled by default (except for libmpv).

``--input-cursor``, ``--no-input-cursor``
    Permit mpv to receive pointer events reported by the video output
    driver. Necessary to use the OSC, or to select the buttons in DVD menus.
    Support depends on the VO in use.

``--input-media-keys=<yes|no>``
    (OS X and Windows only)
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
    the sub-window. It can steal away all keyboard input from the
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

OSD
---

``--osc``, ``--no-osc``
    Whether to load the on-screen-controller (default: yes).

``--no-osd-bar``, ``--osd-bar``
    Disable display of the OSD bar.

    You can configure this on a per-command basis in input.conf using ``osd-``
    prefixes, see ``Input Command Prefixes``. If you want to disable the OSD
    completely, use ``--osd-level=0``.

``--osd-on-seek=<no,bar,msg,msg-bar>``
    Set what is displayed on the OSD during seeks. The default is ``bar``.

    You can configure this on a per-command basis in input.conf using ``osd-``
    prefixes, see ``Input Command Prefixes``.

``--osd-duration=<time>``
    Set the duration of the OSD messages in ms (default: 1000).

``--osd-font=<name>``
    Specify font to use for OSD. The default is ``sans-serif``.

    .. admonition:: Examples

        - ``--osd-font='Bitstream Vera Sans'``
        - ``--osd-font='Comic Sans MS'``

``--osd-font-size=<size>``
    Specify the OSD font size. See ``--sub-font-size`` for details.

    Default: 55.

``--osd-msg1=<string>``
    Show this string as message on OSD with OSD level 1 (visible by default).
    The message will be visible by default, and as long as no other message
    covers it, and the OSD level isn't changed (see ``--osd-level``).
    Expands properties; see `Property Expansion`_.

``--osd-msg2=<string>``
    Similar to ``--osd-msg1``, but for OSD level 2. If this is an empty string
    (default), then the playback time is shown.

``--osd-msg3=<string>``
    Similar to ``--osd-msg1``, but for OSD level 3. If this is an empty string
    (default), then the playback time, duration, and some more information is
    shown.

    This is used for the ``show-progress`` command (by default mapped to ``P``),
    and when seeking if enabled with ``--osd-on-seek`` or by ``osd-`` prefixes
    in input.conf (see ``Input Command Prefixes``).

    ``--osd-status-msg`` is a legacy equivalent (but with a minor difference).

``--osd-status-msg=<string>``
    Show a custom string during playback instead of the standard status text.
    This overrides the status text used for ``--osd-level=3``, when using the
    ``show-progress`` command (by default mapped to ``P``), and when seeking if
    enabled with ``--osd-on-seek`` or ``osd-`` prefixes in input.conf (see
    ``Input Command Prefixes``). Expands properties. See `Property Expansion`_.

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

``--osd-back-color=<color>``
    See ``--osd-color``. Color used for OSD text background.

``--osd-blur=<0..20.0>``
    Gaussian blur factor. 0 means no blur applied (default).

``--osd-bold=<yes|no>``
    Format text on bold.

``--osd-italic=<yes|no>``
    Format text on italic.

``--osd-border-color=<color>``
    See ``--osd-color``. Color used for the OSD font border.

    .. note::

        ignored when ``--osd-back-color`` is
        specified (or more exactly: when that option is not set to completely
        transparent).

``--osd-border-size=<size>``
    Size of the OSD font border in scaled pixels (see ``--sub-font-size``
    for details). A value of 0 disables borders.

    Default: 3.

``--osd-color=<color>``
    Specify the color used for OSD.
    See ``--sub-color`` for details.

``--osd-fractions``
    Show OSD times with fractions of seconds (in millisecond precision). Useful
    to see the exact timestamp of a video frame.

``--osd-level=<0-3>``
    Specifies which mode the OSD should start in.

    :0: OSD completely disabled (subtitles only)
    :1: enabled (shows up only on user interaction)
    :2: enabled + current time visible by default
    :3: enabled + ``--osd-status-msg`` (current time and status by default)

``--osd-margin-x=<size>``
    Left and right screen margin for the OSD in scaled pixels (see
    ``--sub-font-size`` for details).

    This option specifies the distance of the OSD to the left, as well as at
    which distance from the right border long OSD text will be broken.

    Default: 25.

``--osd-margin-y=<size>``
    Top and bottom screen margin for the OSD in scaled pixels (see
    ``--sub-font-size`` for details).

    This option specifies the vertical margins of the OSD.

    Default: 22.

``--osd-align-x=<left|center|right>``
    Control to which corner of the screen OSD should be
    aligned to (default: ``left``).

``--osd-align-y=<top|center|bottom>``
    Vertical position (default: ``top``).
    Details see ``--osd-align-x``.

``--osd-scale=<factor>``
    OSD font size multiplier, multiplied with ``--osd-font-size`` value.

``--osd-scale-by-window=<yes|no>``
    Whether to scale the OSD with the window size (default: yes). If this is
    disabled, ``--osd-font-size`` and other OSD options that use scaled pixels
    are always in actual pixels. The effect is that changing the window size
    won't change the OSD font size.

``--osd-shadow-color=<color>``
    See ``--sub-color``. Color used for OSD shadow.

``--osd-shadow-offset=<size>``
    Displacement of the OSD shadow in scaled pixels (see
    ``--sub-font-size`` for details). A value of 0 disables shadows.

    Default: 0.

``--osd-spacing=<size>``
    Horizontal OSD/sub font spacing in scaled pixels (see ``--sub-font-size``
    for details). This value is added to the normal letter spacing. Negative
    values are allowed.

    Default: 0.

``--video-osd=<yes|no>``
    Enabled OSD rendering on the video window (default: yes). This can be used
    in situations where terminal OSD is preferred. If you just want to disable
    all OSD rendering, use ``--osd-level=0``.

    It does not affect subtitles or overlays created by scripts (in particular,
    the OSC needs to be disabled with ``--no-osc``).

    This option is somewhat experimental and could be replaced by another
    mechanism in the future.

Screenshot
----------

``--screenshot-format=<type>``
    Set the image file type used for saving screenshots.

    Available choices:

    :png:       PNG
    :jpg:       JPEG (default)
    :jpeg:      JPEG (alias for jpg)

``--screenshot-tag-colorspace=<yes|no>``
    Tag screenshots with the appropriate colorspace.

    Note that not all formats are supported.

    Default: ``no``.

``--screenshot-high-bit-depth=<yes|no>``
    If possible, write screenshots with a bit depth similar to the source
    video (default: yes). This is interesting in particular for PNG, as this
    sometimes triggers writing 16 bit PNGs with huge file sizes. This will also
    include an unused alpha channel in the resulting files if 16 bit is used.

``--screenshot-template=<template>``
    Specify the filename template used to save screenshots. The template
    specifies the filename without file extension, and can contain format
    specifiers, which will be substituted when taking a screenshot.
    By default, the template is ``mpv-shot%n``, which results in filenames like
    ``mpv-shot0012.png`` for example.

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

            This is a simple way for getting unique per-frame timestamps. (Frame
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
        Insert the value of the input property 'prop'. E.g. ``%{filename}`` is
        the same as ``%f``. If the property does not exist or is not available,
        an error text is inserted, unless a fallback is specified.
    ``%%``
        Replaced with the ``%`` character itself.

``--screenshot-directory=<path>``
    Store screenshots in this directory. This path is joined with the filename
    generated by ``--screenshot-template``. If the template filename is already
    absolute, the directory is ignored.

    If the directory does not exist, it is created on the first screenshot. If
    it is not a directory, an error is generated when trying to write a
    screenshot.

    This option is not set by default, and thus will write screenshots to the
    directory from which mpv was started. In pseudo-gui mode
    (see `PSEUDO GUI MODE`_), this is set to the desktop.

``--screenshot-jpeg-quality=<0-100>``
    Set the JPEG quality level. Higher means better quality. The default is 90.

``--screenshot-jpeg-source-chroma=<yes|no>``
    Write JPEG files with the same chroma subsampling as the video
    (default: yes). If disabled, the libjpeg default is used.

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

Audio Resampler
---------------

This controls the default options of any resampling done by mpv (but not within
libavfilter, within the system audio API resampler, or any other places).

It also sets the defaults for the ``lavrresample`` audio filter.

``--audio-resample-filter-size=<length>``
    Length of the filter with respect to the lower sampling rate. (default:
    16)

``--audio-resample-phase-shift=<count>``
    Log2 of the number of polyphase entries. (..., 10->1024, 11->2048,
    12->4096, ...) (default: 10->1024)

``--audio-resample-cutoff=<cutoff>``
    Cutoff frequency (0.0-1.0), default set depending upon filter length.

``--audio-resample-linear=<yes|no>``
    If set then filters will be linearly interpolated between polyphase
    entries. (default: no)

``--audio-normalize-downmix=<yes|no>``
    Enable/disable normalization if surround audio is downmixed to stereo
    (default: no). If this is disabled, downmix can cause clipping. If it's
    enabled, the output might be too quiet. It depends on the source audio.

    Technically, this changes the ``normalize`` suboption of the
    ``lavrresample`` audio filter, which performs the downmixing.

    If downmix happens outside of mpv for some reason, or in the decoder
    (decoder downmixing), or in the audio output (system mixer), this has no
    effect.

``--audio-resample-max-output-size=<length>``
    Limit maximum size of audio frames filtered at once, in ms (default: 40).
    The output size size is limited in order to make resample speed changes
    react faster. This is necessary especially if decoders or filters output
    very large frame sizes (like some lossless codecs or some DRC filters).
    This option does not affect the resampling algorithm in any way.

    For testing/debugging only. Can be removed or changed any time.

``--audio-swresample-o=<string>``
    Set AVOptions on the SwrContext or AVAudioResampleContext. These should
    be documented by FFmpeg or Libav.

Terminal
--------

``--quiet``
    Make console output less verbose; in particular, prevents the status line
    (i.e. AV: 3.4 (00:00:03.37) / 5320.6 ...) from being displayed.
    Particularly useful on slow terminals or broken ones which do not properly
    handle carriage return (i.e. ``\r``).

    See also: ``--really-quiet`` and ``--msg-level``.

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
    verbosity of all the modules. The verbosity changes from this option are
    applied in order from left to right, and each item can override a previous
    one.

    Run mpv with ``--msg-level=all=trace`` to see all messages mpv outputs. You
    can use the module names printed in the output (prefixed to each line in
    ``[...]``) to limit the output to interesting modules.

    This also affects ``--log-file``, and in certain cases libmpv API logging.

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

    .. admonition:: Example

        ::

            mpv --msg-level=ao/sndio=no

        Completely silences the output of ao_sndio, which uses the log
        prefix ``[ao/sndio]``.

        ::

            mpv --msg-level=all=warn,ao/alsa=error

        Only show warnings or worse, and let the ao_alsa output show errors
        only.

``--term-osd=<auto|no|force>``
    Control whether OSD messages are shown on the console when no video output
    is available (default: auto).

    :auto:      use terminal OSD if no video output active
    :no:        disable terminal OSD
    :force:     use terminal OSD even if video output active

    The ``auto`` mode also enables terminal OSD if ``--video-osd=no`` was set.

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
    See the console output for a list of all available norms.

    See also: ``--tv-normid``.

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
        names will then be written using OSD, and the input commands
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
    stream cannot be cached, or ``--cache-default`` disables caching).

    May be useful when playing files from slow media, but can also have
    negative effects, especially with file formats that require a lot of
    seeking, such as MP4.

    Note that half the cache size will be used to allow fast seeking back. This
    is also the reason why a full cache is usually not reported as 100% full.
    The cache fill display does not include the part of the cache reserved for
    seeking back. The actual maximum percentage will usually be the ratio
    between readahead and backbuffer sizes.

``--cache-default=<kBytes|no>``
    Set the size of the cache in kilobytes (default: 10000 KB). Using ``no``
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

``--cache-backbuffer=<kBytes>``
    Size of the cache back buffer (default: 10000 KB). This will add to the total
    cache size, and reserved the amount for seeking back. The reserved amount
    will not be used for readahead, and instead preserves already read data to
    enable fast seeking back.

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

    See also: ``--cache-file-size``.

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
    is enabled and the value is larger. The default value is set to something
    very high, so the actually achieved readahead will usually be limited by
    the value of the ``--demuxer-max-bytes`` option.

``--cache-pause=<yes|no>``
    Whether the player should automatically pause when the cache runs out of
    data and stalls decoding/playback (default: yes). If enabled, it will
    pause and unpause once more data is available, aka "buffering".

``--cache-pause-wait=<seconds>``
    Number of seconds the packet cache should have buffered before starting
    playback again if "buffering" was entered (default: 1). This can be used
    to control how long the player rebuffers if ``--cache-pause`` is enabled,
    and the demuxer underruns. If the given time is higher than the maximum
    set with ``--cache-secs`` or  ``--demuxer-readahead-secs``, or prefetching
    ends before that for some other reason (like file end), playback resumes
    earlier.

``--cache-pause-initial=<yes|no>``
    Enter "buffering" mode before starting playback (default: no). This can be
    used to ensure playback starts smoothly, in exchange for waiting some time
    to prefetch network data (as controlled by ``--cache-pause-wait``). For
    example, some common behavior is that playback starts, but network caches
    immediately underrun when trying to decode more data as playback progresses.

    Another thing that can happen is that the network prefetching is so CPU
    demanding (due to demuxing in the background) that playback drops frames
    at first. In these cases, it helps enabling this option, and setting
    ``--cache-secs`` and ``--cache-pause-wait`` to roughly the same value.

    This option also triggers when playback is restarted after seeking.


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

``--http-proxy=<proxy>``
    URL of the HTTP/HTTPS proxy. If this is set, the ``http_proxy`` environment
    is ignored. The ``no_proxy`` environment variable is still respected. This
    option is silently ignored if it does not start with ``http://``. Proxies
    are not used for https URLs. Setting this option does not try to make the
    ytdl script use the proxy.

``--tls-ca-file=<filename>``
    Certificate authority database file for use with TLS. (Silently fails with
    older FFmpeg or Libav versions.)

``--tls-verify``
    Verify peer certificates when using TLS (e.g. with ``https://...``).
    (Silently fails with older FFmpeg or Libav versions.)

``--tls-cert-file``
    A file containing a certificate to use in the handshake with the
    peer.

``--tls-key-file``
    A file containing the private key for the certificate.

``--referrer=<string>``
    Specify a referrer path or URL for HTTP requests.

``--network-timeout=<seconds>``
    Specify the network timeout in seconds. This affects at least HTTP. The
    special value 0 (default) uses the FFmpeg/Libav defaults. If a protocol
    is used which does not support timeouts, this option is silently ignored.

    .. warning::

        This breaks the RTSP protocol, because of inconsistent FFmpeg API
        regarding its internal timeout option. Not only does the RTSP timeout
        option accept different units (seconds instead of microseconds, causing
        mpv to pass it huge values), it will also overflow FFmpeg internal
        calculations. The worst is that merely setting the option will put RTSP
        into listening mode, which breaks any client uses. Do not use this
        option with RTSP URLs.

``--rtsp-transport=<lavf|udp|tcp|http>``
    Select RTSP transport method (default: tcp). This selects the underlying
    network transport when playing ``rtsp://...`` URLs. The value ``lavf``
    leaves the decision to libavformat.

``--hls-bitrate=<no|min|max|<rate>>``
    If HLS streams are played, this option controls what streams are selected
    by default. The option allows the following parameters:

    :no:        Don't do anything special. Typically, this will simply pick the
                first audio/video streams it can find.
    :min:       Pick the streams with the lowest bitrate.
    :max:       Same, but highest bitrate. (Default.)

    Additionally, if the option is a number, the stream with the highest rate
    equal or below the option value is selected.

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
    transponder to demuxer.
    The player frontend selects the streams from the full TS in this case,
    so the program which is shown initially may not match the chosen channel.
    Switching between the programs is possible by cycling the ``program``
    property.
    This is useful to record multiple programs on a single transponder,
    or to work around issues in the ``channels.conf``.
    It is also recommended to use this for channels which switch PIDs
    on-the-fly, e.g. for regional news.

    Default: ``no``

ALSA audio output options
-------------------------


``--alsa-device=<device>``
    Deprecated, use ``--audio-device`` (requires ``alsa/`` prefix).

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


GPU renderer options
-----------------------

The following video options are currently all specific to ``--vo=gpu`` and
``--vo=opengl-cb`` only, which are the only VOs that implement them.

``--scale=<filter>``
    The filter function to use when upscaling video.

    ``bilinear``
        Bilinear hardware texture filtering (fastest, very low quality). This
        is the default for compatibility reasons.

    ``spline36``
        Mid quality and speed. This is the default when using ``gpu-hq``.

    ``lanczos``
        Lanczos scaling. Provides mid quality and speed. Generally worse than
        ``spline36``, but it results in a slightly sharper image which is good
        for some content types. The number of taps can be controlled with
        ``scale-radius``, but is best left unchanged.

        (This filter is an alias for ``sinc``-windowed ``sinc``)

    ``ewa_lanczos``
        Elliptic weighted average Lanczos scaling. Also known as Jinc.
        Relatively slow, but very good quality. The radius can be controlled
        with ``scale-radius``. Increasing the radius makes the filter sharper
        but adds more ringing.

        (This filter is an alias for ``jinc``-windowed ``jinc``)

    ``ewa_lanczossharp``
        A slightly sharpened version of ewa_lanczos, preconfigured to use an
        ideal radius and parameter. If your hardware can run it, this is
        probably what you should use by default.

    ``mitchell``
        Mitchell-Netravali. The ``B`` and ``C`` parameters can be set with
        ``--scale-param1`` and ``--scale-param2``. This filter is very good at
        downscaling (see ``--dscale``).

    ``oversample``
        A version of nearest neighbour that (naively) oversamples pixels, so
        that pixels overlapping edges get linearly interpolated instead of
        rounded. This essentially removes the small imperfections and judder
        artifacts caused by nearest-neighbour interpolation, in exchange for
        adding some blur. This filter is good at temporal interpolation, and
        also known as "smoothmotion" (see ``--tscale``).

    ``linear``
        A ``--tscale`` filter.

    There are some more filters, but most are not as useful. For a complete
    list, pass ``help`` as value, e.g.::

        mpv --scale=help

``--cscale=<filter>``
    As ``--scale``, but for interpolating chroma information. If the image is
    not subsampled, this option is ignored entirely.

``--dscale=<filter>``
    Like ``--scale``, but apply these filters on downscaling instead. If this
    option is unset, the filter implied by ``--scale`` will be applied.

``--tscale=<filter>``
    The filter used for interpolating the temporal axis (frames). This is only
    used if ``--interpolation`` is enabled. The only valid choices for
    ``--tscale`` are separable convolution filters (use ``--tscale=help`` to
    get a list). The default is ``mitchell``.

    Common ``--tscale`` choices include ``oversample``, ``linear``,
    ``catmull_rom``, ``mitchell``, ``gaussian``, or ``bicubic``. These are
    listed in increasing order of smoothness/blurriness, with ``bicubic``
    being the smoothest/blurriest and ``oversample`` being the sharpest/least
    smooth.

``--scale-param1=<value>``, ``--scale-param2=<value>``, ``--cscale-param1=<value>``, ``--cscale-param2=<value>``, ``--dscale-param1=<value>``, ``--dscale-param2=<value>``, ``--tscale-param1=<value>``, ``--tscale-param2=<value>``
    Set filter parameters. Ignored if the filter is not tunable. Currently,
    this affects the following filter parameters:

    bcspline
        Spline parameters (``B`` and ``C``). Defaults to 0.5 for both.

    gaussian
        Scale parameter (``t``). Increasing this makes the result blurrier.
        Defaults to 1.

    oversample
        Minimum distance to an edge before interpolation is used. Setting this
        to 0 will always interpolate edges, whereas setting it to 0.5 will
        never interpolate, thus behaving as if the regular nearest neighbour
        algorithm was used. Defaults to 0.0.

``--scale-blur=<value>``, ``--scale-wblur=<value>``, ``--cscale-blur=<value>``, ``--cscale-wblur=<value>``, ``--dscale-blur=<value>``, ``--dscale-wblur=<value>``, ``--tscale-blur=<value>``, ``--tscale-wblur=<value>``
    Kernel/window scaling factor (also known as a blur factor). Decreasing this
    makes the result sharper, increasing it makes it blurrier (default 0). If
    set to 0, the kernel's preferred blur factor is used. Note that setting
    this too low (eg. 0.5) leads to bad results. It's generally recommended to
    stick to values between 0.8 and 1.2.

``--scale-clamp=<0.0-1.0>``, ``--cscale-clamp``, ``--dscale-clamp``, ``--tscale-clamp``
    Specifies a weight bias to multiply into negative coefficients. Specifying
    ``--scale-clamp=1`` has the effect of removing negative weights completely,
    thus effectively clamping the value range to [0-1]. Values between 0.0 and
    1.0 can be specified to apply only a moderate diminishment of negative
    weights. This is especially useful for ``--tscale``, where it reduces
    excessive ringing artifacts in the temporal domain (which typically
    manifest themselves as short flashes or fringes of black, mostly around
    moving edges) in exchange for potentially adding more blur. The default for
    ``--tscale-clamp`` is 1.0, the others default to 0.0.

``--scale-cutoff=<value>``, ``--cscale-cutoff=<value>``, ``--dscale-cutoff=<value>``
    Cut off the filter kernel prematurely once the value range drops below
    this threshold. Doing so allows more aggressive pruning of skippable
    coefficients by disregarding parts of the LUT which are effectively zeroed
    out by the window function. Only affects polar (EWA) filters. The default
    is 0.001 for each, which is perceptually transparent but provides a 10%-20%
    speedup, depending on the exact radius and filter kernel chosen.

``--scale-taper=<value>``, ``--scale-wtaper=<value>``, ``--dscale-taper=<value>``, ``--dscale-wtaper=<value>``, ``--cscale-taper=<value>``, ``--cscale-wtaper=<value>``, ``--tscale-taper=<value>``, ``--tscale-wtaper=<value>``
    Kernel/window taper factor. Increasing this flattens the filter function.
    Value range is 0 to 1. A value of 0 (the default) means no flattening, a
    value of 1 makes the filter completely flat (equivalent to a box function).
    Values in between mean that some portion will be flat and the actual filter
    function will be squeezed into the space in between.

``--scale-radius=<value>``, ``--cscale-radius=<value>``, ``--dscale-radius=<value>``, ``--tscale-radius=<value>``
    Set radius for tunable filters, must be a float number between 0.5 and
    16.0. Defaults to the filter's preferred radius if not specified. Doesn't
    work for every scaler and VO combination.

    Note that depending on filter implementation details and video scaling
    ratio, the radius that actually being used might be different (most likely
    being increased a bit).

``--scale-antiring=<value>``, ``--cscale-antiring=<value>``, ``--dscale-antiring=<value>``, ``--tscale-antiring=<value>``
    Set the antiringing strength. This tries to eliminate ringing, but can
    introduce other artifacts in the process. Must be a float number between
    0.0 and 1.0. The default value of 0.0 disables antiringing entirely.

    Note that this doesn't affect the special filters ``bilinear`` and
    ``bicubic_fast``, nor does it affect any polar (EWA) scalers.

``--scale-window=<window>``, ``--cscale-window=<window>``, ``--dscale-window=<window>``, ``--tscale-window=<window>``
    (Advanced users only) Choose a custom windowing function for the kernel.
    Defaults to the filter's preferred window if unset. Use
    ``--scale-window=help`` to get a list of supported windowing functions.

``--scale-wparam=<window>``, ``--cscale-wparam=<window>``, ``--cscale-wparam=<window>``, ``--tscale-wparam=<window>``
    (Advanced users only) Configure the parameter for the window function given
    by ``--scale-window`` etc. Ignored if the window is not tunable. Currently,
    this affects the following window parameters:

    kaiser
        Window parameter (alpha). Defaults to 6.33.
    blackman
        Window parameter (alpha). Defaults to 0.16.
    gaussian
        Scale parameter (t). Increasing this makes the window wider. Defaults
        to 1.

``--scaler-lut-size=<4..10>``
    Set the size of the lookup texture for scaler kernels (default: 6). The
    actual size of the texture is ``2^N`` for an option value of ``N``. So the
    lookup texture with the default setting uses 64 samples.

    All weights are linearly interpolated from those samples, so increasing
    the size of lookup table might improve the accuracy of scaler.

``--scaler-resizes-only``
    Disable the scaler if the video image is not resized. In that case,
    ``bilinear`` is used instead of whatever is set with ``--scale``. Bilinear
    will reproduce the source image perfectly if no scaling is performed.
    Enabled by default. Note that this option never affects ``--cscale``.

``--linear-scaling``
    Scale in linear light. It should only be used with a
    ``--fbo-format`` that has at least 16 bit precision. This option
    has no effect on HDR content.

``--correct-downscaling``
    When using convolution based filters, extend the filter size when
    downscaling. Increases quality, but reduces performance while downscaling.

    This will perform slightly sub-optimally for anamorphic video (but still
    better than without it) since it will extend the size to match only the
    milder of the scale factors between the axes.

``--interpolation``
    Reduce stuttering caused by mismatches in the video fps and display refresh
    rate (also known as judder).

    .. warning:: This requires setting the ``--video-sync`` option to one
                 of the ``display-`` modes, or it will be silently disabled.
                 This was not required before mpv 0.14.0.

    This essentially attempts to interpolate the missing frames by convoluting
    the video along the temporal axis. The filter used can be controlled using
    the ``--tscale`` setting.

``--interpolation-threshold=<0..1,-1>``
    Threshold below which frame ratio interpolation gets disabled (default:
    ``0.0001``). This is calculated as ``abs(disphz/vfps - 1) < threshold``,
    where ``vfps`` is the speed-adjusted video FPS, and ``disphz`` the
    display refresh rate. (The speed-adjusted video FPS is roughly equal to
    the normal video FPS, but with slowdown and speedup applied. This matters
    if you use ``--video-sync=display-resample`` to make video run synchronously
    to the display FPS, or if you change the ``speed`` property.)

    The default is intended to almost always enable interpolation if the
    playback rate is even slightly different from the display refresh rate. But
    note that if you use e.g. ``--video-sync=display-vdrop``, small deviations
    in the rate can disable interpolation and introduce a discontinuity every
    other minute.

    Set this to ``-1`` to disable this logic.

``--opengl-pbo``
    Enable use of PBOs. On some drivers this can be faster, especially if the
    source video size is huge (e.g. so called "4K" video). On other drivers it
    might be slower or cause latency issues.

``--dither-depth=<N|no|auto>``
    Set dither target depth to N. Default: no.

    no
        Disable any dithering done by mpv.
    auto
        Automatic selection. If output bit depth cannot be detected, 8 bits per
        component are assumed.
    8
        Dither to 8 bit output.

    Note that the depth of the connected video display device cannot be
    detected. Often, LCD panels will do dithering on their own, which conflicts
    with this option and leads to ugly output.

``--dither-size-fruit=<2-8>``
    Set the size of the dither matrix (default: 6). The actual size of the
    matrix is ``(2^N) x (2^N)`` for an option value of ``N``, so a value of 6
    gives a size of 64x64. The matrix is generated at startup time, and a large
    matrix can take rather long to compute (seconds).

    Used in ``--dither=fruit`` mode only.

``--dither=<fruit|ordered|no>``
    Select dithering algorithm (default: fruit). (Normally, the
    ``--dither-depth`` option controls whether dithering is enabled.)

``--temporal-dither``
    Enable temporal dithering. (Only active if dithering is enabled in
    general.) This changes between 8 different dithering patterns on each frame
    by changing the orientation of the tiled dithering matrix. Unfortunately,
    this can lead to flicker on LCD displays, since these have a high reaction
    time.

``--temporal-dither-period=<1-128>``
    Determines how often the dithering pattern is updated when
    ``--temporal-dither`` is in use. 1 (the default) will update on every video
    frame, 2 on every other frame, etc.

``--gpu-debug``
    Enables GPU debugging. What this means depends on the API type. For OpenGL,
    it calls ``glGetError()``, and requests a debug context. For Vulkan, it
    enables validation layers.

``--opengl-swapinterval=<n>``
    Interval in displayed frames between two buffer swaps. 1 is equivalent to
    enable VSYNC, 0 to disable VSYNC. Defaults to 1 if not specified.

    Note that this depends on proper OpenGL vsync support. On some platforms
    and drivers, this only works reliably when in fullscreen mode. It may also
    require driver-specific hacks if using multiple monitors, to ensure mpv
    syncs to the right one. Compositing window managers can also lead to bad
    results, as can missing or incorrect display FPS information (see
    ``--display-fps``).

``--vulkan-swap-mode=<mode>``
    Controls the presentation mode of the vulkan swapchain. This is similar
    to the ``--opengl-swapinterval`` option.

    auto
        Use the preferred swapchain mode for the vulkan context. (Default)
    fifo
        Non-tearing, vsync blocked. Similar to "VSync on".
    fifo-relaxed
        Tearing, vsync blocked. Late frames will tear instead of stuttering.
    mailbox
        Non-tearing, not vsync blocked. Similar to "triple buffering".
    immediate
        Tearing, not vsync blocked. Similar to "VSync off".

``--vulkan-queue-count=<1..8>``
    Controls the number of VkQueues used for rendering (limited by how many
    your device supports). In theory, using more queues could enable some
    parallelism between frames (when using a ``--swapchain-depth`` higher than
    1), but it can also slow things down on hardware where there's no true
    parallelism between queues. (Default: 1)

``--vulkan-async-transfer``
    Enables the use of async transfer queues on supported vulkan devices. Using
    them allows transfer operations like texture uploads and blits to happen
    concurrently with the actual rendering, thus improving overall throughput
    and power consumption. Enabled by default, and should be relatively safe.

``--vulkan-async-compute``
    Enables the use of async compute queues on supported vulkan devices. Using
    this, in theory, allows out-of-order scheduling of compute shaders with
    graphics shaders, thus enabling the hardware to do more effective work while
    waiting for pipeline bubbles and memory operations. Not beneficial on all
    GPUs. It's worth noting that if async compute is enabled, and the device
    supports more compute queues than graphics queues (bound by the restrictions
    set by ``--vulkan-queue-count``), mpv will internally try and prefer the
    use of compute shaders over fragment shaders wherever possible. Not enabled
    by default, since it seems to cause issues with some drivers.

``--d3d11-warp=<yes|no|auto>``
    Use WARP (Windows Advanced Rasterization Platform) with the D3D11 GPU
    backend (default: auto). This is a high performance software renderer. By
    default, it is only used when the system has no hardware adapters that
    support D3D11. While the extended GPU features will work with WARP, they
    can be very slow.

``--d3d11-feature-level=<12_1|12_0|11_1|11_0|10_1|10_0|9_3|9_2|9_1>``
    Select a specific feature level when using the D3D11 GPU backend. By
    default, the highest available feature level is used. This option can be
    used to select a lower feature level, which is mainly useful for debugging.
    Most extended GPU features will not work at 9_x feature levels.

``--d3d11-flip=<yes|no>``
    Enable flip-model presentation, which avoids unnecessarily copying the
    backbuffer by sharing surfaces with the DWM (default: yes). This may cause
    performance issues with older drivers. If flip-model presentation is not
    supported (for example, on Windows 7 without the platform update), mpv will
    automatically fall back to the older bitblt presentation model.

``--d3d11-sync-interval=<0..4>``
    Schedule each frame to be presented for this number of VBlank intervals.
    (default: 1) Setting to 1 will enable VSync, setting to 0 will disable it.

``--d3d11va-zero-copy=<yes|no>``
    By default, when using hardware decoding with ``--gpu-api=d3d11``, the
    video image will be copied (GPU-to-GPU) from the decoder surface to a
    shader resource. Set this option to avoid that copy by sampling directly
    from the decoder image. This may increase performance and reduce power
    usage, but can cause the image to be sampled incorrectly on the bottom and
    right edges due to padding, and may invoke driver bugs, since Direct3D 11
    technically does not allow sampling from a decoder surface (though most
    drivers support it.)

    Currently only relevant for ``--gpu-api=d3d11``.

``--spirv-compiler=<compiler>``
    Controls which compiler is used to translate GLSL to SPIR-V. This is
    (currently) only relevant for ``--gpu-api=vulkan``. The possible choices
    are:

    auto
        Use the first available compiler. (Default)
    shaderc
        Use libshaderc, which is an API wrapper around glslang. This is
        generally the most preferred, if available.
    nvidia
        Use nvidia's built-in compiler. Only works for nvidia GPUs. Can be
        buggy, but also supports some features glslang does not. Only works
        with vulkan.

``--glsl-shaders=<file-list>``
    Custom GLSL hooks. These are a flexible way to add custom fragment shaders,
    which can be injected at almost arbitrary points in the rendering pipeline,
    and access all previous intermediate textures. Each use of the option will
    add another file to the internal list of shaders (see `List Options`_).

    .. admonition:: Warning

        The syntax is not stable yet and may change any time.

    The general syntax of a user shader looks like this::

        //!METADATA ARGS...
        //!METADATA ARGS...

        vec4 hook() {
           ...
           return something;
        }

        //!METADATA ARGS...
        //!METADATA ARGS...

        ...

    Each section of metadata, along with the non-metadata lines after it,
    defines a single block. There are currently two types of blocks, HOOKs and
    TEXTUREs.

    A ``TEXTURE`` block can set the following options:

    TEXTURE <name> (required)
        The name of this texture. Hooks can then bind the texture under this
        name using BIND. This must be the first option of the texture block.

    SIZE <width> [<height>] [<depth>] (required)
        The dimensions of the texture. The height and depth are optional. The
        type of texture (1D, 2D or 3D) depends on the number of components
        specified.

    FORMAT <name> (required)
        The texture format for the samples. Supported texture formats are listed
        in debug logging when the ``gpu`` VO is initialized (look for
        ``Texture formats:``). Usually, this follows OpenGL naming conventions.
        For example, ``rgb16`` provides 3 channels with normalized 16 bit
        components. One oddity are float formats: for example, ``rgba16f`` has
        16 bit internal precision, but the texture data is provided as 32 bit
        floats, and the driver converts the data on texture upload.

        Although format names follow a common naming convention, not all of them
        are available on all hardware, drivers, GL versions, and so on.

    FILTER <LINEAR|NEAREST>
        The min/magnification filter used when sampling from this texture.

    BORDER <CLAMP|REPEAT|MIRROR>
        The border wrapping mode used when sampling from this texture.

    Following the metadata is a string of bytes in hexadecimal notation that
    define the raw texture data, corresponding to the format specified by
    `FORMAT`, on a single line with no extra whitespace.

    A ``HOOK`` block can set the following options:

    HOOK <name> (required)
        The texture which to hook into. May occur multiple times within a
        metadata block, up to a predetermined limit. See below for a list of
        hookable textures.

    DESC <title>
        User-friendly description of the pass. This is the name used when
        representing this shader in the list of passes for property
        `vo-passes`.

    BIND <name>
        Loads a texture (either coming from mpv or from a ``TEXTURE`` block)
        and makes it available to the pass. When binding textures from mpv,
        this will also set up macros to facilitate accessing it properly. See
        below for a list. By default, no textures are bound. The special name
        HOOKED can be used to refer to the texture that triggered this pass.

    SAVE <name>
        Gives the name of the texture to save the result of this pass into. By
        default, this is set to the special name HOOKED which has the effect of
        overwriting the hooked texture.

    WIDTH <szexpr>, HEIGHT <szexpr>
        Specifies the size of the resulting texture for this pass. ``szexpr``
        refers to an expression in RPN (reverse polish notation), using the
        operators + - * / > < !, floating point literals, and references to
        sizes of existing texture (such as MAIN.width or CHROMA.height),
        OUTPUT, or NATIVE_CROPPED (size of an input texture cropped after
        pan-and-scan, video-align-x/y, video-pan-x/y, etc. and possibly
        prescaled). By default, these are set to HOOKED.w and HOOKED.h,
        espectively.

    WHEN <szexpr>
        Specifies a condition that needs to be true (non-zero) for the shader
        stage to be evaluated. If it fails, it will silently be omitted. (Note
        that a shader stage like this which has a dependency on an optional
        hook point can still cause that hook point to be saved, which has some
        minor overhead)

    OFFSET <ox> <oy>
        Indicates a pixel shift (offset) introduced by this pass. These pixel
        offsets will be accumulated and corrected during the next scaling pass
        (``cscale`` or ``scale``). The default values are 0 0 which correspond
        to no shift. Note that offsets are ignored when not overwriting the
        hooked texture.

    COMPONENTS <n>
        Specifies how many components of this pass's output are relevant and
        should be stored in the texture, up to 4 (rgba). By default, this value
        is equal to the number of components in HOOKED.

    COMPUTE <bw> <bh> [<tw> <th>]
        Specifies that this shader should be treated as a compute shader, with
        the block size bw and bh. The compute shader will be dispatched with
        however many blocks are necessary to completely tile over the output.
        Within each block, there will bw tw*th threads, forming a single work
        group. In other words: tw and th specify the work group size, which can
        be different from the block size. So for example, a compute shader with
        bw, bh = 32 and tw, th = 8 running on a 500x500 texture would dispatch
        16x16 blocks (rounded up), each with 8x8 threads.

        Compute shaders in mpv are treated a bit different from fragment
        shaders. Instead of defining a ``vec4 hook`` that produces an output
        sample, you directly define ``void hook`` which writes to a fixed
        writeonly image unit named ``out_image`` (this is bound by mpv) using
        `imageStore`. To help translate texture coordinates in the absence of
        vertices, mpv provides a special function ``NAME_map(id)`` to map from
        the texel space of the output image to the texture coordinates for all
        bound textures. In particular, ``NAME_pos`` is equivalent to
        ``NAME_map(gl_GlobalInvocationID)``, although using this only really
        makes sense if (tw,th) == (bw,bh).

    Each bound mpv texture (via ``BIND``) will make available the following
    definitions to that shader pass, where NAME is the name of the bound
    texture:

    vec4 NAME_tex(vec2 pos)
        The sampling function to use to access the texture at a certain spot
        (in texture coordinate space, range [0,1]). This takes care of any
        necessary normalization conversions.
    vec4 NAME_texOff(vec2 offset)
        Sample the texture at a certain offset in pixels. This works like
        NAME_tex but additionally takes care of necessary rotations, so that
        sampling at e.g. vec2(-1,0) is always one pixel to the left.
    vec2 NAME_pos
        The local texture coordinate of that texture, range [0,1].
    vec2 NAME_size
        The (rotated) size in pixels of the texture.
    mat2 NAME_rot
        The rotation matrix associated with this texture. (Rotates pixel space
        to texture coordinates)
    vec2 NAME_pt
        The (unrotated) size of a single pixel, range [0,1].
    float NAME_mul
        The coefficient that needs to be multiplied into the texture contents
        in order to normalize it to the range [0,1].
    sampler NAME_raw
        The raw bound texture itself. The use of this should be avoided unless
        absolutely necessary.

    Normally, users should use either NAME_tex or NAME_texOff to read from the
    texture. For some shaders however , it can be better for performance to do
    custom sampling from NAME_raw, in which case care needs to be taken to
    respect NAME_mul and NAME_rot.

    In addition to these parameters, the following uniforms are also globally
    available:

    float random
        A random number in the range [0-1], different per frame.
    int frame
        A simple count of frames rendered, increases by one per frame and never
        resets (regardless of seeks).
    vec2 input_size
        The size in pixels of the input image (possibly cropped and prescaled).
    vec2 target_size
        The size in pixels of the visible part of the scaled (and possibly
        cropped) image.
    vec2 tex_offset
        Texture offset introduced by user shaders or options like panscan, video-align-x/y, video-pan-x/y.

    Internally, vo_gpu may generate any number of the following textures.
    Whenever a texture is rendered and saved by vo_gpu, all of the passes
    that have hooked into it will run, in the order they were added by the
    user. This is a list of the legal hook points:

    RGB, LUMA, CHROMA, ALPHA, XYZ (resizable)
        Source planes (raw). Which of these fire depends on the image format of
        the source.

    CHROMA_SCALED, ALPHA_SCALED (fixed)
        Source planes (upscaled). These only fire on subsampled content.

    NATIVE (resizable)
        The combined image, in the source colorspace, before conversion to RGB.

    MAINPRESUB (resizable)
        The image, after conversion to RGB, but before
        ``--blend-subtitles=video`` is applied.

    MAIN (resizable)
        The main image, after conversion to RGB but before upscaling.

    LINEAR (fixed)
        Linear light image, before scaling. This only fires when
        ``--linear-scaling`` is in effect.

    SIGMOID (fixed)
        Sigmoidized light, before scaling. This only fires when
        ``--sigmoid-upscaling`` is in effect.

    PREKERNEL (fixed)
        The image immediately before the scaler kernel runs.

    POSTKERNEL (fixed)
        The image immediately after the scaler kernel runs.

    SCALED (fixed)
        The final upscaled image, before color management.

    OUTPUT (fixed)
        The final output image, after color management but before dithering and
        drawing to screen.

    Only the textures labelled with ``resizable`` may be transformed by the
    pass. When overwriting a texture marked ``fixed``, the WIDTH, HEIGHT and
    OFFSET must be left at their default values.

``--glsl-shader=<file>``
    CLI/config file only alias for ``--glsl-shaders-append``.

``--deband``
    Enable the debanding algorithm. This greatly reduces the amount of visible
    banding, blocking and other quantization artifacts, at the expense of
    very slightly blurring some of the finest details. In practice, it's
    virtually always an improvement - the only reason to disable it would be
    for performance.

``--deband-iterations=<1..16>``
    The number of debanding steps to perform per sample. Each step reduces a
    bit more banding, but takes time to compute. Note that the strength of each
    step falls off very quickly, so high numbers (>4) are practically useless.
    (Default 1)

``--deband-threshold=<0..4096>``
    The debanding filter's cut-off threshold. Higher numbers increase the
    debanding strength dramatically but progressively diminish image details.
    (Default 64)

``--deband-range=<1..64>``
    The debanding filter's initial radius. The radius increases linearly for
    each iteration. A higher radius will find more gradients, but a lower
    radius will smooth more aggressively. (Default 16)

    If you increase the ``--deband-iterations``, you should probably decrease
    this to compensate.

``--deband-grain=<0..4096>``
    Add some extra noise to the image. This significantly helps cover up
    remaining quantization artifacts. Higher numbers add more noise. (Default
    48)

``--sigmoid-upscaling``
    When upscaling, use a sigmoidal color transform to avoid emphasizing
    ringing artifacts. This also implies ``--linear-scaling``.

``--sigmoid-center``
    The center of the sigmoid curve used for ``--sigmoid-upscaling``, must be a
    float between 0.0 and 1.0. Defaults to 0.75 if not specified.

``--sigmoid-slope``
    The slope of the sigmoid curve used for ``--sigmoid-upscaling``, must be a
    float between 1.0 and 20.0. Defaults to 6.5 if not specified.

``--sharpen=<value>``
    If set to a value other than 0, enable an unsharp masking filter. Positive
    values will sharpen the image (but add more ringing and aliasing). Negative
    values will blur the image. If your GPU is powerful enough, consider
    alternatives like the ``ewa_lanczossharp`` scale filter, or the
    ``--scale-blur`` option.

``--opengl-glfinish``
    Call ``glFinish()`` before swapping buffers (default: disabled). Slower,
    but might improve results when doing framedropping. Can completely ruin
    performance. The details depend entirely on the OpenGL driver.

``--opengl-waitvsync``
    Call ``glXWaitVideoSyncSGI`` after each buffer swap (default: disabled).
    This may or may not help with video timing accuracy and frame drop. It's
    possible that this makes video output slower, or has no effect at all.

    X11/GLX only.

``--opengl-dwmflush=<no|windowed|yes|auto>``
    Calls ``DwmFlush`` after swapping buffers on Windows (default: auto). It
    also sets ``SwapInterval(0)`` to ignore the OpenGL timing. Values are: no
    (disabled), windowed (only in windowed mode), yes (also in full screen).

    The value ``auto`` will try to determine whether the compositor is active,
    and calls ``DwmFlush`` only if it seems to be.

    This may help to get more consistent frame intervals, especially with
    high-fps clips - which might also reduce dropped frames. Typically, a value
    of ``windowed`` should be enough, since full screen may bypass the DWM.

    Windows only.

``--angle-d3d11-feature-level=<11_0|10_1|10_0|9_3>``
    Selects a specific feature level when using the ANGLE backend with D3D11.
    By default, the highest available feature level is used. This option can be
    used to select a lower feature level, which is mainly useful for debugging.
    Note that OpenGL ES 3.0 is only supported at feature level 10_1 or higher.
    Most extended OpenGL features will not work at lower feature levels
    (similar to ``--gpu-dumb-mode``).

    Windows with ANGLE only.

``--angle-d3d11-warp=<yes|no|auto>``
    Use WARP (Windows Advanced Rasterization Platform) when using the ANGLE
    backend with D3D11 (default: auto). This is a high performance software
    renderer. By default, it is used when the Direct3D hardware does not
    support Direct3D 11 feature level 9_3. While the extended OpenGL features
    will work with WARP, they can be very slow.

    Windows with ANGLE only.

``--angle-egl-windowing=<yes|no|auto>``
    Use ANGLE's built in EGL windowing functions to create a swap chain
    (default: auto). If this is set to ``no`` and the D3D11 renderer is in use,
    ANGLE's built in swap chain will not be used and a custom swap chain that
    is optimized for video rendering will be created instead. If set to
    ``auto``, a custom swap chain will be used for D3D11 and the built in swap
    chain will be used for D3D9. This option is mainly for debugging purposes,
    in case the custom swap chain has poor performance or does not work.

    If set to ``yes``, the ``--angle-max-frame-latency``,
    ``--angle-swapchain-length`` and ``--angle-flip`` options will have no
    effect.

    Windows with ANGLE only.

``--angle-flip=<yes|no>``
    Enable flip-model presentation, which avoids unnecessarily copying the
    backbuffer by sharing surfaces with the DWM (default: yes). This may cause
    performance issues with older drivers. If flip-model presentation is not
    supported (for example, on Windows 7 without the platform update), mpv will
    automatically fall back to the older bitblt presentation model.

    If set to ``no``, the ``--angle-swapchain-length`` option will have no
    effect.

    Windows with ANGLE only.

``--angle-renderer=<d3d9|d3d11|auto>``
    Forces a specific renderer when using the ANGLE backend (default: auto). In
    auto mode this will pick D3D11 for systems that support Direct3D 11 feature
    level 9_3 or higher, and D3D9 otherwise. This option is mainly for
    debugging purposes. Normally there is no reason to force a specific
    renderer, though ``--angle-renderer=d3d9`` may give slightly better
    performance on old hardware. Note that the D3D9 renderer only supports
    OpenGL ES 2.0, so most extended OpenGL features will not work if this
    renderer is selected (similar to ``--gpu-dumb-mode``).

    Windows with ANGLE only.

``--cocoa-force-dedicated-gpu=<yes|no>``
    Deactivates the automatic graphics switching and forces the dedicated GPU.
    (default: no)

    OS X only.

``--cocoa-cb-sw-renderer=<yes|no|auto>``
    Use the Apple Software Renderer when using cocoa-cb (default: auto). If set
    to ``no`` the software renderer is never used and instead fails when a the
    usual pixel format could not be created, ``yes`` will always only use the
    software renderer, and ``auto`` only falls back to the software renderer
    when the usual pixel format couldn't be created.

    OS X only.

``--macos-title-bar-style=<dark|ultradark|light|mediumlight|auto>``
    Sets the styling of the title bar (default: dark).
    OS X and cocoa-cb only

    :dark:        Dark title bar with vibrancy, a subtle blurring effect that
                  dynamically blends the background (Video) into the title bar.
    :ultradark:   Darker title bar with vibrancy (like QuickTime Player).
    :light:       Bright title bar with vibrancy.
    :mediumlight: Less bright title bar with vibrancy.
    :auto:        Detects the system settings and sets the title bar styling
                  appropriately, either ultradark or mediumlight.

``--macos-fs-animation-duration=<default|0-1000>``
    Sets the fullscreen resize animation duration in ms (default: default).
    The default value is slightly less than the system's animation duration
    (500ms) to prevent some problems when the end of an async animation happens
    at the same time as the end of the system wide fullscreen animation. Setting
    anything higher than 500ms will only prematurely cancel the resize animation
    after the system wide animation ended. The upper limit is still set at
    1000ms since it's possible that Apple or the user changes the system
    defaults. Anything higher than 1000ms though seems too long and shouldn't be
    set anyway.
    OS X and cocoa-cb only

``--android-surface-size=<WxH>``
    Set dimensions of the rendering surface used by the Android gpu context.
    Needs to be set by the embedding application if the dimensions change during
    runtime (i.e. if the device is rotated), via the surfaceChanged callback.

    Android with ``--gpu-context=android`` only.

``--swapchain-depth=<N>``
    Allow up to N in-flight frames. This essentially controls the frame
    latency. Increasing the swapchain depth can improve pipelining and prevent
    missed vsyncs, but increases visible latency. This option only mandates an
    upper limit, the implementation can use a lower latency than requested
    internally. A setting of 1 means that the VO will wait for every frame to
    become visible before starting to render the next frame. (Default: 3)

``--gpu-sw``
    Continue even if a software renderer is detected.

``--gpu-context=<sys>``
    The value ``auto`` (the default) selects the GPU context. You can also pass
    ``help`` to get a complete list of compiled in backends (sorted by
    autoprobe order).

    auto
        auto-select (default)
    cocoa
        Cocoa/OS X (deprecated, use --vo=opengl-cb instead)
    win
        Win32/WGL
    winvk
        VK_KHR_win32_surface
    angle
        Direct3D11 through the OpenGL ES translation layer ANGLE. This supports
        almost everything the ``win`` backend does (if the ANGLE build is new
        enough).
    dxinterop (experimental)
        Win32, using WGL for rendering and Direct3D 9Ex for presentation. Works
        on Nvidia and AMD. Newer Intel chips with the latest drivers may also
        work.
    d3d11
        Win32, with native Direct3D 11 rendering.
    x11
        X11/GLX
    x11vk
        VK_KHR_xlib_surface
    x11probe
        For internal autoprobing, equivalent to ``x11`` otherwise. Don't use
        directly, it could be removed without warning as autoprobing is changed.
    wayland
        Wayland/EGL
    waylandvk
        VK_KHR_wayland_surface
    drm
        DRM/EGL
    x11egl
        X11/EGL
    android
        Android/EGL. Requires ``--wid`` be set to an ``android.view.Surface``.
    mali-fbdev
        Direct fbdev/EGL support on some ARM/MALI devices.
    vdpauglx
        Use vdpau presentation with GLX as backing. Experimental use only.
        Using this will have no advantage (other than additional bugs or
        performance problems), and is for doing experiments only. Will not
        be used automatically.

``--gpu-api=<type>``
    Controls which type of graphics APIs will be accepted:

    auto
        Use any available API (default)
    opengl
        Allow only OpenGL (requires OpenGL 2.1+ or GLES 2.0+)
    vulkan
        Allow only Vulkan (requires a valid/working ``--spirv-compiler``)
    d3d11
        Allow only ``--gpu-context=d3d11``

``--opengl-es=<mode>``
    Controls which type of OpenGL context will be accepted:

    auto
        Allow all types of OpenGL (default)
    yes
        Only allow GLES
    no
        Only allow desktop/core GL

``--opengl-restrict=<version>``
    Restricts all OpenGL versions above a certain version. Versions are encoded
    in hundreds, i.e. OpenGL 4.5 -> 450. As an example, --opengl-restrict=300
    would restrict OpenGL 3.0 and higher, effectively only allowing 2.x
    contexts. Note that this only imposes a limit on context creation APIs, the
    actual OpenGL context may still have a higher OpenGL version. (Default: 0)

``--fbo-format=<fmt>``
    Selects the internal format of textures used for FBOs. The format can
    influence performance and quality of the video output. ``fmt`` can be one
    of: rgb8, rgb10, rgb10_a2, rgb16, rgb16f, rgb32f, rgba12, rgba16, rgba16f,
    rgba16hf, rgba32f.

    Default: ``auto``, which first attempts to utilize 16bit float
    (rgba16f, rgba16hf), and falls back to rgba16 if those are not available.
    Finally, attempts to utilize rgb10_a2 or rgba8 if all of the previous formats
    are not available.

``--gamma-factor=<0.1..2.0>``
    Set an additional raw gamma factor (default: 1.0). If gamma is adjusted in
    other ways (like with the ``--gamma`` option or key bindings and the
    ``gamma`` property), the value is multiplied with the other gamma value.

    Recommended values based on the environmental brightness:

    1.0
        Pitch black or dimly lit room (default)
    1.1
        Moderately lit room, home
    1.2
        Brightly illuminated room, office

    NOTE: This is based around the assumptions of typical movie content, which
    contains an implicit end-to-end of about 0.8 from scene to display. For
    bright environments it can be useful to cancel that out.

``--gamma-auto``
    Automatically corrects the gamma value depending on ambient lighting
    conditions (adding a gamma boost for bright rooms).

    With ambient illuminance of 16 lux, mpv will pick the 1.0 gamma value (no
    boost), and slightly increase the boost up until 1.2 for 256 lux.

    NOTE: Only implemented on OS X.

``--target-prim=<value>``
    Specifies the primaries of the display. Video colors will be adapted to
    this colorspace when ICC color management is not being used. Valid values
    are:

    auto
        Disable any adaptation, except for atypical color spaces. Specifically,
        wide/unusual gamuts get automatically adapted to BT.709, while standard
        gamut (i.e. BT.601 and BT.709) content is not touched. (default)
    bt.470m
        ITU-R BT.470 M
    bt.601-525
        ITU-R BT.601 (525-line SD systems, eg. NTSC), SMPTE 170M/240M
    bt.601-625
        ITU-R BT.601 (625-line SD systems, eg. PAL/SECAM), ITU-R BT.470 B/G
    bt.709
        ITU-R BT.709 (HD), IEC 61966-2-4 (sRGB), SMPTE RP177 Annex B
    bt.2020
        ITU-R BT.2020 (UHD)
    apple
        Apple RGB
    adobe
        Adobe RGB (1998)
    prophoto
        ProPhoto RGB (ROMM)
    cie1931
        CIE 1931 RGB (not to be confused with CIE XYZ)
    dci-p3
        DCI-P3 (Digital Cinema Colorspace), SMPTE RP431-2
    v-gamut
        Panasonic V-Gamut (VARICAM) primaries
    s-gamut
        Sony S-Gamut (S-Log) primaries

``--target-trc=<value>``
    Specifies the transfer characteristics (gamma) of the display. Video colors
    will be adjusted to this curve when ICC color management is not being used.
    Valid values are:

    auto
        Disable any adaptation, except for atypical transfers. Specifically,
        HDR or linear light source material gets automatically converted to
        gamma 2.2, while SDR content is not touched. (default)
    bt.1886
        ITU-R BT.1886 curve (assuming infinite contrast)
    srgb
        IEC 61966-2-4 (sRGB)
    linear
        Linear light output
    gamma1.8
        Pure power curve (gamma 1.8), also used for Apple RGB
    gamma2.2
        Pure power curve (gamma 2.2)
    gamma2.8
        Pure power curve (gamma 2.8), also used for BT.470-BG
    prophoto
        ProPhoto RGB (ROMM)
    pq
        ITU-R BT.2100 PQ (Perceptual quantizer) curve, aka SMPTE ST2084
    hlg
        ITU-R BT.2100 HLG (Hybrid Log-gamma) curve, aka ARIB STD-B67
    v-log
        Panasonic V-Log (VARICAM) curve
    s-log1
        Sony S-Log1 curve
    s-log2
        Sony S-Log2 curve

    .. note::

        When using HDR output formats, mpv will encode to the specified
        curve but it will not set any HDMI flags or other signalling that might
        be required for the target device to correctly display the HDR signal.
        The user should independently guarantee this before using these signal
        formats for display.

``--target-peak=<nits>``
    Specifies the measured peak brightness of the output display, in cd/m^2
    (AKA nits). The interpretation of this brightness depends on the configured
    ``--target-trc``. In all cases, it imposes a limit on the signal values
    that will be sent to the display. If the source exceeds this brightness
    level, a tone mapping filter will be inserted. For HLG, it has the
    additional effect of parametrizing the inverse OOTF, in order to get
    colorimetrically consistent results with the mastering display. For SDR, or
    when using an ICC (profile (``--icc-profile``), setting this to a value
    above 100 essentially causes the display to be treated as if it were an HDR
    display in disguise. (See the note below)

    By default, the chosen peak defaults to an appropriate value based on the
    TRC in use. For SDR curves, it defaults to 100. For HDR curves, it
    defaults to 100 * the transfer function's nominal peak.

    .. note::

        When using an SDR transfer function, this is normally not needed, and
        setting it may lead to very unexpected results. The one time it *is*
        useful is if you want to calibrate a HDR display using traditional
        transfer functions and calibration equipment. In such cases, you can
        set your HDR display to a high brightness such as 800 cd/m^2, and then
        calibrate it to a standard curve like gamma2.8. Setting this value to
        800 would then instruct mpv to essentially treat it as an HDR display
        with the given peak. This may be a good alternative in environments
        where PQ or HLG input to the display is not possible, and makes it
        possible to use HDR displays with mpv regardless of operating system
        support for HDMI HDR metadata.

        In such a configuration, we highly recommend setting ``--tone-mapping``
        to ``mobius`` or even ``clip``.

``--tone-mapping=<value>``
    Specifies the algorithm used for tone-mapping images onto the target
    display. This is relevant for both HDR->SDR conversion as well as gamut
    reduction (e.g. playing back BT.2020 content on a standard gamut display).
    Valid values are:

    clip
        Hard-clip any out-of-range values. Use this when you care about
        perfect color accuracy for in-range values at the cost of completely
        distorting out-of-range values. Not generally recommended.
    mobius
        Generalization of Reinhard to a Möbius transform with linear section.
        Smoothly maps out-of-range values while retaining contrast and colors
        for in-range material as much as possible. Use this when you care about
        color accuracy more than detail preservation. This is somewhere in
        between ``clip`` and ``reinhard``, depending on the value of
        ``--tone-mapping-param``.
    reinhard
        Reinhard tone mapping algorithm. Very simple continuous curve.
        Preserves overall image brightness but uses nonlinear contrast, which
        results in flattening of details and degradation in color accuracy.
    hable
        Similar to ``reinhard`` but preserves both dark and bright details
        better (slightly sigmoidal), at the cost of slightly darkening /
        desaturating everything. Developed by John Hable for use in video
        games. Use this when you care about detail preservation more than
        color/brightness accuracy. This is roughly equivalent to
        ``--tone-mapping=reinhard --tone-mapping-param=0.24``. If possible,
        you should also enable ``--hdr-compute-peak`` for the best results.
        (Default)
    gamma
        Fits a logarithmic transfer between the tone curves.
    linear
        Linearly stretches the entire reference gamut to (a linear multiple of)
        the display.

``--tone-mapping-param=<value>``
    Set tone mapping parameters. Ignored if the tone mapping algorithm is not
    tunable. This affects the following tone mapping algorithms:

    clip
        Specifies an extra linear coefficient to multiply into the signal
        before clipping. Defaults to 1.0.
    mobius
        Specifies the transition point from linear to mobius transform. Every
        value below this point is guaranteed to be mapped 1:1. The higher the
        value, the more accurate the result will be, at the cost of losing
        bright details. Defaults to 0.3, which due to the steep initial slope
        still preserves in-range colors fairly accurately.
    reinhard
        Specifies the local contrast coefficient at the display peak. Defaults
        to 0.5, which means that in-gamut values will be about half as bright
        as when clipping.
    gamma
        Specifies the exponent of the function. Defaults to 1.8.
    linear
        Specifies the scale factor to use while stretching. Defaults to 1.0.

``--hdr-compute-peak=<auto|yes|no>``
    Compute the HDR peak and frame average brightness per-frame instead of
    relying on tagged metadata. These values are averaged over local regions as
    well as over several frames to prevent the value from jittering around too
    much. This option basically gives you dynamic, per-scene tone mapping.
    Requires compute shaders, which is a fairly recent OpenGL feature, and will
    probably also perform horribly on some drivers, so enable at your own risk.
    The special value ``auto`` (default) will enable HDR peak computation
    automatically if compute shaders and SSBOs are supported.

``--tone-mapping-desaturate=<value>``
    Apply desaturation for highlights. The parameter essentially controls the
    steepness of the desaturation curve. The higher the parameter, the more
    aggressively colors will be desaturated. This setting helps prevent
    unnaturally blown-out colors for super-highlights, by (smoothly) turning
    into white instead. This makes images feel more natural, at the cost of
    reducing information about out-of-range colors.

    The default of 0.5 provides a good balance. This value is weaker than the
    ACES ODT curves' recommendation, but works better for most content in
    practice. A setting of 0.0 disables this option.

``--gamut-warning``
    If enabled, mpv will mark all clipped/out-of-gamut pixels that exceed a
    given threshold (currently hard-coded to 101%). The affected pixels will be
    inverted to make them stand out. Note: This option applies after the
    effects of all of mpv's color space transformation / tone mapping options,
    so it's a good idea to combine this with ``--tone-mapping=clip`` and use
    ``--target-prim`` to set the gamut to simulate. For example,
    ``--target-prim=bt.709`` would make mpv highlight all pixels that exceed the
    gamut of a standard gamut (sRGB) display. This option also does not work
    well with ICC profiles, since the 3DLUTs are always generated against the
    source color space and have chromatically-accurate clipping built in.

``--use-embedded-icc-profile``
    Load the embedded ICC profile contained in media files such as PNG images.
    (Default: yes). Note that this option only works when also using a display
    ICC profile (``--icc-profile`` or ``--icc-profile-auto``), and also
    requires LittleCMS 2 support.

``--icc-profile=<file>``
    Load an ICC profile and use it to transform video RGB to screen output.
    Needs LittleCMS 2 support compiled in. This option overrides the
    ``--target-prim``, ``--target-trc`` and ``--icc-profile-auto`` options.

``--icc-profile-auto``
    Automatically select the ICC display profile currently specified by the
    display settings of the operating system.

    NOTE: On Windows, the default profile must be an ICC profile. WCS profiles
    are not supported.

    Applications using libmpv with the render API need to provide the ICC
    profile via ``MPV_RENDER_PARAM_ICC_PROFILE``.

``--icc-cache-dir=<dirname>``
    Store and load the 3D LUTs created from the ICC profile in this directory.
    This can be used to speed up loading, since LittleCMS 2 can take a while to
    create a 3D LUT. Note that these files contain uncompressed LUTs. Their
    size depends on the ``--icc-3dlut-size``, and can be very big.

    NOTE: This is not cleaned automatically, so old, unused cache files may
    stick around indefinitely.

``--icc-intent=<value>``
    Specifies the ICC intent used for the color transformation (when using
    ``--icc-profile``).

    0
        perceptual
    1
        relative colorimetric (default)
    2
        saturation
    3
        absolute colorimetric

``--icc-3dlut-size=<r>x<g>x<b>``
    Size of the 3D LUT generated from the ICC profile in each dimension.
    Default is 64x64x64. Sizes may range from 2 to 512.

``--icc-contrast=<0-1000000>``
    Specifies an upper limit on the target device's contrast ratio. This is
    detected automatically from the profile if possible, but for some profiles
    it might be missing, causing the contrast to be assumed as infinite. As a
    result, video may appear darker than intended. This only affects BT.1886
    content. The default of 0 means no limit.

``--blend-subtitles=<yes|video|no>``
    Blend subtitles directly onto upscaled video frames, before interpolation
    and/or color management (default: no). Enabling this causes subtitles to be
    affected by ``--icc-profile``, ``--target-prim``, ``--target-trc``,
    ``--interpolation``, ``--gamma-factor`` and ``--glsl-shaders``. It also
    increases subtitle performance when using ``--interpolation``.

    The downside of enabling this is that it restricts subtitles to the visible
    portion of the video, so you can't have subtitles exist in the black
    margins below a video (for example).

    If ``video`` is selected, the behavior is similar to ``yes``, but subs are
    drawn at the video's native resolution, and scaled along with the video.

    .. warning:: This changes the way subtitle colors are handled. Normally,
                 subtitle colors are assumed to be in sRGB and color managed as
                 such. Enabling this makes them treated as being in the video's
                 color space instead. This is good if you want things like
                 softsubbed ASS signs to match the video colors, but may cause
                 SRT subtitles or similar to look slightly off.

``--alpha=<blend-tiles|blend|yes|no>``
    Decides what to do if the input has an alpha component.

    blend-tiles
        Blend the frame against a 16x16 gray/white tiles background (default).
    blend
        Blend the frame against the background color (``--background``, normally
        black).
    yes
        Try to create a framebuffer with alpha component. This only makes sense
        if the video contains alpha information (which is extremely rare). May
        not be supported on all platforms. If alpha framebuffers are
        unavailable, it silently falls back on a normal framebuffer. Note that
        if you set the ``--fbo-format`` option to a non-default value, a
        format with alpha must be specified, or this won't work.
        This does not work on X11 with EGL and Mesa (freedesktop bug 67676).
    no
        Ignore alpha component.

``--opengl-rectangle-textures``
    Force use of rectangle textures (default: no). Normally this shouldn't have
    any advantages over normal textures. Note that hardware decoding overrides
    this flag. Could be removed any time.

``--background=<color>``
    Color used to draw parts of the mpv window not covered by video. See
    ``--osd-color`` option how colors are defined.

``--gpu-tex-pad-x``, ``--gpu-tex-pad-y``
    Enlarge the video source textures by this many pixels. For debugging only
    (normally textures are sized exactly, but due to hardware decoding interop
    we may have to deal with additional padding, which can be tested with these
    options). Could be removed any time.

``--opengl-early-flush=<yes|no|auto>``
    Call ``glFlush()`` after rendering a frame and before attempting to display
    it (default: auto). Can fix stuttering in some cases, in other cases
    probably causes it. The ``auto`` mode will call ``glFlush()`` only if
    the renderer is going to wait for a while after rendering, instead of
    flipping GL front and backbuffers immediately (i.e. it doesn't call it
    in display-sync mode).

    On OSX this is always deactivated because it only causes performance
    problems and other regressions.

``--gpu-dumb-mode=<yes|no|auto>``
    This mode is extremely restricted, and will disable most extended
    features. That includes high quality scalers and custom shaders!

    It is intended for hardware that does not support FBOs (including GLES,
    which supports it insufficiently), or to get some more performance out of
    bad or old hardware.

    This mode is forced automatically if needed, and this option is mostly
    useful for debugging. The default of ``auto`` will enable it automatically
    if nothing uses features which require FBOs.

    This option might be silently removed in the future.

``--gpu-shader-cache-dir=<dirname>``
    Store and load compiled GLSL shaders in this directory. Normally, shader
    compilation is very fast, so this is usually not needed. It mostly matters
    for GPU APIs that require internally recompiling shaders to other languages,
    for example anything based on ANGLE or Vulkan. Enabling this can improve
    startup performance on these platforms.

    NOTE: This is not cleaned automatically, so old, unused cache files may
    stick around indefinitely.

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
    side effect of turning this option on, for all sound drivers.

``--video-timing-offset=<seconds>``
    Control how long before video display target time the frame should be
    rendered (default: 0.050). If a video frame should be displayed at a
    certain time, the VO will start rendering the frame earlier, and then will
    perform a blocking wait until the display time, and only then "swap" the
    frame to display. The rendering cannot start before the previous frame is
    displayed, so this value is implicitly limited by the video framerate. With
    normal video frame rates, the default value will ensure that rendering is
    always immediately started after the previous frame was displayed. On the
    other hand, setting a too high value can reduce responsiveness with low
    FPS value.

    For client API users using the render API (or the deprecated ``opengl-cb``
    API), this option is interesting, because you can stop the render API
    from limiting your FPS (see ``mpv_render_context_render()`` documentation).

    This applies only to audio timing modes (e.g. ``--video-sync=audio``). In
    other modes (``--video-sync=display-...``), video timing relies on vsync
    blocking, and this option is not used.

``--video-sync=<audio|...>``
    How the player synchronizes audio and video.

    If you use this option, you usually want to set it to ``display-resample``
    to enable a timing mode that tries to not skip or repeat frames when for
    example playing 24fps video on a 24Hz screen.

    The modes starting with ``display-`` try to output video frames completely
    synchronously to the display, using the detected display vertical refresh
    rate as a hint how fast frames will be displayed on average. These modes
    change video speed slightly to match the display. See ``--video-sync-...``
    options for fine tuning. The robustness of this mode is further reduced by
    making a some idealized assumptions, which may not always apply in reality.
    Behavior can depend on the VO and the system's video and audio drivers.
    Media files must use constant framerate. Section-wise VFR might work as well
    with some container formats (but not e.g. mkv). If the sync code detects
    severe A/V desync, or the framerate cannot be detected, the player
    automatically reverts to ``audio`` mode for some time or permanently. These
    modes also require a vsync blocked presentation mode. For OpenGL, this
    translates to ``--opengl-swapinterval=1``. For Vulkan, it translates to
    ``--vulkan-swap-mode=fifo`` (or ``fifo-relaxed``).

    The modes with ``desync`` in their names do not attempt to keep audio/video
    in sync. They will slowly (or quickly) desync, until e.g. the next seek
    happens. These modes are meant for testing, not serious use.

    :audio:             Time video frames to audio. This is the most robust
                        mode, because the player doesn't have to assume anything
                        about how the display behaves. The disadvantage is that
                        it can lead to occasional frame drops or repeats. If
                        audio is disabled, this uses the system clock. This is
                        the default mode.
    :display-resample:  Resample audio to match the video. This mode will also
                        try to adjust audio speed to compensate for other drift.
                        (This means it will play the audio at a different speed
                        every once in a while to reduce the A/V difference.)
    :display-resample-vdrop:  Resample audio to match the video. Drop video
                        frames to compensate for drift.
    :display-resample-desync: Like the previous mode, but no A/V compensation.
    :display-vdrop:     Drop or repeat video frames to compensate desyncing
                        video. (Although it should have the same effects as
                        ``audio``, the implementation is very different.)
    :display-adrop:     Drop or repeat audio data to compensate desyncing
                        video. See ``--video-sync-adrop-size``. This mode will
                        cause severe audio artifacts if the real monitor
                        refresh rate is too different from the reported or
                        forced rate.
    :display-desync:    Sync video to display, and let audio play on its own.
    :desync:            Sync video according to system clock, and let audio play
                        on its own.

``--video-sync-max-video-change=<value>``
    Maximum speed difference in percent that is applied to video with
    ``--video-sync=display-...`` (default: 1). Display sync mode will be
    disabled if the monitor and video refresh way do not match within the
    given range. It tries multiples as well: playing 30 fps video on a 60 Hz
    screen will duplicate every second frame. Playing 24 fps video on a 60 Hz
    screen will play video in a 2-3-2-3-... pattern.

    The default settings are not loose enough to speed up 23.976 fps video to
    25 fps. We consider the pitch change too extreme to allow this behavior
    by default. Set this option to a value of ``5`` to enable it.

    Note that in the ``--video-sync=display-resample`` mode, audio speed will
    additionally be changed by a small amount if necessary for A/V sync. See
    ``--video-sync-max-audio-change``.

``--video-sync-max-audio-change=<value>``
    Maximum *additional* speed difference in percent that is applied to audio
    with ``--video-sync=display-...`` (default: 0.125). Normally, the player
    plays the audio at the speed of the video. But if the difference between
    audio and video position is too high, e.g. due to drift or other timing
    errors, it will attempt to speed up or slow down audio by this additional
    factor. Too low values could lead to video frame dropping or repeating if
    the A/V desync cannot be compensated, too high values could lead to chaotic
    frame dropping due to the audio "overshooting" and skipping multiple video
    frames before the sync logic can react.

``--video-sync-adrop-size=<value>``
    For the ``--video-sync=display-adrop`` mode. This mode duplicates/drops
    audio data to keep audio in sync with video. To avoid audio artifacts on
    jitter (which would add/remove samples all the time), this is done in
    relatively large, fixed units, controlled by this option. The unit is
    seconds.

``--mf-fps=<value>``
    Framerate used when decoding from multiple PNG or JPEG files with ``mf://``
    (default: 1).

``--mf-type=<value>``
    Input file type for ``mf://`` (available: jpeg, png, tga, sgi). By default,
    this is guessed from the file extension.

``--stream-dump=<destination-filename>``
    Instead of playing a file, read its byte stream and write it to the given
    destination file. The destination is overwritten. Can be useful to test
    network-related behavior.

``--stream-lavf-o=opt1=value1,opt2=value2,...``
    Set AVOptions on streams opened with libavformat. Unknown or misspelled
    options are silently ignored. (They are mentioned in the terminal output
    in verbose mode, i.e. ``--v``. In general we can't print errors, because
    other options such as e.g. user agent are not available with all protocols,
    and printing errors for unknown options would end up being too noisy.)

``--vo-mmcss-profile=<name>``
    (Windows only.)
    Set the MMCSS profile for the video renderer thread (default: ``Playback``).

``--priority=<prio>``
    (Windows only.)
    Set process priority for mpv according to the predefined priorities
    available under Windows.

    Possible values of ``<prio>``:
    idle|belownormal|normal|abovenormal|high|realtime

    .. warning:: Using realtime priority can cause system lockup.

``--force-media-title=<string>``
    Force the contents of the ``media-title`` property to this value. Useful
    for scripts which want to set a title, without overriding the user's
    setting in ``--title``.

``--external-files=<file-list>``
    Load a file and add all of its tracks. This is useful to play different
    files together (for example audio from one file, video from another), or
    for advanced ``--lavfi-complex`` used (like playing two video files at
    the same time).

    Unlike ``--sub-files`` and ``--audio-files``, this includes all tracks, and
    does not cause default stream selection over the "proper" file. This makes
    it slightly less intrusive. (In mpv 0.28.0 and before, this was not quite
    strictly enforced.)

    This is a list option. See `List Options`_ for details.

``--external-file=<file>``
    CLI/config file only alias for ``--external-files-append``. Each use of this
    option will add a new external files.

``--autoload-files=<yes|no>``
    Automatically load/select external files (default: yes).

    If set to ``no``, then do not automatically load external files as specified
    by ``--sub-auto`` and ``--audio-file-auto``. If external files are forcibly
    added (like with ``--sub-files``), they will not be auto-selected.

    This does not affect playlist expansion, redirection, or other loading of
    referenced files like with ordered chapters.

``--record-file=<file>``
    Record the current stream to the given target file. The target file will
    always be overwritten without asking.

    This remuxes the source stream without reencoding, which makes this a
    highly fragile and experimental feature. It's entirely possible that this
    writes files which are broken, not standards compliant, not playable with
    all players (including mpv), or incomplete.

    The target file format is determined by the file extension of the target
    filename. It is recommended to use the same target container as the source
    container if possible, and preferring Matroska as fallback.

    Seeking during stream recording, or enabling/disabling stream recording
    during playback, can cut off data, or produce "holes" in the output file.
    These are technical restrictions. In particular, video data or subtitles
    which were read ahead can produce such holes, which might cause playback
    problems with various players (including mpv).

    The behavior of this option might changed in the future, such as changing
    it to a template (similar to ``--screenshot-template``), being renamed,
    removed, or anything else, until it is declared semi-stable.

``--lavfi-complex=<string>``
    Set a "complex" libavfilter filter, which means a single filter graph can
    take input from multiple source audio and video tracks. The graph can result
    in a single audio or video output (or both).

    Currently, the filter graph labels are used to select the participating
    input tracks and audio/video output. The following rules apply:

    - A label of the form ``aidN`` selects audio track N as input (e.g.
      ``aid1``).
    - A label of the form ``vidN`` selects video track N as input.
    - A label named ``ao`` will be connected to the audio output.
    - A label named ``vo`` will be connected to the video output.

    Each label can be used only once. If you want to use e.g. an audio stream
    for multiple filters, you need to use the ``asplit`` filter. Multiple
    video or audio outputs are not possible, but you can use filters to merge
    them into one.

    It's not possible to change the tracks connected to the filter at runtime,
    unless you explicitly change the ``lavfi-complex`` property and set new
    track assignments. When the graph is changed, the track selection is changed
    according to the used labels as well.

    Other tracks, as long as they're not connected to the filter, and the
    corresponding output is not connected to the filter, can still be freely
    changed with the normal methods.

    Note that the normal filter chains (``--af``, ``--vf``) are applied between
    the complex graphs (e.g. ``ao`` label) and the actual output.

    .. admonition:: Examples

        - ``--lavfi-complex='[aid1] [aid2] amix [ao]'``
          Play audio track 1 and 2 at the same time.
        - ``--lavfi-complex='[vid1] [vid2] vstack [vo]'``
          Stack video track 1 and 2 and play them at the same time. Note that
          both tracks need to have the same width, or filter initialization
          will fail (you can add ``scale`` filters before the ``vstack`` filter
          to fix the size).
          To load a video track from another file, you can use
          ``--external-file=other.mkv``.
        - ``--lavfi-complex='[aid1] asplit [t1] [ao] ; [t1] showvolume [t2] ; [vid1] [t2] overlay [vo]'``
          Play audio track 1, and overlay the measured volume for each speaker
          over video track 1.
        - ``null:// --lavfi-complex='life [vo]'``
          A libavfilter source-only filter (Conways' Life Game).

    See the FFmpeg libavfilter documentation for details on the available
    filters.
