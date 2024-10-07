OPTIONS
=======

Track Selection
---------------

``--alang=<languagecode[,languagecode,...]>``
    Specify a prioritized list of audio languages to use, as IETF language tags.
    Equivalent ISO 639-1 two-letter and ISO 639-2 three-letter codes are treated
    the same. The first tag in the list that matches track's language in the file
    will be used. A track that matches more subtags will be preferred over one
    that matches fewer. See also ``--aid``.

    This is a string list option. See `List Options`_ for details.

    .. admonition:: Examples

        - ``mpv dvd://1 --alang=hu,en`` chooses the Hungarian language track
          on a DVD and falls back on English if Hungarian is not available.
        - ``mpv --alang=jpn example.mkv`` plays a Matroska file with Japanese
          audio.

``--slang=<languagecode[,languagecode,...]>``
    Equivalent to ``--alang``, for subtitle tracks.

    This is a string list option. See `List Options`_ for details.

    .. admonition:: Examples

        - ``mpv dvd://1 --slang=hu,en`` chooses the Hungarian subtitle track on
          a DVD and falls back on English if Hungarian is not available.
        - ``mpv --slang=jpn example.mkv`` plays a Matroska file with Japanese
          subtitles.
        - ``mpv --slang=pt-BR example.mkv`` plays a Matroska file with Brazilian
          Portuguese subtitles if available, and otherwise any Portuguese subtitles.

``--vlang=<...>``
    Equivalent to ``--alang`` and ``--slang``, for video tracks.

    This is a string list option. See `List Options`_ for details.

``--aid=<ID|auto|no>``
    Select audio track. ``auto`` selects the default, ``no`` disables audio.
    See also ``--alang``. mpv normally prints available audio tracks on the
    terminal when starting playback of a file.

    ``--audio`` is an alias for ``--aid``.

    ``--aid=no`` or ``--audio=no`` disables audio playback.
    (The latter variant does not work with the client API.)

    .. note::

        The track selection options (``--aid`` but also ``--sid`` and the
        others) sometimes expose behavior that may appear strange. Also, the
        behavior tends to change around with each mpv release.

        The track selection properties will return the option value outside of
        playback (as expected), but during playback, the affective track
        selection is returned. For example, with ``--aid=auto``, the ``aid``
        property will suddenly return ``2`` after playback initialization
        (assuming the file has at least 2 audio tracks, and the second is the
        default).

        At mpv 0.32.0 (and some releases before), if you passed a track value
        for which a corresponding track didn't exist (e.g. ``--aid=2`` and there
        was only 1 audio track), the ``aid`` property returned ``no``. However if
        another audio track was added during playback, and you tried to set the
        ``aid`` property to ``2``, nothing happened, because the ``aid`` option
        still had the value ``2``, and writing the same value has no effect.

        With mpv 0.33.0, the behavior was changed. Now track selection options
        are reset to ``auto`` at playback initialization, if the option had
        tries to select a track that does not exist. The same is done if the
        track exists, but fails to initialize. The consequence is that unlike
        before mpv 0.33.0, the user's track selection parameters are clobbered
        in certain situations.

        Also since mpv 0.33.0, trying to select a track by number will strictly
        select this track. Before this change, trying to select a track which
        did not exist would fall back to track default selection at playback
        initialization. The new behavior is more consistent.

        Setting a track selection property at runtime, and then playing a new
        file might reset the track selection to defaults, if the fingerprint
        of the track list of the new file is different.

        Be aware of tricky combinations of all of all of the above: for example,
        ``mpv --aid=2 file_with_2_audio_tracks.mkv file_with_1_audio_track.mkv``
        would first play the correct track, and the second file without audio.
        If you then go back the first file, its first audio track will be played,
        and the second file is played with audio. If you do the same thing again
        but instead of using ``--aid=2`` you run ``set aid 2`` while the file is
        playing, then changing to the second file will play its audio track.
        This is because runtime selection enables the fingerprint heuristic.

        Most likely this is not the end.

``--sid=<ID|auto|no>``
    Display the subtitle stream specified by ``<ID>``. ``auto`` selects
    the default, ``no`` disables subtitles.

    ``--sub`` is an alias for ``--sid``.

    ``--sid=no`` or ``--sub=no`` disables subtitle decoding.
    (The latter variant does not work with the client API.)

``--vid=<ID|auto|no>``
    Select video channel. ``auto`` selects the default, ``no`` disables video.

    ``--video`` is an alias for ``--vid``.

    ``--vid=no`` or ``--video=no`` disables video playback.
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

``--subs-with-matching-audio=<yes|forced|no>``
    When autoselecting a subtitle track, select it even if the selected audio
    stream matches you preferred subtitle language (default: yes). If this
    option is set to ``no``, then no subtitle track that matches the audio
    language will ever be autoselected by mpv regardless of ``--slang`` or
    ``subs-fallback``. If set to ``forced``, then only forced subtitles
    will be selected.

``--subs-match-os-language=<yes|no>``
    When autoselecting a subtitle track, select the track that matches the language of your OS
    if the audio stream is in a different language if suitable (default track or a forced track
    under the right conditions). Note that if ``--slang`` is set, this will be completely ignored
    (default: yes).

``--subs-fallback=<yes|default|no>``
    When autoselecting a subtitle track, if no tracks match your preferred languages,
    select a full track even if it doesn't match your preferred subtitle language (default: default).
    Setting this to `default` means that only streams flagged as `default` will be selected.

``--subs-fallback-forced=<yes|no|always>``
    When autoselecting a subtitle track, the default value of `yes` will prefer using a forced
    subtitle track if the subtitle language matches the audio language and matches your list of
    preferred languages. The special value `always` will only select forced subtitle tracks and
    never fallback on a non-forced track. Conversely, `no` will never select a forced subtitle
    track.


Playback Control
----------------

``--start=<relative time>``
    Seek to given time position.

    The general format for times is ``[+|-][[hh:]mm:]ss[.ms]``. If the time is
    prefixed with ``-``, the time is considered relative from the end of the
    file (as signaled by the demuxer/the file). A ``+`` is usually ignored (but
    see below).

    The following alternative time specifications are recognized:

    ``pp%`` seeks to percent position pp (0-100).

    ``#c`` seeks to chapter number c. (Chapters start from 1.)

    ``none`` resets any previously set option (useful for libmpv).

    If ``--rebase-start-time=no`` is given, then prefixing times with ``+``
    makes the time relative to the start of the file. A timestamp without
    prefix is considered an absolute time, i.e. should seek to a frame with a
    timestamp as the file contains it. As a bug, but also a hidden feature,
    putting 1 or more spaces before the ``+`` or ``-`` always interprets the
    time as absolute, which can be used to seek to negative timestamps (useful
    for debugging at most).

    .. admonition:: Examples

        ``--start=+56``, ``--start=00:56``
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

    Obscurity note: this does not work correctly if ``--rebase-start-time=no``,
    and the specified time is not an "absolute" time, as defined in the
    ``--start`` option description.

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
    speed higher than normal automatically inserts the ``scaletempo2`` audio
    filter.

``--pitch=<0.01-100>``
    Raise or lower the audio's pitch by the factor given as parameter. Does not
    affect playback speed. Playing with an altered pitch automatically inserts
    the ``scaletempo2`` audio filter.

    Since pitch change is achieved by combining pitch-preserving speed change and
    resampling, the range of pitch change is effectively limited by the
    ``min-speed`` and ``max-speed`` parameters of ``scaletempo2``: for example,
    a ``min-speed`` of 0.25 limits the highest pitch factor to 4 (1/0.25).

    In a standard 12-tone scale system, octaves are separated by a factor of 2
    whereas semitones are represented by a factor of 2^(1/12). This means
    pitches can easily be shifted up or down with a simple multiplier.

    .. admonition:: Examples

        ``--pitch=2``
            Shifts the pitch up a full octave.
        ``--pitch=0.5``
            Shifts the pitch down an octave.
        ``--pitch=1.498307`` (2^(7/12))
            Shifts the pitch up a perfect fifth.
        ``--pitch=0.667420`` (2^(-7/12))
            Shifts the pitch down a perfect fifth.
        ``--pitch=1.059463`` (2^(1/12))
            Shifts the pitch up a semitone.
        ``--pitch=0.943874`` (2^(-1/12))
            Shifts the pitch down a semitone.

``--pause``
    Start the player in paused state.

``--shuffle``
    Play files in random order.

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
    Play files according to a playlist file. Supports some common formats. If
    no format is detected, it will be treated as list of files, separated by
    newline characters. You may need this option to load plaintext files as
    a playlist. Note that XML playlist formats are not supported.

    This option forces ``--demuxer=playlist`` to interpret the playlist file.
    Some playlist formats, notably CUE and optical disc formats, need to use
    different demuxers and will not work with this option. They still can be
    played directly, without using this option.

    You can play playlists directly, without this option. Before mpv version
    0.31.0, this option disabled any security mechanisms that might be in
    place, but since 0.31.0 it uses the same security mechanisms as playing a
    playlist file directly. If you trust the playlist file, you can disable
    any security checks with ``--load-unsafe-playlists``. Because playlists
    can load other playlist entries, consider applying this option only to the
    playlist itself and not its entries, using something along these lines:

        ``mpv --{ --playlist=filename --load-unsafe-playlists --}``

    .. warning::

        The way older versions of mpv played playlist files via ``--playlist``
        was not safe against maliciously constructed files. Such files may
        trigger harmful actions. This has been the case for all versions of
        mpv prior to 0.31.0, and all MPlayer versions, but unfortunately this
        fact was not well documented earlier, and some people have even
        misguidedly recommended the use of ``--playlist`` with untrusted
        sources. Do NOT use ``--playlist`` with random internet sources or
        files you do not trust if you are not sure your mpv is at least 0.31.0.

        In particular, playlists can contain entries using protocols other than
        local files, such as special protocols like ``avdevice://`` (which are
        inherently unsafe).

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

``--hr-seek=<no|absolute|yes|default>``
    Select when to use precise seeks that are not limited to keyframes. Such
    seeks require decoding video from the previous keyframe up to the target
    position and so can take some time depending on decoding performance. For
    some video formats, precise seeks are disabled. This option selects the
    default choice to use for seeks; it is possible to explicitly override that
    default in the definition of key bindings and in input commands.

    :no:       Never use precise seeks.
    :absolute: Use precise seeks if the seek is to an absolute position in the
               file, such as a chapter seek, but not for relative seeks like
               the default behavior of arrow keys.
    :default:  Like ``absolute``, but enable hr-seeks in audio-only cases. The
               exact behavior is implementation specific and may change with
               new releases (default).
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

    In addition, if a playlist is loaded while this is set, the added playlist
    entries are not marked as originating from network or potentially unsafe
    location. (Instead, the behavior is as if the playlist entries were provided
    directly to mpv command line or ``loadfile`` command.)

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

    .. note::

        ``--loop-file`` counts the number of times it causes the player to
        seek to the beginning of the file, not the number of full playthroughs. This
        means ``--loop-file=1`` will end up playing the file twice. Contrast with
        ``--loop-playlist``, which counts the number of full playthroughs.

    ``--loop`` is an alias for this option.

``--ab-loop-a=<time>``, ``--ab-loop-b=<time>``
    Set loop points. If playback passes the ``b`` timestamp, it will seek to
    the ``a`` timestamp. Seeking past the ``b`` point doesn't loop (this is
    intentional).

    If ``a`` is after ``b``, the behavior is as if the points were given in
    the right order, and the player will seek to ``b`` after crossing through
    ``a``. This is different from old behavior, where looping was disabled (and
    as a bug, looped back to ``a`` on the end of the file).

    If either options are set to ``no`` (or unset), looping is disabled. This
    is different from old behavior, where an unset ``a`` implied the start of
    the file, and an unset ``b`` the end of the file.

    The loop-points can be adjusted at runtime with the corresponding
    properties. See also ``ab-loop`` command.

``--ab-loop-count=<N|inf>``
    Run A-B loops only N times, then ignore the A-B loop points (default: inf).
    ``inf`` means that looping goes on forever. If this option is set to 0, A-B
    looping is ignored, and even the ``ab-loop`` command will not enable looping
    again (the command will show ``(disabled)`` on the OSD message if both loop
    points are set, but ``ab-loop-count`` is 0).

``--ordered-chapters=<yes|no>``
    Enable support for Matroska ordered chapters. mpv will load and
    search for video segments from other files, and will also respect any
    chapter order specified for the main file (default: yes).

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

``--play-direction=<forward|+|backward|->``
    Control the playback direction (default: forward). Setting ``backward``
    will attempt to play the file in reverse direction, with decreasing
    playback time. If this is set on playback starts, playback will start from
    the end of the file. If this is changed at during playback, a hr-seek will
    be issued to change the direction.

    ``+`` and ``-`` are aliases for ``forward`` and ``backward``.

    The rest of this option description pertains to the ``backward`` mode.

    .. note::

        Backward playback is extremely fragile. It may not always work, is much
        slower than forward playback, and breaks certain other features. How
        well it works depends mainly on the file being played. Generally, it
        will show good results (or results at all) only if the stars align.

    mpv, as well as most media formats, were designed for forward playback
    only. Backward playback is bolted on top of mpv, and tries to make a medium
    effort to make backward playback work. Depending on your use-case, another
    tool may work much better.

    Backward playback is not exactly a 1st class feature. Implementation
    tradeoffs were made, that are bad for backward playback, but in turn do not
    cause disadvantages for normal playback. Various possible optimizations are
    not implemented in order to keep the complexity down. Normally, a media
    player is highly pipelined (future data is prepared in separate threads, so
    it is available in realtime when the next stage needs it), but backward
    playback will essentially stall the pipeline at various random points.

    For example, for intra-only codecs are trivially backward playable, and
    tools built around them may make efficient use of them (consider video
    editors or camera viewers). mpv won't be efficient in this case, because it
    uses its generic backward playback algorithm, that on top of it is not very
    optimized.

    If you just want to quickly go backward through the video and just show
    "keyframes", just use forward playback, and hold down the left cursor key
    (which on CLI with default config sends many small relative seek commands).

    The implementation consists of mostly 3 parts:

    - Backward demuxing. This relies on the demuxer cache, so the demuxer cache
      should (or must, didn't test it) be enabled, and its size will affect
      performance. If the cache is too small or too large, quadratic runtime
      behavior may result.

    - Backward decoding. The decoder library used (libavcodec) does not support
      this. It is emulated by feeding bits of data in forward, putting the
      result in a queue, returning the queue data to the VO in reverse, and
      then starting over at an earlier position. This can require buffering an
      extreme amount of decoded data, and also completely breaks pipelining.

    - Backward output. This is relatively simple, because the decoder returns
      the frames in the needed order. However, this may cause various problems
      because filters see audio and video going backward.

    Known problems:

    - It's fragile. If anything doesn't work, random non-useful behavior may
      occur. In simple cases, the player will just play nonsense and artifacts.
      In other cases, it may get stuck or heat the CPU. (Exceeding memory usage
      significantly beyond the user-set limits would be a bug, though.)

    - Performance and resource usage isn't good. In part this is inherent to
      backward playback of normal media formats, and in parts due to
      implementation choices and tradeoffs.

    - This is extremely reliant on good demuxer behavior. Although backward
      demuxing requires no special demuxer support, it is required that the
      demuxer performs seeks reliably, fulfills some specific requirements
      about packet metadata, and has deterministic behavior.

    - Starting playback exactly from the end may or may not work, depending on
      seeking behavior and file duration detection.

    - Some container formats, audio, and video codecs are not supported due to
      their behavior. There is no list, and the player usually does not detect
      them. Certain live streams (including TV captures) may exhibit problems
      in particular, as well as some lossy audio codecs. h264 intra-refresh is
      known not to work due to problems with libavcodec. WAV and some other raw
      audio formats tend to have problems - there are hacks for dealing with
      them, which may or may not work.

    - Backward demuxing of subtitles is not supported. Subtitle display still
      works for some external text subtitle formats. (These are fully read into
      memory, and only backward display is needed.) Text subtitles that are
      cached in the subtitle renderer also have a chance to be displayed
      correctly.

    - Some features dealing with playback of broken or hard to deal with files
      will not work fully (such as timestamp correction).

    - If demuxer low level seeks (i.e. seeking the actual demuxer instead of
      just within the demuxer cache) are performed by backward playback, the
      created seek ranges may not join, because not enough overlap is achieved.

    - Trying to use this with hardware video decoding will probably exhaust all
      your GPU memory and then crash a thing or two. Or it will fail because
      ``--hwdec-extra-frames`` will certainly be set too low.

    - Stream recording is broken. ``--stream-record`` may keep working if you
      backward play within a cached region only.

    - Relative seeks may behave weird. Small seeks backward (towards smaller
      time, i.e. ``seek -1``) may not really seek properly, and audio will
      remain muted for a while. Using hr-seek is recommended, which should have
      none of these problems.

    - Some things are just weird. For example, while seek commands manipulate
      playback time in the expected way (provided they work correctly), the
      framestep commands are transposed. Backstepping will perform very
      expensive work to step forward by 1 frame.

    Tuning:

    - Remove all ``--vf``/``--af`` filters you have set. Disable hardware
      decoding. Disable functions like SPDIF passthrough.

    - Increasing ``--video-reversal-buffer`` might help if reversal queue
      overflow is reported, which may happen in high bitrate video, or video
      with large GOP. Hardware decoding mostly ignores this, and you need to
      increase ``--hwdec-extra-frames`` instead (until you get playback without
      logged errors).

    - The demuxer cache is essential for backward demuxing. Make sure to set
      ``--cache=yes``. The cache size might matter. If it's too small, a queue
      overflow will be logged, and backward playback cannot continue, or it
      performs too many low level seeks. If it's too large, implementation
      tradeoffs may cause general performance issues. Use
      ``--demuxer-max-bytes`` to potentially increase the amount of packets the
      demuxer layer can queue for reverse demuxing (basically it's the
      ``--video-reversal-buffer`` equivalent for the demuxer layer).

    - Setting ``--vd-queue-enable=yes`` can help a lot to make playback smooth
      (once it works).

    - ``--demuxer-backward-playback-step`` also factors into how many seeks may
      be performed, and whether backward demuxing could break due to queue
      overflow. If it's set too high, the backstep operation needs to search
      through more packets all the time, even if the cache is large enough.

    - Setting ``--demuxer-cache-wait`` may be useful to cache the entire file
      into the demuxer cache. Set ``--demuxer-max-bytes`` to a large size to
      make sure it can read the entire cache; ``--demuxer-max-back-bytes``
      should also be set to a large size to prevent that tries to trim the
      cache.

    - If audio artifacts are audible, even though the AO does not underrun,
      increasing ``--audio-backward-overlap`` might help in some cases.

``--video-reversal-buffer=<bytesize>``, ``--audio-reversal-buffer=<bytesize>``
    For backward decoding. Backward decoding decodes forward in steps, and then
    reverses the decoder output. These options control the approximate maximum
    amount of bytes that can be buffered. The main use of this is to avoid
    unbounded resource usage; during normal backward playback, it's not supposed
    to hit the limit, and if it does, it will drop frames and complain about it.

    Use this option if you get reversal queue overflow errors during backward
    playback. Increase the size until the warning disappears. Usually, the video
    buffer will overflow first, especially if it's high resolution video.

    This does not work correctly if video hardware decoding is used. The video
    frame size will not include the referenced GPU and driver memory. Some
    hardware decoders may also be limited by ``--hwdec-extra-frames``.

    How large the queue size needs to be depends entirely on the way the media
    was encoded. Audio typically requires a very small buffer, while video can
    require excessively large buffers.

    (Technically, this allows the last frame to exceed the limit. Also, this
    does not account for other buffered frames, such as inside the decoder or
    the video output.)

    This does not affect demuxer cache behavior at all.

    See ``--list-options`` for defaults and value range. ``<bytesize>`` options
    accept suffixes such as ``KiB`` and ``MiB``.

``--video-backward-overlap=<auto|number>``, ``--audio-backward-overlap=<auto|number>``
    Number of overlapping keyframe ranges to use for backward decoding (default:
    auto) ("keyframe" to be understood as in the mpv/ffmpeg specific meaning).
    Backward decoding works by forward decoding in small steps. Some codecs
    cannot restart decoding from any packet (even if it's marked as seek point),
    which becomes noticeable with backward decoding (in theory this is a problem
    with seeking too, but ``--hr-seek-demuxer-offset`` can fix it for seeking).
    In particular, MDCT based audio codecs are affected.

    The solution is to feed a previous packet to the decoder each time, and then
    discard the output. This option controls how many packets to feed. The
    ``auto`` choice is currently hardcoded to 0 for video, and uses 1 for lossy
    audio, 0 for lossless audio. For some specific lossy audio codecs, this is
    set to 2.

    ``--video-backward-overlap`` can potentially handle intra-refresh video,
    depending on the exact conditions. You may have to use the
    ``--vd-lavc-show-all`` option as well.

``--video-backward-batch=<number>``, ``--audio-backward-batch=<number>``
    Number of keyframe ranges to decode at once when backward decoding (default:
    1 for video, 10 for audio). Another pointless tuning parameter nobody should
    use. This should affect performance only. In theory, setting a number higher
    than 1 for audio will reduce overhead due to less frequent backstep
    operations and less redundant decoding work due to fewer decoded overlap
    frames (see ``--audio-backward-overlap``). On the other hand, it requires
    a larger reversal buffer, and could make playback less smooth due to
    breaking pipelining (e.g. by decoding a lot, and then doing nothing for a
    while).

    It probably never makes sense to set ``--video-backward-batch``. But in
    theory, it could help with intra-only video codecs by reducing backstep
    operations.

``--demuxer-backward-playback-step=<seconds>``
    Number of seconds the demuxer should seek back to get new packets during
    backward playback (default: 60). This is useful for tuning backward
    playback, see ``--play-direction`` for details.

    Setting this to a very low value or 0 may make the player think seeking is
    broken, or may make it perform multiple seeks.

    Setting this to a high value may lead to quadratic runtime behavior.

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
    Do not load default configuration or any user files. This prevents loading of
    both the user-level and system-wide ``mpv.conf`` and ``input.conf`` files. Other
    user files are blocked as well, such as resume playback files and cache files.
    This option only takes effect when used as a command line flag.

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

    A special case is the macOS bundle, it will create a log file at
    ``~/Library/Logs/mpv.log`` by default.

``--config-dir=<path>``
    Force a different configuration directory. If this is set, the given
    directory is used to load configuration files, and all other configuration
    directories are ignored. This means the global mpv configuration directory
    as well as per-user directories are ignored, and overrides through
    environment variables (``MPV_HOME``) are also ignored.

    Note that the cache and state paths (``~~/cache``, ``~~/state``) are not
    considered "configuration" and keep their auto-detection logic.

    Note that the ``--no-config`` option takes precedence over this option.

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

``--script=<filename>``, ``--scripts=file1.lua:file2.lua:...``
    Load a Lua script. The second option allows you to load multiple scripts by
    separating them with the path separator (``:`` on Unix, ``;`` on Windows).

    ``--scripts`` is a path list option. See `List Options`_ for details.

``--script-opts=key1=value1,key2=value2,...``
    Set options for scripts. A script can query an option by key. If an
    option is used and what semantics the option value has depends entirely on
    the loaded scripts. Values not claimed by any scripts are ignored.

    This is a key/value list option. See `List Options`_ for details.

``--merge-files``
    Pretend that all files passed to mpv are concatenated into a single, big
    file. This uses timeline/EDL support internally.

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

    The special name ``all`` resets as many options as possible.

    This is a string list option. See `List Options`_ for details.

    .. admonition:: Examples

        - ``--reset-on-next-file=pause``
          Reset pause mode when switching to the next file.
        - ``--reset-on-next-file=fullscreen,speed``
          Reset fullscreen and playback speed settings if they were changed
          during playback.
        - ``--reset-on-next-file=all``
          Try to reset all settings that were changed during playback.

``--show-profile=<profile>``
    Show the description and content of a profile. Lists all profiles if no
    parameter is provided.

``--use-filedir-conf``
    Look for a file-specific configuration file in the same directory as the
    file that is being played. See `File-specific Configuration Files`_.

    .. warning::

        May be dangerous if playing from untrusted media.

``--ytdl=<yes|no>``
    Enable the youtube-dl hook-script. It will look at the input URL, and will
    play the video located on the website. This works with many streaming sites,
    not just the one that the script is named after. This requires a recent
    version of youtube-dl to be installed on the system (default: yes).

    If the script can't do anything with an URL, it will do nothing.

    This accepts a set of options, which can be passed to it with the
    ``--script-opts`` option (using ``ytdl_hook-`` as prefix):

    ``try_ytdl_first=<yes|no>``
        If 'yes' will try parsing the URL with youtube-dl first, instead of the
        default where it's only after mpv failed to open it. This mostly depends
        on whether most of your URLs need youtube-dl parsing.

    ``exclude=<URL1|URL2|...``
        A ``|``-separated list of URL patterns which mpv should not use with
        youtube-dl. The patterns are matched after the ``http(s)://`` part of
        the URL.

        ``^`` matches the beginning of the URL, ``$`` matches its end, and you
        should use ``%`` before any of the characters ``^$()%|,.[]*+-?`` to
        match that character.

        .. admonition:: Examples

            - ``--script-opts=ytdl_hook-exclude='^youtube%.com'``
              will exclude any URL that starts with ``http://youtube.com`` or
              ``https://youtube.com``.
            - ``--script-opts=ytdl_hook-exclude='%.mkv$|%.mp4$'``
              will exclude any URL that ends with ``.mkv`` or ``.mp4``.

        See more lua patterns here: https://www.lua.org/manual/5.1/manual.html#5.4.1

    ``all_formats=<yes|no>``
        If 'yes' will attempt to add all formats found reported by youtube-dl
        (default: no). Each format is added as a separate track. In addition,
        they are delay-loaded, and actually opened only when a track is selected
        (this should keep load times as low as without this option).

        It adds average bitrate metadata, if available, which means you can use
        ``--hls-bitrate`` to decide which track to select. (HLS used to be the
        only format whose alternative quality streams were exposed in a similar
        way, thus the option name.)

        Tracks which represent formats that were selected by youtube-dl as
        default will have the default flag set. This means mpv should generally
        still select formats chosen with ``--ytdl-format`` by default.

        Although this mechanism makes it possible to switch streams at runtime,
        it's not suitable for this purpose for various technical reasons. (It's
        slow, which can't be really fixed.) In general, this option is not
        useful, and was only added to show that it's possible.

        There are two cases that must be considered when doing quality/bandwidth
        selection:

            1. Completely separate audio and video streams (DASH-like). Each of
               these streams contain either only audio or video, so you can
               mix and combine audio/video bandwidths without restriction. This
               intuitively matches best with the concept of selecting quality
               by track (what ``all_formats`` is supposed to do).

            2. Separate sets of muxed audio and video streams. Each version of
               the media contains both an audio and video stream, and they are
               interleaved. In order not to waste bandwidth, you should only
               select one of these versions (if, for example, you select an
               audio stream, then video will be downloaded, even if you selected
               video from a different stream).

               mpv will still represent them as separate tracks, but will set
               the title of each track to ``muxed-N``, where ``N`` is replaced
               with the youtube-dl format ID of the originating stream.

        Some sites will mix 1. and 2., but we assume that they do so for
        compatibility reasons, and there is no reason to use them at all.

    ``force_all_formats=<yes|no>``
        If set to 'yes', and ``all_formats`` is also set to 'yes', this will
        try to represent all youtube-dl reported formats as tracks, even if
        mpv would normally use the direct URL reported by it (default: yes).

        It appears this normally makes a difference if youtube-dl works on a
        master HLS playlist.

        If this is set to 'no', this specific kind of stream is treated like
        ``all_formats`` is set to 'no', and the stream selection as done by
        youtube-dl (via ``--ytdl-format``) is used.

    ``thumbnails=<all|best|none>``
        Add thumbnails as video tracks (default: none).

        Thumbnails get downloaded when they are added as tracks, so 'all' can
        have a noticable impact on how long it takes to open the video when
        there are a lot of thumbnails.

    ``use_manifests=<yes|no>``
        Make mpv use the master manifest URL for formats like HLS and DASH,
        if available, allowing for video/audio selection in runtime (default:
        no). It's disabled ("no") by default for performance reasons.

    ``ytdl_path=youtube-dl``
        Configure paths to youtube-dl's executable or a compatible fork's. The
        paths should be separated by : on Unix and ; on Windows. mpv looks in
        order for the configured paths in PATH and in mpv's config directory.
        The defaults are "yt-dlp", "yt-dlp_x86" and "youtube-dl". On Windows
        the suffix extension is not necessary, but only ".exe" is acceptable.

    .. admonition:: Why do the option names mix ``_`` and ``-``?

        I have no idea.

``--ytdl-format=<ytdl|best|worst|mp4|webm|...>``
    Video format/quality that is directly passed to youtube-dl. The possible
    values are specific to the website and the video, for a given url the
    available formats can be found with the command
    ``youtube-dl --list-formats URL``. See youtube-dl's documentation for
    available aliases.
    (Default: ``bestvideo+bestaudio/best``)

    The ``ytdl`` value does not pass a ``--format`` option to youtube-dl at all,
    and thus does not override its default. Note that sometimes youtube-dl
    returns a format that mpv cannot use, and in these cases the mpv default
    may work better.

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

    This is a key/value list option. See `List Options`_ for details.

    .. admonition:: Example

        - ``--ytdl-raw-options=username=user,password=pass``
        - ``--ytdl-raw-options=force-ipv6=``
        - ``--ytdl-raw-options=proxy=[http://127.0.0.1:3128]``
        - ``--ytdl-raw-options-append=proxy=http://127.0.0.1:3128``

``--js-memory-report=<yes|no>``
    Enable memory reporting for javascript scripts in the stats overlay.
    This is disabled by default because it has an overhead and increases
    memory usage. This option will only work if it is enabled before mpv is
    started.

``--load-stats-overlay=<yes|no>``
    Enable the builtin script that shows useful playback information on a key
    binding (default: yes). By default, the ``i`` key is used (``I`` to make
    the overlay permanent).

``--load-osd-console=<yes|no>``
    Enable the built-in script that shows a console on a key binding and lets
    you enter commands (default: yes). The ````` key is used to show the
    console by default, and ``ESC`` to hide it again.

``--load-auto-profiles=<yes|no|auto>``
    Enable the builtin script that does auto profiles (default: auto). See
    `Conditional auto profiles`_ for details. ``auto`` will load the script,
    but immediately unload it if there are no conditional profiles.

``--load-select=<yes|no>``
    Enable the builtin script that lets you select from lists of items (default:
    yes). By default, its keybindings start with the ``g`` key.

``--player-operation-mode=<cplayer|pseudo-gui>``
    For enabling "pseudo GUI mode", which means that the defaults for some
    options are changed. This option should not normally be used directly, but
    only by mpv internally, or mpv-provided scripts, config files, or .desktop
    files. See `PSEUDO GUI MODE`_ for details.

Watch Later
-----------

``--save-position-on-quit``
    Always save the current playback position on quit. When this file is
    played again later, the player will seek to the old playback position on
    start. This does not happen if playback of a file is stopped in any other
    way than quitting. For example, going to the next file in the playlist
    will not save the position, and start playback at beginning the next time
    the file is played.

    This behavior is disabled by default, but is always available when quitting
    the player with Shift+Q.

    See `RESUMING PLAYBACK`_.

``--watch-later-dir=<path>``
    The directory in which to store the "watch later" temporary files.

    ``--watch-later-directory`` is an alias for ``--watch-later-dir``.

    If this option is unset, the files will be stored in a subdirectory
    named "watch_later" underneath the local state directory
    (usually ``~/.local/state/mpv/``).

``--resume-playback=<yes|no>``
    Restore playback position from the ``watch_later`` configuration
    subdirectory, usually ``~/.config/mpv/watch_later/`` (default: yes).

``--resume-playback-check-mtime=<yes|no>``
    Only restore the playback position from the ``watch_later`` configuration
    subdirectory (usually ``~/.config/mpv/watch_later/``) if the file's
    modification time is the same as at the time of saving. This may prevent
    skipping forward in files with the same name which have different content.
    (Default: ``no``)

``--watch-later-options=option1,option2,...``
    The options that are saved in "watch later" files if they have been changed
    since when mpv started. These values will be restored the next time the
    files are played. Note that the playback position is saved via the ``start``
    option.

    When removing options, existing watch later data won't be modified and will
    still be applied fully, but new watch later data won't contain these
    options.

    See ``--help=watch-later-options`` for the list of the properties that are
    restored by default.

    This is a string list option. See `List Options`_ for details.

    .. admonition:: Examples

        - ``--watch-later-options-remove=sid``
          The subtitle track selection will not be restored.
        - ``--watch-later-options-remove=volume``
          ``--watch-later-options-remove=mute``
          The volume and mute state won't be saved to watch later files.
        - ``--watch-later-options=start``
          No option will be saved to watch later files, except the playback
          position.

``--write-filename-in-watch-later-config``
    Prepend the watch later config files with the name of the file they refer
    to. This is simply written as comment on the top of the file.

    .. warning::

        This option may expose privacy-sensitive information and is thus
        disabled by default.

``--ignore-path-in-watch-later-config``
    Ignore path (i.e. use filename only) when using watch later feature.
    (Default: disabled)

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
    The option variants ``--vf-add``, ``--vf-pre``, and ``--vf-clr`` exist
    to modify a previously specified list, but you should not need these for
    typical use.

``--untimed``
    Do not sleep when outputting video frames. Useful for benchmarks when used
    with ``--audio=no``.

``--framedrop=<mode>``
    Skip displaying some frames to maintain A/V sync on slow systems, or
    playing high framerate video on video outputs that have an upper framerate
    limit.

    The argument selects the drop methods, and can be one of the following:

    <no>
        Disable any frame dropping. Not recommended, for testing only.
    <vo>
        Drop late frames on video output (default). This still decodes and
        filters all frames, but doesn't render them on the VO. Drops are
        indicated in the terminal status line as ``Dropped:`` field.

        In audio sync. mode, this drops frames that are outdated at the time of
        display. If the decoder is too slow, in theory all frames would have to
        be dropped (because all frames are too late) - to avoid this, frame
        dropping stops  if the effective framerate is below 10 FPS.

        In display-sync. modes (see ``--video-sync``), this affects only how
        A/V drops or repeats frames. If this mode is disabled, A/V desync will
        in theory not affect video scheduling anymore (much like the
        ``display-resample-desync`` mode). However, even if disabled, frames
        will still be skipped (i.e. dropped) according to the ratio between
        video and display frequencies.

        This is the recommended mode, and the default.
    <decoder>
        Old, decoder-based framedrop mode. (This is the same as ``--framedrop=yes``
        in mpv 0.5.x and before.) This tells the decoder to skip frames (unless
        they are needed to decode future frames). May help with slow systems,
        but can produce unwatchable choppy output, or even freeze the display
        completely.

        This uses a heuristic which may not make sense, and in  general cannot
        achieve good results, because the decoder's frame dropping cannot be
        controlled in a predictable manner. Not recommended.

        Even if you want to use this, prefer ``decoder+vo`` for better results.

        The ``--vd-lavc-framedrop`` option controls what frames to drop.
    <decoder+vo>
        Enable both modes. Not recommended. Better than just ``decoder`` mode.

    .. note::

        ``--vo=vdpau`` has its own code for the ``vo`` framedrop mode. Slight
        differences to other VOs are possible.

``--video-latency-hacks=<yes|no>``
    Enable some things which tend to reduce video latency by 1 or 2 frames
    (default: no). Note that this option might be removed without notice once
    the player's timing code does not inherently need to do these things
    anymore. Using this option is known to break other options such as
    interpolation, so it is not recommended to enable this.

    This does:

    - Use the demuxer reported FPS for frame dropping. This avoids the
      player needing to decode 1 frame in advance, lowering total latency in
      effect. This also means that if the demuxer reported FPS is wrong, or
      the video filter chain changes FPS (e.g. deinterlacing), then it could
      drop too many or not enough frames.
    - Disable waiting for the first video frame. Normally the player waits for
      the first video frame to be fully rendered before starting playback
      properly. Some VOs will lazily initialize stuff when rendering the first
      frame, so if this is not done, there is some likeliness that the VO has
      to drop some frames if rendering the first frame takes longer than needed.

``--display-fps-override=<fps>``
    Set the display FPS used with the ``--video-sync=display-*`` modes. By
    default, a detected value is used. Keep in mind that setting an incorrect
    value (even if slightly incorrect) can ruin video playback. On multi-monitor
    systems, there is a chance that the detected value is from the wrong
    monitor.

    Set this option only if you have reason to believe the automatically
    determined value is wrong.

``--hwdec=<api1,api2,...|no|auto|auto-safe|auto-copy>``
    Specify the hardware video decoding API that should be used if possible.
    Whether hardware decoding is actually done depends on the video codec. If
    hardware decoding is not possible, mpv will fall back on software decoding.

    Hardware decoding is not enabled by default, to keep the out-of-the-box
    configuration as reliable as possible. However, when using modern hardware,
    hardware video decoding should work correctly, offering reduced CPU usage,
    and possibly lower power consumption. On older systems, it may be necessary
    to use hardware decoding due to insufficient CPU resources; and even on
    modern systems, sufficiently complex content (eg: 4K60 AV1) may require it.

    .. note::

        Use the ``Ctrl+h`` shortcut to toggle hardware decoding at runtime. It
        toggles this option between ``auto-safe`` and ``no``.

        If you decide you want to use hardware decoding by default, the general
        recommendation is to try out decoding with the command line option, and
        prove to yourself that it works as desired for the content you care
        about. After that, you can add it to your config file.

        When testing, you should start by using ``hwdec=auto-safe`` as it will
        limit itself to choosing from hwdecs that are actively supported by the
        development team. If that doesn't result in working hardware decoding,
        you can try ``hwdec=auto`` to have it attempt to load every possible
        hwdec, but if ``auto-safe`` didn't work, you will probably need to know
        exactly which hwdec matches your hardware and read up on that entry
        below.

        If ``auto-safe`` or ``auto`` produced the desired results, we recommend
        just sticking with that and only setting a specific hwdec in your config
        file if it is really necessary.

        If you use the Ubuntu package, keep in mind that their
        ``/etc/mpv/mpv.conf`` contains ``hwdec=vaapi``, which is less than
        ideal as it may not be the right choice for your system, and it may end
        up using an inefficient wrapper library under the covers. We recommend
        removing this line or deleting the file altogether.

    .. note::

        Even if enabled, hardware decoding is still only white-listed for some
        codecs. See ``--hwdec-codecs`` to enable hardware decoding in more cases.

    .. admonition:: Which method to choose?

        - If you only want to enable hardware decoding at runtime, don't set the
          parameter, or put ``hwdec=no`` into your ``mpv.conf`` (relevant on
          distros which force-enable it by default, such as on Ubuntu). Use the
          ``Ctrl+h`` default binding to enable it at runtime.
        - If you're not sure, but want hardware decoding always enabled by
          default, put ``hwdec=auto-safe`` into your ``mpv.conf``, and
          acknowledge that this may cause problems.
        - If you want to test available hardware decoding methods, pass
          ``--hwdec=auto --hwdec-codecs=all`` and look at the terminal output.
        - If you're a developer, or want to perform elaborate tests, you may
          need any of the other possible option values.

    This option accepts a comma delimited list of ``api`` types, along with certain
    special values:

    :no:        always use software decoding (default)
    :auto-safe: enable any whitelisted hw decoder (see below)
    :auto:      forcibly enable any hw decoder found (see below)
    :yes:       exactly the same as ``auto-safe``
    :auto-copy: enable best hw decoder with copy-back (see below)

    .. note::

        Special values can be mixed with api names. eg: ``vaapi,auto`` will try
        and use the ``vaapi`` hwdec, and if that fails, will run through the
        normal ``auto`` logic.

    Actively supported hwdecs:

    :d3d11va:   requires ``--vo=gpu`` with ``--gpu-context=d3d11`` or
                ``--gpu-context=angle`` (Windows 8+ only)
    :d3d11va-copy: copies video back to system RAM (Windows 8+ only)
    :videotoolbox: requires ``--vo=gpu`` (macOS 10.8 and up),
                   or ``--vo=libmpv`` (iOS 9.0 and up)
    :videotoolbox-copy: copies video back into system RAM (macOS 10.8 or iOS 9.0 and up)
    :vaapi:     requires ``--vo=gpu``, ``--vo=vaapi`` or ``--vo=dmabuf-wayland`` (Linux only)
    :vaapi-copy: copies video back into system RAM (Linux with some GPUs or Windows)
    :nvdec:     requires ``--vo=gpu`` (Any platform CUDA is available)
    :nvdec-copy: copies video back to system RAM (Any platform CUDA is available)
    :drm:       requires ``--vo=gpu`` (Linux only)
    :drm-copy:   copies video back to system RAM (Linux only)
    :vulkan:    requires ``--vo=gpu-next`` (Any platform with Vulkan Video Decoding)
    :vulkan-copy: copies video back to system RAM (Any platform with Vulkan Video Decoding)

    Other hwdecs (only use if you know you have to):

    :dxva2:     requires ``--vo=gpu`` with ``--gpu-context=d3d11``,
                ``--gpu-context=angle`` or ``--gpu-context=dxinterop``
                (Windows only)
    :dxva2-copy: copies video back to system RAM (Windows only)
    :vdpau:     requires ``--vo=gpu`` with ``--gpu-context=x11``, or
                ``--vo=vdpau`` (Linux only)
    :vdpau-copy: copies video back into system RAM (Linux with some GPUs only)
    :mediacodec: requires ``--vo=gpu --gpu-context=android``
                 or ``--vo=mediacodec_embed`` (Android only)
    :mediacodec-copy: copies video back to system RAM (Android only)
    :cuda:      requires ``--vo=gpu`` (Any platform CUDA is available)
    :cuda-copy: copies video back to system RAM (Any platform CUDA is available)
    :crystalhd: copies video back to system RAM (Any platform supported by hardware)
    :rkmpp:     requires ``--vo=gpu`` (some RockChip devices only)

    ``auto`` tries to automatically enable hardware decoding using the first
    available method. This still depends what VO you are using. See the list
    above, for which ``--vo`` and ``gpu-context`` is required for a given
    hwdec. It will go down the list of available hwdecs until one is
    successfully initialised. If all of them fail, it will fallback to software
    decoding.

    ``auto-safe`` is similar to ``auto``, but allows only whitelisted methods
    that are considered "safe". This is supposed to be a reasonable way to
    enable hardware decdoding by default in a config file (even though you
    shouldn't do that anyway; prefer runtime enabling with ``Ctrl+h``). Unlike
    ``auto``, this will not try to enable unknown or known-to-be-bad methods. In
    addition, this may disable hardware decoding in other situations when it's
    known to cause problems, but currently this mechanism is quite primitive.
    (As an example for something that still causes problems: certain
    combinations of HEVC and Intel chips on Windows tend to cause mpv to crash,
    most likely due to driver bugs.)

    ``auto-copy-safe`` selects the union of methods selected with ``auto-safe``
    and ``auto-copy``.

    ``auto-copy`` selects only modes that copy the video data back to system
    memory after decoding. This selects modes like ``vaapi-copy`` (and so on).
    If none of these work, hardware decoding is disabled. This mode is usually
    guaranteed to incur no additional quality loss compared to software
    decoding (assuming modern codecs and an error free video stream), and will
    allow CPU processing with video filters. This mode works with all video
    filters and VOs.

    Because these copy the decoded video back to system RAM, they're often less
    efficient than the direct modes, and may not help too much over software
    decoding if you are short on CPU resources.

    .. note::

       Most non-copy methods only work with the OpenGL GPU backend. Currently,
       only the ``vaapi``, ``nvdec``, ``cuda`` and ``vulkan`` methods work with
       Vulkan.

    The ``vaapi`` mode, if used with ``--vo=gpu``, requires Mesa 11, and most
    likely works with Intel and AMD GPUs only. It also requires the opengl EGL
    backend.

    ``nvdec`` and ``nvdec-copy`` are the newest, and recommended method to do
    hardware decoding on Nvidia GPUs.

    ``cuda`` and ``cuda-copy`` are an older implementation of hardware decoding
    on Nvidia GPUs that uses Nvidia's bitstream parsers rather than FFmpeg's.
    This can lead to feature deficiencies, such as incorrect playback of HDR
    content, and ``nvdec``/``nvdec-copy`` should always be preferred unless you
    specifically need Nvidia's deinterlacing algorithms. To use this
    deinterlacing you must pass the option:
    ``vd-lavc-o=deint=[weave|bob|adaptive]``.
    Pass ``weave`` (or leave the option unset) to not attempt any
    deinterlacing.

    .. admonition:: Quality reduction with hardware decoding

        In theory, hardware decoding does not reduce video quality (at least
        for the codecs h264 and HEVC). However, due to restrictions in video
        output APIs, as well as bugs in the actual hardware decoders, there can
        be some loss, or even blatantly incorrect results. This has largely
        ceased to be a problem with modern hardware, but there is a lot of
        hardware out there, so caveat emptor. Known problems are discussed
        below, but the list cannot be considered exhaustive, as even hwdecs that
        work well on certain hardware generations may be problematic on other
        ones.

        In some cases, RGB conversion is forced, which means the RGB conversion
        is performed by the hardware decoding API, instead of the shaders
        used by ``--vo=gpu``. This means certain colorspaces may not display
        correctly, and certain filtering (such as debanding) cannot be applied
        in an ideal way. This will also usually force the use of low quality
        chroma scalers instead of the one specified by ``--cscale``. In other
        cases, hardware decoding can also reduce the bit depth of the decoded
        image, which can introduce banding or precision loss for 10-bit files.

        ``vdpau`` always does RGB conversion in hardware, which does not
        support newer colorspaces like BT.2020 correctly. However, ``vdpau``
        doesn't support 10 bit or HDR encodings, so these limitations are
        unlikely to be relevant.

        ``dxva2`` is not safe. It appears to always use BT.601 for forced RGB
        conversion, but actual behavior depends on the GPU drivers. Some drivers
        appear to convert to limited range RGB, which gives a faded appearance.
        In addition to driver-specific behavior, global system settings might
        affect this additionally. This can give incorrect results even with
        completely ordinary video sources.

        ``mediacodec`` is not safe. It forces RGB conversion (not with ``-copy``)
        and how well it handles non-standard colorspaces is not known.
        In the rare cases where 10-bit is supported the bit depth of the output
        will be reduced to 8.

        ``cuda`` should usually be safe, but depending on how a file/stream
        has been mixed, it has been reported to corrupt the timestamps causing
        glitched, flashing frames. It can also sometimes cause massive
        framedrops for unknown reasons. Caution is advised, and ``nvdec``
        should always be preferred.

        ``crystalhd`` is not safe. It always converts to 4:2:2 YUV, which
        may be lossy, depending on how chroma sub-sampling is done during
        conversion. It also discards the top left pixel of each frame for
        some reason.

        If you run into any weird decoding issues, frame glitches or
        discoloration, and you have ``--hwdec`` turned on, the first thing you
        should try is disabling it.

``--gpu-hwdec-interop=<auto|all|no|name>``
    This option is for troubleshooting hwdec interop issues. Since it's a
    debugging option, its semantics may change at any time.

    This is useful for the ``gpu`` and ``libmpv`` VOs for selecting which
    hwdec interop context to use exactly. Effectively it also can be used
    to block loading of certain backends.

    If set to ``auto`` (default), the behavior depends on the VO: for ``gpu``,
    it does nothing, and the interop context is loaded on demand (when the
    decoder probes for ``--hwdec`` support). For ``libmpv``, which has
    has no on-demand loading, this is equivalent to ``all``.

    The empty string is equivalent to ``auto``.

    If set to ``all``, it attempts to load all interop contexts at GL context
    creation time.

    Other than that, a specific backend can be set, and the list of them can
    be queried with ``help`` (mpv CLI only).

    Runtime changes to this are ignored (the current option value is used
    whenever the renderer is created).

``--hwdec-extra-frames=<N>``
    Number of GPU frames hardware decoding should preallocate (default: see
    ``--list-options`` output). If this is too low, frame allocation may fail
    during decoding, and video frames might get dropped and/or corrupted.
    Setting it too high simply wastes GPU memory and has no advantages.

    This value is used only for hardware decoding APIs which require
    preallocating surfaces (known examples include ``d3d11va`` and ``vaapi``).
    For other APIs, frames are allocated as needed. The details depend on the
    libavcodec implementations of the hardware decoders.

    The required number of surfaces depends on dynamic runtime situations. The
    default is a fixed value that is thought to be sufficient for most uses. But
    in certain situations, it may not be enough.

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
    Choose the GPU device used for decoding when using the ``cuda`` or
    ``nvdec`` hwdecs with the OpenGL GPU backend, and with the ``cuda-copy``
    or ``nvdec-copy`` hwdecs in all cases.

    For the OpenGL GPU backend, the default device used for decoding is the one
    being used to provide ``gpu`` output (and in the vast majority of cases,
    only one GPU will be present).

    For the ``copy`` hwdecs, the default device will be the first device
    enumerated by the CUDA libraries - however that is done.

    For the Vulkan GPU backend, decoding must always happen on the display
    device, and this option has no effect.

``--vaapi-device=<device file|adapter name>``
    Choose the DRM device for ``vaapi-copy``. This should be the path to a
    DRM device file. (Default: ``/dev/dri/renderD128``)

    On Windows this takes adapter name as an input. Will pick the default adapter
    if unset. Alternatives are listed when the name "help" is given.

``--panscan=<0.0-1.0>``
    Enables pan-and-scan functionality (cropping the sides of e.g. a 16:9
    video to make it fit a 4:3 display without black bands). The range
    controls how much of the image is cropped. May not work with all video
    output drivers.

    This option has no effect if ``--video-unscaled`` option is used.

``--video-aspect-override=<ratio|no>``
    Override video aspect ratio, in case aspect information is incorrect or
    missing in the file being played.

    These values have special meaning:

    :0:  disable aspect ratio handling, pretend the video has square pixels
    :no: same as ``0``
    :-1: use the video stream or container aspect (default)

    But note that handling of these special values might change in the future.

    .. admonition:: Examples

        - ``--video-aspect-override=4:3``  or ``--video-aspect-override=1.3333``
        - ``--video-aspect-override=16:9`` or ``--video-aspect-override=1.7777``
        - ``--no-video-aspect-override`` or ``--video-aspect-override=no``

``--video-aspect-method=<bitstream|container>``
    This sets the default video aspect determination method (if the aspect is
    _not_ overridden by the user with ``--video-aspect-override`` or others).

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

    This option is disabled if ``--keepaspect=no`` is used.

``--video-pan-x=<value>``, ``--video-pan-y=<value>``
    Moves the displayed video rectangle by the given value in the X or Y
    direction. The unit is in fractions of the size of the scaled video (the
    full size, even if parts of the video are not visible due to panscan or
    other options).

    For example, displaying a video fullscreen on a 1920x1080 screen with
    ``--video-pan-x=-0.1`` would move the video 192 pixels to the left and
    ``--video-pan-y=-0.1`` would move the video 108 pixels up.

    This option is disabled if ``--keepaspect=no`` is used.

``--video-rotate=<0-359|no>``
    Rotate the video clockwise, in degrees. If ``no`` is given, the video is
    never rotated, even if the file has rotation metadata. (The rotation value
    is added to the rotation metadata, which means the value ``0`` would rotate
    the video according to the rotation metadata.)

    When using hardware decoding without copy-back, only 90° steps work, while
    software decoding and hardware decoding methods that copy the video back to
    system memory support all values between 0 and 359.

``--video-crop=<[W[xH]][+x+y]>``, ``--video-crop=<x:y>``
    Crop the video by starting at the x, y offset for w, h pixels. The crop is
    applied to the source video rectangle (before anamorphic stretch) by the VO.
    A crop rectangle that is not within the video rectangle will be ignored.
    This works with hwdec, unlike the equivalent 'lavfi-crop'. When offset is
    omitted, the central area will be cropped. Setting the crop to empty one
    ``--video-crop=0x0+0+0`` overrides container crop and disables cropping.
    Setting the crop to ``--video-crop=""`` disables manual cropping and restores
    the container crop if it's specified.

``--video-zoom=<value>``
    Adjust the video display scale factor by the given value. The parameter is
    given log 2. For example, ``--video-zoom=0`` is unscaled,
    ``--video-zoom=1`` is twice the size, ``--video-zoom=-2`` is one fourth of
    the size, and so on.

    This option is disabled if ``--keepaspect=no`` is used.

``--video-scale-x=<value>``, ``--video-scale-y=<value>``
    Multiply the video display size with the given value (default: 1.0). If a
    non-default value is used, this will be different from the window size, so
    video will be either cut off, or black bars are added.

    This value is multiplied with the value derived from ``--video-zoom`` and
    the normal video aspect ratio. This option is disabled if
    ``--keepaspect=no`` is used.

``--video-align-x=<-1-1>``, ``--video-align-y=<-1-1>``
    Moves the video rectangle within the black borders, which are usually added
    to pad the video to screen if video and screen aspect ratios are different.
    ``--video-align-y=-1`` would move the video to the top of the screen
    (leaving a border only on the bottom), a value of ``0`` centers it
    (default), and a value of ``1`` would put the video at the bottom of the
    screen.

    If video and screen aspect match perfectly, these options do nothing.

    This option is disabled if ``--keepaspect=no`` is used.

``--video-margin-ratio-left=<val>``, ``--video-margin-ratio-right=<val>``, ``--video-margin-ratio-top=<val>``, ``--video-margin-ratio-bottom=<val>``
    Set extra video margins on each border (default: 0). Each value is a ratio
    of the window size, using a range 0.0-1.0. For example, setting the option
    ``--video-margin-ratio-right=0.2`` at a window size of 1000 pixels will add
    a 200 pixels border on the right side of the window.

    The video is "boxed" by these margins. The window size is not changed. In
    particular it does not enlarge the window, and the margins will cause the
    video to be downscaled by default. This may or may not change in the future.

    The margins are applied after 90° video rotation, but before any other video
    transformations.

    This option is disabled if ``--keepaspect=no`` is used.

    Subtitles still may use the margins, depending on ``--sub-use-margins`` and
    similar options.

    These options were created for the OSC. Some odd decisions, such as making
    the margin values a ratio (instead of pixels), were made for the sake of
    the OSC. It's possible that these options may be replaced by ones that are
    more generally useful. The behavior of these options may change to fit
    OSC requirements better, too.

``--correct-pts=<yes|no>``
    ``--correct-pts=no`` switches mpv to a mode where video timing is
    determined using a fixed framerate value (either using the
    ``--container-fps-override`` option, or using file information). Sometimes,
    files with very broken timestamps can be played somewhat well in this mode.
    Note that video filters, subtitle rendering, seeking (including hr-seeks and
    backstepping), and audio synchronization can be completely broken in this mode.

``--container-fps-override=<float>``
    Override video framerate. Useful if the original value is wrong or missing.

    .. note::

        Works in ``--correct-pts=no`` mode only.

``--deinterlace=<yes|no|auto>``
    Enable or disable deinterlacing (default: no).
    Interlaced video shows ugly comb-like artifacts, which are visible on
    fast movement. Enabling this typically inserts the bwdif video filter in
    order to deinterlace the video, or lets the video output apply deinterlacing
    if supported.

    When using ``auto``, mpv will insert a deinterlacing filter if ffmpeg
    detects that the video frame is interlaced. Be aware that there can be false
    positives in certain cases, such as when files are encoded as interlaced
    despite the video not actually being so. This is why ``auto`` is not the
    default value.

    Keep in mind that using this filter **will** conflict with any manually
    inserted deinterlacing filters, and that this will make video look worse if
    it's not actually interlaced.

``--deinterlace-field-parity=<tff|bff|auto>``
    Specify the field parity/order when deinterlacing (default: auto).
    Each frame of an interlaced video is divided into two fields, which are
    then separately transmitted. Top field represents even lines while bottom
    field represents odd lines. When deinterlacing the deinterlacer needs to
    know the correct temporal order of the fields else the video will appear
    jittery.

    ``auto`` will automatically try to detect the field order of the video,
    ``tff`` forces top field first while ``bff`` forces bottom field first.

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

    By default, this is set to ``h264,vc1,hevc,vp8,vp9,av1,prores``. Note that
    the hardware acceleration special codecs like ``h264_vdpau`` are not
    relevant anymore, and in fact have been removed from FFmpeg in this form.

    This is usually only needed with broken GPUs, where a codec is reported
    as supported, but decoding causes more problems than it solves.

    .. note::

        On some broken drivers (e.g. NVIDIA on Linux), probing for codecs which
        the GPU does not support can unnecessarily slow down video playback
        initialization. To alleviate this, explicitly specify a list which
        only includes the codecs supported on the setup.

    .. admonition:: Example

        ``mpv --hwdec=vdpau --hwdec-codecs=h264,mpeg2video``
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

    Setting this to a higher number might break the playback start fallback: if
    a fallback happens, parts of the file will be skipped, approximately by to
    the number of packets that could not be decoded. Values below an unspecified
    count will not have this problem, because mpv retains the packets.

``--vd-lavc-film-grain=<auto|cpu|gpu>``
    Enables film grain application on the GPU. If video decoding is done on
    the CPU, doing film grain application on the GPU can speed up decoding.
    This option can also help hardware decoding, as it can reduce the number
    of frame copies done.

    By default, it's set to ``auto``, so if the VO supports film grain
    application, then it will be treated as ``gpu``. If the VO does not
    support this, then it will be treated as ``cpu``, regardless of the setting.
    Currently, only ``gpu-next`` supports film grain application.

``--vd-lavc-dr=<auto|yes|no>``
    Enable direct rendering (default: auto). If this is set to ``yes``, the
    video will be decoded directly to GPU video memory (or staging buffers).
    This can speed up video upload, and may help with large resolutions or
    slow hardware. This works only with the following VOs:

        - ``gpu``: requires at least OpenGL 4.4 or Vulkan.
        - ``libmpv``: The libmpv render API has optional support.

    The ``auto`` option will try to guess whether DR can improve performance
    on your particular hardware. Currently this enables it on AMD or NVIDIA
    if using OpenGL or unconditionally if using Vulkan.

    Using video filters of any kind that write to the image data (or output
    newly allocated frames) will silently disable the DR code path.

``--vd-lavc-bitexact``
    Only use bit-exact algorithms in all decoding steps (for codec testing).

``--vd-lavc-fast`` (MPEG-1/2 and H.264 only)
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

    This is a key/value list option. See `List Options`_ for details.

    .. admonition:: Example

        ``--vd-lavc-o=debug=pict``

``--vd-lavc-show-all=<yes|no>``
    Show even broken/corrupt frames (default: no). If this option is set to
    no, libavcodec won't output frames that were either decoded before an
    initial keyframe was decoded, or frames that are recognized as corrupted.

``--vd-lavc-skiploopfilter=<skipvalue>`` (H.264, HEVC only)
    Skips the loop filter (AKA deblocking) during decoding. Since
    the filtered frame is supposed to be used as reference for decoding
    dependent frames, this has a worse effect on quality than not doing
    deblocking on e.g. MPEG-2 video. But at least for high bitrate HDTV,
    this provides a big speedup with little visible quality loss.
    Codecs other than H.264 or HEVC may have partial support for this option
    (often only ``all`` and ``none``).

    ``<skipvalue>`` can be one of the following:

    :none:    Never skip.
    :default: Skip useless processing steps (e.g. 0 size packets in AVI).
    :nonref:  Skip frames that are not referenced (i.e. not used for
              decoding other frames, the error cannot "build up").
    :bidir:   Skip B-Frames.
    :nonkey:  Skip all frames except keyframes.
    :all:     Skip all frames.

``--vd-lavc-skipidct=<skipvalue>`` (MPEG-1/2/4 only)
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
    uses 4:4:4 chroma, then libavcodec will by default show corrupted video.
    This option sets the libavcodec ``x264_build`` option to ``150``, which
    means that if the stream contains no version info, or was not encoded by
    x264 at all, it assumes it was encoded by the old version. Enabling this
    option is pretty safe if you want your broken files to work, but in theory
    this can break on streams not encoded by x264, or if a stream encoded by a
    newer x264 version contains no version info.

``--vd-apply-cropping``
    Certain video codecs support cropping, meaning that only a sub-rectangle of
    the decoded frame is intended for display. This option controls how cropping
    is handled by libavcodec. Cropping during decoding has certain limitations
    with regards to alignment and hardware decoding. If this option is enabled,
    decoder will apply the crop, else VO will handle it. Enabled by default.

``--swapchain-depth=<N>``
    Allow up to N in-flight frames. This essentially controls the frame
    latency. Increasing the swapchain depth can improve pipelining and prevent
    missed vsyncs, but increases visible latency. This option only mandates an
    upper limit, the implementation can use a lower latency than requested
    internally. A setting of 1 means that the VO will wait for every frame to
    become visible before starting to render the next frame. (Default: 3)

Audio
-----

``--audio-pitch-correction=<yes|no>``
    If this is enabled (default), playing with a speed different from normal
    automatically inserts the ``scaletempo2`` audio filter. You can insert
    filters besides ``scaletempo2`` and modify their params using
    `Conditional auto profiles`:

    ::

        [af_insert]
        profile-cond=speed ~= 1
        profile-restore=copy
        af-add=scaletempo2=search-interval=50 # Insert filter and params here.

    Filters set this way replace the ``scaletempo2`` default, instead of
    overlapping with it. If there are multiple audio filters inserted that can do
    pitch correction, then only the last one in the filter chain is used.
    For details on the specifics of each available filter, see the audio filter
    section.

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

    This only works for some audio outputs, such as ``wasapi``, ``coreaudio``
    and ``pipewire``. Other audio outputs silently ignore this option.
    They either have no concept of exclusive mode, or the mpv side of the
    implementation is missing.

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
    The option variants ``--af-add``, ``--af-pre``, and ``--af-clr`` exist
    to modify a previously specified list, but you should not need these for
    typical use.

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

    .. warning::

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
            Prefer the FFmpeg ``mp3float`` decoder over all other MP3
            decoders.

        ``--ad=help``
            List all available decoders.

    .. warning::

        Enabling compressed audio passthrough (AC3 and DTS via SPDIF/HDMI) with
        this option is not possible. Use ``--audio-spdif`` instead.

``--volume=<value>``
    Set the startup volume. 0 means silence, 100 means no volume reduction or
    amplification. Negative values can be passed for compatibility, but are
    treated as 0.

    Since mpv 0.18.1, this always controls the internal mixer (aka software
    volume).

``--volume-max=<100.0-1000.0>``
    Set the maximum amplification level in percent (default: 130). A value of
    130 will allow you to adjust the volume up to about double the normal level.

``--volume-gain=<db>``
    Set the volume gain in dB. This is applied on top of other volume and gain
    settings.

``--volume-gain-max=<0.0-150.0>``, ``--volume-gain-min=<-150.0-0.0>``
    Set the volume gain range in dB (default: -96 dB min, 12 dB max).

``--replaygain=<no|track|album>``
    Adjust volume gain according to replaygain values stored in the file
    metadata. With ``--replaygain=no`` (the default), perform no adjustment.
    With ``--replaygain=track``, apply track gain. With ``--replaygain=album``,
    apply album gain if present and fall back to track gain otherwise.

``--replaygain-preamp=<db>``
    Pre-amplification gain in dB to apply to the selected replaygain gain
    (default: 0).

``--replaygain-clip=<yes|no>``
    Allow the volume gain to clip (default: no). If this option is not
    enabled, mpv automatically will prevent clipping by lowering the gain.

``--replaygain-fallback=<db>``
    Gain in dB to apply if the file has no replay gain tags. This option
    is always applied if the replaygain logic is somehow inactive. If this
    is applied, no other replaygain options are applied.

``--audio-delay=<sec>``
    Audio delay in seconds (positive or negative float value). Positive values
    delay the audio, and negative values delay the video.

``--mute=<yes|no>``
    Set startup audio mute status (default: no).

    See also: ``--volume``.

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
    Whether to request audio channel downmixing from the decoder (default: no).
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

    This is a key/value list option. See `List Options`_ for details.

``--ad-spdif-dtshd=<yes|no>``, ``--dtshd=<yes|no>``
    If DTS is passed through, use DTS-HD.

    .. warning::

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
    - ``--audio-channels=<stereo|mono>``
        Force a downmix to stereo or mono. These are special-cases of the
        previous item. (See paragraphs below for implications.)

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

    .. warning::

        Using ``auto`` can cause issues when using audio over HDMI. The OS will
        typically report all channel layouts that _can_ go over HDMI, even if
        the receiver does not support them. If a receiver gets an unsupported
        channel layout, random things can happen, such as dropping the
        additional channels, or adding noise.

        You are recommended to set an explicit whitelist of the layouts you
        want. For example, most A/V receivers connected via HDMI and that can
        do 7.1 would  be served by: ``--audio-channels=7.1,5.1,stereo``

``--audio-display=<no|embedded-first|external-first>``
    Determines whether to display cover art when playing audio files and with
    what priority. It will display the first image found, and additional images
    are available as video tracks.

    :no:             Disable display of video entirely when playing audio
                     files.
    :embedded-first: Display embedded images and external cover art, giving
                     priority to embedded images (default).
    :external-first: Display embedded images and external cover art, giving
                     priority to external files.

    This option has no influence on files with normal video tracks.

``--audio-files=<files>``
    Play audio from an external file while viewing a video.

    This is a path list option. See `List Options`_ for details.

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
    of the current media, the internal swresample audio filter will be inserted
    into the audio filter layer to compensate for the difference.

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
            If video is still going on when there is still audio, trying to use
            gapless is also explicitly given up.

    .. note::

        This feature is implemented in a simple manner and relies on audio
        output device buffering to continue playback while moving from one file
        to another. If playback of the new file starts slowly, for example
        because it is played from a remote network location or because you have
        specified cache settings that require time for the initial cache fill,
        then the buffered audio may run out before playback of the new file
        can start.

``--initial-audio-sync=<yes|no>``
    When starting a video file or after events such as seeking, mpv will by
    default modify the audio stream to make it start from the same timestamp
    as video, by either inserting silence at the start or cutting away the
    first samples. Disabling this option makes the player behave like older
    mpv versions did: video and audio are both started immediately even if
    their start timestamps differ, and then video timing is gradually adjusted
    if necessary to reach correct synchronization later.

``--audio-file-auto=<no|exact|fuzzy|all>``
    Load additional audio files matching the video filename. The parameter
    specifies how external audio files are matched.

    :no:    Don't automatically load external audio files (default).
    :exact: Load the media filename with audio file extension.
    :fuzzy: Load all audio files containing the media filename.
    :all:   Load all audio files in the current and ``--audio-file-paths``
            directories.

``--audio-exts=ext1,ext2,...``
    Audio file extentions to try to match when using ``--audio-file-auto``,
    ``--autocreate-playlist`` or ``--directory-filter-types``.

    This is a string list option. See `List Options`_ for details.
    Use ``--help=audio-exts`` to see default extensions.

``--audio-file-paths=<path1:path2:...>``
    Equivalent to ``--sub-file-paths`` option, but for auto-loaded audio files.

    This is a path list option. See `List Options`_ for details.

``--audio-client-name=<name>``
    The application name the player reports to the audio API. Can be useful
    if you want to force a different audio profile (e.g. with PulseAudio),
    or to set your own application name when using libmpv.

``--audio-buffer=<seconds>``
    Set the audio output minimum buffer. The audio device might actually create
    a larger buffer if it pleases. If the device creates a smaller buffer,
    additional audio is buffered in an additional software buffer.

    Making this larger may make soft-volume and other filters react slower,
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

    .. warning::

        This modifies certain subtle player behavior, like A/V-sync and underrun
        handling. Enabling this option is strongly discouraged.

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

``--sub-demuxer=<[+]name>``
    Force subtitle demuxer type for ``--sub-file``. Give the demuxer name as
    printed by ``--sub-demuxer=help``.

``--sub-lavc-o=<key>=<value>[,<key>=<value>[,...]]``
    Pass AVOptions to libavcodec decoder. Note, a patch to make the o=
    unneeded and pass all unknown options through the AVOption system is
    welcome. A full list of AVOptions can be found in the FFmpeg manual.

    This is a key/value list option. See `List Options`_ for details.

``--sub-delay=<sec>``
    Delays primary subtitles by ``<sec>`` seconds. Can be negative.

``--secondary-sub-delay=<sec>``
    Delays secondary subtitles by ``<sec>`` seconds. Can be negative.

``--sub-files=<file-list>``, ``--sub-file=<filename>``
    Add a subtitle file to the list of external subtitles.

    If you use ``--sub-file`` only once, this subtitle file is displayed by
    default.

    If ``--sub-file`` is used multiple times, the subtitle to use can be
    switched at runtime by cycling subtitle tracks. It's possible to show
    two subtitles at once: use ``--sid`` to select the first subtitle index,
    and ``--secondary-sid`` to select the second index. (The index is printed
    on the terminal output after the ``--sid=`` in the list of streams.)

    ``--sub-files`` is a path list option (see `List Options`_  for details), and
    can take multiple file names separated by ``:`` (Unix) or ``;`` (Windows),
    while  ``--sub-file`` takes a single filename, but can be used multiple
    times to add multiple files. Technically, ``--sub-file`` is a CLI/config
    file only alias for  ``--sub-files-append``.

``--secondary-sid=<ID|auto|no>``
    Select a secondary subtitle stream. This is similar to ``--sid``. If a
    secondary subtitle is selected, it will be rendered as toptitle (i.e. on
    the top of the screen) alongside the normal subtitle by default, and
    provides a way to render two subtitles at once.

    There are some caveats associated with this feature. For example, bitmap
    subtitles will always be rendered in their usual position, so selecting a
    bitmap subtitle as secondary subtitle will result in overlapping subtitles.
    Secondary subtitles are never shown on the terminal if video is disabled.

    .. note::

        Styling and interpretation of any formatting tags is disabled for the
        secondary subtitle. Internally, the same mechanism as ``--sub-ass=no``
        is used to strip the styling.

    .. note::

        If the main subtitle stream contains formatting tags which display the
        subtitle at the top of the screen, it will overlap with the secondary
        subtitle. To prevent this, you could use ``--sub-ass=no`` to disable
        styling in the main subtitle stream.

``--sub-scale=<0-100>``
    Factor for the text subtitle font size (default: 1).

    .. note::

        This affects ASS subtitles as well, and may lead to incorrect subtitle
        rendering. Use with care, or use ``--sub-font-size`` instead.

``--sub-scale-by-window=<yes|no>``
    Whether to scale subtitles with the window size (default: yes). If this is
    disabled while ``--sub-scale-with-window`` is set to yes, changing the window
    size won't change the subtitle font size.

    Affects plain text subtitles only (or ASS if ``--sub-ass-override`` is set
    high enough).

``--sub-scale-with-window=<yes|no>``
    Make the subtitle font size relative to the window (default: yes). If this is
    disabled while ``--sub-scale-by-window`` is set to yes, the subtitle font
    size is scaled relative to the video size instead.

    Affects plain text subtitles only (or ASS if ``--sub-ass-override`` is set
    high enough).

    .. note::

        By default, the subtitle font size is scaled with the window size.
        To make the font size constant, set only ``--sub-scale-by-window`` to no.
        To make the font size scale with video size instead, set only
        ``--sub-scale-with-window`` to no.
        It's not meaningful to set both options to no.

``--sub-ass-scale-with-window=<yes|no>``
    Like ``--sub-scale-with-window``, but affects subtitles in ASS format only.
    Like ``--sub-scale``, this can break ASS subtitles.

    Default: no.

``--embeddedfonts=<yes|no>``
    Use fonts embedded in Matroska container files and ASS scripts (default:
    yes). These fonts can be used for SSA/ASS subtitle rendering.

``--sub-pos=<0-150>``
    Specify the position of subtitles on the screen. The value is the vertical
    position of the subtitle in % of the screen height. 100 is the original
    position, which is often not the absolute bottom of the screen, but with
    some margin between the bottom and the subtitle. Values above 100 move the
    subtitle further down.

    .. warning::

        Text subtitles (as opposed to image subtitles) may be cut off if the
        value of the option is above 100. This is a libass restriction.

        This affects ASS subtitles as well, and may lead to incorrect subtitle
        rendering in addition to the problem above.

        Using ``--sub-margin-y`` can achieve this in a better way.

``--secondary-sub-pos=<0-150>``
    Specify the position of secondary subtitles on the screen. This is similar
    to ``--sub-pos`` but for secondary subtitles.

``--sub-speed=<0.1-10.0>``
    Multiply the subtitle event timestamps with the given value. Can be used
    to fix the playback speed for frame-based subtitle formats. Affects text
    subtitles only.

    .. admonition:: Example

        ``--sub-speed=25/23.976`` plays frame based subtitles which have been
        loaded assuming a framerate of 23.976 at 25 FPS.

``--sub-ass-style-overrides=<[Style.]Param=Value[,...]>``
    Override some style or script info parameters.

    This is a string list option. See `List Options`_ for details.

    .. admonition:: Examples

        - ``--sub-ass-style-overrides=FontName=Arial,Default.Bold=1``
        - ``--sub-ass-style-overrides=PlayResY=768``

    .. note::

        Using this option may lead to incorrect subtitle rendering.

``--sub-ass-hinting=<none|light|normal|native>``
    Set font hinting type. <type> can be:

    :none:       no hinting (default)
    :light:      FreeType autohinter, light mode
    :normal:     FreeType autohinter, normal mode
    :native:     font native hinter

    .. warning::

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

``--sub-ass-override=<no|yes|scale|force|strip>``
    Control whether user style overrides should be applied. Note that all of
    these overrides try to be somewhat smart about figuring out whether or not
    a subtitle is considered a "sign".

    :no:    Render subtitles as specified by the subtitle scripts, without
            overrides.
    :yes:   Apply all the ``--sub-ass-*`` style override options. Changing the
            default for any of these options can lead to incorrect subtitle
            rendering.
    :scale: Like ``yes``, but also apply ``--sub-scale`` (default).
    :force: Like ``yes``, but also force all ``--sub-*`` options. Can break
            rendering easily.
    :strip: Radically strip all ASS tags and styles from the subtitle. This
            is equivalent to the old ``--no-ass`` / ``--no-sub-ass`` options.

    This also controls some bitmap subtitle overrides, as well as HTML tags in
    formats like SRT, despite the name of the option.

``--secondary-sub-ass-override=<no|yes|scale|force|strip>``
    Control whether user secondary substyle overrides should be applied. This
    works exactly like ``--sub-ass-override``.

    Default: strip.

``--sub-ass-force-margins``
    Enables placing toptitles and subtitles in black borders when they are
    available, if the subtitles are in the ASS format.

    Default: no.

``--sub-use-margins``
    Enables placing toptitles and subtitles in black borders when they are
    available, if the subtitles are in a plain text format  (or ASS if
    ``--sub-ass-override`` is set high enough).

    Default: yes.

``--sub-ass-use-video-data=<none|aspect-ratio|all>``
    Controls which information about the video stream is passed to libass.
    Any option but ``all`` is incompatible with standard ASS as defined by VSFilter,
    whose behavior most subtitle scripts and renderers target, including libass.
    Video stream properties are needed to accurately emulate VSFilter semantics and
    withholding them will likely result in broken subtitle rendering for most files.
    It's thus recommended to only change this selectively if required on a per-file basis.

    :none:  Don't forward any video stream information.
    :aspect-ratio: Only forward aspect ratio; fallbacks are used for other properties.
                   This makes behaviour consistent across different video resolutions.
    :all:   Forward all available information, notably including storage resolution.

    For certain kinds of broken ASS files which got repurposed across
    several video resolutions without either setting ``LayoutRes`` headers
    or adjusting affected effects, it may be desirable to withhold storage resolution
    information from libass to ensure consistent rendering across resolutions.
    Among others this affects 3D rotations and blurs.
    When encountering such files, try setting ``aspect-ratio``.

    Even more broken files on anamorphic video might also exhibit stretching
    unless aspect ratio information is also faked, in this case you can try
    using ``none``. This has never an effect on non-anamorphic video.

    Default: ``all``

``--sub-ass-video-aspect-override=<no|ratio>``
    Allows passing any arbitrary aspect ratio to libass instead of the video’s
    actual aspect ratio. Zero or negative aspect ratios are identical to ``no``.

    This has no effect if ``sub-ass-use-video-data`` is set to ``none``.

``--sub-vsfilter-bidi-compat=<yes|no>``
    Set implicit bidi detection to ``ltr`` instead of ``auto`` to match ASS'
    default. This also disables libass' incompatible extensions. This currently
    includes bracket pair matching according to the revised Unicode
    Bidirectional Algorithm introduced in Unicode 6.3, and also affects how BiDi
    runs are split and processed, as well as soft linewrapping of unicode text.

    This affects plaintext (non-ASS) subtitles only. Default: no.

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

``--sub-ass=<yes|no>``
    Render ASS subtitles natively (default: yes).

    .. note::

        This has been deprecated by ``--sub-ass-override=strip``. You also
        may need ``--embeddedfonts=no`` to get the same behavior. Also,
        using ``--sub-ass-override=style`` should give better results
        without breaking subtitles too much.

    If ``--sub-ass=no`` is specified, all tags and style declarations are
    stripped and ignored on display. The subtitle renderer uses the font style
    as specified by the ``--sub-`` options instead.

    .. note::

        Using ``--sub-ass=no`` may lead to incorrect or completely broken
        rendering of ASS/SSA subtitles. It can sometimes be useful to forcibly
        override the styling of ASS subtitles, but should be avoided in general.

``--sub-auto=<no|exact|fuzzy|all>``
    Load additional subtitle files matching the video filename. The parameter
    specifies how external subtitle files are matched. ``exact`` is enabled by
    default.

    :no:    Don't automatically load external subtitle files.
    :exact: Load the media filename with subtitle file extension and possibly
            language suffixes (default).
    :fuzzy: Load all subs containing the media filename.
    :all:   Load all subs in the current and ``--sub-file-paths`` directories.

``--sub-auto-exts=ext1,ext2,...``
    Subtitle extentions to try and match when using ``--sub-auto``. Note that
    modifying this list will also affect what mpv recognizes as subtitles when
    using drag and drop.

    This is a string list option. See `List Options`_ for details.

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

    .. note::

        This works for text subtitle files only. Other types of subtitles (in
        particular subtitles in mkv files) are always assumed to be UTF-8.


``--sub-stretch-durations=<yes|no>``
    Stretch a subtitle duration so it ends when the next one starts.
    Should help with subtitles which erroneously have zero durations.

    .. note::

        Only applies to text subtitles.

``--sub-fix-timing=<yes|no>``
    Adjust subtitle timing is to remove minor gaps or overlaps between
    subtitles (if the difference is smaller than 210 ms, the gap or overlap
    is removed).

``--sub-forced-events-only=<yes|no>``
    Enabling this displays only forced events within subtitle streams. Only
    some bitmap subtitle formats (such as DVD or PGS) are capable of having a
    mixture of forced and unforced events within the stream. Enabling this on
    text subtitles will cause no subtitles to be displayed (default: ``no``).

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

    This is a path list option. See `List Options`_ for details.

``--sub-visibility=<yes|no>``
    Can be used to disable display of subtitles, but still select and decode
    them.

``--secondary-sub-visibility=<yes|no>``
    Can be used to disable display of secondary subtitles, but still select and
    decode them.

``--sub-clear-on-seek``
    (Obscure, rarely useful.) Can be used to play broken mkv files with
    duplicate ReadOrder fields. ReadOrder is the first field in a
    Matroska-style ASS subtitle packets. It should be unique, and libass
    uses it for fast elimination of duplicates. This option disables caching
    of subtitles across seeks, so after a seek libass can't eliminate subtitle
    packets with the same ReadOrder as earlier packets. Note that enabling this
    option can result in broken subtitle behavior if you are not actually
    playing one of the aforementioned broken mkv files.

``--teletext-page=<-1-999>``
    Select a teletext page number to decode.

    This works for ``dvb_teletext`` subtitle streams, and if FFmpeg has been
    compiled with support for it.

    Values ``1-999`` are for individual pages. Special value ``0`` (default)
    matches all subtitle pages. Special value ``-1`` matches all pages.

    Note that page ``100`` is the default start page of actual teletext. It is
    also the former default value of this option.

    See the ``libzvbi-teletext`` section in FFmpeg documentation for details.

    Default: 0

``--sub-past-video-end``
    After the last frame of video, if this option is enabled, subtitles will
    continue to update based on audio timestamps. Otherwise, the subtitles
    for the last video frame will stay onscreen.

    Default: disabled

``--sub-font=<name>``
    Specify font to use for subtitles that do not themselves
    specify a particular font. The default is ``sans-serif``.

    .. admonition:: Examples

        - ``--sub-font='Bitstream Vera Sans'``
        - ``--sub-font='Comic Sans MS'``

    .. note::

        The ``--sub-font`` option (and many other style related ``--sub-``
        options) are ignored when ASS-subtitles are rendered, unless
        ``--sub-ass=no`` is specified.

        This used to support fontconfig patterns. Starting with libass 0.13.0,
        this stopped working.

``--sub-font-size=<size>``
    Specify the sub font size. The unit is the size in scaled pixels at a
    window height of 720. The actual pixel size is scaled with the window
    height: if the window height is larger or smaller than 720, the actual size
    of the text increases or decreases as well.

    Default: 55.

``--sub-blur=<0..20.0>``
    Gaussian blur factor applied to the sub font border.
    0 means no blur applied (default).

``--sub-bold=<yes|no>``
    Format text on bold.

``--sub-italic=<yes|no>``
    Format text on italic.

``--sub-outline-color=<color>``
    See ``--sub-color``. Color used for the sub font outline.

    ``--sub-border-color`` is an alias for ``--sub-outline-color``.

``--sub-back-color=<color>``
    See ``--sub-color``. Color used for sub text background.

    ``--sub-shadow-color`` is an alias for ``--sub-back-color``.

``--sub-outline-size=<size>``
    Size of the sub font outline in scaled pixels (see ``--sub-font-size``
    for details). A value of 0 disables outlines.

    ``--sub-border-size`` is an alias for ``--sub-outline-size``.

    Default: 3.

``--sub-border-style=<outline-and-shadow|opaque-box|background-box>``
    The style of the border.

    - ``outline-and-shadow``: draw outline and shadow.
      The size of the outline is determined by ``--sub-outline-size``,
      and the offset of the shadow is determined by ``--sub-shadow-offset``.
      The outline is colored by ``--sub-outline-color``,
      and the shadow is colored by ``--sub-back-color``.
      This corresponds to ``BorderStyle=1`` in the ASS spec.
    - ``opaque-box``: draw outline and shadow as opaque boxes that tightly wrap each lines of text.
      The margin of the outline opaque box is determined by ``--sub-outline-size``,
      and the offset of the shadow opaque box is determined by ``--sub-shadow-offset``.
      The outline opaque box is colored by ``--sub-outline-color``,
      and the shadow opaque box is colored by ``--sub-back-color``.
      Despite its name, the opaque box can be semi-transparent.
      This corresponds to ``BorderStyle=3`` in the ASS spec.
    - ``background-box``: draw a background box that bounds all lines of text.
      The background box is colored by ``--sub-back-color``,
      and the margin of the background box is determined by ``--sub-shadow-offset``.
      The behavior of the outline is the same as the ``outline-and-shadow`` style.
      This corresponds to ``BorderStyle=4``, which is a libass-specific extension.

    Default: ``outline-and-shadow``.

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
    Alpha is given with ``#AARRGGBB``.

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

    Never applied to ASS subtitles, except in ``--sub-ass=no`` mode. Likewise,
    this does not apply to image subtitles.

``--sub-align-y=<top|center|bottom>``
    Vertical position (default: ``bottom``).
    Details see ``--sub-align-x``.

``--sub-justify=<auto|left|center|right>``
    Control how multi line subs are justified irrespective of where they
    are aligned (default: ``auto`` which justifies as defined by
    ``--sub-align-x``).
    Left justification is recommended to make the subs easier to read
    as it is easier for the eyes.

``--sub-ass-justify=<yes|no>``
    Applies justification as defined by ``--sub-justify`` on ASS subtitles
    if ``--sub-ass-override`` is not set to ``no``.
    Default: ``no``.

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

    It removes speaker labels (like MAN:) and any text enclosed within symbols like
    parentheses or brackets as specified by the ``--sub-filter-sdh-enclosures`` option.
    Note that parenthesis (full width parenthesis and the normal variant) are a special
    case and only upper case text is removed. For more filtering, you can use the
    ``--sub-filter-sdh-harder`` option.

    Default: ``no``.

``--sub-filter-sdh-harder=<yes|no>``
    Do harder SDH filtering (if enabled by ``--sub-filter-sdh``).
    Will also remove speaker labels and text within parentheses using both
    lower and upper case letters.

    Default: ``no``.

``--sub-filter-sdh-enclosures=<string>``
    Specify a string of characters that ``--sub-filter-sdh`` will use to potentially
    remove text. Text that is enclosed within characters specified by this string will
    be removed. Note that bracket characters with known pairs (such as ``(`` or ``[``)
    will be mapped internally to their matching right hand character, so you only need
    to specify left hand characters.

    Default: ``([（``.

``--sub-filter-regex-...=...``
    Set a list of regular expressions to match on text subtitles, and remove any
    lines that match (default: empty). This is a string list option. See
    `List Options`_ for details. Normally, you should use
    ``--sub-filter-regex-append=<regex>``, where each option use will append a
    new regular expression, without having to fight escaping problems.

    List items are matched in order. If a regular expression matches, the
    process is stopped, and the subtitle line is discarded. The text matched
    against is, by default, the ``Text`` field of ASS events (if the
    subtitle format is different, it is always converted). This may include
    formatting tags. Matching is case-insensitive, but how this is done depends
    on the libc, and most likely works in ASCII only. It does not work on
    bitmap/image subtitles. Unavailable on inferior OSes (requires POSIX regex
    support).

    .. admonition:: Example

        ``--sub-filter-regex-append=opensubtitles\.org`` filters some ads.

    Technically, using a list for matching is redundant, since you could just
    use a single combined regular expression. But it helps with diagnosis,
    ease of use, and temporarily disabling or enabling individual filters.

    .. warning::

        This is experimental. The semantics most likely will change, and if you
        use this, you should be prepared to update the option later. Ideas
        include replacing the regexes with a very primitive and small subset of
        sed, or some method to control case-sensitivity.

``--sub-filter-jsre-...=...``
    Same as ``--sub-filter-regex`` but with JavaScript regular expressions.
    Shares/affected-by all ``--sub-filter-regex-*`` control options (see below),
    and also experimental. Requires only JavaScript support.

``--sub-filter-regex-plain=<yes|no>``
    Whether to first convert the ASS "Text" field to plain-text (default: no).
    This strips ASS tags and applies ASS directives, like ``\N`` to new-line.
    If the result is multi-line then the regexp anchors ``^`` and ``$`` match
    each line, but still any match discards all lines.

``--sub-filter-regex-warn=<yes|no>``
    Log dropped lines with warning log level, instead of verbose (default: no).
    Helpful for testing.

``--sub-filter-regex-enable=<yes|no>``
    Whether to enable regex filtering (default: yes). Note that if no regexes
    are added to the ``--sub-filter-regex`` list, setting this option to ``yes``
    has no effect. It's meant to easily disable or enable filtering
    temporarily.

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

``--sub-font-provider=<auto|none|fontconfig>``
    Which libass font provider backend to use (default: auto). ``auto`` will
    attempt to use the native font provider: fontconfig on Linux, CoreText on
    macOS, DirectWrite on Windows. ``fontconfig`` forces fontconfig, if libass
    was built with support (if not, it behaves like ``none``).

    The ``none`` font provider effectively disables system fonts. It will still
    attempt to use embedded fonts (unless ``--embeddedfonts=no`` is set; this is
    the same behavior as with all other font providers), ``subfont.ttf`` if
    provided, and fonts in  the ``fonts`` sub-directory if provided. (The
    fallback is more strict than that of other font providers, and if a font
    name does not match, it may prefer not to render any text that uses the
    missing font.)

``--sub-fonts-dir=<path>``
    Font files in this directory are used by mpv/libass for subtitles. Useful
    if you do not want to install fonts to your system. Note that files in this
    directory are loaded into memory before being used by mpv. If you have a
    lot of fonts, consider using fonts.conf (see `FILES`_ section) to include
    additional mpv user settings.

    If this option is not specified, ``~~/fonts`` will be used by default.

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

    .. admonition:: Note (Wayland)

        This option does not actually work on wayland since window placement is
        not allowed. However setting this option does influence mpv's initial
        guess at finding an output which may be useful for options like
        ``--geometry`` or ``--autofit`` which depend on the monitor resolution.

    See also ``--fs-screen``.

``--screen-name=<string>``
    In multi-monitor configurations, this option tells mpv which screen to
    display the video on based on the screen name from the video backend. The
    same caveats in the ``--screen`` option also apply here. This option is
    ignored and does nothing if ``--screen`` is explicitly set.

``--fullscreen``, ``--fs``
    Fullscreen playback.

``--fs-screen=<all|current|0-32>``
    In multi-monitor configurations (i.e. a single desktop that spans across
    multiple displays), this option tells mpv which screen to go fullscreen to.
    If ``current`` is used mpv will fallback on what the user provided with
    the ``screen`` option.

    .. admonition:: Note (X11)

        This option works properly only with window managers which
        understand the EWMH ``_NET_WM_FULLSCREEN_MONITORS`` hint.

    .. admonition:: Note (macOS)

        ``all`` does not work on macOS and will behave like ``current``.

    See also ``--screen``.

``--fs-screen-name=<string>``
    In multi-monitor configurations, this option tells mpv which screen to go
    fullscreen to based on the screen name from the video backend. The same
    caveats in the ``--fs-screen`` option also apply here. This option is
    ignored and does nothing if ``--fs-screen`` is explicitly set.

``--keep-open=<yes|no|always>``
    Do not terminate when playing or seeking beyond the end of the file, and
    there is no next file to be played (and ``--loop`` is not used).
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

    When viewing images, the playback time is not tracked on the command line
    output, and the image frame is not duplicated when encoding. To force the
    player into "dumb mode" and actually count out seconds, or to duplicate the
    image when encoding, you need to use ``--demuxer=lavf
    --demuxer-lavf-o=loop=1``, and use ``--length`` or ``--frames`` to stop
    after a particular time.

``--force-window=<yes|no|immediate>``
    Create a video output window even if there is no video. This can be useful
    when pretending that mpv is a GUI application. Currently, the window
    always has the size 960x540, and is subject to ``--geometry``,
    ``--autofit``, and similar options.

    .. warning::

        The window is created only after initialization (to make sure default
        window placement still works if the video size is different from the
        ``--force-window`` default window size). This can be a problem if
        initialization doesn't work perfectly, such as when opening URLs with
        bad network connection, or opening broken video files. The ``immediate``
        mode can be used to create the window always on program start, but this
        may cause other issues.

``--taskbar-progress=<yes|no>``
    (Windows only)
    Enable/disable playback progress rendering in taskbar (Windows 7 and above).

    Enabled by default.

``--snap-window``
    (Windows only) Snap the player window to screen edges.

``--drag-and-drop=<no|auto|replace|append|insert-next>``
    Controls the default behavior of drag and drop on platforms that support
    this. ``auto`` will obey what the underlying os/platform gives mpv.
    Typically, holding shift during the drag and drop will append the item to
    the playlist. Otherwise, it will completely replace it. ``replace``,
    ``append``, and ``insert-next`` always force replacing, appending to, and
    inserting next into the playlist respectively. ``no`` disables all drag and
    drop behavior.

``--ontop``
    Makes the player window stay on top of other windows.

    On Windows, if combined with fullscreen mode, this causes mpv to be
    treated as exclusive fullscreen window that bypasses the Desktop Window
    Manager.

``--ontop-level=<window|system|desktop|level>``
    (macOS only)
    Sets the level of an ontop window (default: window).

    :window:  On top of all other windows.
    :system:  On top of system elements like Taskbar, Menubar and Dock.
    :desktop: On top of the Desktop behind windows and Desktop icons.
    :level:   A level as integer.

``--focus-on=<never|open|all>``,
    (macOS only)
    Focus the video window and make it the front most window on specific events (default: open).

    :never: Never focus the window on open or new file load events.
    :open:  Focus the window on creation, eg when a vo is initialised.
    :all:   Focus the window on open and new file load event.

``--window-corners=<default|donotround|round|roundsmall>``
    (Windows only)
    Set the preference for window corner rounding.

    :default: Let the system decide whether or not to round window corners
    :donotround: Never round window corners
    :round: Round the corners if appropriate
    :roundsmall: Round the corners if appropriate, with a small radius

``--border=<yes|no>``
    Play video with window border and decorations. Since this is on by
    default, use ``--no-border`` to disable the standard window decorations.

``--title-bar=<yes|no>``
    (Windows and X11 only)
    Play video with the window title bar. Since this is on by default,
    use ``--title-bar=no`` to hide the title bar. The ``--border`` option takes
    precedence.

``--on-all-workspaces``
    (X11 and macOS only)
    Show the video window on all virtual desktops.

``--geometry=<[W[xH]][+-x+-y][/WS]>``, ``--geometry=<x:y>``
    Adjust the initial window position or size. ``W`` and ``H`` set the window
    size in pixels. ``x`` and ``y`` set the window position, measured in pixels
    from the top-left corner of the screen to the top-left corner of the image
    being displayed. If a percentage sign (``%``) is given after the argument,
    it turns the value into a percentage of the screen size in that direction.
    Positions are specified similar to the standard X11 ``--geometry`` option
    format, in which e.g. +10-50 means "place 10 pixels from the left border and
    50 pixels from the lower border" and "--20+-10" means "place 20 pixels
    beyond the right and 10 pixels beyond the top border". A trailing ``/``
    followed by an integer denotes on which workspace (virtual desktop) the
    window should appear (X11 only).

    If an external window is specified using the ``--wid`` option, this
    option is ignored.

    The coordinates are relative to the screen given with ``--screen`` for the
    video output drivers that fully support ``--screen``.

    .. note::

        Generally only supported by GUI VOs. Ignored for encoding.

    .. admonition:: Note (macOS)

        On macOS, the origin of the screen coordinate system is located on the
        bottom-left corner. For instance, ``0:0`` will place the window at the
        bottom-left of the screen.

    .. admonition:: Note (X11)

        This option does not work properly with all window managers.

    .. admonition:: Note (Wayland)

        Wayland does not allow a client to position itself so this option will
        only affect the window size.

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
            ratio (with most VOs and with ``--keepaspect=yes``).
        ``50%+10+10/2``
            Sets the window to half the screen widths, and positions it 10
            pixels below/left of the top left corner of the screen, on the
            second workspace.

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
    This option behaves exactly like ``--autofit``, except that it sets the
    maximum size of the window.

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
    they override this option). Changing this option while the window is
    maximized can unmaximize the window depending on the OS and window manager.
    If the window does not unmaximize, the multiplier will be applied if the user
    unmaximizes the window later.

    For example, ``--window-scale=0.5`` would show the window at half the
    video size.

``--window-minimized=<yes|no>``
    Whether the video window is minimized or not. Setting this will minimize,
    or unminimize, the video window if the current VO supports it. Note that
    some VOs may support minimization while not supporting unminimization
    (eg: Wayland).

    Whether this option and ``--window-maximized`` work on program start or
    at runtime, and whether they're (at runtime) updated to reflect the actual
    window state, heavily depends on the VO and the windowing system. Some VOs
    simply do not implement them or parts of them, while other VOs may be
    restricted by the windowing systems (especially Wayland).

``--window-maximized=<yes|no>``
    Whether the video window is maximized or not. Setting this will maximize,
    or unmaximize, the video window if the current VO supports it. See
    ``--window-minimized`` for further remarks.

``--cursor-autohide=<number|no|always>``
    Make mouse cursor automatically hide after given number of milliseconds
    (default: 1000 ms). ``no`` will disable cursor autohide. ``always``
    means the cursor will stay hidden.

``--cursor-autohide-fs-only``
    If this option is given, the cursor is always visible in windowed mode. In
    fullscreen mode, the cursor is shown or hidden according to
    ``--cursor-autohide``.

``--force-rgba-osd-rendering``
    Change how some video outputs render the OSD and text subtitles. This
    does not change appearance of the subtitles and only has performance
    implications. For VOs which support native ASS rendering (like ``gpu``,
    ``vdpau``, ``direct3d``), this can be slightly faster or slower,
    depending on GPU drivers and hardware. For other VOs, this just makes
    rendering slower.

``--force-render``
    Forces mpv to always render frames regardless of the visibility of the
    window. Currently only affects X11 and Wayland VOs since they are the
    only ones that have this optimization (i.e. everything else always renders
    regardless of visibility).

``--force-window-position``
    Forcefully move mpv's video output window to default location whenever
    there is a change in video parameters, video stream or file. This used to
    be the default behavior. Currently only affects X11 and SDL VOs.

``--auto-window-resize=<yes|no>``
    By default, mpv will automatically resize itself if the video's size changes
    (i.e. advancing forward in a playlist). Setting this to ``no`` disables this
    behavior so the window size never changes automatically. This option does
    not have any impact on the ``--autofit`` or ``--geometry`` options.

``--keepaspect=<yes|no>``
    ``--keepaspect=no`` will always stretch the video to window size, and will
    disable the window manager hints that force the window aspect ratio.
    (Ignored in fullscreen mode.)

``--keepaspect-window=<yes|no>``
    ``--keepaspect-window=yes`` (the default) will lock the window size to the
    video aspect. ``--keepaspect-window=no`` disables this behavior, and will
    instead add black bars if window aspect and video aspect mismatch. Whether
    this actually works depends on the VO backend.
    (Ignored in fullscreen mode.)

``--monitoraspect=<ratio>``
    Set the aspect ratio of your monitor or TV screen. A value of 0 disables a
    previous setting (e.g. in the config file). Overrides the
    ``--monitorpixelaspect`` setting if enabled.

    See also ``--monitorpixelaspect`` and ``--video-aspect-override``.

    .. admonition:: Examples

        - ``--monitoraspect=4:3``  or ``--monitoraspect=1.3333``
        - ``--monitoraspect=16:9`` or ``--monitoraspect=1.7777``

``--hidpi-window-scale=<yes|no>``
    Scale the window size according to the backing DPI scale factor from the OS
    (default: no). For example, if the OS DPI scaling is set to 200%, mpv's window
    size will be multiplied by 2.

``--native-fs=<yes|no>``
    (macOS only)
    Uses the native fullscreen mechanism of the OS (default: yes).

``--show-in-taskbar=<yes|no>``
    (Windows and X11 only)
    Show mpv in the taskbar (default: yes). If set to no, mpv will no longer
    appear in taskbars and tasklists in supported window managers, and may be
    excluded from Alt+Tab window switching.

``--monitorpixelaspect=<ratio>``
    Set the aspect of a single pixel of your monitor or TV screen (default:
    1). A value of 1 means square pixels (correct for (almost?) all LCDs). See
    also ``--monitoraspect`` and ``--video-aspect-override``.

``--stop-screensaver=<yes|no|always>``
    Turns off the screensaver (or screen blanker and similar mechanisms) at
    startup and turns it on again on exit (default: yes). When using ``yes``,
    the screensaver will re-enable when playback is not active. ``always`` will
    always disable the screensaver. Note that stopping the screensaver is only
    possible if a video output is available (i.e. there is an open mpv window).
    This is not supported on all video outputs, platforms, or desktop environments.

    Before mpv 0.33.0, the X11 backend ran ``xdg-screensaver reset`` in 10 second
    intervals when not paused in order to support screensaver inhibition in some
    environments. This functionality was removed in 0.33.0, but it is possible to
    call the ``xdg-screensaver`` command line program from a user script instead.

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
    ``uint32_t`` (all Windows handles are 32-bit), this is important as mpv will
    not accept negative values. mpv will create its own window and set the
    wid window as parent, like with X11.

    On macOS/Cocoa, the ID is interpreted as ``NSView*``. Pass it as value cast
    to ``intptr_t``. mpv will create its own sub-view. Because macOS does not
    support window embedding of foreign processes, this works only with libmpv,
    and will crash when used from the command line.

    On Android, the ID is interpreted as ``android.view.Surface``. Pass it as a
    value cast to ``intptr_t``. Use with ``--vo=mediacodec_embed`` and
    ``--hwdec=mediacodec`` for direct rendering using MediaCodec, or with
    ``--vo=gpu --gpu-context=android`` (with or without ``--hwdec=mediacodec``).

``--window-dragging=<yes|no>``
    Move the window when clicking on it and moving the mouse pointer (default: yes).

``--x11-name=<string>``
    Set the window instance name for X11-based video output methods.

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

``--x11-present=<no|auto|yes>``
    Whether or not to use presentation statistics from X11's presentation
    extension (default: ``auto``).

    mpv asks X11 for present events which it then may use for more accurate
    frame presentation. This only has an effect if ``--video-sync=display-...``
    is being used.

    The auto option enumerates XRandr providers for autodetection. If amd, radeon,
    intel, or nouveau (the standard x86 Mesa drivers) is found and nvidia is NOT
    found, presentation feedback is enabled. Other drivers are not assumed to
    work, so they are not enabled automatically.

    ``yes`` or ``no`` can still be passed regardless to enable/disable this
    mechanism in case there is good/bad behavior with whatever your combination
    of hardware/drivers/etc. happens to be.

``--x11-wid-title=<yes|no>``
    Whether or not to set the window title when mpv is embedded on X11 (default:
    ``no``).


Disc Devices
------------

``--cdda-device=<path>``
    Specify the CD device for CDDA playback (default: ``/dev/cdrom``).

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

    This is a key/value list option. See `List Options`_ for details.

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

``--demuxer-lavf-linearize-timestamps=<yes|no|auto>``
    Attempt to linearize timestamp resets in demuxed streams (default: auto).
    This was tested only for single audio streams. It's unknown whether it
    works correctly for video (but likely won't). Note that the implementation
    is slightly incorrect either way, and will introduce a discontinuity by
    about 1 codec frame size.

    The ``auto`` mode enables this for OGG audio stream. This covers the common
    and annoying case of OGG web radio streams. Some of these will reset
    timestamps to 0 every time a new song begins. This breaks the mpv seekable
    cache, which can't deal with timestamp resets. Note that FFmpeg/libavformat's
    seeking API can't deal with this either; it's likely that if this option
    breaks this even more, while if it's disabled, you can at least seek within
    the first song in the stream. Well, you won't get anything useful either
    way if the seek is outside of mpv's cache.

``--demuxer-lavf-propagate-opts=<yes|no>``
    Propagate FFmpeg-level options to recursively opened connections (default:
    yes). This is needed because FFmpeg will apply these settings to nested
    AVIO contexts automatically. On the other hand, this could break in certain
    situations - it's the FFmpeg API, you just can't win.

    This affects in particular the ``--timeout`` option and anything passed
    with ``--demuxer-lavf-o``.

    If this option is deemed unnecessary at some point in the future, it will
    be removed without notice.

``--demuxer-mkv-subtitle-preroll=<yes|index|no>``
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

``--demuxer-mkv-subtitle-preroll-secs=<value>``
    See ``--demuxer-mkv-subtitle-preroll``.

``--demuxer-mkv-subtitle-preroll-secs-index=<value>``
    See ``--demuxer-mkv-subtitle-preroll``.

``--demuxer-mkv-probe-start-time=<yes|no>``
    Check the start time of Matroska files (default: yes). This simply reads the
    first cluster timestamps and assumes it is the start time. Technically, this
    also reads the first timestamp, which may increase latency by one frame
    (which may be relevant for live streams).

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
    is useful only if the cache is enabled.

    Unlike the forward cache, there is no control how many seconds are actually
    cached - it will simply use as much memory this option allows. Setting this
    option to 0 will strictly disable any back buffer, but this will lead to
    the situation that the forward seek range starts after the current playback
    position (as it removes past packets that are seek points).

    If the end of the file is reached, the remaining unused forward buffer space
    is "donated" to the backbuffer (unless the backbuffer size is set to 0, or
    ``--demuxer-donate-buffer`` is set to ``no``).
    This still limits the total cache usage to the sum of the forward and
    backward cache, and effectively makes better use of the total allowed memory
    budget. (The opposite does not happen: free backward buffer is never
    "donated" to the forward buffer.)

    Keep in mind that other buffers in the player (like decoders) will cause the
    demuxer to cache "future" frames in the back buffer, which can skew the
    impression about how much data the backbuffer contains.

    See ``--list-options`` for defaults and value range.

``--demuxer-donate-buffer=<yes|no>``
    Whether to let the back buffer use part of the forward buffer (default: yes).
    If set to ``yes``, the "donation" behavior described in the option
    description for ``--demuxer-max-back-bytes`` is enabled. This means the
    back buffer may use up memory up to the sum of the forward and back buffer
    options, minus the active size of the forward buffer. If set to ``no``, the
    options strictly limit the forward and back buffer sizes separately.

    Note that if the end of the file is reached, the buffered data stays the
    same, even if you seek back within the cache. This is because the back
    buffer is only reduced when new data is read.

``--demuxer-seekable-cache=<yes|no|auto>``
    Debugging option to control whether seeking can use the demuxer cache
    (default: auto). Normally you don't ever need to set this; the default
    ``auto`` does the right thing and enables cache seeking it if ``--cache``
    is set to ``yes`` (or is implied ``yes`` if ``--cache=auto``).

    If enabled, short seek offsets will not trigger a low level demuxer seek
    (which means for example that slow network round trips or FFmpeg seek bugs
    can be avoided). If a seek cannot happen within the cached range, a low
    level seek will be triggered. Seeking outside of the cache will start a new
    cached range, but can discard the old cache range if the demuxer exhibits
    certain unsupported behavior.

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

``--demuxer-termination-timeout=<seconds>``
    Number of seconds the player should wait to shutdown the demuxer (default:
    0.1). The player will wait up to this much time before it closes the
    stream layer forcefully. Forceful closing usually means the network I/O is
    given no chance to close its connections gracefully (of course the OS can
    still close TCP connections properly), and might result in annoying messages
    being logged, and in some cases, confused remote servers.

    This timeout is usually only applied when loading has finished properly. If
    loading is aborted by the user, or in some corner cases like removing
    external tracks sourced from network during playback, forceful closing is
    always used.

``--demuxer-readahead-secs=<seconds>``
    If ``--demuxer-thread`` is enabled, this controls how much the demuxer
    should buffer ahead in seconds (default: 1). As long as no packet has
    a timestamp difference higher than the readahead amount relative to the
    last packet returned to the decoder, the demuxer keeps reading.

    Note that enabling the cache (such as ``--cache=yes``, or if the input
    is considered a network stream, and ``--cache=auto`` is used), this option
    is mostly ignored. (``--cache-secs`` will override this. Technically, the
    maximum of both options is used.)

    The main purpose of this option is to limit the readhead for local playback,
    since a large readahead value is not overly useful in this case.

    (This value tends to be fuzzy, because many file formats don't store linear
    timestamps.)

``--demuxer-hysteresis-secs=<seconds>``
    Once the demuxer limit is reached (``--demuxer-max-bytes``,
    ``--demuxer-readahead-secs`` or ``--cache-secs``), this value can be used
    to specify a hysteresis before the demuxer will buffer ahead again. This
    specifies the maximum number of seconds from the current playback position
    that needs to be remaining in the cache before the demuxer will continue
    buffering ahead.

    For example, with a value of 10 seconds specified, the demuxer will buffer
    ahead up to the demuxer limit and won't start buffering ahead again until
    there is only 10 seconds of content left in the cache.

    This can provide significant power savings and reduce load by making the
    demuxer only buffer ahead in chunks at a time rather than buffering ahead
    nonstop to keep the cache filled.

    If you want to save power and reduce load, configure this to a small number
    that's much lower than ``--cache-secs`` or ``--demuxer-readahead-secs``.
    If it takes a long time to buffer anything at all for a given stream (like
    when reading from a very slow disk is involved), then the hysteresis value
    should be larger to compensate.

    The default value is 0 seconds, which disables the caching hysteresis. A
    value of 10 seconds probably works well for most usecases.

``--prefetch-playlist=<yes|no>``
    Prefetch next playlist entry while playback of the current entry is ending
    (default: no).

    This does not prefill the cache with the video data of the next URL.
    Prefetching video data is supported only for the current playlist entry,
    and depends on the demuxer cache settings (on by default). This merely
    opens the URL of the next playlist entry as soon the current URL is fully
    read.

    This does **not** work with URLs resolved by the ``youtube-dl`` wrapper,
    and it won't.

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

``--demuxer-cache-wait=<yes|no>``
    Before starting playback, read data until either the end of the file was
    reached, or the demuxer cache has reached maximum capacity. Only once this
    is done, playback starts. This intentionally happens before the initial
    seek triggered with ``--start``. This does not change any runtime behavior
    after the initial caching. This option is useless if the file cannot be
    cached completely.

``--rar-list-all-volumes=<yes|no>``
    When opening multi-volume rar files, open all volumes to create a full list
    of contained files (default: no). If disabled, only the archive entries
    whose headers are located within the first volume are listed (and thus
    played when opening a .rar file with mpv). Doing so speeds up opening, and
    the typical idiotic use-case of playing uncompressed multi-volume rar files
    that contain a single media file is made faster.

    Opening is still slow, because for unknown, idiotic, and unnecessary reasons
    libarchive opens all volumes anyway when playing the main file, even though
    mpv iterated no archive entries yet.

``--directory-mode=<auto|lazy|recursive|ignore>``
    When opening a directory, open subdirectories lazily, recursively or not at
    all. The default is ``auto``, which behaves like ``recursive`` with
    ``--shuffle``, and like ``lazy`` otherwise.

``--directory-filter-types=<video,audio,image>``
    Media file types to filter when opening directory. If the list is empty,
    all files are added to the playlist. (Default: ``video,audio,image``)

    This is a string list option. See `List Options`_ for details.

``--autocreate-playlist=<no|filter|same>``
    When opening a local file, act as if the parent directory is opened and
    create a playlist automatically.

    :no:     Load a single file (default).
    :filter: Create a playlist from the parent directory with files matching
             ``--directory-filter-types``.
    :same:   Create a playlist from the parent directory with files matching the
             same category as the currently loaded file. One of the
             ``*-exts`` is selected based on the input file
             and only files with matching extensions are added to the playlist.
             If the input file itself is not matched to any extension list,
             the playlist is not autogenerated.

Input
-----

``--native-keyrepeat=<yes|no>``
    Use system settings for keyrepeat delay and rate, instead of
    ``--input-ar-delay`` and ``--input-ar-rate`` (default: no).
    Whether this applies depends on the VO backend and how it handles
    keyboard input. Does not apply to terminal input.

``--native-touch=<yes|no>``
    (Windows only)
    For platforms which send emulated mouse inputs for touch-unaware clients,
    such as Windows, use system native touch events, instead of receiving them
    as emulated mouse events (default: no). This is required for multi-touch
    support for these platforms.

    Note that this option has no effect on other platforms: either native touch
    is not supported by mpv, or the platform does not give an option to receive
    emulated mouse inputs (so native touch is always enabled, e.g. Wayland).

``--input-ar-delay``
    Delay in milliseconds before we start to autorepeat a key (default: 200).
    Set it to 0 to disable.

``--input-ar-rate``
    Number of key presses to generate per second on autorepeat (default: 40).

``--input-conf=<filename>``
    Specify input configuration file other than the default location in the mpv
    configuration directory (usually ``~/.config/mpv/input.conf``).

``--input-default-bindings=<yes|no>``
    Enable default-level ("weak") key bindings (default: yes). These are bindings
    which config files like ``input.conf`` can override. It currently affects the
    builtin key bindings, and keys which scripts bind using ``mp.add_key_binding``
    (but not ``mp.add_forced_key_binding`` because this overrides ``input.conf``).

``--input-builtin-bindings=<yes|no>``
    Enable loading of built-in key bindings during start-up (default: yes). This
    option is applied only during (lib)mpv initialization, and if disabled then it
    will not be not possible to enable them later. May be useful to libmpv clients.

``--input-builtin-dragging=<yes|no>``
    Enable the built-in window-dragging behavior (default: yes). Setting it to no
    disables the built-in dragging behavior. Note that unlike the ``window-dragging``
    option, this option only affects VOs which support the ``begin-vo-dragging``
    command, and does not disable window dragging initialized with the command.

``--input-cmdlist``
    Prints all commands that can be bound to keys.

``--input-commands=<cmd1,cmd2,...>``
    Define a list of commands for mpv to run. The syntax is the same as format
    as ``input.conf`` but without the key binding argument at the beginning.
    When this option is set at startup, the commands will run after audio and
    video playback are about to begin if applicable (in idle mode with no file,
    it will run immediately). When changing values at runtime, the commands will
    also run as soon as possible.

    This is a string list option. See `List Options`_ for details.

    .. admonition:: Example

        ``--input-commands="playlist-play-index 1,set ao-volume 40"``
            sets the playlist index to 1 and the ao-volume to 40

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

``--input-terminal=<yes|no>``
    ``--input-terminal=no`` prevents the player from reading key events from
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

``--input-ipc-client=fd://<N>``
    Connect a single IPC client to the given FD. This is somewhat similar to
    ``--input-ipc-server``, except no socket is created, and instead the passed
    FD is treated like a socket connection received from ``accept()``. In
    practice, you could pass either a FD created by ``socketpair()``, or a pipe.
    In both cases, you must make sure that the FD is actually inherited by mpv
    (do not set the POSIX ``CLOEXEC`` flag).

    The player quits when the connection is closed.

    This is somewhat similar to the removed ``--input-file`` option, except it
    supports only integer FDs, and cannot open actual paths.

    .. admonition:: Example

        ``--input-ipc-client=fd://123``

    .. note::

        To use this option on Windows, the fd must refer to a wrapped
        (created by ``_open_osfhandle``) named pipe server handle with a client
        already connected. The named pipe must be created duplex with overlapped
        IO and inheritable handles. The program communicates with mpv through
        the client handle.

    .. warning::

        Writing to the ``input-ipc-server`` option at runtime will start another
        instance of an IPC client handler for the ``input-ipc-client`` option,
        because initialization is bundled, and this thing is stupid. This is a
        bug. Writing to ``input-ipc-client`` at runtime will start another IPC
        client handler for the new value, without stopping the old one, even if
        the FD value is the same (but the string is different e.g. due to
        whitespace). This is not a bug.

``--input-gamepad=<yes|no>``
    Enable/disable SDL2 Gamepad support. Disabled by default.

``--input-cursor=<yes|no>``
    Permit mpv to receive pointer events reported by the video output
    driver. Necessary to use the OSC. Support depends on the VO in use.

``--input-cursor-passthrough=<yes|no>``
    Tell the backend windowing system to allow pointer events to passthrough
    the mpv window. This allows windows under mpv to instead receive pointer
    events as if the mpv window was never there.

``--input-media-keys=<yes|no>``
    On systems where mpv can choose between receiving media keys or letting
    the system handle them - this option controls whether mpv should receive
    them.

    Default: yes (except for libmpv). macOS and Windows only, because elsewhere
    mpv doesn't have a choice - the system decides whether to send media keys
    to mpv. For instance, on X11 or Wayland, system-wide media keys are not
    implemented. Whether media keys work when the mpv window is focused is
    implementation-defined.

``--input-preprocess-wheel=<yes|no>``
    Preprocess ``WHEEL_*`` events so that while scrolling on the horizontal
    or vertical direction, the events aren't generated for another direction
    even when the two directions are scrolled together (default: yes).

    This preprocessing can be beneficial for preventing accidentally seeking
    while changing the volume by scrolling on a touchpad with the default
    keybind. Due to the deadzone mechanism used, disabling the preprocessing
    allows for diagonal scrolling (such as panning) and potentially reduces
    input latency.

    Note that disabling the preprocessing does not affect any filtering done
    by the OS/driver before these events are delivered to mpv, if any.

``--input-right-alt-gr=<yes|no>``
    (macOS and Windows only)
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

``--input-touch-emulate-mouse=<yes|no>``
    When multi-touch support is enabled (either required by the platform,
    or enabled by ``--native-touch``), emulate mouse move and button presses
    for the touch events (default: yes). This is useful for compatibility
    for mouse key bindings and scripts which read mouse positions for platforms
    which do not support ``--native-touch=no`` (e.g. Wayland).

``--input-dragging-deadzone=<N>``
    Begin the built-in window dragging when the mouse moves outside a deadzone of
    ``N`` pixels while the mouse button is being held down (default: 3). This only
    affects VOs which support the ``begin-vo-dragging`` command.

OSD
---

``--osc=<yes|no>``
    Whether to load the on-screen-controller (default: yes).

``--osd-bar=<yes|no>``
    Enable display of the OSD bar (default: yes).

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

``--osd-playing-msg-duration=<time>``
    Set the duration of ``osd-playing-msg`` in ms. If this is unset,
    ``osd-playing-msg`` stays on screen for the duration of ``osd-duration``.

``--osd-playlist-entry=<title|filename|both>``
    Whether to display the media title, filename, or both. If the
    ``media-title`` is not available, it will display only the ``filename``.

    Default: ``title``.

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

``--osd-bar-outline-size=<size>``
    Size of the outline of the OSD bar in scaled pixels (see ``--sub-font-size``
    for details).

    ``--osd-bar-border-size`` is an alias for ``--osd-bar-outline-size``.

    Default: 0.5.

``--osd-blur=<0..20.0>``
    Gaussian blur factor applied to the OSD font border.
    0 means no blur applied (default).

``--osd-bold=<yes|no>``
    Format text on bold.

``--osd-italic=<yes|no>``
    Format text on italic.

``--osd-outline-color=<color>``
    See ``--sub-color``. Color used for the OSD font outline.

    ``--osd-border-color`` is an alias for ``--osd-outline-color``.

``--osd-back-color=<color>``
    See ``--sub-color``. Color used for OSD text background.

    ``--osd-shadow-color`` is an alias for ``--osd-back-color``.

``--osd-outline-size=<size>``
    Size of the OSD font outline in scaled pixels (see ``--sub-font-size``
    for details). A value of 0 disables outlines.

    ``--osd-border-size`` is an alias for ``--osd-outline-size``.

    Default: 3.

``--osd-border-style=<outline-and-shadow|opaque-box|background-box>``
    See ``--sub-border-style``. Style used for OSD text border.

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

    .. note::

        For scripts which draw user interface elements, it is recommended to
        respect the value of this option when deciding whether the elements
        are scaled with window size or not.

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
    the OSC needs to be disabled with ``--osc=no``).

    This option is somewhat experimental and could be replaced by another
    mechanism in the future.

``--osd-font-provider=<...>``
    See ``--sub-font-provider`` for details and accepted values. Note that
    unlike subtitles, OSD never uses embedded fonts from media files.

``--osd-fonts-dir=<path>``
    See ``--sub-fonts-dir`` for details.  Defaults to ``~~/fonts``.

Screenshot
----------

``--screenshot-format=<type>``
    Set the image file type used for saving screenshots.

    Available choices:

    :png:       PNG
    :jpg:       JPEG (default)
    :jpeg:      JPEG (alias for jpg)
    :webp:      WebP
    :jxl:       JPEG XL
    :avif:      AVIF

``--screenshot-tag-colorspace=<yes|no>``
    Tag screenshots with the appropriate colorspace (default: yes).

    Note that not all formats support this. When it is unsupported, or when
    this option is disabled, screenshots will be converted to sRGB before
    being written.

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

``--screenshot-dir=<path>``
    Store screenshots in this directory. This path is joined with the filename
    generated by ``--screenshot-template``. If the template filename is already
    absolute, the directory is ignored.

    ``--screenshot-directory`` is an alias for ``--screenshot-dir``.

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

``--screenshot-webp-lossless=<yes|no>``
    Write lossless WebP files. ``--screenshot-webp-quality`` is ignored if this
    is set. The default is no.

``--screenshot-webp-quality=<0-100>``
    Set the WebP quality level. Higher means better quality. The default is 75.

``--screenshot-webp-compression=<0-6>``
    Set the WebP compression level. Higher means better compression, but takes
    more CPU time. Note that this also affects the screenshot quality when used
    with lossy WebP files. The default is 4.

``--screenshot-jxl-distance=<0-15>``
    Set the JPEG XL Butteraugli distance. Lower means better quality. Lossless
    is 0.0, and 1.0 is approximately equivalent to JPEG quality 90 for
    photographic content. Use 0.1 for "visually lossless" screenshots. The
    default is 1.0.

``--screenshot-jxl-effort=<1-9>``
    Set the JPEG XL compression effort. Higher effort (usually) means better
    compression, but takes more CPU time. The default is 4.

``--screenshot-avif-encoder=<encoder>``
    Specify the AV1 encoder to be used by libavcodec for encoding avif
    screenshots.

    Default: ``libaom-av1``

``--screenshot-avif-pixfmt=<format>``
    Specify the pixel format for the libavcodec encoder. Defaults to empty,
    which lets mpv pick one close to the source format.

``--screenshot-avif-opts=key1=value1,key2=value2,...``
    Specifies libavcodec options for selected encoder. For more information,
    consult the FFmpeg documentation.

    Default: ``usage=allintra,crf=0,cpu-used=8``

    Note: the default is only guaranteed to work with the libaom-av1 encoder.
    Above options may not be valid and or optimal for other encoders.

    This is a key/value list option. See `List Options`_ for details.

    .. admonition:: Example

        "``--screenshot-avif-opts=crf=23,aq-mode=complexity``"
            sets the crf to 23 and quantization (aq-mode) to complexity based.

``--screenshot-sw=<yes|no>``
    Whether to use software rendering for screenshots (default: no).

    If set to no, the screenshot will be rendered by the current VO (only vo_gpu
    or vo_gpu_next currently). The advantage is that this will (probably) always
    show up as in the video window, because the same code is used for rendering.
    But since the renderer needs to be reinitialized, this can be slow and
    interrupt playback.

    If set to yes, the software scaler is used to convert the video to RGB (or
    whatever the target screenshot requires). In this case, conversion will
    run in a separate thread and will probably not interrupt playback. The
    software renderer may lack some capabilities, such as HDR rendering.
    If ``window`` mode is used, the image will also be scaled in software
    which may not accurately reflect the actual visible result.

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

``--sws-bitexact=<yes|no>``
    Unknown functionality (default: no). Consult libswscale source code. The
    primary purpose of this, as far as libswscale API goes), is to produce
    exactly the same output for the same input on all platforms (output has the
    same "bits" everywhere, thus "bitexact"). Typically disables optimizations.

``--sws-fast=<yes|no>``
    Allow optimizations that help with performance, but reduce quality (default:
    no).

    VOs like ``drm`` and ``x11`` will benefit a lot from using ``--sws-fast``.
    You may need to set other options, like ``--sws-scaler``. The builtin
    ``sws-fast`` profile sets this option and some others to gain performance
    for reduced quality. Also see ``--sws-allow-zimg``.

``--sws-allow-zimg=<yes|no>``
    Allow using zimg (if the component using the internal swscale wrapper
    explicitly allows so) (default: yes). In this case, zimg *may* be used, if
    the internal zimg wrapper supports the input and output formats. It will
    silently or noisily fall back to libswscale if one of these conditions does
    not apply.

    If zimg is used, the other ``--sws-`` options are ignored, and the
    ``--zimg-`` options are used instead.

    If the internal component using the swscale wrapper hooks up logging
    correctly, a verbose priority log message will indicate whether zimg is
    being used.

    Most things which need software conversion can make use of this.

    .. note::

        Do note that zimg *may* be slower than libswscale. Usually,
        it's faster on x86 platforms, but slower on ARM (due to lack of ARM
        specific optimizations). The mpv zimg wrapper uses unoptimized repacking
        for some formats, for which zimg cannot be blamed.

``--zimg-scaler=<point|bilinear|bicubic|spline16|spline36|lanczos>``
    Zimg luma scaler to use (default: lanczos).

``--zimg-scaler-param-a=<default|float>``, ``--zimg-scaler-param-b=<default|float>``
    Set scaler parameters. By default, these are set to the special string
    ``default``, which maps to a scaler-specific default value. Ignored if the
    scaler is not tunable.

    ``lanczos``
        ``--zimg-scaler-param-a`` is the number of taps.

    ``bicubic``
        a and b are the bicubic b and c parameters.

``--zimg-scaler-chroma=...``
    Same as ``--zimg-scaler``, for for chroma interpolation (default: bilinear).

``--zimg-scaler-chroma-param-a``, ``--zimg-scaler-chroma-param-b``
    Same as ``--zimg-scaler-param-a`` / ``--zimg-scaler-param-b``, for chroma.

``--zimg-dither=<no|ordered|random|error-diffusion>``
    Dithering (default: random).

``--zimg-threads=<auto|integer>``
    Set the maximum number of threads to use for scaling (default: auto).
    ``auto`` uses the number of logical cores on the current machine. Note that
    the scaler may use less threads (or even just 1 thread) depending on stuff.
    Passing a value of 1 disables threading and always scales the image in a
    single operation. Higher thread counts waste resources, but make it
    typically faster.

    Note that some zimg git versions had bugs that will corrupt the output if
    threads are used.

``--zimg-fast=<yes|no>``
    Allow optimizations that help with performance, but reduce quality (default:
    yes). Currently, this may simplify gamma conversion operations.


Audio Resampler
---------------

This controls the default options of any resampling done by mpv (but not within
libavfilter, within the system audio API resampler, or any other places).

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
    be documented by FFmpeg.

    This is a key/value list option. See `List Options`_ for details.

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

``--terminal=<yes|no>``
    ``--terminal=no`` disables any use of the terminal and stdin/stdout/stderr.
    This completely silences any message output.

    Unlike ``--really-quiet``, this disables input and terminal initialization
    as well.

``--msg-color=<yes|no>``
    Enable colorful console output on terminals (default: yes).

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

``--term-osd-bar=<yes|no>``
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

``--term-title=<string>``
    Set the terminal title. Currently, this simply concatenates the escape
    sequence setting the window title with the provided (property expanded)
    string. This will mess up if the expanded string contain bytes that end the
    escape sequence, or if the terminal does not understand the sequence. The
    latter probably includes the regrettable win32.

    Expands properties. See `Property Expansion`_.

``--msg-module``
    Prepend module name to each console message.

``--msg-time``
    Prepend timing information to each console message. The time is in
    seconds since the player process was started (technically, slightly
    later actually), using a monotonic time source depending on the OS. This
    is ``CLOCK_MONOTONIC`` on sane UNIX variants.

Cache
-----

``--cache=<yes|no|auto>``
    Decide whether to use network cache settings (default: auto).

    If enabled, use up to ``--cache-secs`` for the cache size (but still limited
    to ``--demuxer-max-bytes``), and make the cached data seekable (if possible).
    If disabled, ``--cache-pause`` and related are implicitly disabled.

    The ``auto`` choice enables this depending on whether the stream is thought
    to involve network accesses or other slow media (this is an imperfect
    heuristic).

    Before mpv 0.30.0, this used to accept a number, which specified the size
    of the cache in kilobytes. Use e.g. ``--cache --demuxer-max-bytes=123k``
    instead.

``--cache-secs=<seconds>``
    How many seconds of audio/video to prefetch if the cache is active. This
    overrides the ``--demuxer-readahead-secs`` option if and only if the cache
    is enabled and the value is larger. The default value is set to something
    very high, so the actually achieved readahead will usually be limited by
    the value of the ``--demuxer-max-bytes`` option. Setting this option is
    usually only useful for limiting readahead.

``--cache-on-disk=<yes|no>``
    Write packet data to a temporary file, instead of keeping them in memory.
    This makes sense only with ``--cache``. If the normal cache is disabled,
    this option is ignored.

    The cache file is append-only. Even if the player appears to prune data, the
    file space freed by it is not reused. The cache file is deleted when
    playback is closed.

    Note that packet metadata is still kept in memory. ``--demuxer-max-bytes``
    and related options are applied to metadata *only*. The size of this
    metadata  varies, but 50 MB per hour of media is typical. The cache
    statistics will report this metadats size, instead of the size of the cache
    file. If the metadata hits the size limits, the metadata is pruned (but not
    the cache file).

    When the media is closed, the cache file is deleted. A cache file is
    generally worthless after the media is closed, and it's hard to retrieve
    any media data from it (it's not supported by design).

    If the option is enabled at runtime, the cache file is created, but old data
    will remain in the memory cache. If the option is disabled at runtime, old
    data remains in the disk cache, and the cache file is not closed until the
    media is closed. If the option is disabled and enabled again, it will
    continue to use the cache file that was opened first.

``--demuxer-cache-dir=<path>``
    Directory where to create temporary files. Cache is stored in the system's
    cache directory (usually ``~/.cache/mpv``) if this is unset.

    Currently, this is used for ``--cache-on-disk`` only.

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
    ends before that for some other reason (like file end or maximum configured
    cache size reached), playback resumes earlier.

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

``--demuxer-cache-unlink-files=<immediate|whendone|no>``
    Whether or when to unlink cache files (default: immediate). This affects
    cache files which are inherently temporary, and which make no sense to
    remain on disk after the player terminates. This is a debugging option.

    ``immediate``
        Unlink cache file after they were created. The cache files won't be
        visible anymore, even though they're in use. This ensures they are
        guaranteed to be removed from disk when the player terminates, even if
        it crashes.

    ``whendone``
        Delete cache files after they are closed.

    ``no``
        Don't delete cache files. They will consume disk space without having a
        use.

    Currently, this is used for ``--cache-on-disk`` only.

``--stream-buffer-size=<bytesize>``
    Size of the low level stream byte buffer (default: 128KB). This is used as
    buffer between demuxer and low level I/O (e.g. sockets). Generally, this
    can be very small, and the main purpose is similar to the internal buffer
    FILE in the C standard library will have.

    Half of the buffer is always used for guaranteed seek back, which is
    important for unseekable input.

    There are known cases where this can help performance to set a large buffer:

        1. mp4 files. libavformat may trigger many small seeks in both
           directions, depending on how the file was muxed.

        2. Certain network filesystems, which do not have a cache, and where
           small reads can be inefficient.

    In other cases, setting this to a large value can reduce performance.

    Usually, read accesses are at half the buffer size, but it may happen that
    accesses are done alternating with smaller and larger sizes (this is due to
    the internal ring buffer wrap-around).

    See ``--list-options`` for defaults and value range. ``<bytesize>`` options
    accept suffixes such as ``KiB`` and ``MiB``.

``--vd-queue-enable=<yes|no>, --ad-queue-enable``
    Enable running the video/audio decoder on a separate thread (default: no).
    If enabled, the decoder is run on a separate thread, and a frame queue is
    put between decoder and higher level playback logic. The size of the frame
    queue is defined by the other options below.

    This is probably quite pointless. libavcodec already has multithreaded
    decoding (enabled by default), which makes this largely unnecessary. It
    might help in some corner cases with high bandwidth video that is slow to
    decode (in these cases libavcodec would block the playback logic, while
    using a decoding thread would distribute the decoding time evenly without
    affecting the playback logic). In other situations, it will simply make
    seeking slower and use significantly more memory.

    The queue size is restricted by the other ``--vd-queue-...`` options. The
    final queue size is the minimum as indicated by the option with the lowest
    limit. Each decoder/track has its own queue that may use the full configured
    queue size.

    Most queue options can be changed at runtime. ``--vd-queue-enable`` itself
    (and the audio equivalent) update only if decoding is completely
    reinitialized. However, setting ``--vd-queue-max-samples=1`` should almost
    lead to the same behavior as ``--vd-queue-enable=no``, so that value can
    be used for effectively runtime enabling/disabling the queue.

    This should not be used with hardware decoding. It is possible to enable
    this for audio, but it makes even less sense.

``--vd-queue-max-bytes=<bytesize>``, ``--ad-queue-max-bytes``
    Maximum approximate allowed size of the queue. If exceeded, decoding will
    be stopped. The maximum size can be exceeded by about 1 frame.

    See ``--list-options`` for defaults and value range. ``<bytesize>`` options
    accept suffixes such as ``KiB`` and ``MiB``.

``--vd-queue-max-samples=<int>``, ``--ad-queue-max-samples``
    Maximum number of frames (video) or samples (audio) of the queue. The audio
    size may be exceeded by about 1 frame.

    See ``--list-options`` for defaults and value range.

``--vd-queue-max-secs=<seconds>``, ``--ad-queue-max-secs``
    Maximum number of seconds of media in the queue. The special value 0 means
    no limit is set. The queue size may be exceeded by about 2 frames. Timestamp
    resets may lead to random queue size usage.

    See ``--list-options`` for defaults and value range.

Network
-------

``--user-agent=<string>``
    Use ``<string>`` as user agent for HTTP streaming.

``--cookies=<yes|no>``
    Support cookies when making HTTP requests. Disabled by default.

``--cookies-file=<filename>``
    Read HTTP cookies from <filename>. The file is assumed to be in Netscape
    format.

``--http-header-fields=<field1,field2>``
    Set custom HTTP fields when accessing HTTP stream.

    This is a string list option. See `List Options`_ for details.

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
    older FFmpeg versions.)

``--tls-verify``
    Verify peer certificates when using TLS (e.g. with ``https://...``).
    (Silently fails with older FFmpeg versions.)

``--tls-cert-file``
    A file containing a certificate to use in the handshake with the
    peer.

``--tls-key-file``
    A file containing the private key for the certificate.

``--referrer=<string>``
    Specify a referrer path or URL for HTTP requests.

``--network-timeout=<seconds>``
    Specify the network timeout in seconds (default: 60 seconds). This affects
    at least HTTP. The special value 0 uses the FFmpeg defaults. If a
    protocol is used which does not support timeouts, this option is silently
    ignored.

    .. warning::

        This breaks the RTSP protocol, because of inconsistent FFmpeg API
        regarding its internal timeout option. Not only does the RTSP timeout
        option accept different units (seconds instead of microseconds, causing
        mpv to pass it huge values), it will also overflow FFmpeg internal
        calculations. The worst is that merely setting the option will put RTSP
        into listening mode, which breaks any client uses. At time of this
        writing, the fix was not made effective yet. For this reason, this
        option is ignored (or should be ignored) on RTSP URLs. You can still
        set the timeout option directly with ``--demuxer-lavf-o``.

``--rtsp-transport=<lavf|udp|udp_multicast|tcp|http>``
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

``--dvbin-prog=<string>``
    This defines the program to tune to. Usually, you may specify this
    by using a stream URI like ``"dvb://ZDF HD"``, but you can tune to a
    different channel by writing to this property at runtime.
    Also see ``dvbin-channel-switch-offset`` for more useful channel
    switching functionality.

``--dvbin-card=<0-15>``
    Specifies using card number 0-15 (default: 0).

``--dvbin-file=<filename>``
    Instructs mpv to read the channels list from ``<filename>``. The default is
    in the mpv configuration directory (usually ``~/.config/mpv``) with the
    filename ``channels.conf.{sat,ter,cbl,atsc,isdbt}`` (based on your card
    type) or ``channels.conf`` as a last resort.
    Please note that using specific file name with card type is recommended,
    since the legacy channel format is not fully standardized
    so autodetection of the delivery system may fail otherwise.
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

``--dvbin-channel-switch-offset=<integer>``
    This value is not meant for setting via configuration, but used in channel
    switching. An ``input.conf`` can ``cycle`` this value ``up`` and ``down``
    to perform channel switching. This number effectively gives the offset
    to the initially tuned to channel in the channel list.

    An example ``input.conf`` could contain:
    ``H cycle dvbin-channel-switch-offset up``, ``K cycle dvbin-channel-switch-offset down``


GPU renderer options
-----------------------

The following video options are currently all specific to ``--vo=gpu``,
``--vo=libmpv`` and ``--vo=gpu-next``, which are the only VOs that implement
them.

``--scale=<filter>``
    The filter function to use when upscaling video.

    ``bilinear``
        Bilinear hardware texture filtering (fastest, very low quality). This is
        the default when using the ``fast`` profile.

    ``lanczos``
        Lanczos scaling. Provides good balance between quality and performance.
        This is the default for ``scale``. The number of taps can be controlled
        with ``scale-radius``, but is best left unchanged.

        (This filter is an alias for ``sinc``-windowed ``sinc``)

    ``ewa_lanczos``
        Elliptic weighted average Lanczos scaling. Also known as Jinc.
        Relatively slow, but very good quality. The radius can be controlled
        with ``scale-radius``. Increasing the radius makes the filter sharper
        but adds more ringing.

        (This filter is an alias for ``jinc``-windowed ``jinc``)

    ``ewa_lanczossharp``
        A slightly sharpened version of ``ewa_lanczos``. This is the default
        when using the ``high-quality`` profile.

    ``ewa_lanczos4sharpest``
        Very sharp scaler, but also slightly slower than ``ewa_lanczossharp``.
        Prone to ringing, so it's recommended to combine this with an
        anti-ringing shader. On ``--vo=gpu-next``, setting this filter enables
        built-in anti-ringing, so no extra action needs to be taken.

    ``mitchell``
        Mitchell-Netravali. Piecewise cubic filter with a support of radius 2.0.
        Provides a balanced compromise of all scaling artifacts. This filter has
        both ``B`` and ``C`` set to ``1/3``. The ``B`` and ``C`` parameters can
        be controlled with ``--scale-param1`` and ``--scale-param2``.

    ``hermite``
        Hermite spline. Similar to ``bicubic`` but with ``B`` set to ``0.0``.
        This filter has the special property of having a support of radius 1.0,
        making it very fast in comparison, but prone to blocking. This is the
        default for ``--dscale``.

    ``catmull_rom``
        Catmull-Rom spline. Similar to ``mitchell``, but with ``B`` and ``C``
        set to ``0.0`` and ``0.5`` respectively. This filter is sharper than
        ``mitchell``, but prone to ringing.

    ``oversample``
        A version of nearest neighbour that (naively) oversamples pixels, so
        that pixels overlapping edges get linearly interpolated instead of
        rounded. This essentially removes the small imperfections and judder
        artifacts caused by nearest-neighbour interpolation, in exchange for
        adding some blur. This can also be used for frame mixing, where it
        is commonly known as "smoothmotion" (see ``--tscale``).

    ``linear``
        A ``--tscale`` filter.

    There are some more filters, but most are not as useful. For a complete
    list, pass ``help`` as value, e.g.::

        mpv --scale=help

``--cscale=<filter>``
    As ``--scale``, but for interpolating chroma information. If the image is
    not subsampled, this option is ignored entirely. If this option is unset,
    the filter implied by ``--scale`` will be applied.

``--dscale=<filter>``
    Like ``--scale``, but apply these filters on downscaling instead.

``--tscale=<filter>``
    The filter used for interpolating the temporal axis (frames). This is only
    used if ``--interpolation`` is enabled. The only valid choices for
    ``--tscale`` are separable convolution filters (use ``--tscale=help`` to
    get a list). The default is ``oversample``.

    Common ``--tscale`` choices include ``oversample``, ``linear``,
    ``catmull_rom``, ``mitchell``, ``gaussian``, or ``bicubic``. These are
    listed in increasing order of smoothness/blurriness, with ``bicubic``
    being the smoothest/blurriest and ``oversample`` being the sharpest/least
    smooth.

``--scale-param1=<value>``, ``--scale-param2=<value>``, ``--cscale-param1=<value>``, ``--cscale-param2=<value>``, ``--dscale-param1=<value>``, ``--dscale-param2=<value>``, ``--tscale-param1=<value>``, ``--tscale-param2=<value>``
    Set filter parameters. By default, these are set to the special string
    ``default``, which maps to a scaler-specific default value. Ignored if the
    filter is not tunable. Currently, this affects the following filter
    parameters:

    bicubic
        Spline parameters (``B`` and ``C``). Defaults to B=1 and C=0.

    gaussian
        Scale parameter (``t``). Increasing this makes the result blurrier.
        Defaults to 1.

    oversample
        Minimum distance to an edge before interpolation is used. Setting this
        to 0 will always interpolate edges, whereas setting it to 0.5 will
        never interpolate, thus behaving as if the regular nearest neighbour
        algorithm was used. Defaults to 0.0.

``--scale-blur=<value>``, ``--cscale-blur=<value>``, ``--dscale-blur=<value>``, ``--tscale-blur=<value>``
    Kernel scaling factor (also known as a blur factor). Decreasing this makes
    the result sharper, increasing it makes it blurrier (default 0). If set to
    0, the kernel's preferred blur factor is used. Note that setting this too
    low (eg. 0.5) leads to bad results. It's generally recommended to stick to
    values between 0.8 and 1.2.

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

    On ``--vo=gpu-next``, this also affects polar (EWA) scalers. Certain
    filter aliases may also implicitly enable antiringing, regardless of this
    setting (see ``--scale``).

    .. note::

        When downscaling with separable (orthogonal) filters, setting
        ``--dscale-antiring`` to a value other than 0.0 (default) will reduce
        scaler quality and produce aliasing artifacts. On ``--vo=gpu-next``,
        ``--dscale-antiring`` is disabled for separable (orthogonal) filters.

``--scale-window=<window>``, ``--cscale-window=<window>``, ``--dscale-window=<window>``, ``--tscale-window=<window>``
    (Advanced users only) Choose a custom windowing function for the kernel.
    Defaults to the filter's preferred window if unset. Use
    ``--scale-window=help`` to get a list of supported windowing functions.

``--scale-wparam=<window>``, ``--cscale-wparam=<window>``, ``--cscale-wparam=<window>``, ``--tscale-wparam=<window>``
    (Advanced users only) Configure the parameter for the window function given
    by ``--scale-window`` etc. By default, these are set to the special string
    ``default``, which maps to a window-specific default value. Ignored if the
    window is not tunable. Currently, this affects the following window
    parameters:

    kaiser
        Window parameter (alpha). Defaults to 6.33.
    blackman
        Window parameter (alpha). Defaults to 0.16.
    gaussian
        Scale parameter (t). Increasing this makes the window wider. Defaults
        to 1.

``--scaler-resizes-only``
    Disable the scaler if the video image is not resized. In that case,
    ``bilinear`` is used instead of whatever is set with ``--scale``. Bilinear
    will reproduce the source image perfectly if no scaling is performed.
    Enabled by default. Note that this option never affects ``--cscale``.

``--correct-downscaling``
    When using convolution based filters, extend the filter size when
    downscaling. Increases quality, but reduces performance while downscaling.
    Enabled by default.

    This will perform slightly sub-optimally for anamorphic video (but still
    better than without it) since it will extend the size to match only the
    milder of the scale factors between the axes.

    Note: this option is ignored when using bilinear downscaling with ``--vo=gpu``.

``--linear-downscaling``
    Scale in linear light when downscaling. It should only be used with a
    ``--fbo-format`` that has at least 16 bit precision. This option
    has no effect on HDR content. Enabled by default.

``--linear-upscaling``
    Scale in linear light when upscaling. Like ``--linear-downscaling``, it
    should only be used with a ``--fbo-format`` that has at least 16 bits
    precisions. This is not usually recommended except for testing/specific
    purposes. Users are advised to either enable ``--sigmoid-upscaling`` or
    keep both options disabled (i.e. scaling in gamma light).

``--sigmoid-upscaling``
    When upscaling, use a sigmoidal color transform to avoid emphasizing
    ringing artifacts. Enabled by default. This is incompatible with and replaces
    ``--linear-upscaling``. (Note that sigmoidization also requires
    linearization, so the ``LINEAR`` rendering step fires in both cases)

``--sigmoid-center``
    The center of the sigmoid curve used for ``--sigmoid-upscaling``, must be a
    float between 0.0 and 1.0. Defaults to 0.75 if not specified.

``--sigmoid-slope``
    The slope of the sigmoid curve used for ``--sigmoid-upscaling``, must be a
    float between 1.0 and 20.0. Defaults to 6.5 if not specified.

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
    ``0.01``). This is calculated as ``abs(disphz/vfps - 1) < threshold``,
    where ``vfps`` is the speed-adjusted video FPS, and ``disphz`` the
    display refresh rate. (The speed-adjusted video FPS is roughly equal to
    the normal video FPS, but with slowdown and speedup applied. This matters
    if you use ``--video-sync=display-resample`` to make video run synchronously
    to the display FPS, or if you change the ``speed`` property.)

    The default is intended to enable interpolation in scenarios where
    retiming with the ``--video-sync=display-*`` cannot adjust the speed of
    the video sufficiently for smooth playback. For example if a video is
    60.00 FPS and your display refresh rate is 59.94 Hz, interpolation will
    never be activated, since the mismatch is within 1% of the refresh
    rate. The default also handles the scenario when mpv cannot determine the
    container FPS, such as during certain live streams, and may dynamically
    toggle interpolation on and off. In this scenario, the default would be to
    not use interpolation but rather to allow ``--video-sync=display-*`` to
    retime the video to match display refresh rate. See
    ``--video-sync-max-video-change`` for more information about how mpv
    will retime video.

    Also note that if you use e.g. ``--video-sync=display-vdrop``, small
    deviations in the rate can disable interpolation and introduce a
    discontinuity every other minute.

    Set this to ``-1`` to disable this logic.

``--interpolation-preserve``
    Preserve the previous frames' interpolated results even when renderer
    parameters are changed - with the exception of options related to
    cropping and video placement, which always invalidate the cache. Enabling
    this option makes dynamic updates of renderer settings slightly smoother at
    the cost of slightly higher latency in response to such changes. Defaults
    to on. (Only affects ``--vo=gpu-next``, note that ``--vo=gpu`` always
    invalidates interpolated frames)

``--opengl-pbo``
    Enable use of PBOs. On some drivers this can be faster, especially if the
    source video size is huge (e.g. so called "4K" video). On other drivers it
    might be slower or cause latency issues.

``--dither-depth=<N|no|auto>``
    Set dither target depth to N. Default: auto.

    no
        Disable any dithering done by mpv.
    auto
        Automatic selection.
        On ``--vo=gpu``: detected depth or 8 bpc otherwise
        On ``--vo=gpu-next``: detected depth or 8 bpc (for SDR target)
    8
        Dither to 8 bit output.

    Note that the on-the-wire bit depth cannot be detected except when using
    ``gpu-api=d3d11``. Explicitly setting the value to your display's bit depth
    is recommended, as dithering performed by some LCD panels can be of low
    quality.

``--dither-size-fruit=<2-8>``
    Set the size of the dither matrix (default: 6). The actual size of the
    matrix is ``(2^N) x (2^N)`` for an option value of ``N``, so a value of 6
    gives a size of 64x64. The matrix is generated at startup time, and a large
    matrix can take rather long to compute (seconds).

    Used in ``--dither=fruit`` mode only.

``--dither=<fruit|ordered|error-diffusion|no>``
    Select dithering algorithm (default: fruit). (Normally, the
    ``--dither-depth`` option controls whether dithering is enabled.)

    The ``error-diffusion`` option requires compute shader support. It also
    requires large amount of shared memory to run, the size of which depends on
    both the kernel (see ``--error-diffusion`` option below) and the height of
    video window. It will fallback to ``fruit`` dithering if there is no enough
    shared memory to run the shader.

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

``--error-diffusion=<kernel>``
    The error diffusion kernel to use when ``--dither=error-diffusion`` is set.

    ``simple``
        Propagate error to only two adjacent pixels. Fastest but low quality.

    ``sierra-lite``
        Fast with reasonable quality. This is the default.

    ``floyd-steinberg``
        Most notable error diffusion kernel.

    ``atkinson``
        Looks different from other kernels because only fraction of errors will
        be propagated during dithering. A typical use case of this kernel is
        saving dithered screenshot (in window mode). This kernel produces
        slightly smaller file, with still reasonable dithering quality.

    There are other kernels (use ``--error-diffusion=help`` to list) but most of
    them are much slower and demanding even larger amount of shared memory.
    Among these kernels, ``burkes`` achieves a good balance between performance
    and quality, and probably is the one you want to try first.

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
    ``--display-fps-override``).

``--egl-config-id=<ID>``
    (EGL only)
    Select EGLConfig with specific EGL_CONFIG_ID.
    Rendering surfaces and contexts will be created using this EGLConfig.
    You can use ``--msg-level=vo=trace`` to obtain a list of available configs.

``--egl-output-format=<auto|rgb8|rgba8|rgb10|rgb10_a2|rgb16|rgba16|rgb16f|rgba16f|rgb32f|rgba32f>``
    (EGL only)
    Select a specific EGL output format to utilize for OpenGL rendering.
    This option is mutually exclusive with ``--egl-config-id``.
    "auto" is the default, which will pick the first usable config
    based on the order given by the driver.

    All formats are not available.
    A fatal error is caused if an unavailable format is selected.

    .. note::

        There is no reliable API to query desktop bit depth in EGL.
        You can manually set this option
        according to the bit depth of your display.
        This option also affects the auto-detection of ``--dither-depth``.

    .. note::

        Unlike  ``--d3d11-output-format``, this option also takes effect with ``--vo=gpu-next``.

``--vulkan-device=<device name|UUID>``
    The name or UUID of the Vulkan device to use for rendering and presentation. Use
    ``--vulkan-device=help`` to see the list of available devices and their
    names and UUIDs. If left unspecified, the first enumerated hardware Vulkan
    device will be used.

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
    use of compute shaders over fragment shaders wherever possible. Enabled by
    default, although Nvidia users may want to disable it.

``--vulkan-display-display=<n>``
    The index of the display, on the selected Vulkan device, to present on when
    using the ``displayvk`` GPU context. Use ``--vulkan-display-display=help``
    to see the list of available displays. If left unspecified, the first
    enumerated display will be used.


``--vulkan-display-mode=<n>``
    The index of the display mode, of the selected Vulkan display, to use when
    using the ``displayvk`` GPU context. Use ``--vulkan-display-mode=help``
    to see the list of available modes. If left unspecified, the first
    enumerated mode will be used.

``--vulkan-display-plane=<n>``
    The index of the plane, on the selected Vulkan device, to present on when
    using the ``displayvk`` GPU context. Use ``--vulkan-display-plane=help``
    to see the list of available planes. If left unspecified, the first
    enumerated plane will be used.

``--d3d11-exclusive-fs=<yes|no>``
    Switches the D3D11 swap chain fullscreen state to 'fullscreen' when
    fullscreen video is requested. Also known as "exclusive fullscreen" or
    "D3D fullscreen" in other applications. Gives mpv full control of
    rendering on the swap chain's screen. Off by default.

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

``--d3d11-adapter=<adapter name|help>``
    Select a specific D3D11 adapter to utilize for D3D11 rendering.
    Will pick the default adapter if unset. Alternatives are listed
    when the name "help" is given.

    Checks for matches based on the start of the string, case
    insensitive. Thus, if the description of the adapter starts with
    the vendor name, that can be utilized as the selection parameter.

    Hardware decoders utilizing the D3D11 rendering abstraction's helper
    functionality to receive a device, such as D3D11VA or DXVA2's DXGI
    mode, will be affected by this choice.

``--d3d11-output-format=<auto|rgba8|bgra8|rgb10_a2|rgba16f>``
    Select a specific D3D11 output format to utilize for D3D11 rendering.
    "auto" is the default, which will pick either rgba8 or rgb10_a2 depending
    on the configured desktop bit depth. rgba16f and bgra8 are left out of
    the autodetection logic, and are available for manual testing.

    .. note::

        Desktop bit depth querying is only available from an API available
        from Windows 10. Thus on older systems it will only automatically
        utilize the rgba8 output format.

    .. note::

        For ``--vo=gpu-next``, this is used as a best-effort hint and
        libplacebo has the last say on which format is utilized.

``--d3d11-output-csp=<auto|srgb|linear|pq|bt.2020>``
    Select a specific D3D11 output color space to utilize for D3D11 rendering.
    "auto" is the default, which will select the color space of the desktop
    on which the swap chain is located.

    Values other than "srgb" and "pq" have had issues in testing, so they
    are mostly available for manual testing.

    .. note::

        Swap chain color space configuration is only available from an API
        available from Windows 10. Thus on older systems it will not work.

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

``--wayland-app-id=<string>``
    Set the client app id for Wayland-based video output methods (default: ``mpv``).

``--wayland-configure-bounds=<auto|yes|no>``
    Controls whether or not mpv opts into the configure bounds event if sent by the
    compositor (default: auto). This restricts the initial size of the mpv window to
    a certain maximum size intended by the compositor. In most cases, this simply
    just prevents the mpv window from being larger than the size of the monitor when
    it first renders. With the default value of ``auto``, configure-bounds will
    silently be ignored if any ``autofit`` or ``geometry`` type option is also set.

``--wayland-content-type=<auto|none|photo|video|game>``
    If supported by the compositor, mpv will send a hint using the content-type
    protocol telling the compositor what type of content is being displayed. ``auto``
    (default) will automatically switch between telling the compositor the content
    is a photo, video or possibly none depending on internal heuristics.

``--wayland-disable-vsync=<yes|no>``
    Disable mpv's internal vsync for Wayland-based video output (default: no).
    This is mainly useful for benchmarking wayland VOs when combined with
    ``video-sync=display-desync``, ``--audio=no``, and ``--untimed=yes``.

``--wayland-edge-pixels-pointer=<value>``
    Defines the size of an edge border (default: 16) to initiate client side
    resize events in the wayland contexts with the mouse. This is only active if
    there are no server side decorations from the compositor.

``--wayland-edge-pixels-touch=<value>``
    Defines the size of an edge border (default: 32) to initiate client side
    resizes events in the wayland contexts with touch events.

``--wayland-present=<yes|no>``
    Enable the use of wayland's presentation time protocol for more accurate
    frame presentation if it is supported by the compositor (default: ``yes``).
    This only has an effect if ``--video-sync=display-...`` is being used.

``--spirv-compiler=<compiler>``
    Controls which compiler is used to translate GLSL to SPIR-V. This is
    only relevant for ``--gpu-api=d3d11`` with ``--vo=gpu``.
    The possible choices are currently:

    auto
        Use the first available compiler. (Default)
    shaderc
        Use libshaderc, which is an API wrapper around glslang. This is
        generally the most preferred, if available.

    .. note::

        This option is deprecated, since there is only one usable value.
        It may be removed in the future.

``--glsl-shader=<file>``, ``--glsl-shaders=<file-list>``
    Custom GLSL hooks. These are a flexible way to add custom fragment shaders,
    which can be injected at almost arbitrary points in the rendering pipeline,
    and access all previous intermediate textures.

    Each use of the ``--glsl-shader`` option will add another file to the
    internal list of shaders, while ``--glsl-shaders`` takes a list of files,
    and overwrites the internal list with it. The latter is a path list option
    (see `List Options`_ for details).

    .. warning::

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

    OFFSET <ox oy | ALIGN>
        Indicates a pixel shift (offset) introduced by this pass. These pixel
        offsets will be accumulated and corrected during the next scaling pass
        (``cscale`` or ``scale``). The default values are 0 0 which correspond
        to no shift. Note that offsets are ignored when not overwriting the
        hooked texture.

        A special value of ``ALIGN`` will attempt to fix existing offset of
        HOOKED by align it with reference. It requires HOOKED to be resizable
        (see below). It works transparently with fragment shader. For compute
        shader, the predefined ``texmap`` macro is required to handle coordinate
        mapping.

    COMPONENTS <n>
        Specifies how many components of this pass's output are relevant and
        should be stored in the texture, up to 4 (rgba). By default, this value
        is equal to the number of components in HOOKED.

    COMPUTE <bw> <bh> [<tw> <th>]
        Specifies that this shader should be treated as a compute shader, with
        the block size bw and bh. The compute shader will be dispatched with
        however many blocks are necessary to completely tile over the output.
        Within each block, there will be tw*th threads, forming a single work
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
        ``--linear-upscaling``, ``--linear-downscaling`` or
        ``--sigmoid-upscaling`` is in effect.

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

``--glsl-shader-opts=param1=value1,param2=value2,...``
    Specifies the options to use for tunable shader parameters. You can target
    specific named shaders by prefixing the shader name with a ``/``, e.g.
    ``shader/param=value``. Without a prefix, parameters affect all shaders.
    The shader name is the base part of the shader filename, without the
    extension. (``--vo=gpu-next`` only)

``--deband``
    Enable the debanding algorithm. This greatly reduces the amount of visible
    banding, blocking and other quantization artifacts, at the expense of
    very slightly blurring some of the finest details. In practice, it's
    virtually always an improvement - the only reason to disable it would be
    for performance.

``--deband-iterations=<0..16>``
    The number of debanding steps to perform per sample. Each step reduces a
    bit more banding, but takes time to compute. Note that the strength of each
    step falls off very quickly, so high numbers (>4) are practically useless.
    (Default 1)

``--deband-threshold=<0..4096>``
    The debanding filter's cut-off threshold. Higher numbers increase the
    debanding strength dramatically but progressively diminish image details.
    (Default 48)

``--deband-range=<1..64>``
    The debanding filter's initial radius. The radius increases linearly for
    each iteration. A higher radius will find more gradients, but a lower
    radius will smooth more aggressively. (Default 16)

    If you increase the ``--deband-iterations``, you should probably decrease
    this to compensate.

``--deband-grain=<0..4096>``
    Add some extra noise to the image. This significantly helps cover up
    remaining quantization artifacts. Higher numbers add more noise. (Default
    32)

``--corner-rounding=<0..1>``
    If set to a value above 0.0, the output will be rendered with rounded
    corners, as if an alpha transparency mask had been applied. The value
    indicates the relative fraction of the side length to round - a value of
    1.0 rounds the corners as much as possible. (``--vo=gpu-next`` only)

``--sharpen=<value>``
    If set to a value other than 0, enable an unsharp masking filter. Positive
    values will sharpen the image (but add more ringing and aliasing). Negative
    values will blur the image. If your GPU is powerful enough, consider
    alternatives like the ``ewa_lanczossharp`` scale filter, or the
    ``--scale-blur`` option. (Only for ``--vo=gpu``)

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
    (Windows only)
    Calls ``DwmFlush`` after swapping buffers on Windows (default: auto). It
    also sets ``SwapInterval(0)`` to ignore the OpenGL timing. Values are: no
    (disabled), windowed (only in windowed mode), yes (also in full screen).

    The value ``auto`` will try to determine whether the compositor is active,
    and calls ``DwmFlush`` only if it seems to be.

    This may help to get more consistent frame intervals, especially with
    high-fps clips - which might also reduce dropped frames. Typically, a value
    of ``windowed`` should be enough, since full screen may bypass the DWM.

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

    If set to ``yes``, the ``--angle-flip`` option will have no effect.

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

``--macos-force-dedicated-gpu=<yes|no>``
    Deactivates the automatic graphics switching and forces the dedicated GPU.
    (default: no)

    macOS only.

``--cocoa-cb-sw-renderer=<yes|no|auto>``
    Use the Apple Software Renderer when using cocoa-cb (default: auto). If set
    to ``no`` the software renderer is never used and instead fails when a the
    usual pixel format could not be created, ``yes`` will always only use the
    software renderer, and ``auto`` only falls back to the software renderer
    when the usual pixel format couldn't be created.

    macOS and cocoa-cb only.

``--cocoa-cb-10bit-context=<yes|no>``
    Creates a 10bit capable pixel format for the context creation (default: yes).
    Instead of 8bit integer framebuffer a 16bit half-float framebuffer is
    requested.

    macOS and cocoa-cb only.

``--cocoa-cb-output-csp=<csp>``
    This sets the color space of the layer to activate the macOS color
    transformation. Depending on the color space used the system's EDR (HDR)
    support will be activated. To get correct results, this needs to be set to
    the color primaries/transfer characteristics of the VO target. It is recommended
    to use this switch together with ``--target-trc`` and ``--target-prim``.

    ``<csp>`` can be one of the following:

    :auto:               Sets the color space to the icc profile of the
                         screen (default).
    :display-p3:         DCI P3 primaries, a D65 white point and the sRGB
                         transfer function.
    :display-p3-hlg:     DCI P3 primaries, a D65 white point and the Hybrid
                         Log-Gamma (HLG) transfer function.
    :display-p3-pq:      DCI P3 primaries, a D65 white point and the Perceptual
                         Quantizer (PQ) transfer function.
    :display-p3-linear:  DCI P3 primaries, a D65 white point and linear transfer function.
    :dci-p3:             DCI P3 color space.
    :bt.2020:            ITU BT.2020 color space.
    :bt.2020-linear:     ITU BT.2020 color space and linear transfer function.
    :bt.2100-hlg:        ITU BT.2100 and the Hybrid Log-Gamma (HLG) transfer function.
    :bt.2100-pq:         ITU BT.2100 and the Perceptual Quantizer (PQ) transfer function.
    :bt.709:             ITU BT.709 color space.
    :srgb:               sRGB colorimetry and non-linear transfer function.
    :srgb-linear:        Same as sRGB but linear transfer function.
    :rgb-linear:         RGB and linear transfer function.
    :adobe:              Adobe RGB (1998) color space.

    macOS and cocoa-cb only.

``--macos-title-bar-appearance=<appearance>``
    Sets the appearance of the title bar (default: auto). Not all combinations
    of appearances and ``--macos-title-bar-material`` materials make sense or
    are unique. Appearances that are not supported by you current macOS version
    fall back to the default value.
    macOS only

    ``<appearance>`` can be one of the following:

    :auto:                     Detects the system settings and sets the title
                               bar appearance appropriately. On macOS 10.14 it
                               also detects run time changes.
    :aqua:                     The standard macOS Light appearance.
    :darkAqua:                 The standard macOS Dark appearance. (macOS 10.14+)
    :vibrantLight:             Light vibrancy appearance with.
    :vibrantDark:              Dark vibrancy appearance with.
    :aquaHighContrast:         Light Accessibility appearance. (macOS 10.14+)
    :darkAquaHighContrast:     Dark Accessibility appearance. (macOS 10.14+)
    :vibrantLightHighContrast: Light vibrancy Accessibility appearance.
                               (macOS 10.14+)
    :vibrantDarkHighContrast:  Dark vibrancy Accessibility appearance.
                               (macOS 10.14+)

``--macos-title-bar-material=<material>``
    Sets the material of the title bar (default: titlebar). All deprecated
    materials should not be used on macOS 10.14+ because their functionality
    is not guaranteed. Not all combinations of materials and
    ``--macos-title-bar-appearance`` appearances make sense or are unique.
    Materials that are not supported by you current macOS version fall back to
    the default value.
    macOS only

    ``<material>`` can be one of the following:

    :titlebar:              The standard macOS title bar material.
    :selection:             The standard macOS selection material.
    :menu:                  The standard macOS menu material. (macOS 10.11+)
    :popover:               The standard macOS popover material. (macOS 10.11+)
    :sidebar:               The standard macOS sidebar material. (macOS 10.11+)
    :headerView:            The standard macOS header view material.
                            (macOS 10.14+)
    :sheet:                 The standard macOS sheet material. (macOS 10.14+)
    :windowBackground:      The standard macOS window background material.
                            (macOS 10.14+)
    :hudWindow:             The standard macOS hudWindow material. (macOS 10.14+)
    :fullScreen:            The standard macOS full screen material.
                            (macOS 10.14+)
    :toolTip:               The standard macOS tool tip material. (macOS 10.14+)
    :contentBackground:     The standard macOS content background material.
                            (macOS 10.14+)
    :underWindowBackground: The standard macOS under window background material.
                            (macOS 10.14+)
    :underPageBackground:   The standard macOS under page background material.
                            (deprecated in macOS 10.14+)
    :dark:                  The standard macOS dark material.
                            (deprecated in macOS 10.14+)
    :light:                 The standard macOS light material.
                            (macOS 10.14+)
    :mediumLight:           The standard macOS mediumLight material.
                            (macOS 10.11+, deprecated in macOS 10.14+)
    :ultraDark:             The standard macOS ultraDark material.
                            (macOS 10.11+ deprecated in macOS 10.14+)

``--macos-title-bar-color=<color>``
    Sets the color of the title bar (default: completely transparent). Is
    influenced by ``--macos-title-bar-appearance`` and
    ``--macos-title-bar-material``.
    See ``--sub-color`` for color syntax.

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
    (macOS)


``--macos-app-activation-policy=<regular|accessory|prohibited>``
    Changes the App activation policy. With accessory the mpv icon in the Dock
    can be hidden. (default: regular)

    macOS only.

``--macos-geometry-calculation=<visible|whole>``
    This changes the rectangle which is used to calculate the screen position
    and size of the window (default: visible). ``visible`` takes the the menu
    bar and Dock into account and the window is only positioned/sized within the
    visible screen frame rectangle, ``whole`` takes the whole screen frame
    rectangle and ignores the menu bar and Dock. Other previous restrictions
    still apply, like the window can't be placed on top of the menu bar etc.

    macOS only.

``--macos-render-timer=<timer>``
    Sets the mode (default: callback) for syncing the rendering of frames to the display's
    vertical refresh rate.
    macOS and Vulkan (macvk) only.

    ``<timer>`` can be one of the following:

    :callback: Syncs to the CVDisplayLink callback
    :precise:  Syncs to the time of the next vertical display refresh reported by the
               CVDisplayLink callback provided information
    :system:   No manual syncing, depend on the layer mechanic and the next drawable
    :feedback: Same as precise but uses the presentation feedback core mechanism

``--macos-menu-shortcuts=<yes|no>``
    Enables the default menu bar shortcuts (default: yes). The menu bar shortcuts always take
    precedence over any other shortcuts, they are not propagated to the mpv core and they can't be
    used in config files like ``input.conf`` or script bindings.

``--android-surface-size=<WxH>``
    Set dimensions of the rendering surface used by the Android gpu context.
    Needs to be set by the embedding application if the dimensions change during
    runtime (i.e. if the device is rotated), via the surfaceChanged callback.

    Android with ``--gpu-context=android`` only.

``--gpu-sw``
    Continue even if a software renderer is detected.

``--gpu-context=<context1,context2,...[,]>``
    Specify a priority list of the GPU contexts to be used.
    The value ``auto`` (the default) selects the GPU context with the default autoprobe
    order. You can also pass ``help`` to get a complete list of compiled in backends
    (sorted by the default autoprobe order).

    Note that the default GPU context is subject to change, and must not be relied upon.
    If a certain GPU context needs to be used, it must be explicitly specified.

    auto
        auto-select (default). Note that this context must be used alone and
        does not participate in the priority list.
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
        X11/GLX (deprecated/legacy, EGL is preferred these days)
    x11vk
        VK_KHR_xlib_surface
    wayland
        Wayland/EGL
    waylandvk
        VK_KHR_wayland_surface
    drm
        DRM/EGL
    displayvk
        VK_KHR_display. This backend is roughly the Vulkan equivalent of
        DRM/EGL, allowing for direct rendering via Vulkan without a display
        manager.
    x11egl
        X11/EGL
    android
        Android/EGL. Requires ``--wid`` be set to an ``android.view.Surface``.
    macvk
        Vulkan on macOS with a metal surface through a translation layer (experimental)

``--gpu-api=<type1,type2,...[,]>``
    Specify a priority list of accepted graphics APIs.

    auto
        Use any available API (default). Note that the default GPU API used for this
        value is subject to change, and must not be relied upon. If a certain GPU API
        needs to be used, it must be explicitly specified.
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

    This option is deprecated and may be removed in the future.

``--gamma-auto``
    Automatically corrects the gamma value depending on ambient lighting
    conditions (adding a gamma boost for bright rooms).

    This option is deprecated and may be removed in the future.

    NOTE: Only implemented on macOS.

``--image-lut=<file>``
    Specifies a custom LUT file (in Adobe .cube format) to apply to the colors
    during image decoding. The exact interpretation of the LUT depends on
    the value of ``--image-lut-type``. (Only for ``--vo=gpu-next``)

``--image-lut-type=<value>``
    Controls the interpretation of color values fed to and from the LUT
    specified as ``--image-lut``. Valid values are:

    auto
        Chooses the interpretation of the LUT automatically from tagged
        metadata, and otherwise falls back to ``native``. (Default)
    native
        Applied to the raw image contents in its native colorspace, before
        decoding to RGB. For example, for a HDR10 image, this would be fed
        PQ-encoded YCbCr values in the range 0.0 - 1.0.
    normalized
        Applied to the normalized RGB image contents, after decoding from
        its native color encoding, but before linearization.
    conversion
        Fully replaces the color decoding. A LUT of this type should ingest the
        image's native colorspace and output normalized non-linear RGB.

``--target-colorspace-hint``
    Automatically configure the output colorspace of the display to pass
    through the input values of the stream (e.g. for HDR passthrough), if
    possible. Requires a supporting driver and ``--vo=gpu-next``.

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
    gamma2.0
        Pure power curve (gamma 2.0)
    gamma2.2
        Pure power curve (gamma 2.2)
    gamma2.4
        Pure power curve (gamma 2.4)
    gamma2.6
        Pure power curve (gamma 2.6)
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

``--target-peak=<auto|nits>``
    Specifies the measured peak brightness of the output display, in cd/m^2
    (AKA nits). The interpretation of this brightness depends on the configured
    ``--target-trc``. In all cases, it imposes a limit on the signal values
    that will be sent to the display. If the source exceeds this brightness
    level, a tone mapping filter will be inserted. For HLG, it has the
    additional effect of parametrizing the inverse OOTF, in order to get
    colorimetrically consistent results with the mastering display. For SDR, or
    when using an ICC (profile (``--icc-profile``), setting this to a value
    above 203 essentially causes the display to be treated as if it were an HDR
    display in disguise. (See the note below)

    In ``auto`` mode (the default), the chosen peak is an appropriate value
    based on the TRC in use. For SDR curves, it uses 203. For HDR curves, it
    uses 203 * the transfer function's nominal peak.

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

``--target-contrast=<auto|10-1000000|inf>``
    Specifies the measured contrast of the output display. ``--target-contrast``
    in conjunction with ``--target-peak`` value is used to calculate display
    black point. Used in black point compensation during HDR tone-mapping.
    ``auto`` is the default and assumes 1000:1 contrast as a typical SDR display
    would have or an infinite contrast when HDR ``--target-trc`` is used.
    ``inf`` contrast specifies display with perfect black level, in practice OLED.
    (Only for ``--vo=gpu-next``)

``--target-gamut=<value>``
    Constrains the gamut of the display. You can use this option to output e.g.
    DCIP3-in-BT.2020. Set ``--target-prim`` to the primaries of the containing
    colorspace (into which values will be encoded), and ``--target-gamut`` to
    the gamut you want to limit colors to. Takes the same values as
    ``--target-prim``. (Only for ``--vo=gpu-next``)

``--target-lut=<file>``
    Specifies a custom LUT file (in Adobe .cube format) to apply to the colors
    before display on-screen. This LUT is fed values in normalized RGB, after
    encoding into the target colorspace, so after the application of
    ``--target-trc``. (Only for ``--vo=gpu-next``)

``--tone-mapping=<value>``
    Specifies the algorithm used for tone-mapping images onto the target
    display. This is relevant for both HDR->SDR conversion as well as gamut
    reduction (e.g. playing back BT.2020 content on a standard gamut display).
    Valid values are:

    auto
        Choose the best curve according to internal heuristics. (Default)
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
    bt.2390
        Perceptual tone mapping curve (EETF) specified in ITU-R Report BT.2390.
    gamma
        Fits a logarithmic transfer between the tone curves.
    linear
        Linearly stretches the entire reference gamut to (a linear multiple of)
        the display.
    spline
        Perceptually linear single-pivot polynomial. (``--vo=gpu-next`` only)
    bt.2446a
        HDR<->SDR mapping specified in ITU-R Report BT.2446, method A. This is
        the recommended curve for well-mastered content. (``--vo=gpu-next``
        only)
    st2094-40
        Dynamic HDR10+ tone-mapping method specified in SMPTE ST2094-40 Annex
        B. In the absence of metadata, falls back to a fixed spline matched to
        the input/output average brightness characteristics. (``--vo=gpu-next``
        only)
    st2094-10
        Dynamic tone-mapping method specified in SMPTE ST2094-10 Annex B.2.
        Conceptually simpler than ST2094-40, and generally produces worse
        results.

``--tone-mapping-param=<value>``
    Set tone mapping parameters. By default, this is set to the special string
    ``default``, which maps to an algorithm-specific default value. Ignored if
    the tone mapping algorithm is not tunable. This affects the following tone
    mapping algorithms:

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
    bt.2390
        Specifies the offset for the knee point. Defaults to 1.0, which is
        higher than the value from the original ITU-R specification (0.5).
        (``--vo=gpu-next`` only)
    gamma
        Specifies the exponent of the function. Defaults to 1.8.
    linear
        Specifies the scale factor to use while stretching. Defaults to 1.0.
    spline
        Specifies the knee point (in PQ space). Defaults to 0.30.
    st2094-10
        Specifies the contrast (slope) at the knee point. Defaults to 1.0.

``--inverse-tone-mapping``
    If set, allows inverse tone mapping (expanding SDR to HDR). Not supported
    by all tone mapping curves. Use with caution. (``--vo=gpu-next`` only)

``--tone-mapping-max-boost=<1.0..10.0>``
    Upper limit for how much the tone mapping algorithm is allowed to boost
    the average brightness by over-exposing the image. The default value of 1.0
    allows no additional brightness boost. A value of 2.0 would allow
    over-exposing by a factor of 2, and so on. Raising this setting can help
    reveal details that would otherwise be hidden in dark scenes, but raising
    it too high will make dark scenes appear unnaturally bright. (``--vo=gpu``
    only)

``--tone-mapping-visualize``
    Display a (PQ-PQ) graph of the active tone-mapping LUT. Intended only for
    debugging purposes. The X axis shows PQ input values, the Y axis shows PQ
    output values. The tone-mapping curve is shown in green/yellow. Yellow
    means the brightness has been boosted from the source, dark blue regions
    show where the brightness has been reduced. The extra colored regions and
    lines indicate various monitor limits, as well a reference diagonal
    (neutral tone-mapping) and source scene average brightness information (if
    available). (``--vo=gpu-next`` only)

``--gamut-mapping-mode``
    Specifies the algorithm used for reducing the gamut of images for the
    target display, after any tone mapping is done.

    auto
        Choose the best mode automatically. (Default)
    clip
        Hard-clip to the gamut (per-channel). Very low quality, but free.
    perceptual
        Performs a perceptually balanced gamut mapping using a soft knee
        function to roll-off clipped regions, and a hue shifting function to
        preserve saturation. (``--vo=gpu-next`` only)
    relative
        Performs relative colorimetric clipping, while maintaining an
        exponential relationship between brightness and chromaticity.
        (``--vo=gpu-next`` only)
    saturation
        Performs simple RGB->RGB saturation mapping. The input R/G/B channels
        are mapped directly onto the output R/G/B channels. Will never clip,
        but will distort all hues and/or result in a faded look.
        (``--vo=gpu-next`` only)
    absolute
        Performs absolute colorimetric clipping. Like ``relative``, but does
        not adapt the white point. (``--vo=gpu-next`` only)
    desaturate
        Performs constant-luminance colorimetric clipping, desaturing colors
        towards white until they're in-range.
    darken
        Uniformly darkens the input slightly to prevent clipping on blown-out
        highlights, then clamps colorimetrically to the input gamut boundary,
        biased slightly to preserve chromaticity over luminance.
        (``--vo=gpu-next`` only)
    warn
        Performs no gamut mapping, but simply highlights out-of-gamut pixels.
    linear
        Linearly/uniformly desaturates the image in order to bring the entire
        image into the target gamut. (``--vo=gpu-next`` only)

``--hdr-compute-peak=<auto|yes|no>``
    Compute the HDR peak and frame average brightness per-frame instead of
    relying on tagged metadata. These values are averaged over local regions as
    well as over several frames to prevent the value from jittering around too
    much. This option basically gives you dynamic, per-scene tone mapping.
    Requires compute shaders, which is a fairly recent OpenGL feature, and will
    probably also perform horribly on some drivers, so enable at your own risk.
    The special value ``auto`` (default) will enable HDR peak computation
    automatically if compute shaders and SSBOs are supported.

``--allow-delayed-peak-detect``
    When using ``--hdr-compute-peak``, allow delaying the detected peak by a
    frame when beneficial for performance. In particular, this is required to
    avoid an unnecessary FBO indirection when no advanced rendering is required
    otherwise. Has no effect if there already is an indirect pass, such as when
    advanced scaling is enabled. Defaults to no. (Only affects
    ``--vo=gpu-next``, note that ``--vo=gpu`` always delays the peak.)

``--hdr-peak-percentile=<0.0..100.0>``
    Which percentile of the input image brightness histogram to consider as the
    true peak of the scene. If this is set to 100 (default), the
    brightest pixel is measured. Otherwise, the top of the frequency
    distribution is progressively cut off. Setting this too low will cause
    clipping of very bright details, but can improve the dynamic brightness
    range of scenes with very bright isolated highlights. Values other than 100
    come with a small performance penalty. (Only for ``--vo=gpu-next``)

``--hdr-peak-decay-rate=<0.0..1000.0>``
    The decay rate used for the HDR peak detection algorithm (default: 20.0).
    This is only relevant when ``--hdr-compute-peak`` is enabled. Higher values
    make the peak decay more slowly, leading to more stable values at the cost
    of more "eye adaptation"-like effects (although this is mitigated somewhat
    by ``--hdr-scene-threshold``). A value of 0.0 (the lowest possible) disables
    all averaging, meaning each frame's value is used directly as measured,
    but doing this is not recommended for "noisy" sources since it may lead
    to excessive flicker. (In signal theory terms, this controls the time
    constant "tau" of an IIR low pass filter)

``--hdr-scene-threshold-low=<0.0..100.0>``, ``--hdr-scene-threshold-high=<0.0..100.0>``
    The lower and upper thresholds (in dB) for a brightness difference
    to be considered a scene change (default: 1.0 low, 3.0 high). This is only
    relevant when ``--hdr-compute-peak`` is enabled. Normally, small
    fluctuations in the frame brightness are compensated for by the peak
    averaging mechanism, but for large jumps in the brightness this can result
    in the frame remaining too bright or too dark for up to several seconds,
    depending on the value of ``--hdr-peak-decay-rate``. To counteract this,
    when the brightness between the running average and the current frame
    exceeds the low threshold, mpv will make the averaging filter more
    aggressive, up to the limit of the high threshold (at which point the
    filter becomes instant).

``--hdr-contrast-recovery=<0.0..2.0>``, ``--hdr-contrast-smoothness=<1.0..100.0>``
    Enables the HDR contrast recovery algorithm, which is to designed to
    enhance contrast of HDR video after tone mapping. The strength (default:
    0.0) indicates the degree of contrast recovery, with 0.0 being completely
    disabled and 1.0 being 100% strength. Values higher than 1.0 are allowed,
    but may result in excessive sharpening. The smoothness (default: 3.5)
    indicates the degree to which the HDR source is low-passed in order to
    obtain contrast information - a value of 2.0 corresponds to 2x downscaling.
    Users on low DPI displays (<= 100) may want to lower this value, while
    users on very high DPI displays ("retina") may want to increase it. (Only
    for ``vo=gpu-next``)

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

``--icc-cache``
    Store and load 3DLUTs created from the ICC profile on disk in the
    cache directory (Default: ``yes``). This can be used to speed up loading,
    since LittleCMS 2 can take a while to create a 3D LUT. Note that these
    files contain uncompressed LUTs. Their size depends on the
    ``--icc-3dlut-size``, and can be very big.

    On `--vo=gpu-next`, files that have not been accessed in the last 24 hours
    may be cleared if the cache limit (1.5 GiB) is exceeded.

    On ``--vo=gpu``, this is not cleaned automatically, so old, unused cache
    files may stick around indefinitely.

``--icc-cache-dir``
    The directory where icc cache is stored. Cache is stored in the system's
    cache directory (usually ``~/.cache/mpv``) if this is unset.

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

``--icc-3dlut-size=<auto|RxGxB>``
    Size of the 3D LUT generated from the ICC profile in each dimension. The
    default of ``auto`` means to pick the size automatically based on the
    profile characteristics. Sizes may range from 2 to 512.

    NOTE: Setting this option to anything other than ``auto`` is **strongly**
    discouraged, except for testing.

``--icc-force-contrast=<no|0-1000000|inf>``
    Override the target device's detected contrast ratio by a specific value.
    This is detected automatically from the profile if possible, but for some
    profiles it might be missing, causing the contrast to be assumed as
    infinite. As a result, video may appear darker than intended. If this is
    the case, setting this option might help. This only affects BT.1886
    content. The default of ``no`` means to use the profile values. The special
    value ``inf`` causes the BT.1886 curve to be treated as a pure power gamma
    2.4 function.

``--icc-use-luma``
    Use ICC profile luminance value. (Only for ``--vo=gpu-next``)

``--lut=<file>``
    Specifies a custom LUT (in Adobe .cube format) to apply to the colors
    as part of color conversion. The exact interpretation depends on the value
    of ``--lut-type``. (Only for ``--vo=gpu-next``)

``--lut-type=<value>``
    Controls the interpretation of color values fed to and from the LUT
    specified as ``--lut``. Valid values are:

    auto
        Chooses the interpretation of the LUT automatically from tagged
        metadata, and otherwise falls back to ``native``. (Default)
    native
        Applied to raw image contents in its native RGB colorspace (non-linear
        light), before conversion to the output color space.
    normalized
        Applied to the normalized RGB image contents, in linear light, before
        conversion to the output color space.
    conversion
        Fully replaces the conversion from the image color space to the output
        color space. If such a LUT is present, it has the highest priority, and
        overrides any ICC profiles, as well as options related to tone mapping
        and output colorimetry (``--target-prim``, ``--target-trc`` etc.).

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

``--background=<none|color|tiles>``
    If the frame has an alpha component, decide what kind of background, if any,
    to blend it with. This does nothing if there is no alpha component.

    color
        Blend the frame against the background color (``--background-color``,
        normally black).
    tiles
        Blend the frame against a 16x16 gray/white tiles background (default).
    none
        Do not blend the frame and leave the alpha as is.

    Before mpv 0.38.0, this option used to accept a color value specifying the
    background color. This is now done by the ``--background-color`` option.
    Use that instead.

``--background-color=<color>``
    Color used to draw parts of the mpv window not covered by video. See the
    ``--sub-color`` option for how colors are defined.

``--border-background=<none|color|tiles>``
    Same as ``--background`` but only applies to the black bar/border area of
    the window. ``vo=gpu-next`` only. Defaults to ``color``.

``--opengl-rectangle-textures``
    Force use of rectangle textures (default: no). Normally this shouldn't have
    any advantages over normal textures. Note that hardware decoding overrides
    this flag. Could be removed any time.

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

    On macOS this is always deactivated because it only causes performance
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

``--gpu-shader-cache``
    Store and load compiled GLSL shaders in the cache directory (Default:
    ``yes``). Normally, shader compilation is very fast, so this is not usually
    needed. It mostly matters for anything involving GLSL to SPIR-V conversion,
    that is: D3D11, ANGLE or Vulkan, as well as on some other proprietary
    drivers. Enabling this can improve startup performance on these platforms.

    On `--vo=gpu-next`, files that have not been accessed in the last 24 hours
    may be cleared if the cache limit (128 MiB) is exceeded.

    On ``--vo=gpu``, this is not cleaned automatically, so old, unused cache
    files may stick around indefinitely.

``--gpu-shader-cache-dir``
    The directory where gpu shader cache is stored. Cache is stored in the system's
    cache directory (usually ``~/.cache/mpv``) if this is unset.

``--libplacebo-opts=<key>=<value>[,<key>=<value>[,...]]``
    Passes extra raw option to the libplacebo rendering backend (used by
    ``--vo=gpu-next``). May override the effects of any other options set using
    the normal options system. Requires libplacebo v6.309 or higher. Included
    for debugging purposes only. For more information, see:

    https://libplacebo.org/options/

Miscellaneous
-------------

``--display-tags=tag1,tags2,...``
    Set the list of tags that should be displayed on the terminal and stats.
    Tags that are in the list, but are not present in the played file, will not
    be shown. If a value ends with ``*``, all tags are matched by prefix (though
    there is no general globbing). Just passing ``*`` essentially filtering.

    The default includes a common list of tags, call mpv with ``--list-options``
    to see it.

    This is a string list option. See `List Options`_ for details.

``--mc=<seconds/frame>``
    Maximum A-V sync correction per frame (in seconds)

``--autosync=<factor>``
    Gradually adjusts the A/V sync based on audio delay measurements.
    Specifying ``--autosync=0``, the default, will cause frame timing to be
    based entirely on audio delay measurements. Specifying ``--autosync=1``
    will do the same, but will subtly change the A/V correction algorithm. An
    uneven video framerate in a video which plays fine with ``--audio=no`` can
    often be helped by setting this to an integer value greater than 1. The
    higher the value, the closer the timing will be to ``--audio=no``. Try
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

    This option is interesting for client API users using the render API
    because you can stop it from limiting your FPS
    (see ``mpv_render_context_render()`` documentation).

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
    with some container formats (but not e.g. mkv).

    Under some circumstances, the player automatically reverts to ``audio`` mode
    for some time or permanently. This can happen on very low framerate video,
    or if the framerate cannot be detected.

    Also in display-sync modes it can happen that interruptions to video
    playback (such as toggling fullscreen mode, or simply resizing the window)
    will skip the video frames that should have been displayed, while ``audio``
    mode will display them after the renderer has resumed (typically resulting
    in a short A/V desync and the video "catching up").

    Before mpv 0.30.0, there was a fallback to ``audio`` mode on severe A/V
    desync. This was changed for the sake of not sporadically stopping. Now,
    ``display-desync`` does what it promises and may desync with audio by an
    arbitrary amount, until it is manually fixed with a seek.

    These modes also require a vsync blocked presentation mode. For OpenGL, this
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
    :display-tempo:     Same as ``display-resample``, but apply audio speed
                        changes to audio filters instead of resampling to avoid
                        the change in pitch. Beware that some audio filters
                        don't do well with a speed close to 1. It is recommend
                        to use a conditional profile to automatically switch to
                        ``display-resample`` when speed gets too close to 1 for
                        your filter setup. Use (speed * video_speed_correction)
                        to get the actual playback speed in the condition.
                        See `Conditional auto profiles`_ for details.
    :display-vdrop:     Drop or repeat video frames to compensate desyncing
                        video. (Although it should have the same effects as
                        ``audio``, the implementation is very different.)
    :display-adrop:     Drop or repeat audio data to compensate desyncing
                        video. This mode will cause severe audio artifacts if
                        the real monitor refresh rate is too different from
                        the reported or forced rate. Since mpv 0.33.0, this
                        acts on entire audio frames, instead of single samples.
    :display-desync:    Sync video to display, and let audio play on its own.
    :desync:            Sync video according to system clock, and let audio play
                        on its own.

``--video-sync-max-factor=<value>``
    Maximum multiple for which to try to fit the video's FPS to the display's
    FPS (default: 5).

    For example, if this is set to 1, the video FPS is forced to an integer
    multiple of the display FPS, as long as the speed change does not exceed
    the value set by ``--video-sync-max-video-change``.

    See ``--interpolation-threshold`` for how this option affects
    interpolation.

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

    Note that ``--video-sync=display-tempo`` avoids this pitch change.

    Also note that in the ``--video-sync=display-resample`` or
    ``--video-sync=display-tempo`` mode, audio speed will additionally be
    changed by a small amount if necessary for A/V sync. See
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

    This is a key/value list option. See `List Options`_ for details.

``--backdrop-type=<auto|none|mica|acrylic|mica-alt>``
    (Windows only)
    Controls the backdrop/border style.

    :auto: Default Windows behavior
    :none: The backdrop will be black or white depending on the system's theme settings.
    :mica: Enables the Mica style, which is the default on Windows 11.
    :acrylic: Enables the Acrylic style (frosted glass look).
    :mica-alt: Same as Mica, except reversed.

``--window-affinity=<default|excludefromcmcapture|monitor>``
    (Windows only)
    Controls the window affinity behavior of mpv.

    :default: Default Windows behavior
    :excludefromcapture: mpv's window will be completely excluded from capture by external applications or screen recording software.
    :monitor: Blacks out the mpv window

``--vo-mmcss-profile=<name>``
    (Windows only)
    Set the MMCSS profile for the video renderer thread (default: ``Playback``).

``--priority=<prio>``
    (Windows only)
    Set process priority for mpv according to the predefined priorities
    available under Windows.

    Possible values of ``<prio>``:
    idle|belownormal|normal|abovenormal|high|realtime

    .. warning:: Using realtime priority can cause system lockup.

``--media-controls=<yes|player|no>``
    (Windows only)
    Enable integration of media control interface SystemMediaTransportControls.
    If set to ``player``, only the player will use the controls. Setting it to
    ``yes`` will also enable the controls for libmpv integrations.
    (default: ``player``)

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

    This is a path list option. See `List Options`_ for details.

``--external-file=<file>``
    CLI/config file only alias for ``--external-files-append``. Each use of this
    option will add a new external file.

``--cover-art-files=<file-list>``
    Use an external file as cover art while playing audio. This makes it appear
    on the track list and subject to automatic track selection. Options like
    ``--audio-display`` control whether such tracks are supposed to be selected.

    (The difference to loading a file with ``--external-files`` is that video
    tracks will be marked as being pictures, which affects the auto-selection
    method. If the passed file is a video, only the first frame will be decoded
    and displayed. Enabling the cover art track during playback may show a
    random frame if the source file is a video. Normally you're not supposed to
    pass videos to this option, so this paragraph describes the behavior
    coincidentally resulting from implementation details.)

    This is a path list option. See `List Options`_ for details.

``--cover-art-file=<file>``
    CLI/config file only alias for ``--cover-art-files-append``. Each use of this
    option will add a new external file.

``--cover-art-auto=<no|exact|fuzzy|all>``
    Whether to load _external_ cover art automatically. Similar to
    ``--sub-auto`` and ``--audio-file-auto``. If a video already has tracks
    (which are not marked as cover art), external cover art will not be loaded.

    :no:    Don't automatically load cover art.
    :exact: Load the media filename with an image file extension (default).
    :fuzzy: Load all cover art containing the media filename.
    :all:   Load all images in the current directory.

    See ``--cover-art-files`` for details about what constitutes cover art.

    See ``--audio-display`` how to control display of cover art (this can be
    used to disable cover art that is part of the file).

``--image-exts=ext1,ext2,...``
    Image file extentions to try to match when using ``--cover-art-auto``,
    ``--autocreate-playlist`` or ``--directory-filter-types``.

    This is a string list option. See `List Options`_ for details.
    Use ``--help=image-exts`` to see default extensions.

``--cover-art-whitelist=filename1,filename2,...``
    Filenames to load as cover art, sorted by descending priority. They are
    combined with the extensions in ``--image-exts``. This has no
    effect if ``cover-art-auto`` is ``no``.

    Default: ``AlbumArt,Album,cover,front,AlbumArtSmall,Folder,.folder,thumb``

    This is a string list option. See `List Options`_ for details.

``--video-exts=ext1,ext2,...``
    Video file extentions to try to match when using ``--autocreate-playlist`` or
    ``--directory-filter-types``.

    This is a string list option. See `List Options`_ for details.
    Use ``--help=video-exts`` to see default extensions.

``--autoload-files=<yes|no>``
    Automatically load/select external files (default: yes).

    If set to ``no``, then do not automatically load external files as specified
    by ``--sub-auto``, ``--audio-file-auto`` and ``--cover-art-auto``. If
    external files are forcibly added (like with ``--sub-files``), they will
    not be auto-selected.

    This does not affect playlist expansion, redirection, or other loading of
    referenced files like with ordered chapters.

``--stream-record=<file>``
    Write received/read data from the demuxer to the given output file. The
    output file will always be overwritten without asking. The output format
    is determined by the extension of the output file.

    Switching streams or seeking during recording might result in recording
    being stopped and/or broken files. Use with care.

    Seeking outside of the demuxer cache will result in "skips" in the output
    file, but seeking within  the demuxer cache should not affect recording. One
    exception is when you seek back far enough to exceed the forward buffering
    size, in which case the cache stops actively reading. This will return in
    dropped data if it's a live stream.

    If this is set at runtime, the old file is closed, and the new file is
    opened. Note that this will write only data that is appended at the end of
    the cache, and the already cached data cannot be written. You can try the
    ``dump-cache`` command as an alternative.

    External files (``--audio-file`` etc.) are ignored by this, it works on the
    "main" file only. Using this with files using ordered chapters or EDL files
    will also not work correctly in general.

    There are some glitches with this because it uses FFmpeg's libavformat for
    writing the output file. For example, it's typical that it will only work if
    the output format is the same as the input format. This is the case even if
    it works with the ``ffmpeg`` tool. One reason for this is that ``ffmpeg``
    and its libraries contain certain hacks and workarounds for these issues,
    that are unavailable to outside users.

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
        - ``--lavfi-complex='[vid1] [vid2] [vid3] hstack=inputs=3 [vo]'``
          Use the inputs option to stack more than 2 tracks.
        - ``--lavfi-complex='[aid1] asplit [t1] [ao] ; [t1] showvolume [t2] ; [vid1] [t2] overlay [vo]'``
          Play audio track 1, and overlay the measured volume for each speaker
          over video track 1.

    See the FFmpeg libavfilter documentation for details on the available
    filters.

``--metadata-codepage=<codepage>``
    Codepage for various input metadata (default: ``auto``). This affects how
    file tags, chapter titles, etc. are interpreted. In most cases, this merely
    evaluates to UTF-8 as non-UTF-8 codepages are obscure.

    See ``--sub-codepage`` option on how codepages are specified and further
    details regarding autodetection and codepage conversion. (The underlying
    code is the same.)

    Conversion is not applied to metadata that is updated at runtime.
