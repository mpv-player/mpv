VIDEO OUTPUT DRIVERS
====================

Video output drivers are interfaces to different video output facilities. The
syntax is:

``--vo=<driver1[:suboption1[=value]:...],driver2,...[,]>``
    Specify a priority list of video output drivers to be used.

If the list has a trailing ',', mpv will fall back on drivers not contained
in the list. Suboptions are optional and can mostly be omitted.

You can also set defaults for each driver. The defaults are applied before the
normal driver parameters.

``--vo-defaults=<driver1[:parameter1:parameter2:...],driver2,...>``
    Set defaults for each driver.

.. note::

    See ``--vo=help`` for a list of compiled-in video output drivers.

    The recommended output drivers are ``--vo=vdpau`` and ``--vo=opengl-hq``.
    All other drivers are just for compatibility or special purposes.

.. admonition:: Example

    ``--vo=opengl,xv,``
        Try the ``opengl`` driver, then the ``xv`` driver, then others.

Available video output drivers are:

``xv`` (X11 only)
    Uses the XVideo extension to enable hardware-accelerated display. This is
    the most compatible VO on X, but may be low-quality, and has issues with
    OSD and subtitle display.

    .. note:: This driver is for compatibility with old systems.

    ``adaptor=<number>``
        Select a specific XVideo adapter (check xvinfo results).
    ``port=<number>``
        Select a specific XVideo port.
    ``ck=<cur|use|set>``
        Select the source from which the color key is taken (default: cur).

        cur
          The default takes the color key currently set in Xv.
        use
          Use but do not set the color key from mpv (use the ``--colorkey``
          option to change it).
        set
          Same as use but also sets the supplied color key.

    ``ck-method=<man|bg|auto>``
        Sets the color key drawing method (default: man).

        man
          Draw the color key manually (reduces flicker in some cases).
        bg
          Set the color key as window background.
        auto
          Let Xv draw the color key.

    ``colorkey=<number>``
        Changes the color key to an RGB value of your choice. ``0x000000`` is
        black and ``0xffffff`` is white.

    ``no-colorkey``
        Disables color-keying.

``x11`` (X11 only)
    Shared memory video output driver without hardware acceleration that works
    whenever X11 is present.

    .. note:: This is a fallback only, and should not be normally used.

``vdpau`` (X11 only)
    Uses the VDPAU interface to display and optionally also decode video.
    Hardware decoding is used with ``--hwdec=vdpau``.

    .. note::

        Earlier versions of mpv (and MPlayer, mplayer2) provided sub-options
        to tune vdpau post-processing, like ``deint``, ``sharpen``, ``denoise``,
        ``chroma-deint``, ``pullup``, ``hqscaling``. These sub-options are
        deprecated, and you should use the ``vdpaupp`` video filter instead.

    ``sharpen=<-1-1>``
        (Deprecated. See note about ``vdpaupp``.)

        For positive values, apply a sharpening algorithm to the video, for
        negative values a blurring algorithm (default: 0).
    ``denoise=<0-1>``
        (Deprecated. See note about ``vdpaupp``.)

        Apply a noise reduction algorithm to the video (default: 0; no noise
        reduction).
    ``deint=<-4-4>``
        (Deprecated. See note about ``vdpaupp``.)

        Select deinterlacing mode (default: 0). In older versions (as well as
        MPlayer/mplayer2) you could use this option to enable deinterlacing.
        This doesn't work anymore, and deinterlacing is enabled with either
        the ``D`` key (by default mapped to the command ``cycle deinterlace``),
        or the ``--deinterlace`` option. Also, to select the default deint mode,
        you should use something like ``--vf-defaults=vdpaupp:deint-mode=temporal``
        instead of this sub-option.

        0
            Pick the ``vdpaupp`` video filter default, which corresponds to 3.
        1
            Show only first field.
        2
            Bob deinterlacing.
        3
            Motion-adaptive temporal deinterlacing. May lead to A/V desync
            with slow video hardware and/or high resolution.
        4
            Motion-adaptive temporal deinterlacing with edge-guided spatial
            interpolation. Needs fast video hardware.
    ``chroma-deint``
        (Deprecated. See note about ``vdpaupp``.)

        Makes temporal deinterlacers operate both on luma and chroma (default).
        Use no-chroma-deint to solely use luma and speed up advanced
        deinterlacing. Useful with slow video memory.
    ``pullup``
        (Deprecated. See note about ``vdpaupp``.)

        Try to apply inverse telecine, needs motion adaptive temporal
        deinterlacing.
    ``hqscaling=<0-9>``
        (Deprecated. See note about ``vdpaupp``.)

        0
            Use default VDPAU scaling (default).
        1-9
            Apply high quality VDPAU scaling (needs capable hardware).
    ``fps=<number>``
        Override autodetected display refresh rate value (the value is needed
        for framedrop to allow video playback rates higher than display
        refresh rate, and for vsync-aware frame timing adjustments). Default 0
        means use autodetected value. A positive value is interpreted as a
        refresh rate in Hz and overrides the autodetected value. A negative
        value disables all timing adjustment and framedrop logic.
    ``composite-detect``
        NVIDIA's current VDPAU implementation behaves somewhat differently
        under a compositing window manager and does not give accurate frame
        timing information. With this option enabled, the player tries to
        detect whether a compositing window manager is active. If one is
        detected, the player disables timing adjustments as if the user had
        specified ``fps=-1`` (as they would be based on incorrect input). This
        means timing is somewhat less accurate than without compositing, but
        with the composited mode behavior of the NVIDIA driver, there is no
        hard playback speed limit even without the disabled logic. Enabled by
        default, use ``no-composite-detect`` to disable.
    ``queuetime_windowed=<number>`` and ``queuetime_fs=<number>``
        Use VDPAU's presentation queue functionality to queue future video
        frame changes at most this many milliseconds in advance (default: 50).
        See below for additional information.
    ``output_surfaces=<2-15>``
        Allocate this many output surfaces to display video frames (default:
        3). See below for additional information.
    ``colorkey=<#RRGGBB|#AARRGGBB>``
        Set the VDPAU presentation queue background color, which in practice
        is the colorkey used if VDPAU operates in overlay mode (default:
        ``#020507``, some shade of black). If the alpha component of this value
        is 0, the default VDPAU colorkey will be used instead (which is usually
        green).
    ``force-yuv``
        Never accept RGBA input. This means mpv will insert a filter to convert
        to a YUV format before the VO. Sometimes useful to force availability
        of certain YUV-only features, like video equalizer or deinterlacing.

    Using the VDPAU frame queuing functionality controlled by the queuetime
    options makes mpv's frame flip timing less sensitive to system CPU load and
    allows mpv to start decoding the next frame(s) slightly earlier, which can
    reduce jitter caused by individual slow-to-decode frames. However, the
    NVIDIA graphics drivers can make other window behavior such as window moves
    choppy if VDPAU is using the blit queue (mainly happens if you have the
    composite extension enabled) and this feature is active. If this happens on
    your system and it bothers you then you can set the queuetime value to 0 to
    disable this feature. The settings to use in windowed and fullscreen mode
    are separate because there should be no reason to disable this for
    fullscreen mode (as the driver issue should not affect the video itself).

    You can queue more frames ahead by increasing the queuetime values and the
    ``output_surfaces`` count (to ensure enough surfaces to buffer video for a
    certain time ahead you need at least as many surfaces as the video has
    frames during that time, plus two). This could help make video smoother in
    some cases. The main downsides are increased video RAM requirements for
    the surfaces and laggier display response to user commands (display
    changes only become visible some time after they're queued). The graphics
    driver implementation may also have limits on the length of maximum
    queuing time or number of queued surfaces that work well or at all.

``direct3d_shaders`` (Windows only)
    Video output driver that uses the Direct3D interface.

    .. note:: This driver is for compatibility with systems that don't provide
              proper OpenGL drivers.

    ``prefer-stretchrect``
        Use ``IDirect3DDevice9::StretchRect`` over other methods if possible.

    ``disable-stretchrect``
        Never render the video using ``IDirect3DDevice9::StretchRect``.

    ``disable-textures``
        Never render the video using D3D texture rendering. Rendering with
        textures + shader will still be allowed. Add ``disable-shaders`` to
        completely disable video rendering with textures.

    ``disable-shaders``
        Never use shaders when rendering video.

    ``only-8bit``
        Never render YUV video with more than 8 bits per component.
        Using this flag will force software conversion to 8-bit.

    ``disable-texture-align``
        Normally texture sizes are always aligned to 16. With this option
        enabled, the video texture will always have exactly the same size as
        the video itself.


    Debug options. These might be incorrect, might be removed in the future,
    might crash, might cause slow downs, etc. Contact the developers if you
    actually need any of these for performance or proper operation.

    ``force-power-of-2``
        Always force textures to power of 2, even if the device reports
        non-power-of-2 texture sizes as supported.

    ``texture-memory=<mode>``
        Only affects operation with shaders/texturing enabled, and (E)OSD.
        Possible values:

        ``default`` (default)
            Use ``D3DPOOL_DEFAULT``, with a ``D3DPOOL_SYSTEMMEM`` texture for
            locking. If the driver supports ``D3DDEVCAPS_TEXTURESYSTEMMEMORY``,
            ``D3DPOOL_SYSTEMMEM`` is used directly.

        ``default-pool``
            Use ``D3DPOOL_DEFAULT``. (Like ``default``, but never use a
            shadow-texture.)

        ``default-pool-shadow``
            Use ``D3DPOOL_DEFAULT``, with a ``D3DPOOL_SYSTEMMEM`` texture for
            locking. (Like ``default``, but always force the shadow-texture.)

        ``managed``
            Use ``D3DPOOL_MANAGED``.

        ``scratch``
            Use ``D3DPOOL_SCRATCH``, with a ``D3DPOOL_SYSTEMMEM`` texture for
            locking.

    ``swap-discard``
        Use ``D3DSWAPEFFECT_DISCARD``, which might be faster.
        Might be slower too, as it must(?) clear every frame.

    ``exact-backbuffer``
        Always resize the backbuffer to window size.

``direct3d`` (Windows only)
    Same as ``direct3d_shaders``, but with the options ``disable-textures``
    and ``disable-shaders`` forced.

    .. note:: This driver is for compatibility with old systems.

``opengl``
    OpenGL video output driver. It supports extended scaling methods, dithering
    and color management.

    By default, it tries to use fast and fail-safe settings. Use the alias
    ``opengl-hq`` to use this driver with defaults set to high quality
    rendering.

    Requires at least OpenGL 2.1.

    Some features are available with OpenGL 3 capable graphics drivers only
    (or if the necessary extensions are available).

    OpenGL ES 2.0 and 3.0 are supported as well.

    Hardware decoding over OpenGL-interop is supported to some degree. Note
    that in this mode, some corner case might not be gracefully handled, and
    color space conversion and chroma upsampling is generally in the hand of
    the hardware decoder APIs.

    ``scale=<filter>``

        ``bilinear``
            Bilinear hardware texture filtering (fastest, very low quality).
            This is the default for compatibility reasons.

        ``spline36``
            Mid quality and speed. This is the default when using ``opengl-hq``.

        ``lanczos``
            Lanczos scaling. Provides mid quality and speed. Generally worse
            than ``spline36``, but it results in a slightly sharper image
            which is good for some content types. The number of taps can be
            controlled with ``scale-radius``, but is best left unchanged.

            This filter corresponds to the old ``lanczos3`` alias if the default
            radius is used, while ``lanczos2`` corresponds to a radius of 2.

            (This filter is an alias for ``sinc``-windowed ``sinc``)

        ``ewa_lanczos``
            Elliptic weighted average Lanczos scaling. Also known as Jinc.
            Relatively slow, but very good quality. The radius can be
            controlled with ``scale-radius``. Increasing the radius makes the
            filter sharper but adds more ringing.

            (This filter is an alias for ``jinc``-windowed ``jinc``)

        ``ewa_lanczossharp``
            A slightly sharpened version of ewa_lanczos, preconfigured to use
            an ideal radius and parameter. If your hardware can run it, this is
            probably what you should use by default.

        ``mitchell``
            Mitchell-Netravali. The ``B`` and ``C`` parameters can be set with
            ``scale-param1`` and ``scale-param2``. This filter is very good at
            downscaling (see ``dscale``).

        ``oversample``
            A version of nearest neighbour that (naively) oversamples pixels,
            so that pixels overlapping edges get linearly interpolated instead
            of rounded. This essentially removes the small imperfections and
            judder artifacts caused by nearest-neighbour interpolation, in
            exchange for adding some blur. This filter is good at temporal
            interpolation, and also known as "smoothmotion" (see ``tscale``).

        There are some more filters, but most are not as useful. For a complete
        list, pass ``help`` as value, e.g.::

            mpv --vo=opengl:scale=help

    ``scale-param1=<value>``, ``scale-param2=<value>``
        Set filter parameters. Ignored if the filter is not tunable.
        Currently, this affects the following filter parameters:

        bcspline
            Spline parameters (``B`` and ``C``). Defaults to 0.5 for both.

        gaussian
            Scale parameter (``t``). Increasing this makes the result blurrier.
            Defaults to 1.

        sharpen3, sharpen5
            Sharpening strength. Increasing this makes the image sharper but
            adds more ringing and aliasing. Defaults to 0.5.

        oversample
            Minimum distance to an edge before interpolation is used. Setting
            this to 0 will always interpolate edges, whereas setting it to 0.5
            will never interpolate, thus behaving as if the regular nearest
            neighbour algorithm was used. Defaults to 0.0.

    ``scale-blur=<value>``
        Kernel scaling factor (also known as a blur factor). Decreasing this
        makes the result sharper, increasing it makes it blurrier (default 0).
        If set to 0, the kernel's preferred blur factor is used. Note that
        setting this too low (eg. 0.5) leads to bad results. It's generally
        recommended to stick to values between 0.8 and 1.2.

    ``scale-radius=<value>``
        Set radius for filters listed below, must be a float number between 0.5
        and 16.0. Defaults to the filter's preferred radius if not specified.

            ``sinc`` and derivatives, ``jinc`` and derivatives, ``gaussian``, ``box`` and ``triangle``

        Note that depending on filter implementation details and video scaling
        ratio, the radius that actually being used might be different
        (most likely being increased a bit).

    ``scale-antiring=<value>``
        Set the antiringing strength. This tries to eliminate ringing, but can
        introduce other artifacts in the process. Must be a float number
        between 0.0 and 1.0. The default value of 0.0 disables antiringing
        entirely.

        Note that this doesn't affect the special filters ``bilinear``,
        ``bicubic_fast`` or ``sharpen``.

    ``scale-window=<window>``
        (Advanced users only) Choose a custom windowing function for the kernel.
        Defaults to the filter's preferred window if unset. Use
        ``scale-window=help`` to get a list of supported windowing functions.

    ``scale-wparam=<window>``
        (Advanced users only) Configure the parameter for the window function
        given by ``scale-window``. Ignored if the window is not tunable.
        Currently, this affects the following window parameters:

        kaiser
            Window parameter (alpha). Defaults to 6.33.
        blackman
            Window parameter (alpha). Defaults to 0.16.
        gaussian
            Scale parameter (t). Increasing this makes the window wider.
            Defaults to 1.

    ``scaler-resizes-only``
        Disable the scaler if the video image is not resized. In that case,
        ``bilinear`` is used instead whatever is set with ``scale``. Bilinear
        will reproduce the source image perfectly if no scaling is performed.
        Note that this option never affects ``cscale``.

    ``pbo``
        Enable use of PBOs. This is slightly faster, but can sometimes lead to
        sporadic and temporary image corruption (in theory, because reupload
        is not retried when it fails), and perhaps actually triggers slower
        paths with drivers that don't support PBOs properly.

    ``dither-depth=<N|no|auto>``
        Set dither target depth to N. Default: no.

        no
            Disable any dithering done by mpv.
        auto
            Automatic selection. If output bit depth cannot be detected,
            8 bits per component are assumed.
        8
            Dither to 8 bit output.

        Note that the depth of the connected video display device can not be
        detected. Often, LCD panels will do dithering on their own, which
        conflicts with ``opengl``'s dithering and leads to ugly output.

    ``dither-size-fruit=<2-8>``
        Set the size of the dither matrix (default: 6). The actual size of
        the matrix is ``(2^N) x (2^N)`` for an option value of ``N``, so a
        value of 6 gives a size of 64x64. The matrix is generated at startup
        time, and a large matrix can take rather long to compute (seconds).

        Used in ``dither=fruit`` mode only.

    ``dither=<fruit|ordered|no>``
        Select dithering algorithm (default: fruit). (Normally, the
        ``dither-depth`` option controls whether dithering is enabled.)

    ``temporal-dither``
        Enable temporal dithering. (Only active if dithering is enabled in
        general.) This changes between 8 different dithering pattern on each
        frame by changing the orientation of the tiled dithering matrix.
        Unfortunately, this can lead to flicker on LCD displays, since these
        have a high reaction time.

    ``debug``
        Check for OpenGL errors, i.e. call ``glGetError()``. Also request a
        debug OpenGL context (which does nothing with current graphics drivers
        as of this writing).

    ``interpolation``
        Reduce stuttering caused by mismatches in the video fps and display
        refresh rate (also known as judder).

        This essentially attempts to interpolate the missing frames by
        convoluting the video along the temporal axis. The filter used can be
        controlled using the ``tscale`` setting.

        Note that this relies on vsync to work, see ``swapinterval`` for more
        information.

    ``swapinterval=<n>``
        Interval in displayed frames between two buffer swaps.
        1 is equivalent to enable VSYNC, 0 to disable VSYNC. Defaults to 1 if
        not specified.

        Note that this depends on proper OpenGL vsync support. On some platforms
        and drivers, this only works reliably when in fullscreen mode. It may
        also require driver-specific hacks if using multiple monitors, to
        ensure mpv syncs to the right one. Compositing window managers can
        also lead to bad results, as can missing or incorrect display FPS
        information (see ``--display-fps``).

    ``dscale=<filter>``
        Like ``scale``, but apply these filters on downscaling instead. If this
        option is unset, the filter implied by ``scale`` will be applied.

    ``cscale=<filter>``
        As ``scale``, but for interpolating chroma information. If the image
        is not subsampled, this option is ignored entirely.

    ``tscale=<filter>``
        The filter used for interpolating the temporal axis (frames). This is
        only used if ``interpolation`` is enabled. The only valid choices
        for ``tscale`` are separable convolution filters (use ``tscale=help``
        to get a list). The default is ``oversample``.

        Note that the maximum supported filter radius is currently 3, and that
        using filters with larger radius may introduce issues when pausing or
        framestepping, proportional to the radius used. It is recommended to
        stick to a radius of 1 or 2.

    ``dscale-radius``, ``cscale-radius``, ``tscale-radius``, etc.
        Set filter parameters for ``dscale``, ``cscale`` and ``tscale``,
        respectively.

        See the corresponding options for ``scale``.

    ``linear-scaling``
        Scale in linear light. This is automatically enabled if
        ``target-prim``, ``target-trc``, ``icc-profile`` or
        ``sigmoid-upscaling`` is set. It should only be used with a
        ``fbo-format`` that has at least 16 bit precision.

    ``fancy-downscaling``
        When using convolution based filters, extend the filter size
        when downscaling. Trades quality for reduced downscaling performance.

        This is automatically disabled for anamorphic video, because this
        feature doesn't work correctly with different scale factors in
        different directions.

    ``sigmoid-upscaling``
        When upscaling, use a sigmoidal color transform to avoid emphasizing
        ringing artifacts. This also enables ``linear-scaling``.

    ``sigmoid-center``
        The center of the sigmoid curve used for ``sigmoid-upscaling``, must
        be a float between 0.0 and 1.0. Defaults to 0.75 if not specified.

    ``sigmoid-slope``
        The slope of the sigmoid curve used for ``sigmoid-upscaling``, must
        be a float between 1.0 and 20.0. Defaults to 6.5 if not specified.

    ``no-npot``
        Force use of power-of-2 texture sizes. For debugging only.
        Borders will be distorted due to filtering.

    ``glfinish``
        Call ``glFinish()`` before and after swapping buffers (default: disabled).
        Slower, but might help getting better results when doing framedropping.
        Can completely ruin performance. The details depend entirely on the
        OpenGL driver.

    ``waitvsync``
        Call ``glXWaitVideoSyncSGI`` after each buffer swap (default: disabled).
        This may or may not help with video timing accuracy and frame drop. It's
        possible that this makes video output slower, or has no effect at all.

        X11/GLX only.

    ``dwmflush=<no|windowed|yes>``
        Calls ``DwmFlush`` after swapping buffers on Windows (default: no).
        It also sets ``SwapInterval(0)`` to ignore the OpenGL timing. Values
        are: no (disabled), windowed (only in windowed mode), yes (also in
        full screen).
        This may help getting more consistent frame intervals, especially with
        high-fps clips - which might also reduce dropped frames. Typically a
        value of 1 should be enough since full screen may bypass the DWM.

        Windows only.

    ``sw``
        Continue even if a software renderer is detected.

    ``backend=<sys>``
        The value ``auto`` (the default) selects the windowing backend. You
        can also pass ``help`` to get a complete list of compiled in backends
        (sorted by autoprobe order).

        auto
            auto-select (default)
        cocoa
            Cocoa/OS X
        win
            Win32/WGL
        x11, x11es
            X11/GLX (the ``es`` variant forces GLES)
        wayland
            Wayland/EGL
        x11egl, x11egles
            X11/EGL (the ``es`` variant forces GLES)

    ``fbo-format=<fmt>``
        Selects the internal format of textures used for FBOs. The format can
        influence performance and quality of the video output. (FBOs are not
        always used, and typically only when using extended scalers.)
        ``fmt`` can be one of: rgb, rgba, rgb8, rgb10, rgb10_a2, rgb16, rgb16f,
        rgb32f, rgba12, rgba16, rgba16f, rgba32f.
        Default: rgba16.

    ``gamma=<0.1..2.0>``
        Set a gamma value (default: 1.0). If gamma is adjusted in other ways
        (like with the ``--gamma`` option or key bindings and the ``gamma``
        property), the value is multiplied with the other gamma value.

        Recommended values based on the environmental brightness:

        1.0
            Brightly illuminated (default)
        0.9
            Slightly dim
        0.8
            Pitch black room

    ``gamma-auto``
        Automatically corrects the gamma value depending on ambient lighting
        conditions (adding a gamma boost for dark rooms).

        With ambient illuminance of 64lux, mpv will pick the 1.0 gamma value
        (no boost), and slightly increase the boost up until 0.8 for 16lux.

        NOTE: Only implemented on OS X.

    ``target-prim=<value>``
        Specifies the primaries of the display. Video colors will be adapted
        to this colorspace if necessary. Valid values are:

        auto
            Disable any adaptation (default)
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

    ``target-trc=<value>``
        Specifies the transfer characteristics (gamma) of the display. Video
        colors will be adjusted to this curve. Valid values are:

        auto
            Disable any adaptation (default)
        bt.1886
            ITU-R BT.1886 curve, without the brightness drop (approx. 1.961)
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

    ``icc-profile=<file>``
        Load an ICC profile and use it to transform linear RGB to screen output.
        Needs LittleCMS 2 support compiled in. This option overrides the
        ``target-prim`` and ``target-trc`` options. It also enables
        ``linear-scaling``.

    ``icc-profile-auto``
        Automatically select the ICC display profile currently specified by
        the display settings of the operating system.

        NOTE: Only implemented on OS X and X11

    ``icc-cache=<file>``
        Store and load the 3D LUT created from the ICC profile in this file.
        This can be used to speed up loading, since LittleCMS 2 can take a while
        to create the 3D LUT. Note that this file contains an uncompressed LUT.
        Its size depends on the ``3dlut-size``, and can be very big.

    ``icc-intent=<value>``
        Specifies the ICC intent used for the color transformation (when using
        ``icc-profile``).

        0
            perceptual
        1
            relative colorimetric (default)
        2
            saturation
        3
            absolute colorimetric

    ``3dlut-size=<r>x<g>x<b>``
        Size of the 3D LUT generated from the ICC profile in each dimension.
        Default is 128x256x64.
        Sizes must be a power of two, and 512 at most.

    ``blend-subtitles=<yes|video|no>``
        Blend subtitles directly onto upscaled video frames, before
        interpolation and/or color management (default: no). Enabling this
        causes subtitles to be affected by ``icc-profile``, ``target-prim``,
        ``target-trc``, ``interpolation``, ``gamma`` and ``linear-scaling``.
        It also increases subtitle performance when using ``interpolation``.

        The downside of enabling this is that it restricts subtitles to the
        visible portion of the video, so you can't have subtitles exist in the
        black margins below a video (for example).

        If ``video`` is selected, the behavior is similar to ``yes``, but subs
        are drawn at the video's native resolution, and scaled along with the
        video.

        .. warning:: This changes the way subtitle colors are handled. Normally,
                     subtitle colors are assumed to be in sRGB and color managed
                     as such. Enabling this makes them treated as being in the
                     video's color space instead. This is good if you want
                     things like softsubbed ASS signs to match the video colors,
                     but may cause SRT subtitles or similar to look slightly off.

    ``alpha=<blend|yes|no>``
        Decides what to do if the input has an alpha component (default: blend).

        blend
            Blend the frame against a black background.
        yes
            Try to create a framebuffer with alpha component. This only makes sense
            if the video contains alpha information (which is extremely rare). May
            not be supported on all platforms. If alpha framebuffers are
            unavailable, it silently falls back on a normal framebuffer. Note
            that if you set the ``fbo-format`` option to a non-default value,
            a format with alpha must be specified, or this won't work.
        no
            Ignore alpha component.

    ``rectangle-textures``
        Force use of rectangle textures (default: no). Normally this shouldn't
        have any advantages over normal textures. Note that hardware decoding
        overrides this flag.

    ``background=<color>``
        Color used to draw parts of the mpv window not covered by video.
        See ``--osd-color`` option how colors are defined.

``opengl-hq``
    Same as ``opengl``, but with default settings for high quality rendering.

    This is equivalent to::

        --vo=opengl:scale=spline36:cscale=spline36:dscale=mitchell:dither-depth=auto:fancy-downscaling:sigmoid-upscaling

    Note that some cheaper LCDs do dithering that gravely interferes with
    ``opengl``'s dithering. Disabling dithering with ``dither-depth=no`` helps.

    Unlike ``opengl``, ``opengl-hq`` makes use of FBOs by default. Sometimes you
    can achieve better quality or performance by changing the ``fbo-format``
    suboption to ``rgb16f``, ``rgb32f`` or ``rgb``. Known problems include
    Mesa/Intel not accepting ``rgb16``, Mesa sometimes not being compiled with
    float texture support, and some OS X setups being very slow with ``rgb16``
    but fast with ``rgb32f``.

``sdl``
    SDL 2.0+ Render video output driver, depending on system with or without
    hardware acceleration. Should work on all platforms supported by SDL 2.0.
    For tuning, refer to your copy of the file ``SDL_hints.h``.

    .. note:: This driver is for compatibility with systems that don't provide
              proper graphics drivers, or which support GLES only.

    ``sw``
        Continue even if a software renderer is detected.

    ``switch-mode``
        Instruct SDL to switch the monitor video mode when going fullscreen.

``vaapi``
    Intel VA API video output driver with support for hardware decoding. Note
    that there is absolutely no reason to use this, other than wanting to use
    hardware decoding to save power on laptops, or possibly preventing video
    tearing with some setups.

    .. note:: This driver is for compatibility with crappy systems. You can
              use vaapi hardware decoding with ``--vo=opengl`` too.

    ``scaling=<algorithm>``
        default
            Driver default (mpv default as well).
        fast
            Fast, but low quality.
        hq
            Unspecified driver dependent high-quality scaling, slow.
        nla
            ``non-linear anamorphic scaling``

    ``deint-mode=<mode>``
        Select deinterlacing algorithm. Note that by default deinterlacing is
        initially always off, and needs to be enabled with the ``D`` key
        (default key binding for ``cycle deinterlace``).

        This option doesn't apply if libva supports video post processing (vpp).
        In this case, the default for ``deint-mode`` is ``no``, and enabling
        deinterlacing via user interaction using the methods mentioned above
        actually inserts the ``vavpp`` video filter. If vpp is not actually
        supported with the libva backend in use, you can use this option to
        forcibly enable VO based deinterlacing.

        no
            Don't allow deinterlacing (default for newer libva).
        first-field
            Show only first field (going by ``--field-dominance``).
        bob
            bob deinterlacing (default for older libva).

    ``scaled-osd=<yes|no>``
        If enabled, then the OSD is rendered at video resolution and scaled to
        display resolution. By default, this is disabled, and the OSD is
        rendered at display resolution if the driver supports it.

``null``
    Produces no video output. Useful for benchmarking.

``caca``
    Color ASCII art video output driver that works on a text console.

    .. note:: This driver is a joke.

``image``
    Output each frame into an image file in the current directory. Each file
    takes the frame number padded with leading zeros as name.

    ``format=<format>``
        Select the image file format.

        jpg
            JPEG files, extension .jpg. (Default.)
        jpeg
            JPEG files, extension .jpeg.
        png
            PNG files.
        ppm
            Portable bitmap format.
        pgm
            Portable graymap format.
        pgmyuv
            Portable graymap format, using the YV12 pixel format.
        tga
            Truevision TGA.

    ``png-compression=<0-9>``
        PNG compression factor (speed vs. file size tradeoff) (default: 7)
    ``png-filter=<0-5>``
        Filter applied prior to PNG compression (0 = none; 1 = sub; 2 = up;
        3 = average; 4 = Paeth; 5 = mixed) (default: 5)
    ``jpeg-quality=<0-100>``
        JPEG quality factor (default: 90)
    ``(no-)jpeg-progressive``
        Specify standard or progressive JPEG (default: no).
    ``(no-)jpeg-baseline``
        Specify use of JPEG baseline or not (default: yes).
    ``jpeg-optimize=<0-100>``
        JPEG optimization factor (default: 100)
    ``jpeg-smooth=<0-100>``
        smooth factor (default: 0)
    ``jpeg-dpi=<1->``
        JPEG DPI (default: 72)
    ``outdir=<dirname>``
        Specify the directory to save the image files to (default: ``./``).

``wayland`` (Wayland only)
    Wayland shared memory video output as fallback for ``opengl``.

    .. note:: This driver is for compatibility with systems that don't provide
              working OpenGL drivers.

    ``alpha``
        Use a buffer format that supports videos and images with alpha
        information
    ``rgb565``
        Use RGB565 as buffer format. This format is implemented on most
        platforms, especially on embedded where it is far more efficient then
        RGB8888.
    ``triple-buffering``
        Use 3 buffers instead of 2. This can lead to more fluid playback, but
        uses more memory.

``opengl-cb``
    For use with libmpv direct OpenGL embedding; useless in any other contexts.
    (See ``<mpv/opengl_cb.h>``.)
    Usually, ``opengl-cb`` renders frames asynchronously by client and this
    can cause some frame drops. In order to provide a way to handle this
    situation, ``opengl-cb`` has its own frame queue and calls update callback
    more frequently if the queue is not empty regardless of existence of new frame.
    Once the queue is filled, ``opengl-cb`` drops frames automatically.

    With default options, ``opengl-cb`` renders only the latest frame and drops
    all frames handed over while waiting render function after update callback.

    ``frame-queue-size=<1..100>``
        The maximum count of frames which the frame queue can hold (default: 1)

    ``frame-drop-mode=<pop|clear>``
        Select the behavior when the frame queue is full.

        pop
            Drop the oldest frame in the frame queue. (default)
        clear
            Drop all frames in the frame queue.

    This also supports many of the suboptions the ``opengl`` VO has. Run
    ``mpv --vo=opengl-cb:help`` for a list.

    This also supports the ``vo_cmdline`` command.

``rpi`` (Raspberry Pi)
    Native video output on the Raspberry Pi using the MMAL API.

    ``display=<number>``
        Select the display number on which the video overlay should be shown
        (default: 0).

    ``layer=<number>``
        Select the dispmanx layer on which the video overlay should be shown
        (default: -10). Note that mpv will also use the 2 layers above the
        selected layer, to handle the window background and OSD. Actual video
        rendering will happen on the layer above the selected layer.

``drm`` (Direct Rendering Manager)
    Video output driver using Kernel Mode Setting / Direct Rendering Manager.
    Does not support hardware acceleration. Should be used when one doesn't
    want to install full-blown graphical environment (e.g. no X).

    ``connector=<number>``
        Select the connector to use (usually this is a monitor.) If set to -1,
        mpv renders the output on the first available connector. (default: -1)

    ``devpath=<filename>``
        Path to graphic card device.
        (default: /dev/dri/card0)
