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

    .. note:: This is a fallback only, and should not be normally used.

``vdpau`` (X11 only)
    Uses the VDPAU interface to display and optionally also decode video.
    Hardware decoding is used with ``--hwdec=vdpau``.

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
    ``--vo-vdpau-deint=<-4-4>``
        (Deprecated. See note about ``vdpaupp``.)

        Select deinterlacing mode (default: 0). In older versions (as well as
        MPlayer/mplayer2) you could use this option to enable deinterlacing.
        This doesn't work anymore, and deinterlacing is enabled with either
        the ``d`` key (by default mapped to the command ``cycle deinterlace``),
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

    .. note:: Before to 0.21.0, ``direct3d_shaders`` and ``direct3d`` were
              different, with ``direct3d`` not using shader by default. Now
              both use shaders by default, and ``direct3d_shaders`` is a
              deprecated alias. Use the ``--vo-direct3d-prefer-stretchrect``
              or the ``--vo-direct3d-disable-shaders`` options to get the old
              behavior of ``direct3d``.

    The following global options are supported by this video output:

    ``--vo-direct3d-prefer-stretchrect``
        Use ``IDirect3DDevice9::StretchRect`` over other methods if possible.

    ``--vo-direct3d-disable-stretchrect``
        Never render the video using ``IDirect3DDevice9::StretchRect``.

    ``--vo-direct3d-disable-textures``
        Never render the video using D3D texture rendering. Rendering with
        textures + shader will still be allowed. Add ``disable-shaders`` to
        completely disable video rendering with textures.

    ``--vo-direct3d-disable-shaders``
        Never use shaders when rendering video.

    ``--vo-direct3d-only-8bit``
        Never render YUV video with more than 8 bits per component.
        Using this flag will force software conversion to 8-bit.

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

``gpu``
    General purpose, customizable, GPU-accelerated video output driver. It
    supports extended scaling methods, dithering, color management, custom
    shaders, HDR, and more.

    See `GPU renderer options`_ for options specific to this VO.

    By default, it tries to use fast and fail-safe settings. Use the
    ``gpu-hq`` profile to use this driver with defaults set to high quality
    rendering. The profile can be applied with ``--profile=gpu-hq`` and its
    contents can be viewed with ``--show-profile=gpu-hq``.

    This VO abstracts over several possible graphics APIs and windowing
    contexts, which can be influenced using the ``--gpu-api`` and
    ``--gpu-context`` options.

    Hardware decoding over OpenGL-interop is supported to some degree. Note
    that in this mode, some corner case might not be gracefully handled, and
    color space conversion and chroma upsampling is generally in the hand of
    the hardware decoder APIs.

    ``gpu`` makes use of FBOs by default. Sometimes you can achieve better
    quality or performance by changing the ``--gpu-fbo-format`` option to
    ``rgb16f``, ``rgb32f`` or ``rgb``. Known problems include Mesa/Intel not
    accepting ``rgb16``, Mesa sometimes not being compiled with float texture
    support, and some OS X setups being very slow with ``rgb16`` but fast
    with ``rgb32f``. If you have problems, you can also try enabling the
    ``--gpu-dumb-mode=yes`` option.

``sdl``
    SDL 2.0+ Render video output driver, depending on system with or without
    hardware acceleration. Should work on all platforms supported by SDL 2.0.
    For tuning, refer to your copy of the file ``SDL_hints.h``.

    .. note:: This driver is for compatibility with systems that don't provide
              proper graphics drivers, or which support GLES only.

    The following global options are supported by this video output:

    ``--sdl-sw``
        Continue even if a software renderer is detected.

    ``--sdl-switch-mode``
        Instruct SDL to switch the monitor video mode when going fullscreen.

``vaapi``
    Intel VA API video output driver with support for hardware decoding. Note
    that there is absolutely no reason to use this, other than compatibility.
    This is low quality, and has issues with OSD.

    .. note:: This driver is for compatibility with crappy systems. You can
              use vaapi hardware decoding with ``--vo=gpu`` too.

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

    ``--vo-vaapi-deint-mode=<mode>``
        Select deinterlacing algorithm. Note that by default deinterlacing is
        initially always off, and needs to be enabled with the ``d`` key
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
            Show only first field.
        bob
            bob deinterlacing (default for older libva).

    ``--vo-vaapi-scaled-osd=<yes|no>``
        If enabled, then the OSD is rendered at video resolution and scaled to
        display resolution. By default, this is disabled, and the OSD is
        rendered at display resolution if the driver supports it.

``null``
    Produces no video output. Useful for benchmarking.

    Usually, it's better to disable video with ``--no-video`` instead.

    The following global options are supported by this video output:

    ``--vo-null-fps=<value>``
        Simulate display FPS. This artificially limits how many frames the
        VO accepts per second.

``caca``
    Color ASCII art video output driver that works on a text console.

    .. note:: This driver is a joke.

``tct``
    Color Unicode art video output driver that works on a text console.
    Depends on support of true color by modern terminals to display the images
    at full color range. On Windows it requires an ansi terminal such as mintty.

    ``--vo-tct-algo=<algo>``
        Select how to write the pixels to the terminal.

        half-blocks
            Uses unicode LOWER HALF BLOCK character to achieve higher vertical
            resolution. (Default.)
        plain
            Uses spaces. Causes vertical resolution to drop twofolds, but in
            theory works in more places.

    ``--vo-tct-width=<width>``  ``--vo-tct-height=<height>``
        Assume the terminal has the specified character width and/or height.
        These default to 80x25 if the terminal size cannot be determined.

    ``--vo-tct-256=<yes|no>`` (default: no)
        Use 256 colors - for terminals which don't support true color.

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

    ``--vo-image-png-compression=<0-9>``
        PNG compression factor (speed vs. file size tradeoff) (default: 7)
    ``--vo-image-png-filter=<0-5>``
        Filter applied prior to PNG compression (0 = none; 1 = sub; 2 = up;
        3 = average; 4 = Paeth; 5 = mixed) (default: 5)
    ``--vo-image-jpeg-quality=<0-100>``
        JPEG quality factor (default: 90)
    ``--vo-image-jpeg-optimize=<0-100>``
        JPEG optimization factor (default: 100)
    ``--vo-image-outdir=<dirname>``
        Specify the directory to save the image files to (default: ``./``).

``libmpv``
    For use with libmpv direct embedding. As a special case, on OS X it
    is used like a normal VO within mpv (cocoa-cb). Otherwise useless in any
    other contexts.
    (See ``<mpv/render.h>``.)

    This also supports many of the options the ``gpu`` VO has, depending on the
    backend.

``rpi`` (Raspberry Pi)
    Native video output on the Raspberry Pi using the MMAL API.

    This is deprecated. Use ``--vo=gpu`` instead, which is the default and
    provides the same functionality. The ``rpi`` VO will be removed in
    mpv 0.23.0. Its functionality was folded into --vo=gpu, which now uses
    RPI hardware decoding by treating it as a hardware overlay (without applying
    GL filtering). Also to be changed in 0.23.0: the --fs flag will be reset to
    "no" by default (like on the other platforms).

    The following deprecated global options are supported by this video output:

    ``--rpi-display=<number>``
        Select the display number on which the video overlay should be shown
        (default: 0).

    ``--rpi-layer=<number>``
        Select the dispmanx layer on which the video overlay should be shown
        (default: -10). Note that mpv will also use the 2 layers above the
        selected layer, to handle the window background and OSD. Actual video
        rendering will happen on the layer above the selected layer.

    ``--rpi-background=<yes|no>``
        Whether to render a black background behind the video (default: no).
        Normally it's better to kill the console framebuffer instead, which
        gives better performance.

    ``--rpi-osd=<yes|no>``
        Enabled by default. If disabled with ``no``, no OSD layer is created.
        This also means there will be no subtitles rendered.

``drm`` (Direct Rendering Manager)
    Video output driver using Kernel Mode Setting / Direct Rendering Manager.
    Should be used when one doesn't want to install full-blown graphical
    environment (e.g. no X). Does not support hardware acceleration (if you
    need this, check the ``drm`` backend for ``gpu`` VO).

    The following global options are supported by this video output:

    ``--drm-connector=[<gpu_number>.]<name>``
        Select the connector to use (usually this is a monitor.) If ``<name>``
        is empty or ``auto``, mpv renders the output on the first available
        connector. Use ``--drm-connector=help`` to get list of available
        connectors. When using multiple graphic cards, use the ``<gpu_number>``
        argument to disambiguate.
        (default: empty)

    ``--drm-mode=<number>``
        Mode ID to use (resolution and frame rate).
        (default: 0)

    ``--drm-osd-plane-id=<number>``
        Select the DRM plane index to use for OSD (or OSD and video).
        Index is zero based, and related to crtc.
        When using this option with the drm_prime renderer, it will only affect
        the OSD contents. Otherwise it will set OSD & video plane.
        (default: primary plane)

    ``--drm-video-plane-id=<number>``
        Select the DRM plane index to use for video layer.
        Index is zero based, and related to crtc.
        This option only has effect when using the drm_prime renderer (which
        supports several layers) together with ``vo=gpu`` and ``gpu-context=drm``.
        (default: first overlay plane)

    ``--drm-format=<xrgb8888|xrgb2101010>``
        Select the DRM format to use (default: xrgb8888). This allows you to
        choose the bit depth of the DRM mode. xrgb8888 is your usual 24 bit per
        pixel/8 bits per channel packed RGB format with 8 bits of padding.
        xrgb2101010 is a packed 30 bits per pixel/10 bits per channel packed RGB
        format with 2 bits of padding.

        Unless you have an intel graphics card, a recent kernel and a recent
        version of mesa (>=18) xrgb2101010 is unlikely to work for you.

        This currently only has an effect when used together with the ``drm``
        backend for the ``gpu`` VO. The ``drm`` VO always uses xrgb8888.

    ``--drm-osd-size=<[WxH]>``
        Sets the OSD OpenGL size to the specified size. OSD will then be upscaled
        to the current screen resolution. This option can be useful when using
        several layers in high resolutions with a GPU which cannot handle it.
        Note : this option is only available with DRM atomic support.
        (default: display resolution)

``mediacodec_embed`` (Android)
    Renders ``IMGFMT_MEDIACODEC`` frames directly to an ``android.view.Surface``.
    Requires ``--hwdec=mediacodec`` for hardware decoding, along with
    ``--vo=mediacodec_embed`` and ``--wid=(intptr_t)(*android.view.Surface)``.

    Since this video output driver uses native decoding and rendering routines,
    many of mpv's features (subtitle rendering, OSD/OSC, video filters, etc)
    are not available with this driver.

    To use hardware decoding with ``--vo-gpu`` instead, use
    ``--hwdec=mediacodec-copy`` along with ``--gpu-context=android``.
