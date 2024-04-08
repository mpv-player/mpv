VIDEO OUTPUT DRIVERS
====================

Video output drivers are interfaces to different video output facilities. The
syntax is:

``--vo=<driver1,driver2,...[,]>``
    Specify a priority list of video output drivers to be used.

If the list has a trailing ``,``, mpv will fall back on drivers not contained
in the list.

.. note::

    See ``--vo=help`` for a list of compiled-in video output drivers.

    The recommended output driver is ``--vo=gpu``, which is the default. All
    other drivers are for compatibility or special purposes. If the default
    does not work, it will fallback to other drivers (in the same order as
    listed by ``--vo=help``).

Available video output drivers are:

``gpu``
    General purpose, customizable, GPU-accelerated video output driver. It
    supports extended scaling methods, dithering, color management, custom
    shaders, HDR, and more.

    See `GPU renderer options`_ for options specific to this VO.

    By default, mpv utilizes settings that balance quality and performance.
    Additionally, two predefined profiles are available: ``fast`` for maximum
    performance and ``high-quality`` for superior rendering quality. You can
    apply a specific profile using the ``--profile=<name>`` option and inspect
    its contents using ``--show-profile=<name>``.

    This VO abstracts over several possible graphics APIs and windowing
    contexts, which can be influenced using the ``--gpu-api`` and
    ``--gpu-context`` options.

    Hardware decoding over OpenGL-interop is supported to some degree. Note
    that in this mode, some corner case might not be gracefully handled, and
    color space conversion and chroma upsampling is generally in the hand of
    the hardware decoder APIs.

    ``gpu`` makes use of FBOs by default. Sometimes you can achieve better
    quality or performance by changing the ``--fbo-format`` option to
    ``rgb16f``, ``rgb32f`` or ``rgb``. Known problems include Mesa/Intel not
    accepting ``rgb16``, Mesa sometimes not being compiled with float texture
    support, and some macOS setups being very slow with ``rgb16`` but fast
    with ``rgb32f``. If you have problems, you can also try enabling the
    ``--gpu-dumb-mode=yes`` option.

``gpu-next``
    Experimental video renderer based on ``libplacebo``. This supports almost
    the same set of features as ``--vo=gpu``. See `GPU renderer options`_ for a
    list.

    Should generally be faster and higher quality, but some features may still
    be missing or misbehave. Expect (and report!) bugs. See here for a list of
    known differences and bugs:

    https://github.com/mpv-player/mpv/wiki/GPU-Next-vs-GPU

``xv`` (X11 only)
    Uses the XVideo extension to enable hardware-accelerated display. This is
    the most compatible VO on X, but may be low-quality, and has issues with
    OSD and subtitle display.

    .. note:: This driver is for compatibility with old systems.

    The following global options are supported by this video output:

    ``--xv-adaptor=<number>``
        Select a specific XVideo adapter (check xvinfo results).
    ``--xv-port=<number>``
        Select a specific XVideo port.
    ``--xv-ck=<cur|use|set>``
        Select the source from which the color key is taken (default: cur).

        cur
          The default takes the color key currently set in Xv.
        use
          Use but do not set the color key from mpv (use the ``--colorkey``
          option to change it).
        set
          Same as use but also sets the supplied color key.

    ``--xv-ck-method=<none|man|bg|auto>``
        Sets the color key drawing method (default: man).

        none
          Disables color-keying.
        man
          Draw the color key manually (reduces flicker in some cases).
        bg
          Set the color key as window background.
        auto
          Let Xv draw the color key.

    ``--xv-colorkey=<number>``
        Changes the color key to an RGB value of your choice. ``0x000000`` is
        black and ``0xffffff`` is white.

    ``--xv-buffers=<number>``
        Number of image buffers to use for the internal ringbuffer (default: 2).
        Increasing this will use more memory, but might help with the X server
        not responding quickly enough if video FPS is close to or higher than
        the display refresh rate.

``x11`` (X11 only)
    Shared memory video output driver without hardware acceleration that works
    whenever X11 is present.

    Since mpv 0.30.0, you may need to use ``--profile=sw-fast`` to get decent
    performance.

    .. note:: This is a fallback only, and should not be normally used.

``vdpau`` (X11 only)
    Uses the VDPAU interface to display and optionally also decode video.
    Hardware decoding is used with ``--hwdec=vdpau``. Note that there is
    absolutely no reason to use this, other than compatibility. We strongly
    recommend that you use ``--vo=gpu`` with ``--hwdec=nvdec`` instead.

    .. note::

        Earlier versions of mpv (and MPlayer, mplayer2) provided sub-options
        to tune vdpau post-processing, like ``deint``, ``sharpen``, ``denoise``,
        ``chroma-deint``, ``pullup``, ``hqscaling``. These sub-options are
        deprecated, and you should use the ``vdpaupp`` video filter instead.

    The following global options are supported by this video output:

    ``--vo-vdpau-sharpen=<-1-1>``
        (Deprecated. See note about ``vdpaupp``.)

        For positive values, apply a sharpening algorithm to the video, for
        negative values a blurring algorithm (default: 0).
    ``--vo-vdpau-denoise=<0-1>``
        (Deprecated. See note about ``vdpaupp``.)

        Apply a noise reduction algorithm to the video (default: 0; no noise
        reduction).
    ``--vo-vdpau-chroma-deint``
        (Deprecated. See note about ``vdpaupp``.)

        Makes temporal deinterlacers operate both on luma and chroma (default).
        Use no-chroma-deint to solely use luma and speed up advanced
        deinterlacing. Useful with slow video memory.
    ``--vo-vdpau-pullup``
        (Deprecated. See note about ``vdpaupp``.)

        Try to apply inverse telecine, needs motion adaptive temporal
        deinterlacing.
    ``--vo-vdpau-hqscaling=<0-9>``
        (Deprecated. See note about ``vdpaupp``.)

        0
            Use default VDPAU scaling (default).
        1-9
            Apply high quality VDPAU scaling (needs capable hardware).
    ``--vo-vdpau-fps=<number>``
        Override autodetected display refresh rate value (the value is needed
        for framedrop to allow video playback rates higher than display
        refresh rate, and for vsync-aware frame timing adjustments). Default 0
        means use autodetected value. A positive value is interpreted as a
        refresh rate in Hz and overrides the autodetected value. A negative
        value disables all timing adjustment and framedrop logic.
    ``--vo-vdpau-composite-detect``
        NVIDIA's current VDPAU implementation behaves somewhat differently
        under a compositing window manager and does not give accurate frame
        timing information. With this option enabled, the player tries to
        detect whether a compositing window manager is active. If one is
        detected, the player disables timing adjustments as if the user had
        specified ``fps=-1`` (as they would be based on incorrect input). This
        means timing is somewhat less accurate than without compositing, but
        with the composited mode behavior of the NVIDIA driver, there is no
        hard playback speed limit even without the disabled logic. Enabled by
        default, use ``--vo-vdpau-composite-detect=no`` to disable.
    ``--vo-vdpau-queuetime-windowed=<number>`` and ``queuetime-fs=<number>``
        Use VDPAU's presentation queue functionality to queue future video
        frame changes at most this many milliseconds in advance (default: 50).
        See below for additional information.
    ``--vo-vdpau-output-surfaces=<2-15>``
        Allocate this many output surfaces to display video frames (default:
        3). See below for additional information.
    ``--vo-vdpau-colorkey=<#RRGGBB|#AARRGGBB>``
        Set the VDPAU presentation queue background color, which in practice
        is the colorkey used if VDPAU operates in overlay mode (default:
        ``#020507``, some shade of black). If the alpha component of this value
        is 0, the default VDPAU colorkey will be used instead (which is usually
        green).
    ``--vo-vdpau-force-yuv``
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

``direct3d`` (Windows only)
    Video output driver that uses the Direct3D interface.

    .. note:: This driver is for compatibility with systems that don't provide
              proper OpenGL drivers, and where ANGLE does not perform well.

    The following global options are supported by this video output:

    ``--vo-direct3d-disable-texture-align``
        Normally texture sizes are always aligned to 16. With this option
        enabled, the video texture will always have exactly the same size as
        the video itself.


    Debug options. These might be incorrect, might be removed in the future,
    might crash, might cause slow downs, etc. Contact the developers if you
    actually need any of these for performance or proper operation.

    ``--vo-direct3d-force-power-of-2``
        Always force textures to power of 2, even if the device reports
        non-power-of-2 texture sizes as supported.

    ``--vo-direct3d-texture-memory=<mode>``
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

    ``--vo-direct3d-swap-discard``
        Use ``D3DSWAPEFFECT_DISCARD``, which might be faster.
        Might be slower too, as it must(?) clear every frame.

    ``--vo-direct3d-exact-backbuffer``
        Always resize the backbuffer to window size.

``sdl``
    SDL 2.0+ Render video output driver, depending on system with or without
    hardware acceleration. Should work on all platforms supported by SDL 2.0.
    For tuning, refer to your copy of the file ``SDL_hints.h``.

    .. note:: This driver is for compatibility with systems that don't provide
              proper graphics drivers.

    The following global options are supported by this video output:

    ``--sdl-sw``
        Continue even if a software renderer is detected.

    ``--sdl-switch-mode``
        Instruct SDL to switch the monitor video mode when going fullscreen.

``dmabuf-wayland``
    Experimental Wayland output driver designed for use with either drm stateless
    or VA API hardware decoding. The driver is designed to avoid any GPU to CPU copies,
    and to perform scaling and color space conversion using fixed-function hardware,
    if available, rather than GPU shaders. This frees up GPU resources for other tasks.
    It is highly recommended to use this VO with the appropriate ``--hwdec`` option such
    as ``auto-safe``. It can still work in some circumstances without ``--hwdec`` due to
    mpv's internal conversion filters, but this is not recommended as it's a needless
    extra step. Correct output depends on support from your GPU, drivers, and compositor.
    Weston and wlroots-based compositors like Sway and Intel GPUs are known to generally
    work.

``vaapi``
    Intel VA API video output driver with support for hardware decoding. Note
    that there is absolutely no reason to use this, other than compatibility.
    This is low quality, and has issues with OSD. We strongly recommend that
    you use ``--vo=gpu`` with ``--hwdec=vaapi`` instead.

    The following global options are supported by this video output:

    ``--vo-vaapi-scaling=<algorithm>``
        default
            Driver default (mpv default as well).
        fast
            Fast, but low quality.
        hq
            Unspecified driver dependent high-quality scaling, slow.
        nla
            ``non-linear anamorphic scaling``

    ``--vo-vaapi-scaled-osd=<yes|no>``
        If enabled, then the OSD is rendered at video resolution and scaled to
        display resolution. By default, this is disabled, and the OSD is
        rendered at display resolution if the driver supports it.

``null``
    Produces no video output. Useful for benchmarking.

    Usually, it's better to disable video with ``--video=no`` instead.

    The following global options are supported by this video output:

    ``--vo-null-fps=<value>``
        Simulate display FPS. This artificially limits how many frames the
        VO accepts per second.

``caca``
    Color ASCII art video output driver that works on a text console.

    This driver reserves some keys for runtime configuration. These keys are
    hardcoded and cannot be bound:

    d and D
        Toggle dithering algorithm.

    a and A
        Toggle antialiasing method.

    h and H
        Toggle charset method.

    c and C
        Toggle color method.

    .. note:: This driver is a joke.

``tct``
    Color Unicode art video output driver that works on a text console.
    By default depends on support of true color by modern terminals to display
    the images at full color range, but 256-colors output is also supported (see
    below). On Windows it requires an ansi terminal such as mintty.

    Since mpv 0.30.0, you may need to use ``--profile=sw-fast`` to get decent
    performance.

    Note: the TCT image output is not synchronized with other terminal output
    from mpv, which can lead to broken images. The options ``--terminal=no`` or
    ``--really-quiet`` can help with that.

    ``--vo-tct-algo=<algo>``
        Select how to write the pixels to the terminal.

        half-blocks
            Uses unicode LOWER HALF BLOCK character to achieve higher vertical
            resolution. (Default.)
        plain
            Uses spaces. Causes vertical resolution to drop twofolds, but in
            theory works in more places.

    ``--vo-tct-buffering=<pixel|line|frame>``
        Specifies the size of data batches buffered before being sent to the
        terminal.

        TCT image output is not synchronized with other terminal output from mpv,
        which can lead to broken images. Sending data to the terminal in small
        batches may improve parallelism between terminal processing and mpv
        processing but incurs a static overhead of generating tens of thousands
        of small writes. Also, depending on the terminal used, sending frames in
        one chunk might help with tearing of the output, especially if not used
        with ``--really-quiet`` and other logs interrupt the data stream.

        pixel
            Send data to terminal for each pixel.
        line
            Send data to terminal for each line. (Default)
        frame
            Send data to terminal for each frame.

    ``--vo-tct-width=<width>``  ``--vo-tct-height=<height>``
        Assume the terminal has the specified character width and/or height.
        These default to 80x25 if the terminal size cannot be determined.

    ``--vo-tct-256=<yes|no>`` (default: no)
        Use 256 colors - for terminals which don't support true color.

``kitty``
    Graphical output for the terminal, using the kitty graphics protocol.
    Tested with kitty and Konsole.

    You may need to use ``--profile=sw-fast`` to get decent performance.

    Kitty size and alignment options:

    ``--vo-kitty-cols=<columns>``, ``--vo-kitty-rows=<rows>`` (default: 0)
        Specify the terminal size in character cells, otherwise (0) read it
        from the terminal, or fall back to 80x25.

    ``--vo-kitty-width=<width>``, ``--vo-kitty-height=<height>`` (default: 0)
        Specify the available size in pixels, otherwise (0) read it from the
        terminal, or fall back to 320x240.

    ``--vo-kitty-left=<col>``, ``--vo-kitty-top=<row>`` (default: 0)
        Specify the position in character cells where the image starts (1 is
        the first column or row). If 0 (default) then try to automatically
        determine it according to the other values and the image aspect ratio
        and zoom.

    ``--vo-kitty-config-clear=<yes|no>`` (default: yes)
        Whether or not to clear the terminal whenever the output is
        reconfigured (e.g. when video size changes).

    ``--vo-kitty-alt-screen=<yes|no>`` (default: yes)
        Whether or not to use the alternate screen buffer and return the
        terminal to its previous state on exit. When set to no, the last
        kitty image stays on screen after quit, with the cursor following it.

    ``--vo-kitty-use-shm=<yes|no>`` (default: no)
        Use shared memory objects to transfer image data to the terminal.
        This is much faster than sending the data as escape codes, but is not
        supported by as many terminals. It also only works on the local machine
        and not via e.g. SSH connections.

        This option is not implemented on Windows.

``sixel``
    Graphical output for the terminal, using sixels. Tested with ``mlterm`` and
    ``xterm``.

    Note: the Sixel image output is not synchronized with other terminal
    output from mpv, which can lead to broken images.
    The option ``--really-quiet`` can help with that, and is recommended.
    On some platforms, using the ``--vo-sixel-buffered`` option may work as
    well.

    You may need to use ``--profile=sw-fast`` to get decent performance.

    Note: at the time of writing, ``xterm`` does not enable sixel by default -
    launching it as ``xterm -ti 340`` is one way to enable it. Also, ``xterm``
    does not display images bigger than 1000x1000 pixels by default.

    To render and align sixel images correctly, mpv needs to know the terminal
    size both in cells and in pixels. By default it tries to use values which
    the terminal reports, however, due to differences between terminals this is
    an error-prone process which cannot be automated with certainty - some
    terminals report the size in pixels including the padding - e.g. ``xterm``,
    while others report the actual usable number of pixels - like ``mlterm``.
    Additionally, they may behave differently when maximized or in fullscreen,
    and mpv cannot detect this state using standard methods.

    Sixel size and alignment options:

    ``--vo-sixel-cols=<columns>``, ``--vo-sixel-rows=<rows>`` (default: 0)
        Specify the terminal size in character cells, otherwise (0) read it
        from the terminal, or fall back to 80x25. Note that mpv doesn't use the
        the last row with sixel because this seems to result in scrolling.

    ``--vo-sixel-width=<width>``, ``--vo-sixel-height=<height>`` (default: 0)
        Specify the available size in pixels, otherwise (0) read it from the
        terminal, or fall back to 320x240. Other than excluding the last line,
        the height is also further rounded down to a multiple of 6 (sixel unit
        height) to avoid overflowing below the designated size.

    ``--vo-sixel-left=<col>``, ``--vo-sixel-top=<row>`` (default: 0)
        Specify the position in character cells where the image starts (1 is
        the first column or row). If 0 (default) then try to automatically
        determine it according to the other values and the image aspect ratio
        and zoom.

    ``--vo-sixel-pad-x=<pad_x>``, ``--vo-sixel-pad-y=<pad_y>`` (default: -1)
        Used only when mpv reads the size in pixels from the terminal.
        Specify the number of padding pixels (on one side) which are included
        at the size which the terminal reports. If -1 (default) then the number
        of pixels is rounded down to a multiple of number of cells (per axis),
        to take into account padding at the report - this only works correctly
        when the overall padding per axis is smaller than the number of cells.

    ``--vo-sixel-config-clear=<yes|no>`` (default: yes)
        Whether or not to clear the terminal whenever the output is
        reconfigured (e.g. when video size changes).

    ``--vo-sixel-alt-screen=<yes|no>`` (default: yes)
        Whether or not to use the alternate screen buffer and return the
        terminal to its previous state on exit. When set to no, the last
        sixel image stays on screen after quit, with the cursor following it.

        ``--vo-sixel-exit-clear`` is a deprecated alias for this option and
        may be removed in the future.

    ``--vo-sixel-buffered=<yes|no>`` (default: no)
        Buffers the full output sequence before writing it to the terminal.
        On POSIX platforms, this can help prevent interruption (including from
        other applications) and thus broken images, but may come at a
        performance cost with some terminals and is subject to implementation
        details.

    Sixel image quality options:

    ``--vo-sixel-dither=<algo>``
        Selects the dither algorithm which libsixel should apply.
        Can be one of the below list as per libsixel's documentation.

        auto (Default)
            Let libsixel choose the dithering method.
        none
            Don't diffuse
        atkinson
            Diffuse with Bill Atkinson's method.
        fs
            Diffuse with Floyd-Steinberg method
        jajuni
            Diffuse with Jarvis, Judice & Ninke method
        stucki
            Diffuse with Stucki's method
        burkes
            Diffuse with Burkes' method
        arithmetic
            Positionally stable arithmetic dither
        xor
            Positionally stable arithmetic xor based dither

    ``--vo-sixel-fixedpalette=<yes|no>`` (default: yes)
        Use libsixel's built-in static palette using the XTERM256 profile
        for dither. Fixed palette uses 256 colors for dithering. Note that
        using ``no`` (at the time of writing) will slow down ``xterm``.

    ``--vo-sixel-reqcolors=<colors>`` (default: 256)
        Has no effect with fixed palette. Set up libsixel to use required
        number of colors for dynamic palette. This value depends on the
        terminal emulator as well. Xterm supports 256 colors. Can set this to
        a lower value for faster performance.

    ``--vo-sixel-threshold=<threshold>`` (default: -1)
        Has no effect with fixed palette. Defines the threshold to change the
        palette - as percentage of the number of colors, e.g. 20 will change
        the palette when the number of colors changed by 20%. It's a simple
        measure to reduce the number of palette changes, because it can be slow
        in some terminals (``xterm``). The default (-1) will choose a palette
        on every frame and will have better quality.

``image``
    Output each frame into an image file in the current directory. Each file
    takes the frame number padded with leading zeros as name.

    The following global options are supported by this video output:

    ``--vo-image-format=<format>``
        Select the image file format.

        jpg
            JPEG files, extension .jpg. (Default.)
        jpeg
            JPEG files, extension .jpeg.
        png
            PNG files.
        webp
            WebP files.

    ``--vo-image-png-compression=<0-9>``
        PNG compression factor (speed vs. file size tradeoff) (default: 7)
    ``--vo-image-png-filter=<0-5>``
        Filter applied prior to PNG compression (0 = none; 1 = sub; 2 = up;
        3 = average; 4 = Paeth; 5 = mixed) (default: 5)
    ``--vo-image-jpeg-quality=<0-100>``
        JPEG quality factor (default: 90)
    ``--vo-image-jpeg-optimize=<0-100>``
        JPEG optimization factor (default: 100)
    ``--vo-image-webp-lossless=<yes|no>``
        Enable writing lossless WebP files (default: no)
    ``--vo-image-webp-quality=<0-100>``
        WebP quality (default: 75)
    ``--vo-image-webp-compression=<0-6>``
        WebP compression factor (default: 4)
    ``--vo-image-outdir=<dirname>``
        Specify the directory to save the image files to (default: ``./``).

``libmpv``
    For use with libmpv direct embedding. As a special case, on macOS it
    is used like a normal VO within mpv (cocoa-cb). Otherwise useless in any
    other contexts.
    (See ``<mpv/render.h>``.)

    This also supports many of the options the ``gpu`` VO has, depending on the
    backend.

``drm`` (Direct Rendering Manager)
    Video output driver using Kernel Mode Setting / Direct Rendering Manager.
    Should be used when one doesn't want to install full-blown graphical
    environment (e.g. no X). Does not support hardware acceleration (if you
    need this, check the ``drm`` backend for ``gpu`` VO).

    Since mpv 0.30.0, you may need to use ``--profile=sw-fast`` to get decent
    performance.

    The following global options are supported by this video output:

    ``--drm-connector=<name>``
        Select the connector to use (usually this is a monitor.) If ``<name>``
        is empty or ``auto``, mpv renders the output on the first available
        connector. Use ``--drm-connector=help`` to get a list of available
        connectors. (default: empty)

    ``--drm-device=<path>``
        Select the DRM device file to use. If specified this overrides automatic
        card selection. (default: empty)

    ``--drm-mode=<preferred|highest|N|WxH[@R]>``
        Mode to use (resolution and frame rate).
        Possible values:

        :preferred: Use the preferred mode for the screen on the selected
                    connector. (default)
        :highest:   Use the mode with the highest resolution available on the
                    selected connector.
        :N:         Select mode by index.
        :WxH[@R]:   Specify mode by width, height, and optionally refresh rate.
                    In case several modes match, selects the mode that comes
                    first in the EDID list of modes.

        Use ``--drm-mode=help`` to get a list of available modes for all active
        connectors.

    ``--drm-draw-plane=<primary|overlay|N>``
        Select the DRM plane to which video and OSD is drawn to, under normal
        circumstances. The plane can be specified as ``primary``, which will
        pick the first applicable primary plane; ``overlay``, which will pick
        the first applicable overlay plane; or by index. The index is zero
        based, and related to the CRTC.
        (default: primary)

        When using this option with the drmprime-overlay hwdec interop, only the
        OSD is rendered to this plane.

    ``--drm-drmprime-video-plane=<primary|overlay|N>``
        Select the DRM plane to use for video with the drmprime-overlay hwdec
        interop (used by e.g. the rkmpp hwdec on RockChip SoCs, and v4l2 hwdec:s
        on various other SoC:s). The plane is unused otherwise. This option
        accepts the same values as ``--drm-draw-plane``. (default: overlay)

        To be able to successfully play 4K video on various SoCs you might need
        to set ``--drm-draw-plane=overlay --drm-drmprime-video-plane=primary``
        and setting ``--drm-draw-surface-size=1920x1080``, to render the OSD at a
        lower resolution (the video when handled by the hwdec will be on the
        drmprime-video plane and at full 4K resolution)

    ``--drm-format=<xrgb8888|xbgr8888|xrgb2101010|xbgr2101010|yuyv>``
        Select the DRM format to use (default: xrgb8888). This allows you to
        choose the bit depth and color type of the DRM mode.

        xrgb8888 is your usual 24bpp packed RGB format with 8 bits of padding.
        xrgb2101010 is a 30bpp packed RGB format with 2 bits of padding.
        yuyv is a 32bpp packed YUV 4:2:2 format. No planar formats are currently
        supported.

        There are cases when xrgb2101010 will work with the ``drm`` VO, but not
        with the ``drm`` backend for the ``gpu`` VO. This is because with the
        ``gpu`` VO, in addition to requiring support in your DRM driver,
        requires support for xrgb2101010 in your EGL driver.
        yuyv only ever works with the ``drm`` VO.

    ``--drm-draw-surface-size=<[WxH]>``
        Sets the size of the surface used on the draw plane. The surface will
        then be upscaled to the current screen resolution. This option can be
        useful when used together with the drmprime-overlay hwdec interop at
        high resolutions, as it allows scaling the draw plane (which in this
        case only handles the OSD) down to a size the GPU can handle.

        When used without the drmprime-overlay hwdec interop this option will
        just cause the video to get rendered at a different resolution and then
        scaled to screen size.

        (default: display resolution)

    ``--drm-vrr-enabled=<no|yes|auto>``
        Toggle use of Variable Refresh Rate (VRR), aka Freesync or Adaptive Sync
        on compatible systems. VRR allows for the display to be refreshed at any
        rate within a range (usually ~40Hz-60Hz for 60Hz displays). This can help
        with playback of 24/25/50fps content. Support depends on the use of a
        compatible monitor, GPU, and a sufficiently new kernel with drivers
        that support the feature.

        :no:    Do not attempt to enable VRR. (default)
        :yes:   Attempt to enable VRR, whether the capability is reported or not.
        :auto:  Attempt to enable VRR if support is reported.

``mediacodec_embed`` (Android)
    Renders ``IMGFMT_MEDIACODEC`` frames directly to an ``android.view.Surface``.
    Requires ``--hwdec=mediacodec`` for hardware decoding, along with
    ``--vo=mediacodec_embed`` and ``--wid=(intptr_t)(*android.view.Surface)``.

    Since this video output driver uses native decoding and rendering routines,
    many of mpv's features (subtitle rendering, OSD/OSC, video filters, etc)
    are not available with this driver.

    To use hardware decoding with ``--vo=gpu`` instead, use ``--hwdec=mediacodec``
    or ``mediacodec-copy`` along with ``--gpu-context=android``.

``wlshm`` (Wayland only)
    Shared memory video output driver without hardware acceleration that works
    whenever Wayland is present.

    Since mpv 0.30.0, you may need to use ``--profile=sw-fast`` to get decent
    performance.

    .. note:: This is a fallback only, and should not be normally used.
