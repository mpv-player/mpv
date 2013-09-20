VIDEO OUTPUT DRIVERS
====================

Video output drivers are interfaces to different video output facilities. The
syntax is:

``--vo=<driver1[:suboption1[=value]:...],driver2,...[,]>``
    Specify a priority list of video output drivers to be used.

If the list has a trailing ',', mpv will fall back on drivers not contained
in the list. Suboptions are optional and can mostly be omitted.

.. note::

    See ``--vo=help`` for a list of compiled-in video output drivers.

.. admonition:: Example

    ``--vo=opengl,xv,``
        Try the ``opengl`` driver, then the ``xv`` driver, then others.

Available video output drivers are:

``xv`` (X11 only)
    Uses the XVideo extension to enable hardware-accelerated display. This is
    the most compatible VO on X, but may be low-quality, and has issues with
    OSD and subtitle display.

    ``adaptor=<number>``
        Select a specific XVideo adaptor (check xvinfo results).
    ``port=<number>``
        Select a specific XVideo port.
    ``ck=<cur|use|set>``
        Select the source from which the colorkey is taken (default: cur).

        cur
          The default takes the colorkey currently set in Xv.
        use
          Use but do not set the colorkey from mpv (use the ``--colorkey``
          option to change it).
        set
          Same as use but also sets the supplied colorkey.

    ``ck-method=<man|bg|auto>``
        Sets the colorkey drawing method (default: man).

        man
          Draw the colorkey manually (reduces flicker in some cases).
        bg
          Set the colorkey as window background.
        auto
          Let Xv draw the colorkey.

    ``colorkey=<number>``
        Changes the colorkey to an RGB value of your choice. ``0x000000`` is
        black and ``0xffffff`` is white.

    ``no-colorkey``
        Disables colorkeying.

``x11`` (X11 only)
    Shared memory video output driver without hardware acceleration that works
    whenever X11 is present.

    .. note:: This is a fallback only, and should not be normally used.

``vdpau`` (X11 only)
    Uses the VDPAU interface to display and optionally also decode video.
    Hardware decoding is used with ``--hwdec=vdpau``.

    ``sharpen=<-1-1>``
        For positive values, apply a sharpening algorithm to the video, for
        negative values a blurring algorithm (default: 0).
    ``denoise=<0-1>``
        Apply a noise reduction algorithm to the video (default: 0; no noise
        reduction).
    ``deint=<-4-4>``
        Select deinterlacing mode (default: -3). Positive values choose mode
        and enable deinterlacing. Corresponding negative values select the
        same deinterlacing mode, but do not enable deinterlacing on startup
        (useful in configuration files to specify which mode will be enabled by
        the "D" key). All modes respect ``--field-dominance``.

        0
            Same as -3.
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
        Makes temporal deinterlacers operate both on luma and chroma (default).
        Use no-chroma-deint to solely use luma and speed up advanced
        deinterlacing. Useful with slow video memory.
    ``pullup``
        Try to apply inverse telecine, needs motion adaptive temporal
        deinterlacing.
    ``hqscaling=<0-9>``
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

    Using the VDPAU frame queueing functionality controlled by the queuetime
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

    ``texture-memory=N``
        Only affects operation with shaders/texturing enabled, and (E)OSD.
        Values for N:

            0
                default, will often use an additional shadow texture + copy
            1
                use ``D3DPOOL_MANAGED``
            2
                use ``D3DPOOL_DEFAULT``
            3
                use ``D3DPOOL_SYSTEMMEM``, but without shadow texture

    ``swap-discard``
        Use ``D3DSWAPEFFECT_DISCARD``, which might be faster.
        Might be slower too, as it must(?) clear every frame.

    ``exact-backbuffer``
        Always resize the backbuffer to window size.

``direct3d`` (Windows only)
    Same as ``direct3d_shaders``, but with the options ``disable-textures``
    and ``disable-shaders`` forced.

``corevideo`` (Mac OS X 10.6 and later)
    Mac OS X CoreVideo video output driver. Uses the CoreVideo APIs to fill
    PixelBuffers and generate OpenGL textures from them (useful as a fallback
    for ``opengl``).

``opengl``
    OpenGL video output driver. It supports extended scaling methods, dithering
    and color management.

    By default, it tries to use fast and fail-safe settings. Use the alias
    ``opengl-hq`` to use this driver with defaults set to high quality
    rendering.

    Requires at least OpenGL 2.1 and the ``GL_ARB_texture_rg`` extension. For
    older drivers, ``opengl-old`` may work.

    Some features are available with OpenGL 3 capable graphics drivers only
    (or if the necessary extensions are available).

    ``lscale=<filter>``

        ``bilinear``
            Bilinear hardware texture filtering (fastest, mid-quality).
            This is the default.

        ``lanczos2``
            Lanczos scaling with radius=2. Provides good quality and speed.
            This is the default when using ``opengl-hq``.

        ``lanczos3``
            Lanczos with radius=3.

        ``bicubic_fast``
            Bicubic filter. Has a blurring effect on the image, even if no
            scaling is done.

        ``sharpen3``
            Unsharp masking (sharpening) with radius=3 and a default strength
            of 0.5 (see ``lparam1``).

        ``sharpen5``
            Unsharp masking (sharpening) with radius=5 and a default strength
            of 0.5 (see ``lparam1``).

        ``mitchell``
            Mitchell-Netravali. The ``b`` and ``c`` parameters can be set with
            ``lparam1`` and ``lparam2``. Both are set to 1/3 by default.


        There are some more filters. For a complete list, pass ``help`` as
        value, e.g.::

            mpv --vo=opengl:lscale=help

    ``lparam1=<value>``
        Set filter parameters. Ignored if the filter is not tunable. These are
        unset by default, and use the filter specific default if applicable.

    ``lparam2=<value>``
        See ``lparam1``.

    ``scaler-resizes-only``
        Disable the scaler if the video image is not resized. In that case,
        ``bilinear`` is used instead whatever is set with ``lscale``. Bilinear
        will reproduce the source image perfectly if no scaling is performed.
        Note that this option never affects ``cscale``, although the different
        processing chain might do chroma scaling differently if ``lscale`` is
        disabled.

    ``stereo=<value>``
        Select a method for stereo display. You may have to use ``--aspect`` to
        fix the aspect value. Experimental, do not expect too much from it.

        no
            Normal 2D display
        red-cyan
            Convert side by side input to full-color red-cyan stereo.
        green-magenta
            Convert side by side input to full-color green-magenta stereo.
        quadbuffer
            Convert side by side input to quadbuffered stereo. Only supported
            by very few OpenGL cards.

    ``srgb``
        Enable gamma-correct scaling by working in linear light. This
        makes use of sRGB textures and framebuffers.
        This option forces the options ``indirect`` and ``gamma``.

        .. note::

            for YUV colorspaces, gamma 1/0.45 (2.222) is assumed. RGB input is
            always assumed to be in sRGB.

        This option is not really useful, as gamma-correct scaling has not much
        influence on typical video playback. Most visible effect comes from
        slightly different gamma.

    ``pbo``
        Enable use of PBOs. This is faster, but can sometimes lead to sporadic
        and temporary image corruption.

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
        Select dithering algorithm (default: fruit).

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

    ``swapinterval=<n>``
        Interval in displayed frames between two buffer swaps.
        1 is equivalent to enable VSYNC, 0 to disable VSYNC.

    ``no-scale-sep``
        When using a separable scale filter for luma, usually two filter
        passes are done. This is often faster. However, it forces
        conversion to RGB in an extra pass, so it can actually be slower
        if used with fast filters on small screen resolutions. Using
        this options will make rendering a single operation.
        Note that chroma scalers are always done as 1-pass filters.

    ``cscale=<n>``
        As ``lscale``, but for chroma (2x slower with little visible effect).
        Note that with some scaling filters, upscaling is always done in
        RGB. If chroma is not subsampled, this option is ignored, and the
        luma scaler is used instead. Setting this option is often useless.

    ``fancy-downscaling``
        When using convolution based filters, extend the filter size
        when downscaling. Trades quality for reduced downscaling performance.

    ``no-npot``
        Force use of power-of-2 texture sizes. For debugging only.
        Borders will be distorted due to filtering.

    ``glfinish``
        Call ``glFinish()`` before swapping buffers

    ``sw``
        Continue even if a software renderer is detected.

    ``backend=<sys>``
        The value ``auto`` (the default) selects the windowing backend. You
        can also pass ``help`` to get a complete list of compiled in backends
        (sorted by autoprobe order).

        auto
            auto-select (default)
        cocoa
            Cocoa/OSX
        win
            Win32/WGL
        x11
            X11/GLX
        wayland
            Wayland/EGL

    ``indirect``
        Do YUV conversion and scaling as separate passes. This will first render
        the video into a video-sized RGB texture, and draw the result on screen.
        The luma scaler is used to scale the RGB image when rendering to screen.
        The chroma scaler is used only on YUV conversion, and only if the video
        is chroma-subsampled (usually the case).
        This mechanism is disabled on RGB input.
        Specifying this option directly is generally useful for debugging only.

    ``fbo-format=<fmt>``
        Selects the internal format of textures used for FBOs. The format can
        influence performance and quality of the video output. (FBOs are not
        always used, and typically only when using extended scalers.)
        ``fmt`` can be one of: rgb, rgba, rgb8, rgb10, rgb16, rgb16f, rgb32f,
        rgba12, rgba16, rgba16f, rgba32f.
        Default: rgb.

    ``gamma``
        Always enable gamma control. (Disables delayed enabling.)

    ``icc-profile=<file>``
        Load an ICC profile and use it to transform linear RGB to screen output.
        Needs LittleCMS2 support compiled in.

    ``icc-cache=<file>``
        Store and load the 3D LUT created from the ICC profile in this file.
        This can be used to speed up loading, since LittleCMS2 can take a while
        to create the 3D LUT. Note that this file contains an uncompressed LUT.
        Its size depends on the ``3dlut-size``, and can be very big.

    ``icc-intent=<value>``
        0
            perceptual
        1
            relative colorimetric
        2
            saturation
        3
            absolute colorimetric (default)

    ``3dlut-size=<r>x<g>x<b>``
        Size of the 3D LUT generated from the ICC profile in each dimension.
        Default is 128x256x64.
        Sizes must be a power of two, and 256 at most.

    ``alpha=<blend|yes|no>``
        Decides what to do if the input has an alpha component (default: blend).

        blend
            Blend the frame against a black background.
        yes
            Try to create a framebuffer with alpha component. This only makes sense
            if the video contains alpha information (which is extremely rare). May
            not be supported on all platforms. If alpha framebuffers are
            unavailable, it silently falls back on a normal framebuffer. Note
            that when using FBO indirections (such as with ``opengl-hq``), an FBO
            format with alpha must be specified with the ``fbo-format`` option.
        no
            Ignore alpha component.

    ``chroma-location=<auto|center|left>``
        Set the YUV chroma sample location. auto means use the bitstream
        flags (default: auto).

``opengl-hq``
    Same as ``opengl``, but with default settings for high quality rendering.

    This is equivalent to::

        --vo=opengl:lscale=lanczos2:dither-depth=auto:pbo:fbo-format=rgb16

    Note that some cheaper LCDs do dithering that gravely interferes with
    ``opengl``'s dithering. Disabling dithering with ``dither-depth=no`` helps.

    Unlike ``opengl``, ``opengl-hq`` makes use of FBOs by default. Sometimes you
    can achieve better quality or performance by changing the ``fbo-format``
    suboption to ``rgb16f``, ``rgb32f`` or ``rgb``. Known problems include
    Mesa/Intel not accepting ``rgb16``, Mesa sometimes not being compiled with
    float texture support, and some OSX setups being very slow with ``rgb16``
    but fast with ``rgb32f``.

``opengl-old``
    OpenGL video output driver, old version. Video size must be smaller
    than the maximum texture size of your OpenGL implementation. Intended to
    work even with the most basic OpenGL implementations, but also makes use
    of newer extensions which allow support for more color spaces.

    The code performs very few checks, so if a feature does not work, this
    might be because it is not supported by your card and/or OpenGL
    implementation, even if you do not get any error message. Use ``glxinfo``
    or a similar tool to display the supported OpenGL extensions.

    ``(no-)ati-hack``
        ATI drivers may give a corrupted image when PBOs are used (when using
        ``force-pbo``). This option fixes this, at the expense of using a bit
        more memory.
    ``(no-)force-pbo``
        Always uses PBOs to transfer textures even if this involves an extra
        copy. Currently this gives a little extra speed with NVIDIA drivers
        and a lot more speed with ATI drivers. May need the ``ati-hack``
        suboption to work correctly.
    ``(no-)scaled-osd``
        Scales the OSD and subtitles instead of rendering them at display size
        (default: disabled).
    ``rectangle=<0,1,2>``
        Select usage of rectangular textures, which saves video RAM, but often
        is slower (default: 0).

        0
            Use power-of-two textures (default).
        1
            Use the ``GL_ARB_texture_rectangle`` extension.
        2
            Use the ``GL_ARB_texture_non_power_of_two`` extension. In some
            cases only supported in software and thus very slow.

    ``swapinterval=<n>``
        Minimum interval between two buffer swaps, counted in displayed frames
        (default: 1). 1 is equivalent to enabling VSYNC, 0 to disabling VSYNC.
        Values below 0 will leave it at the system default. This limits the
        framerate to (horizontal refresh rate / n). Requires
        ``GLX_SGI_swap_control`` support to work. With some (most/all?)
        implementations this only works in fullscreen mode.
    ``ycbcr``
        Use the ``GL_MESA_ycbcr_texture`` extension to convert YUV to RGB. In
        most cases this is probably slower than doing software conversion to
        RGB.
    ``yuv=<n>``
        Select the type of YUV to RGB conversion. The default is
        auto-detection deciding between values 0 and 2.

        0
            Use software conversion. Compatible with all OpenGL versions.
            Provides brightness, contrast and saturation control.
        1
            Same as 2. This used to use NVIDIA-specific extensions, which
            did not provide any advantages over using fragment programs, except
            possibly on very ancient graphics cards. It produced a gray-ish
            output, which is why it has been removed.
        2
            Use a fragment program. Needs the ``GL_ARB_fragment_program``
            extension and at least three texture units. Provides brightness,
            contrast, saturation and hue control.
        3
            Use a fragment program using the ``POW`` instruction. Needs the
            ``GL_ARB_fragment_program`` extension and at least three texture
            units. Provides brightness, contrast, saturation, hue and gamma
            control. Gamma can also be set independently for red, green and
            blue. Method 4 is usually faster.
        4
            Use a fragment program with additional lookup. Needs the
            ``GL_ARB_fragment_program`` extension and at least four texture
            units. Provides brightness, contrast, saturation, hue and gamma
            control. Gamma can also be set independently for red, green and
            blue.
        5
            Use ATI-specific method (for older cards). This uses an
            ATI-specific extension (``GL_ATI_fragment_shader`` - not
            ``GL_ARB_fragment_shader``!). At least three texture units are
            needed. Provides saturation and hue control. This method is fast
            but inexact.
        6
            Use a 3D texture to do conversion via lookup. Needs the
            ``GL_ARB_fragment_program extension`` and at least four texture
            units. Extremely slow (software emulation) on some (all?) ATI
            cards since it uses a texture with border pixels. Provides
            brightness, contrast, saturation, hue and gamma control. Gamma can
            also be set independently for red, green and blue. Speed depends
            more on GPU memory bandwidth than other methods.

    ``lscale=<n>``
        Select the scaling function to use for luminance scaling. Only valid
        for yuv modes 2, 3, 4 and 6.

        0
            Use simple linear filtering (default).
        1
            Use bicubic B-spline filtering (better quality). Needs one
            additional texture unit. Older cards will not be able to handle
            this for chroma at least in fullscreen mode.
        2
            Use cubic filtering in horizontal, linear filtering in vertical
            direction. Works on a few more cards than method 1.
        3
            Same as 1 but does not use a lookup texture. Might be faster on
            some cards.
        4
            Use experimental unsharp masking with 3x3 support and a default
            strength of 0.5 (see ``filter-strength``).
        5
            Use experimental unsharp masking with 5x5 support and a default
            strength of 0.5 (see ``filter-strength``).

    ``cscale=<n>``
        Select the scaling function to use for chrominance scaling. For
        details see ``lscale``.
    ``filter-strength=<value>``
        Set the effect strength for the ``lscale``/``cscale`` filters that
        support it.
    ``stereo=<value>``
        Select a method for stereo display. You may have to use ``--aspect`` to
        fix the aspect value. Experimental, do not expect too much from it.

        0
            Normal 2D display
        1
            Convert side by side input to full-color red-cyan stereo.
        2
            Convert side by side input to full-color green-magenta stereo.
        3
            Convert side by side input to quadbuffered stereo. Only supported
            by very few OpenGL cards.

    The following options are only useful if writing your own fragment programs.

    ``customprog=<filename>``
        Load a custom fragment program from ``<filename>``.
    ``customtex=<filename>``
        Load a custom "gamma ramp" texture from ``<filename>``. This can be used
        in combination with ``yuv=4`` or with the ``customprog`` option.
    ``(no-)customtlin``
        If enabled (default) use ``GL_LINEAR`` interpolation, otherwise use
        ``GL_NEAREST`` for customtex texture.
    ``(no-)customtrect``
        If enabled, use ``texture_rectangle`` for the ``customtex`` texture.
        Default is disabled.
    ``(no-)mipmapgen``
        If enabled, mipmaps for the video are automatically generated. This
        should be useful together with the ``customprog`` and the ``TXB``
        instruction to implement blur filters with a large radius. For most
        OpenGL implementations, this is very slow for any non-RGB formats.
        Default is disabled.

    Normally there is no reason to use the following options; they mostly
    exist for testing purposes.

    ``(no-)glfinish``
        Call ``glFinish()`` before swapping buffers. Slower but in some cases
        more correct output (default: disabled).
    ``(no-)manyfmts``
        Enables support for more (RGB and BGR) color formats (default: enabled).
        Needs OpenGL version >= 1.2.
    ``slice-height=<0-...>``
        Number of lines copied to texture in one piece (default: 0). 0 for
        whole image.
    ``sw``
        Continue even if a software renderer is detected.

    ``backend=<sys>``
        auto
            auto-select (default)
        cocoa
            Cocoa/OSX
        win
            Win32/WGL
        x11
            X11/GLX
        wayland
            Wayland/EGL

``sdl``
    SDL 2.0+ Render video output driver, depending on system with or without
    hardware acceleration. Should work on all platforms supported by SDL 2.0.
    For tuning, refer to your copy of the file ``SDL_hints.h``.

    ``sw``
        Continue even if a software renderer is detected.

    ``switch-mode``
        Instruct SDL to switch the monitor video mode when going fullscreen.

``vaapi``
    Intel VA API video output driver with support for hardware decoding. Note
    that there is absolutely no reason to use this, other than wanting to use
    hardware decoding to save power on laptops, or possibly preventing video
    tearing with some setups.

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

        no
            Don't allow deinterlacing.
        first-field
            Show only first field (going by ``--field-dominance``).
        bob
            bob deinterlacing (default).

    ``scaled-osd=<yes|no>``
        If enabled, then the OSD is rendered at video resolution and scaled to
        display resolution. By default, this is disabled, and the OSD is
        rendered at display resolution if the driver supports it.

``null``
    Produces no video output. Useful for benchmarking.

``caca``
    Color ASCII art video output driver that works on a text console.

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

    ``default-format``
        Use the default RGB32 format instead of an auto-detected one.

    ``alpha``
        Use a buffer format that supports videos and images with alpha
        informations
