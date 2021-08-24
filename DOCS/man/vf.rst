VIDEO FILTERS
=============

Video filters allow you to modify the video stream and its properties. All of
the information described in this section applies to audio filters as well
(generally using the prefix ``--af`` instead of ``--vf``).

The exact syntax is:

``--vf=<filter1[=parameter1:parameter2:...],filter2,...>``
    Setup a chain of video filters. This consists on the filter name, and an
    option list of parameters after ``=``. The parameters are separated by
    ``:`` (not ``,``, as that starts a new filter entry).

    Before the filter name, a label can be specified with ``@name:``, where
    name is an arbitrary user-given name, which identifies the filter. This
    is only needed if you want to toggle the filter at runtime.

    A ``!`` before the filter name means the filter is disabled by default. It
    will be skipped on filter creation. This is also useful for runtime filter
    toggling.

    See the ``vf`` command (and ``toggle`` sub-command) for further explanations
    and examples.

    The general filter entry syntax is:

        ``["@"<label-name>":"] ["!"] <filter-name> [ "=" <filter-parameter-list> ]``

    or for the special "toggle" syntax (see ``vf`` command):

        ``"@"<label-name>``

    and the ``filter-parameter-list``:

        ``<filter-parameter> | <filter-parameter> "," <filter-parameter-list>``

    and ``filter-parameter``:

        ``( <param-name> "=" <param-value> ) | <param-value>``

    ``param-value`` can further be quoted in ``[`` / ``]`` in case the value
    contains characters like ``,`` or ``=``. This is used in particular with
    the ``lavfi`` filter, which uses a very similar syntax as mpv (MPlayer
    historically) to specify filters and their parameters.

Filters can be manipulated at run time. You can use ``@`` labels as described
above in combination with the ``vf`` command (see `COMMAND INTERFACE`_) to get
more control over this. Initially disabled filters with ``!`` are useful for
this as well.

You can also set defaults for each filter. The defaults are applied before the
normal filter parameters. This is deprecated and never worked for the
libavfilter bridge.

``--vf-defaults=<filter1[=parameter1:parameter2:...],filter2,...>``
    Set defaults for each filter. (Deprecated. ``--af-defaults`` is deprecated
    as well.)

.. note::

    To get a full list of available video filters, see ``--vf=help`` and
    https://ffmpeg.org/ffmpeg-filters.html .

    Also, keep in mind that most actual filters are available via the ``lavfi``
    wrapper, which gives you access to most of libavfilter's filters. This
    includes all filters that have been ported from MPlayer to libavfilter.

    Most builtin filters are deprecated in some ways, unless they're only available
    in mpv (such as filters which deal with mpv specifics, or which are
    implemented in mpv only).

    If a filter is not builtin, the ``lavfi-bridge`` will be automatically
    tried. This bridge does not support help output, and does not verify
    parameters before the filter is actually used. Although the mpv syntax
    is rather similar to libavfilter's, it's not the same. (Which means not
    everything accepted by vf_lavfi's ``graph`` option will be accepted by
    ``--vf``.)

    You can also prefix the filter name with ``lavfi-`` to force the wrapper.
    This is helpful if the filter name collides with a deprecated mpv builtin
    filter. For example ``--vf=lavfi-scale=args`` would use libavfilter's
    ``scale`` filter over mpv's deprecated builtin one.

Video filters are managed in lists. There are a few commands to manage the
filter list.

``--vf-append=filter``
    Appends the filter given as arguments to the filter list.

``--vf-add=filter``
    Appends the filter given as arguments to the filter list. (Passing multiple
    filters is currently still possible, but deprecated.)

``--vf-pre=filter``
    Prepends the filters given as arguments to the filter list. (Passing
    multiple filters is currently still possible, but deprecated.)

``--vf-remove=filter``
    Deletes the filter from the list. The filter can be either given the way it
    was added (filter name and its full argument list), or by label (prefixed
    with ``@``). Matching of filters works as follows: if either of the compared
    filters has a label set, only the labels are compared. If none of the
    filters have a label, the filter name, arguments, and argument order are
    compared. (Passing multiple filters is currently still possible, but
    deprecated.)

``-vf-toggle=filter``
    Add the given filter to the list if it was not present yet, or remove it
    from the list if it was present. Matching of filters works as described in
    ``--vf-remove``.

``--vf-del=filter``
    Sort of like ``--vf-remove``, but also accepts an index number. Index
    numbers start at 0, negative numbers address the end of the list (-1 is the
    last). Deprecated.

``--vf-clr``
    Completely empties the filter list.

With filters that support it, you can access parameters by their name.

``--vf=<filter>=help``
    Prints the parameter names and parameter value ranges for a particular
    filter.

Available mpv-only filters are:

``format=fmt=<value>:colormatrix=<value>:...``
    Applies video parameter overrides, with optional conversion. By default,
    this overrides the video's parameters without conversion (except for the
    ``fmt`` parameter), but can be made to perform an appropriate conversion
    with ``convert=yes`` for parameters for which conversion is supported.

    ``<fmt>``
        Image format name, e.g. rgb15, bgr24, 420p, etc. (default: don't change).

        This filter always performs conversion to the given format.

        .. note::

            For a list of available formats, use ``--vf=format=fmt=help``.

    ``<convert=yes|no>``
        Force conversion of color parameters (default: no).

        If this is disabled (the default), the only conversion that is possibly
        performed is format conversion if ``<fmt>`` is set. All other parameters
        (like ``<colormatrix>``) are forced without conversion. This mode is
        typically useful when files have been incorrectly tagged.

        If this is enabled, libswscale or zimg is used if any of the parameters
        mismatch. zimg is used of the input/output image formats are supported
        by mpv's zimg wrapper, and if ``--sws-allow-zimg=yes`` is used. Both
        libraries may not support all kinds of conversions. This typically
        results in silent incorrect conversion. zimg has in many cases a better
        chance of performing the conversion correctly.

        In both cases, the color parameters are set on the output stage of the
        image format conversion (if ``fmt`` was set). The difference is that
        with ``convert=no``, the color parameters are not passed on to the
        converter.

        If input and output video parameters are the same, conversion is always
        skipped.

        .. admonition:: Examples

            ``mpv test.mkv --vf=format:colormatrix=ycgco``
                Results in incorrect colors (if test.mkv was tagged correctly).

            ``mpv test.mkv --vf=format:colormatrix=ycgco:convert=yes --sws-allow-zimg``
                Results in true conversion to ``ycgco``, assuming the renderer
                supports it (``--vo=gpu`` normally does). You can add ``--vo=xv``
                to force a VO which definitely does not support it, which should
                show incorrect colors as confirmation.

                Using ``--sws-allow-zimg=no`` (or disabling zimg at build time)
                will use libswscale, which cannot perform this conversion as
                of this writing.

    ``<colormatrix>``
        Controls the YUV to RGB color space conversion when playing video. There
        are various standards. Normally, BT.601 should be used for SD video, and
        BT.709 for HD video. (This is done by default.) Using incorrect color space
        results in slightly under or over saturated and shifted colors.

        These options are not always supported. Different video outputs provide
        varying degrees of support. The ``gpu`` and ``vdpau`` video output
        drivers usually offer full support. The ``xv`` output can set the color
        space if the system video driver supports it, but not input and output
        levels. The ``scale`` video filter can configure color space and input
        levels, but only if the output format is RGB (if the video output driver
        supports RGB output, you can force this with ``-vf scale,format=rgba``).

        If this option is set to ``auto`` (which is the default), the video's
        color space flag will be used. If that flag is unset, the color space
        will be selected automatically. This is done using a simple heuristic that
        attempts to distinguish SD and HD video. If the video is larger than
        1279x576 pixels, BT.709 (HD) will be used; otherwise BT.601 (SD) is
        selected.

        Available color spaces are:

        :auto:          automatic selection (default)
        :bt.601:        ITU-R BT.601 (SD)
        :bt.709:        ITU-R BT.709 (HD)
        :bt.2020-ncl:   ITU-R BT.2020 non-constant luminance system
        :bt.2020-cl:    ITU-R BT.2020 constant luminance system
        :smpte-240m:    SMPTE-240M

    ``<colorlevels>``
        YUV color levels used with YUV to RGB conversion. This option is only
        necessary when playing broken files which do not follow standard color
        levels or which are flagged wrong. If the video does not specify its
        color range, it is assumed to be limited range.

        The same limitations as with ``<colormatrix>`` apply.

        Available color ranges are:

        :auto:      automatic selection (normally limited range) (default)
        :limited:   limited range (16-235 for luma, 16-240 for chroma)
        :full:      full range (0-255 for both luma and chroma)

    ``<primaries>``
        RGB primaries the source file was encoded with. Normally this should be set
        in the file header, but when playing broken or mistagged files this can be
        used to override the setting.

        This option only affects video output drivers that perform color
        management, for example ``gpu`` with the ``target-prim`` or
        ``icc-profile`` suboptions set.

        If this option is set to ``auto`` (which is the default), the video's
        primaries flag will be used. If that flag is unset, the color space will
        be selected automatically, using the following heuristics: If the
        ``<colormatrix>`` is set or determined as BT.2020 or BT.709, the
        corresponding primaries are used. Otherwise, if the video height is
        exactly 576 (PAL), BT.601-625 is used. If it's exactly 480 or 486 (NTSC),
        BT.601-525 is used. If the video resolution is anything else, BT.709 is
        used.

        Available primaries are:

        :auto:         automatic selection (default)
        :bt.601-525:   ITU-R BT.601 (SD) 525-line systems (NTSC, SMPTE-C)
        :bt.601-625:   ITU-R BT.601 (SD) 625-line systems (PAL, SECAM)
        :bt.709:       ITU-R BT.709 (HD) (same primaries as sRGB)
        :bt.2020:      ITU-R BT.2020 (UHD)
        :apple:        Apple RGB
        :adobe:        Adobe RGB (1998)
        :prophoto:     ProPhoto RGB (ROMM)
        :cie1931:      CIE 1931 RGB
        :dci-p3:       DCI-P3 (Digital Cinema)
        :v-gamut:      Panasonic V-Gamut primaries

    ``<gamma>``
       Gamma function the source file was encoded with. Normally this should be set
       in the file header, but when playing broken or mistagged files this can be
       used to override the setting.

       This option only affects video output drivers that perform color management.

       If this option is set to ``auto`` (which is the default), the gamma will
       be set to BT.1886 for YCbCr content, sRGB for RGB content and Linear for
       XYZ content.

       Available gamma functions are:

       :auto:         automatic selection (default)
       :bt.1886:      ITU-R BT.1886 (EOTF corresponding to BT.601/BT.709/BT.2020)
       :srgb:         IEC 61966-2-4 (sRGB)
       :linear:       Linear light
       :gamma1.8:     Pure power curve (gamma 1.8)
       :gamma2.0:     Pure power curve (gamma 2.0)
       :gamma2.2:     Pure power curve (gamma 2.2)
       :gamma2.4:     Pure power curve (gamma 2.4)
       :gamma2.6:     Pure power curve (gamma 2.6)
       :gamma2.8:     Pure power curve (gamma 2.8)
       :prophoto:     ProPhoto RGB (ROMM) curve
       :pq:           ITU-R BT.2100 PQ (Perceptual quantizer) curve
       :hlg:          ITU-R BT.2100 HLG (Hybrid Log-gamma) curve
       :v-log:        Panasonic V-Log transfer curve
       :s-log1:       Sony S-Log1 transfer curve
       :s-log2:       Sony S-Log2 transfer curve

    ``<sig-peak>``
        Reference peak illumination for the video file, relative to the
        signal's reference white level. This is mostly interesting for HDR, but
        it can also be used tone map SDR content to simulate a different
        exposure. Normally inferred from tags such as MaxCLL or mastering
        metadata.

        The default of 0.0 will default to the source's nominal peak luminance.

    ``<light>``
        Light type of the scene. This is mostly correctly inferred based on the
        gamma function, but it can be useful to override this when viewing raw
        camera footage (e.g. V-Log), which is normally scene-referred instead
        of display-referred.

        Available light types are:

       :auto:         Automatic selection (default)
       :display:      Display-referred light (most content)
       :hlg:          Scene-referred using the HLG OOTF (e.g. HLG content)
       :709-1886:     Scene-referred using the BT709+BT1886 interaction
       :gamma1.2:     Scene-referred using a pure power OOTF (gamma=1.2)

    ``<stereo-in>``
        Set the stereo mode the video is assumed to be encoded in. Use
        ``--vf=format:stereo-in=help`` to list all available modes. Check with
        the ``stereo3d`` filter documentation to see what the names mean.

    ``<stereo-out>``
        Set the stereo mode the video should be displayed as. Takes the
        same values as the ``stereo-in`` option.

    ``<rotate>``
        Set the rotation the video is assumed to be encoded with in degrees.
        The special value ``-1`` uses the input format.

    ``<w>``, ``<h>``
        If not 0, perform conversion to the given size. Ignored if
        ``convert=yes`` is not set.

    ``<dw>``, ``<dh>``
        Set the display size. Note that setting the display size such that
        the video is scaled in both directions instead of just changing the
        aspect ratio is an implementation detail, and might change later.

    ``<dar>``
        Set the display aspect ratio of the video frame. This is a float,
        but values such as ``[16:9]`` can be passed too (``[...]`` for quoting
        to prevent the option parser from interpreting the ``:`` character).

    ``<force-scaler=auto|zimg|sws>``
        Force a specific scaler backend, if applicable. This is a debug option
        and could go away any time.

    ``<alpha=auto|straight|premul>``
        Set the kind of alpha the video uses. Undefined effect if the image
        format has no alpha channel (could be ignored or cause an error,
        depending on how mpv internals evolve). Setting this may or may not
        cause downstream image processing to treat alpha differently, depending
        on support. With ``convert`` and zimg used, this will convert the alpha.
        libswscale and other FFmpeg components completely ignore this.

``lavfi=graph[:sws-flags[:o=opts]]``
    Filter video using FFmpeg's libavfilter.

    ``<graph>``
        The libavfilter graph string. The filter must have a single video input
        pad and a single video output pad.

        See `<https://ffmpeg.org/ffmpeg-filters.html>`_ for syntax and available
        filters.

        .. warning::

            If you want to use the full filter syntax with this option, you have
            to quote the filter graph in order to prevent mpv's syntax and the
            filter graph syntax from clashing. To prevent a quoting and escaping
            mess, consider using ``--lavfi-complex`` if you know which video
            track you want to use from the input file. (There is only one video
            track for nearly all video files anyway.)

        .. admonition:: Examples

            ``--vf=lavfi=[gradfun=20:30,vflip]``
                ``gradfun`` filter with nonsense parameters, followed by a
                ``vflip`` filter. (This demonstrates how libavfilter takes a
                graph and not just a single filter.) The filter graph string is
                quoted with ``[`` and ``]``. This requires no additional quoting
                or escaping with some shells (like bash), while others (like
                zsh) require additional ``"`` quotes around the option string.

            ``'--vf=lavfi="gradfun=20:30,vflip"'``
                Same as before, but uses quoting that should be safe with all
                shells. The outer ``'`` quotes make sure that the shell does not
                remove the ``"`` quotes needed by mpv.

            ``'--vf=lavfi=graph="gradfun=radius=30:strength=20,vflip"'``
                Same as before, but uses named parameters for everything.

    ``<sws-flags>``
        If libavfilter inserts filters for pixel format conversion, this
        option gives the flags which should be passed to libswscale. This
        option is numeric and takes a bit-wise combination of ``SWS_`` flags.

        See ``https://git.videolan.org/?p=ffmpeg.git;a=blob;f=libswscale/swscale.h``.

    ``<o>``
        Set AVFilterGraph options. These should be documented by FFmpeg.

        .. admonition:: Example

            ``'--vf=lavfi=yadif:o="threads=2,thread_type=slice"'``
                forces a specific threading configuration.

``sub=[=bottom-margin:top-margin]``
    Moves subtitle rendering to an arbitrary point in the filter chain, or force
    subtitle rendering in the video filter as opposed to using video output OSD
    support.

    ``<bottom-margin>``
        Adds a black band at the bottom of the frame. The SSA/ASS renderer can
        place subtitles there (with ``--sub-use-margins``).
    ``<top-margin>``
        Black band on the top for toptitles  (with ``--sub-use-margins``).

    .. admonition:: Examples

        ``--vf=sub,eq``
            Moves sub rendering before the eq filter. This will put both
            subtitle colors and video under the influence of the video equalizer
            settings.

``vapoursynth=file:buffered-frames:concurrent-frames``
    Loads a VapourSynth filter script. This is intended for streamed
    processing: mpv actually provides a source filter, instead of using a
    native VapourSynth video source. The mpv source will answer frame
    requests only within a small window of frames (the size of this window
    is controlled with the ``buffered-frames`` parameter), and requests outside
    of that will return errors. As such, you can't use the full power of
    VapourSynth, but you can use certain filters.

    .. warning::

        Do not use this filter, unless you have expert knowledge in VapourSynth,
        and know how to fix bugs in the mpv VapourSynth wrapper code.

    If you just want to play video generated by VapourSynth (i.e. using
    a native VapourSynth video source), it's better to use ``vspipe`` and a
    pipe or FIFO to feed the video to mpv. The same applies if the filter script
    requires random frame access (see ``buffered-frames`` parameter).

    ``file``
        Filename of the script source. Currently, this is always a python
        script (``.vpy`` in VapourSynth convention).

        The variable ``video_in`` is set to the mpv video source, and it is
        expected that the script reads video from it. (Otherwise, mpv will
        decode no video, and the video packet queue will overflow, eventually
        leading to only audio playing, or worse.)

        The filter graph created by the script is also expected to pass through
        timestamps using the ``_DurationNum`` and ``_DurationDen`` frame
        properties.

        See the end of the option list for a full list of script variables
        defined by mpv.

        .. admonition:: Example:

            ::

                import vapoursynth as vs
                core = vs.get_core()
                core.std.AddBorders(video_in, 10, 10, 20, 20).set_output()

        .. warning::

            The script will be reloaded on every seek. This is done to reset
            the filter properly on discontinuities.

    ``buffered-frames``
        Maximum number of decoded video frames that should be buffered before
        the filter (default: 4). This specifies the maximum number of frames
        the script can request in backward direction.

        E.g. if ``buffered-frames=5``, and the script just requested frame 15,
        it can still request frame 10, but frame 9 is not available anymore.
        If it requests frame 30, mpv will decode 15 more frames, and keep only
        frames 25-30.

        The only reason why this buffer exists is to serve the random access
        requests the VapourSynth filter can make.

        The VapourSynth API has a ``getFrameAsync`` function, which takes an
        absolute frame number. Source filters must respond to all requests. For
        example, a source filter can request frame 2432, and then frame 3.
        Source filters  typically implement this by pre-indexing the entire
        file.

        mpv on the other hand is stream oriented, and does not allow filters to
        seek. (And it would not make sense to allow it, because it would ruin
        performance.) Filters get frames sequentially in playback direction, and
        cannot request them out of order.

        To compensate for this mismatch, mpv allows the filter to access frames
        within a certain window. ``buffered-frames`` controls the size of this
        window. Most VapourSynth filters happen to work with this, because mpv
        requests frames sequentially increasing from it, and most filters only
        require frames "close" to the requested frame.

        If the filter requests a frame that has a higher frame number than the
        highest buffered frame, new frames will be decoded until the requested
        frame number is reached. Excessive frames will be flushed out in a FIFO
        manner (there are only at most ``buffered-frames`` in this buffer).

        If the filter requests a frame that has a lower frame number than the
        lowest buffered frame, the request cannot be satisfied, and an error
        is returned to the filter. This kind of error is not supposed to happen
        in a "proper" VapourSynth environment. What exactly happens depends on
        the filters involved.

        Increasing this buffer will not improve performance. Rather, it will
        waste memory, and slow down seeks (when enough frames to fill the buffer
        need to be decoded at once). It is only needed to prevent the error
        described in the previous paragraph.

        How many frames a filter requires depends on filter implementation
        details, and mpv has no way of knowing. A scale filter might need only
        1 frame, an interpolation filter may require a small number of frames,
        and the ``Reverse`` filter will require an infinite number of frames.

        If you want reliable operation to the full extend VapourSynth is
        capable, use ``vspipe``.

        The actual number of buffered frames also depends on the value of the
        ``concurrent-frames`` option. Currently, both option values are
        multiplied to get the final buffer size.

    ``concurrent-frames``
        Number of frames that should be requested in parallel. The
        level of concurrency depends on the filter and how quickly mpv can
        decode video to feed the filter. This value should probably be
        proportional to the number of cores on your machine. Most time,
        making it higher than the number of cores can actually make it
        slower.

        Technically, mpv will call the VapourSynth ``getFrameAsync`` function
        in a loop, until there are ``concurrent-frames`` frames that have not
        been returned by the filter yet. This also assumes that the rest of the
        mpv filter chain reads the output of the ``vapoursynth`` filter quickly
        enough. (For example, if you pause the player, filtering will stop very
        soon, because the filtered frames are waiting in a queue.)

        Actual concurrency depends on many other factors.

        By default, this uses the special value ``auto``, which sets the option
        to the number of detected logical CPU cores.

    The following ``.vpy`` script variables are defined by mpv:

    ``video_in``
        The mpv video source as vapoursynth clip. Note that this has an
        incorrect (very high) length set, which confuses many filters. This is
        necessary, because the true number of frames is unknown. You can use the
        ``Trim`` filter on the clip to reduce the length.

    ``video_in_dw``, ``video_in_dh``
        Display size of the video. Can be different from video size if the
        video does not use square pixels (e.g. DVD).

    ``container_fps``
        FPS value as reported by file headers. This value can be wrong or
        completely broken (e.g. 0 or NaN). Even if the value is correct,
        if another filter changes the real FPS (by dropping or inserting
        frames), the value of this variable will not be useful. Note that
        the ``--fps`` command line option overrides this value.

        Useful for some filters which insist on having a FPS.

    ``display_fps``
        Refresh rate of the current display. Note that this value can be 0.

``vavpp``
    VA-API video post processing. Requires the system to support VA-API,
    i.e. Linux/BSD only. Works with ``--vo=vaapi`` and ``--vo=gpu`` only.
    Currently deinterlaces. This filter is automatically inserted if
    deinterlacing is requested (either using the ``d`` key, by default mapped to
    the command ``cycle deinterlace``, or the ``--deinterlace`` option).

    ``deint=<method>``
        Select the deinterlacing algorithm.

        no
            Don't perform deinterlacing.
        auto
             Select the best quality deinterlacing algorithm (default). This
             goes by the order of the options as documented, with
             ``motion-compensated`` being considered best quality.
        first-field
            Show only first field.
        bob
            bob deinterlacing.
        weave, motion-adaptive, motion-compensated
            Advanced deinterlacing algorithms. Whether these actually work
            depends on the GPU hardware, the GPU drivers, driver bugs, and
            mpv bugs.

    ``<interlaced-only>``
        :no:  Deinterlace all frames (default).
        :yes: Only deinterlace frames marked as interlaced.

    ``reversal-bug=<yes|no>``
        :no:  Use the API as it was interpreted by older Mesa drivers. While
              this interpretation was more obvious and intuitive, it was
              apparently wrong, and not shared by Intel driver developers.
        :yes: Use Intel interpretation of surface forward and backwards
              references (default). This is what Intel drivers and newer Mesa
              drivers expect. Matters only for the advanced deinterlacing
              algorithms.

``vdpaupp``
    VDPAU video post processing. Works with ``--vo=vdpau`` and ``--vo=gpu``
    only. This filter is automatically inserted if deinterlacing is requested
    (either using the ``d`` key, by default mapped to the command
    ``cycle deinterlace``, or the ``--deinterlace`` option). When enabling
    deinterlacing, it is always preferred over software deinterlacer filters
    if the ``vdpau`` VO is used, and also if ``gpu`` is used and hardware
    decoding was activated at least once (i.e. vdpau was loaded).

    ``sharpen=<-1-1>``
        For positive values, apply a sharpening algorithm to the video, for
        negative values a blurring algorithm (default: 0).
    ``denoise=<0-1>``
        Apply a noise reduction algorithm to the video (default: 0; no noise
        reduction).
    ``deint=<yes|no>``
        Whether deinterlacing is enabled (default: no). If enabled, it will use
        the mode selected with ``deint-mode``.
    ``deint-mode=<first-field|bob|temporal|temporal-spatial>``
        Select deinterlacing mode (default: temporal).

        Note that there's currently a mechanism that allows the ``vdpau`` VO to
        change the ``deint-mode`` of auto-inserted ``vdpaupp`` filters. To avoid
        confusion, it's recommended not to use the ``--vo=vdpau`` suboptions
        related to filtering.

        first-field
            Show only first field.
        bob
            Bob deinterlacing.
        temporal
            Motion-adaptive temporal deinterlacing. May lead to A/V desync
            with slow video hardware and/or high resolution.
        temporal-spatial
            Motion-adaptive temporal deinterlacing with edge-guided spatial
            interpolation. Needs fast video hardware.
    ``chroma-deint``
        Makes temporal deinterlacers operate both on luma and chroma (default).
        Use no-chroma-deint to solely use luma and speed up advanced
        deinterlacing. Useful with slow video memory.
    ``pullup``
        Try to apply inverse telecine, needs motion adaptive temporal
        deinterlacing.
    ``interlaced-only=<yes|no>``
        If ``yes``, only deinterlace frames marked as interlaced (default: no).
    ``hqscaling=<0-9>``
        0
            Use default VDPAU scaling (default).
        1-9
            Apply high quality VDPAU scaling (needs capable hardware).

``d3d11vpp``
    Direct3D 11 video post processing. Currently requires D3D11 hardware
    decoding for use.

    ``deint=<yes|no>``
        Whether deinterlacing is enabled (default: no).
    ``interlaced-only=<yes|no>``
        If ``yes``, only deinterlace frames marked as interlaced (default: no).
    ``mode=<blend|bob|adaptive|mocomp|ivctc|none>``
        Tries to select a video processor with the given processing capability.
        If a video processor supports multiple capabilities, it is not clear
        which algorithm is actually selected. ``none`` always falls back. On
        most if not all hardware, this option will probably do nothing, because
        a video processor usually supports all modes or none.

``fingerprint=...``
    Compute video frame fingerprints and provide them as metadata. Actually, it
    currently barely deserved to be called ``fingerprint``, because it does not
    compute "proper" fingerprints, only tiny downscaled images (but which can be
    used to compute image hashes or for similarity matching).

    The main purpose of this filter is to support the ``skip-logo.lua`` script.
    If this script is dropped, or mpv ever gains a way to load user-defined
    filters (other than VapourSynth), this filter will be removed. Due to the
    "special" nature of this filter, it will be removed without warning.

    The intended way to read from the filter is using ``vf-metadata`` (also
    see ``clear-on-query`` filter parameter). The property will return a list
    of key/value pairs as follows:

    ::

        fp0.pts = 1.2345
        fp0.hex = 1234abcdef...bcde
        fp1.pts = 1.4567
        fp1.hex = abcdef1234...6789
        ...
        fpN.pts = ...
        fpN.hex = ...
        type = gray-hex-16x16

    Each ``fp<N>`` entry is for a frame. The ``pts`` entry specifies the
    timestamp of the frame (within the filter chain; in simple cases this is
    the same as the display timestamp). The ``hex`` field is the hex encoded
    fingerprint, whose size and meaning depend on the ``type`` filter option.
    The ``type`` field has the same value as the option the filter was created
    with.

    This returns the frames that were filtered since the last query of the
    property. If ``clear-on-query=no`` was set, a query doesn't reset the list
    of frames. In both cases, a maximum of 10 frames is returned. If there are
    more frames, the oldest frames are discarded. Frames are returned in filter
    order.

    (This doesn't return a structured list for the per-frame details because the
    internals of the ``vf-metadata`` mechanism suck. The returned format may
    change in the future.)

    This filter uses zimg for speed and profit. However, it will fallback to
    libswscale in a number of situations: lesser pixel formats, unaligned data
    pointers or strides, or if zimg fails to initialize for unknown reasons. In
    these cases, the filter will use more CPU. Also, it will output different
    fingerprints, because libswscale cannot perform the full range expansion we
    normally request from zimg. As a consequence, the filter may be slower and
    not work correctly in random situations.

    ``type=...``
        What fingerprint to compute. Available types are:

        :gray-hex-8x8:      grayscale, 8 bit, 8x8 size
        :gray-hex-16x16:    grayscale, 8 bit, 16x16 size (default)

        Both types simply remove all colors, downscale the image, concatenate
        all pixel values to a byte array, and convert the array to a hex string.

    ``clear-on-query=yes|no``
        Clear the list of frame fingerprints if the ``vf-metadata`` property for
        this filter is queried (default: yes). This requires some care by the
        user. Some types of accesses might query the filter multiple times,
        which leads to lost frames.

    ``print=yes|no``
        Print computed fingerprints to the terminal (default: no). This is
        mostly for testing and such. Scripts should use ``vf-metadata`` to
        read information from this filter instead.

``gpu=...``
    Convert video to RGB using the OpenGL renderer normally used with
    ``--vo=gpu``. This requires that the EGL implementation supports off-screen
    rendering on the default display. (This is the case with Mesa.)

    Sub-options:

    ``w=<pixels>``, ``h=<pixels>``
        Size of the output in pixels (default: 0). If not positive, this will
        use the size of the first filtered input frame.

    .. warning::

        This is highly experimental. Performance is bad, and it will not work
        everywhere in the first place. Some features are not supported.

    .. warning::

        This does not do OSD rendering. If you see OSD, then it has been
        rendered by the VO backend. (Subtitles are rendered by the ``gpu``
        filter, if possible.)

    .. warning::

        If you use this with encoding mode, keep in mind that encoding mode will
        convert the RGB filter's output back to yuv420p in software, using the
        configured software scaler. Using ``zimg`` might improve this, but in
        any case it might go against your goals when using this filter.

    .. warning::

        Do not use this with ``--vo=gpu``. It will apply filtering twice, since
        most ``--vo=gpu`` options are unconditionally applied to the ``gpu``
        filter. There is no mechanism in mpv to prevent this.

