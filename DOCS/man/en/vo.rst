.. _video_outputs:

VIDEO OUTPUT DRIVERS
====================

Video output drivers are interfaces to different video output facilities. The
syntax is:

--vo=<driver1[:suboption1[=value]:...],driver2,...[,]>
    Specify a priority list of video output drivers to be used.

If the list has a trailing ',' MPlayer will fall back on drivers not contained
in the list. Suboptions are optional and can mostly be omitted.

*NOTE*: See ``--vo=help`` for a list of compiled-in video output drivers.

*EXAMPLE*:

    ``--vo=xmga,xv,``
        Try the Matrox X11 driver, then the Xv driver, then others.
    ``--vo=directx:noaccel``
        Uses the DirectX driver with acceleration features turned off.

Available video output drivers are:

xv (X11 only)
    Uses the XVideo extension to enable hardware accelerated playback. If you
    cannot use a hardware specific driver, this is probably the best option.
    For information about what colorkey is used and how it is drawn run
    MPlayer with ``-v`` option and look out for the lines tagged with ``[xv
    common]`` at the beginning.

    adaptor=<number>
        Select a specific XVideo adaptor (check xvinfo results).
    port=<number>
        Select a specific XVideo port.
    ck=<cur|use|set>
        Select the source from which the colorkey is taken (default: cur).

        cur
          The default takes the colorkey currently set in Xv.
        use
          Use but do not set the colorkey from MPlayer (use the ``--colorkey``
          option to change it).
        set
          Same as use but also sets the supplied colorkey.

    ck-method=<man|bg|auto>
        Sets the colorkey drawing method (default: man).

        man
          Draw the colorkey manually (reduces flicker in some cases).
        bg
          Set the colorkey as window background.
        auto
          Let Xv draw the colorkey.

x11 (X11 only)
    Shared memory video output driver without hardware acceleration that works
    whenever X11 is present.

xover (X11 only)
    Adds X11 support to all overlay based video output drivers. Currently only
    supported by tdfx_vid.

    <vo_driver>
        Select the driver to use as source to overlay on top of X11.

vdpau (X11 only)
    Uses the VDPAU interface to display and optionally also decode video.
    Hardware decoding is used with ``--vc=ffmpeg12vdpau``,
    ``--vc=ffwmv3vdpau``, ``--vc=ffvc1vdpau``, ``--vc=ffh264vdpau`` or
    ``--vc=ffodivxvdpau``.

    sharpen=<-1-1>
        For positive values, apply a sharpening algorithm to the video, for
        negative values a blurring algorithm (default: 0).
    denoise=<0-1>
        Apply a noise reduction algorithm to the video (default: 0, no noise
        reduction).
    deint=<-4-4>
        Select deinterlacing mode (default: -3). Positive values choose mode
        and enable deinterlacing. Corresponding negative values select the
        same deinterlacing mode, but do not enable deinterlacing on startup
        (useful in configuration files to specify what mode will be enabled by
        the "D" key). All modes respect ``--field-dominance``.

        0
            same as -3
        1
            Show only first field, similar to ``--vf=field``.
        2
            Bob deinterlacing, similar to ``--vf=tfields=1``.
        3
            motion adaptive temporal deinterlacing. May lead to A/V desync
            with slow video hardware and/or high resolution.
        4
            motion adaptive temporal deinterlacing with edge-guided spatial
            interpolation. Needs fast video hardware.
    chroma-deint
        Makes temporal deinterlacers operate both on luma and chroma (default).
        Use no-chroma-deint to solely use luma and speed up advanced
        deinterlacing. Useful with slow video memory.
    pullup
        Try to apply inverse telecine, needs motion adaptive temporal
        deinterlacing.
    hqscaling=<0-9>
        0
            Use default VDPAU scaling (default).
        1-9
            Apply high quality VDPAU scaling (needs capable hardware).
    fps=<number>
        Override autodetected display refresh rate value (the value is needed
        for framedrop to allow video playback rates higher than display
        refresh rate, and for vsync-aware frame timing adjustments). Default 0
        means use autodetected value. A positive value is interpreted as a
        refresh rate in Hz and overrides the autodetected value. A negative
        value disables all timing adjustment and framedrop logic.
    composite-detect
        NVIDIA's current VDPAU implementation behaves somewhat differently
        under a compositing window manager and does not give accurate frame
        timing information. With this option enabled, the player tries to
        detect whether a compositing window manager is active. If one is
        detected, the player disables timing adjustments as if the user had
        specified fps=-1 (as they would be based on incorrect input). This
        means timing is somewhat less accurate than without compositing, but
        with the composited mode behavior of the NVIDIA driver there is no
        hard playback speed limit even without the disabled logic. Enabled by
        default, use no-composite-detect to disable.
    queuetime_windowed=<number> and queuetime_fs=<number>
        Use VDPAU's presentation queue functionality to queue future video
        frame changes at most this many milliseconds in advance (default: 50).
        See below for additional information.
    output_surfaces=<2-15>
        Allocate this many output surfaces to display video frames (default:
        3). See below for additional information.

    Using the VDPAU frame queueing functionality controlled by the queuetime
    options makes MPlayer's frame flip timing less sensitive to system CPU
    load and allows MPlayer to start decoding the next frame(s) slightly
    earlier which can reduce jitter caused by individual slow-to-decode
    frames. However the NVIDIA graphics drivers can make other window behavior
    such as window moves choppy if VDPAU is using the blit queue (mainly
    happens if you have the composite extension enabled) and this feature is
    active. If this happens on your system and it bothers you then you can set
    the queuetime value to 0 to disable this feature. The settings to use in
    windowed and fullscreen mode are separate because there should be less
    reason to disable this for fullscreen mode (as the driver issue shouldn't
    affect the video itself).

    You can queue more frames ahead by increasing the queuetime values and the
    output_surfaces count (to ensure enough surfaces to buffer video for a
    certain time ahead you need at least as many surfaces as the video has
    frames during that time, plus two). This could help make video smoother in
    some cases. The main downsides are increased video RAM requirements for
    the surfaces and laggier display response to user commands (display
    changes only become visible some time after they're queued). The graphics
    driver implementation may also have limits on the length of maximum
    queuing time or number of queued surfaces that work well or at all.

dga (X11 only)
    Play video through the XFree86 Direct Graphics Access extension.
    Considered obsolete.

sdl (SDL only, buggy/outdated)
    Highly platform independent SDL (Simple Directmedia Layer) library video
    output driver. Since SDL uses its own X11 layer, MPlayer X11 options do
    not have any effect on SDL. Note that it has several minor bugs
    (``--vm``/``--no-vm`` is mostly ignored, ``--fs`` behaves like ``--no-vm``
    should, window is in top-left corner when returning from fullscreen,
    panscan is not supported, ...).

    driver=<driver>
        Explicitly choose the SDL driver to use.
    (no-)forcexv
        Use XVideo through the sdl video output driver (default: forcexv).
    (no-)hwaccel
        Use hardware accelerated scaler (default: hwaccel).

direct3d (Windows only) (BETA CODE!)
    Video output driver that uses the Direct3D interface (useful for Vista).

directx (Windows only)
    Video output driver that uses the DirectX interface.

    noaccel
        Turns off hardware acceleration. Try this option if you have display
        problems.

corevideo (Mac OS X 10.4 or 10.3.9 with QuickTime 7)
    Mac OS X CoreVideo video output driver

    device_id=<number>
        Choose the display device to use for fullscreen or set it to -1 to
        always use the same screen the video window is on (default: -1 -
        auto).
    shared_buffer
        Write output to a shared memory buffer instead of displaying it and
        try to open an existing NSConnection for communication with a GUI.
    buffer_name=<name>
        Name of the shared buffer created with shm_open as well as the name of
        the NSConnection MPlayer will try to open (default: "mplayerosx").
        Setting buffer_name implicitly enables shared_buffer.

fbdev (Linux only)
    Uses the kernel framebuffer to play video.

    <device>
        Explicitly choose the fbdev device name to use (e.g. ``/dev/fb0``).

fbdev2 (Linux only)
    Uses the kernel framebuffer to play video, alternative implementation.

    <device>
        Explicitly choose the fbdev device name to use (default: ``/dev/fb0``).

vesa
    Very general video output driver that should work on any VESA VBE 2.0
    compatible card.

    (no-)dga
        Turns DGA mode on or off (default: on).
    neotv_pal
        Activate the NeoMagic TV out and set it to PAL norm.
    neotv_ntsc
        Activate the NeoMagic TV out and set it to NTSC norm.
    lvo
        Activate the Linux Video Overlay on top of VESA mode.

svga
    Play video using the SVGA library.

    <video mode>
        Specify video mode to use. The mode can be given in a
        <width>x<height>x<colors> format, e.g. 640x480x16M or be a graphics
        mode number, e.g. 84.
    bbosd
        Draw OSD into black bands below the movie (slower).
    native
        Use only native drawing functions. This avoids direct rendering, OSD
        and hardware acceleration.
    retrace
        Force frame switch on vertical retrace. Usable only with ``--double``.
        It has the same effect as the ``--vsync`` option.
    sq
        Try to select a video mode with square pixels.

gl
    OpenGL video output driver, simple version. Video size must be smaller
    than the maximum texture size of your OpenGL implementation. Intended to
    work even with the most basic OpenGL implementations, but also makes use
    of newer extensions, which allow support for more colorspaces and direct
    rendering. For optimal speed try adding the options ``--dr=-noslices``

    The code performs very few checks, so if a feature does not work, this
    might be because it is not supported by your card/OpenGL implementation
    even if you do not get any error message. Use ``glxinfo`` or a similar
    tool to display the supported OpenGL extensions.

    (no-)ati-hack
        ATI drivers may give a corrupted image when PBOs are used (when using
        ``--dr`` or `force-pbo`). This option fixes this, at the expense of
        using a bit more memory.
    (no-)force-pbo
        Always uses PBOs to transfer textures even if this involves an extra
        copy. Currently this gives a little extra speed with NVidia drivers
        and a lot more speed with ATI drivers. May need ``--no-slices`` and
        the ati-hack suboption to work correctly.
    (no-)scaled-osd
        Changes the way the OSD behaves when the size of the window changes
        (default: disabled). When enabled behaves more like the other video
        output drivers, which is better for fixed-size fonts. Disabled looks
        much better with FreeType fonts and uses the borders in fullscreen
        mode. Does not work correctly with ass subtitles (see ``--ass``), you
        can instead render them without OpenGL support via ``--vf=ass``.
    osdcolor=<0xAARRGGBB>
        Color for OSD (default: 0x00ffffff, corresponds to non-transparent
        white).
    rectangle=<0,1,2>
        Select usage of rectangular textures which saves video RAM, but often
        is slower (default: 0).

        0
            Use power-of-two textures (default).
        1
            Use the ``GL_ARB_texture_rectangle`` extension.
        2
            Use the ``GL_ARB_texture_non_power_of_two`` extension. In some
            cases only supported in software and thus very slow.

    swapinterval=<n>
        Minimum interval between two buffer swaps, counted in displayed frames
        (default: 1). 1 is equivalent to enabling VSYNC, 0 to disabling VSYNC.
        Values below 0 will leave it at the system default. This limits the
        framerate to (horizontal refresh rate / n). Requires
        ``GLX_SGI_swap_control`` support to work. With some (most/all?)
        implementations this only works in fullscreen mode.
    ycbcr
        Use the ``GL_MESA_ycbcr_texture`` extension to convert YUV to RGB. In
        most cases this is probably slower than doing software conversion to
        RGB.
    yuv=<n>
        Select the type of YUV to RGB conversion. The default is
        auto-detection deciding between values 0 and 2.

        0
            Use software conversion. Compatible with all OpenGL versions.
            Provides brightness, contrast and saturation control.
        1
            Same as 2. This used to use nVidia-specific extensions, which
            didn't provide any advantages over using fragment programs, except
            possibly on very ancient graphic cards. It produced a gray-ish
            output, which is why it has been removed.
        2
            Use a fragment program. Needs the ``GL_ARB_fragment_program``
            extension and at least three texture units. Provides brightness,
            contrast, saturation and hue control.
        3
            Use a fragment program using the POW instruction. Needs the
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

    lscale=<n>
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
            strength of 0.5 (see `filter-strength`).
        5
            Use experimental unsharp masking with 5x5 support and a default
            strength of 0.5 (see `filter-strength`).

    cscale=<n>
        Select the scaling function to use for chrominance scaling. For
        details see `lscale`.
    filter-strength=<value>
        Set the effect strength for the `lscale`/`cscale` filters that support
        it.
    stereo=<value>
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

    The following options are only useful if writing your own fragment
    programs.

    customprog=<filename>
        Load a custom fragment program from <filename>. See
        ``TOOLS/edgedect.fp`` for an example.
    customtex=<filename>
        Load a custom "gamma ramp" texture from <filename>. This can be used
        in combination with yuv=4 or with the customprog option.
    (no-)customtlin
        If enabled (default) use ``GL_LINEAR`` interpolation, otherwise use
        ``GL_NEAREST`` for customtex texture.
    (no-)customtrect
        If enabled, use texture_rectangle for customtex texture. Default is
        disabled.
    (no-)mipmapgen
        If enabled, mipmaps for the video are automatically generated. This
        should be useful together with the customprog and the TXB instruction
        to implement blur filters with a large radius. For most OpenGL
        implementations this is very slow for any non-RGB formats. Default is
        disabled.

    Normally there is no reason to use the following options, they mostly
    exist for testing purposes.

    (no-)glfinish
        Call ``glFinish()`` before swapping buffers. Slower but in some cases
        more correct output (default: disabled).
    (no-)manyfmts
        Enables support for more (RGB and BGR) color formats (default:
        enabled). Needs OpenGL version >= 1.2.
    slice-height=<0-...>
        Number of lines copied to texture in one piece (default: 0). 0 for
        whole image.

        *NOTE*: If YUV colorspace is used (see `yuv` suboption), special rules
        apply: If the decoder uses slice rendering (see ``--no-slices``), this
        setting has no effect, the size of the slices as provided by the
        decoder is used. If the decoder does not use slice rendering, the
        default is 16.
    (no-)osd
        Enable or disable support for OSD rendering via OpenGL (default:
        enabled). This option is for testing; to disable the OSD use
        ``--osdlevel=0`` instead.

null
    Produces no video output. Useful for benchmarking.

aa
    ASCII art video output driver that works on a text console. You can get a
    list and an explanation of available suboptions by executing ``mplayer
    --vo=aa:help``.

    *NOTE*: The driver does not handle ``--aspect`` correctly.

    *HINT*: You probably have to specify ``--monitorpixelaspect``. Try
    ``mplayer --vo=aa --monitorpixelaspect=0.5``.

caca
    Color ASCII art video output driver that works on a text console.

bl
    Video playback using the Blinkenlights UDP protocol. This driver is highly
    hardware specific.

    <subdevice>
        Explicitly choose the Blinkenlights subdevice driver to use. It is
        something like ``arcade:host=localhost:2323`` or
        ``hdl:file=name1,file=name2``. You must specify a subdevice.

ggi
    GGI graphics system video output driver

    <driver>
        Explicitly choose the GGI driver to use. Replace any ',' that would
        appear in the driver string by a '.'.

directfb
    Play video using the DirectFB library.

    (no-)input
        Use the DirectFB instead of the MPlayer keyboard code (default:
        enabled).
    buffermode=single|double|triple
        Double and triple buffering give best results if you want to avoid
        tearing issues. Triple buffering is more efficient than double
        buffering as it does not block MPlayer while waiting for the vertical
        retrace. Single buffering should be avoided (default: single).
    fieldparity=top|bottom
        Control the output order for interlaced frames (default: disabled).
        Valid values are top = top fields first, bottom = bottom fields first.
        This option does not have any effect on progressive film material like
        most MPEG movies are. You need to enable this option if you have
        tearing issues or unsmooth motions watching interlaced film material.
    layer=N
        Will force layer with ID N for playback (default: -1 - auto).
    dfbopts=<list>
        Specify a parameter list for DirectFB.

dfbmga
    Matrox G400/G450/G550 specific video output driver that uses the DirectFB
    library to make use of special hardware features. Enables CRTC2 (second
    head), displaying video independently of the first head.

    (no-)input
        same as directfb (default: disabled)
    buffermode=single|double|triple
        same as directfb (default: triple)
    fieldparity=top|bottom
        same as directfb
    (no-)bes
        Enable the use of the Matrox BES (backend scaler) (default: disabled).
        Gives very good results concerning speed and output quality as
        interpolated picture processing is done in hardware. Works only on the
        primary head.
    (no-)spic
        Make use of the Matrox sub picture layer to display the OSD (default:
        enabled).
    (no-)crtc2
        Turn on TV-out on the second head (default: enabled). The output
        quality is amazing as it is a full interlaced picture with proper sync
        to every odd/even field.
    tvnorm=pal|ntsc|auto
        Will set the TV norm of the Matrox card without the need for modifying
        ``/etc/directfbrc`` (default: disabled). Valid norms are pal = PAL,
        ntsc = NTSC. Special norm is auto (auto-adjust using PAL/NTSC) because
        it decides which norm to use by looking at the framerate of the movie.

mga (Linux only)
    Matrox specific video output driver that makes use of the YUV back end
    scaler on Gxxx cards through a kernel module. If you have a Matrox card,
    this is the fastest option.

    <device>
        Explicitly choose the Matrox device name to use (default:
        ``/dev/mga_vid``).

xmga (Linux, X11 only)
    The mga video output driver, running in an X11 window.

    <device>
        Explicitly choose the Matrox device name to use (default:
        ``/dev/mga_vid``).

s3fb (Linux only) (see also ``--dr``)
    S3 Virge specific video output driver. This driver supports the card's YUV
    conversion and scaling, double buffering and direct rendering features.
    Use ``--vf=format=yuy2`` to get hardware-accelerated YUY2 rendering, which
    is much faster than YV12 on this card.

    <device>
        Explicitly choose the fbdev device name to use (default: ``/dev/fb0``).

wii (Linux only)
    Nintendo Wii/GameCube specific video output driver.

3dfx (Linux only)
    3dfx-specific video output driver that directly uses the hardware on top
    of X11. Only 16 bpp are supported.

tdfxfb (Linux only)
    This driver employs the tdfxfb framebuffer driver to play movies with YUV
    acceleration on 3dfx cards.

    <device>
        Explicitly choose the fbdev device name to use (default: ``/dev/fb0``).

tdfx_vid (Linux only)
    3dfx-specific video output driver that works in combination with the
    tdfx_vid kernel module.

    <device>
        Explicitly choose the device name to use (default: ``/dev/tdfx_vid``).

dxr3 (DXR3 only)
    Sigma Designs em8300 MPEG decoder chip (Creative DXR3, Sigma Designs
    Hollywood Plus) specific video output driver. See also the lavc video
    filter.

    overlay
        Activates the overlay instead of TV-out.
    prebuf
        Turns on prebuffering.
    sync
        Will turn on the new sync-engine.
    norm=<norm>
        Specifies the TV norm.

        :0: Does not change current norm (default).
        :1: Auto-adjust using PAL/NTSC.
        :2: Auto-adjust using PAL/PAL-60.
        :3: PAL
        :4: PAL-60
        :5: NTSC

    <0-3>
        Specifies the device number to use if you have more than one em8300
        card.

ivtv (IVTV only)
    Conexant CX23415 (iCompression iTVC15) or Conexant CX23416 (iCompression
    iTVC16) MPEG decoder chip (Hauppauge WinTV PVR-150/250/350/500) specific
    video output driver for TV-out. See also the lavc video filter.

    <device>
        Explicitly choose the MPEG decoder device name to use (default:
        ``/dev/video16``).
    <output>
        Explicitly choose the TV-out output to be used for the video signal.

v4l2 (requires Linux 2.6.22+ kernel)
    Video output driver for V4L2 compliant cards with built-in hardware MPEG
    decoder. See also the lavc video filter.

    <device>
        Explicitly choose the MPEG decoder device name to use (default:
        ``/dev/video16``).
    <output>
        Explicitly choose the TV-out output to be used for the video signal.

mpegpes (DVB only)
    Video output driver for DVB cards that writes the output to an MPEG-PES
    file if no DVB card is installed.

    card=<1-4>
        Specifies the device number to use if you have more than one DVB
        output card (V3 API only, such as 1.x.y series drivers). If not
        specified MPlayer will search the first usable card.
    <filename>
        output filename (default: ``./grab.mpg``)

md5sum
    Calculate MD5 sums of each frame and write them to a file. Supports RGB24
    and YV12 colorspaces. Useful for debugging.

    outfile=<value>
        Specify the output filename (default: ``./md5sums``).

yuv4mpeg
    Transforms the video stream into a sequence of uncompressed YUV 4:2:0
    images and stores it in a file (default: ``./stream.yuv``). The format is
    the same as the one employed by mjpegtools, so this is useful if you want
    to process the video with the mjpegtools suite. It supports the YV12
    format. If your source file has a different format and is interlaced, make
    sure to use ``--vf=scale=::1`` to ensure the conversion uses interlaced
    mode. You can combine it with the ``--fixed-vo`` option to concatenate
    files with the same dimensions and fps value.

    interlaced
        Write the output as interlaced frames, top field first.
    interlaced_bf
        Write the output as interlaced frames, bottom field first.
    file=<filename>
        Write the output to <filename> instead of the default ``stream.yuv``.

    *NOTE*: If you do not specify any option the output is progressive (i.e.
    not interlaced).

gif89a
    Output each frame into a single animated GIF file in the current
    directory. It supports only RGB format with 24 bpp and the output is
    converted to 256 colors.

    <fps>
        Float value to specify framerate (default: 5.0).
    <output>
        Specify the output filename (default: ``./out.gif``).

    *NOTE*: You must specify the framerate before the filename or the
    framerate will be part of the filename.

    *EXAMPLE*: ``mplayer video.nut --vo=gif89a:fps=15:output=test.gif``

jpeg
    Output each frame into a JPEG file in the current directory. Each file
    takes the frame number padded with leading zeros as name.

    [no]progressive
        Specify standard or progressive JPEG (default: noprogressive).
    [no]baseline
        Specify use of baseline or not (default: baseline).
    optimize=<0-100>
        optimization factor (default: 100)
    smooth=<0-100>
        smooth factor (default: 0)
    quality=<0-100>
        quality factor (default: 75)
    outdir=<dirname>
        Specify the directory to save the JPEG files to (default: ``./``).
    subdirs=<prefix>
        Create numbered subdirectories with the specified prefix to save the
        files in instead of the current directory.
    maxfiles=<value> (subdirs only)
        Maximum number of files to be saved per subdirectory. Must be equal to
        or larger than 1 (default: 1000).

pnm
    Output each frame into a PNM file in the current directory. Each file
    takes the frame number padded with leading zeros as name. It supports PPM,
    PGM and PGMYUV files in both raw and ASCII mode. See also ``pnm(5)``,
    ``ppm(5)`` and ``pgm(5)``.

    ppm
        Write PPM files (default).
    pgm
        Write PGM files.
    pgmyuv
        Write PGMYUV files. PGMYUV is like PGM, but it also contains the U and
        V plane, appended at the bottom of the picture.
    raw
        Write PNM files in raw mode (default).
    ascii
        Write PNM files in ASCII mode.
    outdir=<dirname>
        Specify the directory to save the PNM files to (default: ``./``).
    subdirs=<prefix>
        Create numbered subdirectories with the specified prefix to save the
        files in instead of the current directory.
    maxfiles=<value> (subdirs only)
        Maximum number of files to be saved per subdirectory. Must be equal to
        or larger than 1 (default: 1000).

png
    Output each frame into a PNG file in the current directory. Each file
    takes the frame number padded with leading zeros as name. 24bpp RGB and
    BGR formats are supported.

    z=<0-9>
        Specifies the compression level. 0 is no compression, 9 is maximum
        compression.
    alpha
        Create PNG files with an alpha channel. Note that MPlayer in general
        does not support alpha, so this will only be useful in some rare
        cases.

tga
    Output each frame into a Targa file in the current directory. Each file
    takes the frame number padded with leading zeros as name. The purpose of
    this video output driver is to have a simple lossless image writer to use
    without any external library. It supports the BGR[A] color format, with
    15, 24 and 32 bpp. You can force a particular format with the format video
    filter.

    *EXAMPLE*: ``mplayer video.nut --vf=format=bgr15 --vo=tga``
