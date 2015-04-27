VIDEO FILTERS
=============

Video filters allow you to modify the video stream and its properties. The
syntax is:

``--vf=<filter1[=parameter1:parameter2:...],filter2,...>``
    Setup a chain of video filters.

You can also set defaults for each filter. The defaults are applied before the
normal filter parameters.

``--vf-defaults=<filter1[=parameter1:parameter2:...],filter2,...>``
    Set defaults for each filter.

.. note::

    To get a full list of available video filters, see ``--vf=help``.

Video filters are managed in lists. There are a few commands to manage the
filter list.

``--vf-add=<filter1[,filter2,...]>``
    Appends the filters given as arguments to the filter list.

``--vf-pre=<filter1[,filter2,...]>``
    Prepends the filters given as arguments to the filter list.

``--vf-del=<index1[,index2,...]>``
    Deletes the filters at the given indexes. Index numbers start at 0,
    negative numbers address the end of the list (-1 is the last).

``--vf-clr``
    Completely empties the filter list.

With filters that support it, you can access parameters by their name.

``--vf=<filter>=help``
    Prints the parameter names and parameter value ranges for a particular
    filter.

``--vf=<filter=named_parameter1=value1[:named_parameter2=value2:...]>``
    Sets a named parameter to the given value. Use on and off or yes and no to
    set flag parameters.

Available filters are:

``crop[=w:h:x:y]``
    Crops the given part of the image and discards the rest. Useful to remove
    black bands from widescreen videos.

    ``<w>,<h>``
        Cropped width and height, defaults to original width and height.
    ``<x>,<y>``
        Position of the cropped picture, defaults to center.

``expand[=w:h:x:y:aspect:round]``
    Expands (not scales) video resolution to the given value and places the
    unscaled original at coordinates x, y.

    ``<w>,<h>``
        Expanded width,height (default: original width,height). Negative
        values for w and h are treated as offsets to the original size.

        .. admonition:: Example

            ``expand=0:-50:0:0``
                Adds a 50 pixel border to the bottom of the picture.

    ``<x>,<y>``
        position of original image on the expanded image (default: center)

    ``<aspect>``
        Expands to fit an aspect instead of a resolution (default: 0).

        .. admonition:: Example

            ``expand=800::::4/3``
                Expands to 800x600, unless the source is higher resolution, in
                which case it expands to fill a 4/3 aspect.

    ``<round>``
        Rounds up to make both width and height divisible by <r> (default: 1).

``flip``
    Flips the image upside down.

``mirror``
    Mirrors the image on the Y axis.

``rotate[=0|90|180|270]``
    Rotates the image by a multiple of 90 degrees clock-wise.

``scale[=w:h:param:param2:chr-drop:noup:arnd``
    Scales the image with the software scaler (slow) and performs a YUV<->RGB
    color space conversion (see also ``--sws``).

    All parameters are optional.

    ``<w>,<h>``
        scaled width/height (default: original width/height)

        :0:      scaled d_width/d_height
        :-1:     original width/height
        :-2:     Calculate w/h using the other dimension and the prescaled
                 aspect ratio.
        :-3:     Calculate w/h using the other dimension and the original
                 aspect ratio.
        :-(n+8): Like -n above, but rounding the dimension to the closest
                 multiple of 16.

    ``<param>[:<param2>]`` (see also ``--sws``)
        Set some scaling parameters depending on the type of scaler selected
        with ``--sws``::

            --sws=2 (bicubic):  B (blurring) and C (ringing)
                0.00:0.60 default
                0.00:0.75 VirtualDub's "precise bicubic"
                0.00:0.50 Catmull-Rom spline
                0.33:0.33 Mitchell-Netravali spline
                1.00:0.00 cubic B-spline

            --sws=7 (Gaussian): sharpness (0 (soft) - 100 (sharp))

            --sws=9 (Lanczos):  filter length (1-10)

    ``<chr-drop>``
        chroma skipping

        :0: Use all available input lines for chroma (default).
        :1: Use only every 2. input line for chroma.
        :2: Use only every 4. input line for chroma.
        :3: Use only every 8. input line for chroma.

    ``<noup>``
        Disallow upscaling past the original dimensions.

        :0: Allow upscaling (default).
        :1: Disallow upscaling if one dimension exceeds its original value.
        :2: Disallow upscaling if both dimensions exceed their original values.

    ``<arnd>``
        Accurate rounding for the vertical scaler, which may be faster or
        slower than the default rounding.

        :no:  Disable accurate rounding (default).
        :yes: Enable accurate rounding.

``dsize[=w:h:aspect-method:r:aspect]``
    Changes the intended display size/aspect at an arbitrary point in the
    filter chain. Aspect can be given as a fraction (4/3) or floating point
    number (1.33). Alternatively, you may specify the exact display width and
    height desired. Note that this filter does *not* do any scaling itself; it
    just affects what later scalers (software or hardware) will do when
    auto-scaling to correct aspect.

    ``<w>,<h>``
        New display width and height.

        Can also be these special values:

        :0:  original display width and height
        :-1: original video width and height (default)
        :-2: Calculate w/h using the other dimension and the original display
             aspect ratio.
        :-3: Calculate w/h using the other dimension and the original video
             aspect ratio.

        .. admonition:: Example

            ``dsize=800:-2``
                Specifies a display resolution of 800x600 for a 4/3 aspect
                video, or 800x450 for a 16/9 aspect video.

    ``<aspect-method>``
        Modifies width and height according to original aspect ratios.

        :-1: Ignore original aspect ratio (default).
        :0:  Keep display aspect ratio by using ``<w>`` and ``<h>`` as maximum
             resolution.
        :1:  Keep display aspect ratio by using ``<w>`` and ``<h>`` as minimum
             resolution.
        :2:  Keep video aspect ratio by using ``<w>`` and ``<h>`` as maximum
             resolution.
        :3:  Keep video aspect ratio by using ``<w>`` and ``<h>`` as minimum
             resolution.

        .. admonition:: Example

            ``dsize=800:600:0``
                Specifies a display resolution of at most 800x600, or smaller,
                in order to keep aspect.

    ``<r>``
        Rounds up to make both width and height divisible by ``<r>``
        (default: 1).

    ``<aspect>``
        Force an aspect ratio.

``format=fmt=<value>:colormatrix=<value>:...``
    Restricts the color space for the next filter without doing any conversion.
    Use together with the scale filter for a real conversion.

    .. note::

        For a list of available formats, see ``format=fmt=help``.

    ``<fmt>``
        Format name, e.g. rgb15, bgr24, 420p, etc. (default: don't change).
    ``<outfmt>``
        Format name that should be substituted for the output. If they do not
        have the same bytes per pixel and chroma subsamplimg, it will fail.
    ``<colormatrix>``
        Controls the YUV to RGB color space conversion when playing video. There
        are various standards. Normally, BT.601 should be used for SD video, and
        BT.709 for HD video. (This is done by default.) Using incorrect color space
        results in slightly under or over saturated and shifted colors.

        These options are not always supported. Different video outputs provide
        varying degrees of support. The ``opengl`` and ``vdpau`` video output
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

    ``<outputlevels>``
        RGB color levels used with YUV to RGB conversion. Normally, output devices
        such as PC monitors use full range color levels. However, some TVs and
        video monitors expect studio RGB levels. Providing full range output to a
        device expecting studio level input results in crushed blacks and whites,
        the reverse in dim gray blacks and dim whites.

        The same limitations as with ``<colormatrix>`` apply.

        Available color ranges are:

        :auto:      automatic selection (equals to full range) (default)
        :limited:   limited range (16-235 per component), studio levels
        :full:      full range (0-255 per component), PC levels

        .. note::

            It is advisable to use your graphics driver's color range option
            instead, if available.

    ``<primaries>``
        RGB primaries the source file was encoded with. Normally this should be set
        in the file header, but when playing broken or mistagged files this can be
        used to override the setting.

        This option only affects video output drivers that perform color
        management, for example ``opengl`` with the ``target-prim`` or
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
        :bt.1886:      ITU-R BT.1886 (approximation of BT.601/BT.709/BT.2020 curve)
        :srgb:         IEC 61966-2-4 (sRGB)
        :linear:       Linear light
        :gamma1.8:     Pure power curve (gamma 1.8)
        :gamma2.2:     Pure power curve (gamma 2.2)
        :gamma2.8:     Pure power curve (gamma 2.8)
        :prophoto:     ProPhoto RGB (ROMM) curve

    ``<stereo-in>``
        Set the stereo mode the video is assumed to be encoded in. Takes the
        same values as the ``--video-stereo-mode`` option.

    ``<stereo-out>``
        Set the stereo mode the video should be displayed as. Takes the
        same values as the ``--video-stereo-mode`` option.

    ``<rotate>``
        Set the rotation the video is assumed to be encoded with in degrees.
        The special value ``-1`` uses the input format.

    ``<dw>``, ``<dh>``
        Set the display size. Note that setting the display size such that
        the video is scaled in both directions instead of just changing the
        aspect ratio is an implementation detail, and might change later.

    ``<dar>``
        Set the display aspect ratio of the video frame. This is a float,
        but values such as ``[16:9]`` can be passed too (``[...]`` for quoting
        to prevent the option parser from interpreting the ``:`` character).


``noformat[=fmt]``
    Restricts the color space for the next filter without doing any conversion.
    Unlike the format filter, this will allow any color space except the one
    you specify.

    .. note:: For a list of available formats, see ``noformat=fmt=help``.

    ``<fmt>``
        Format name, e.g. rgb15, bgr24, 420p, etc. (default: 420p).

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
            filter graph syntax from clashing.

        .. admonition:: Examples

            ``-vf lavfi=[gradfun=20:30,vflip]``
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

        See ``http://git.videolan.org/?p=ffmpeg.git;a=blob;f=libswscale/swscale.h``.

    ``<o>``
        Set AVFilterGraph options. These should be documented by FFmpeg.

        .. admonition:: Example

            ``'--vf=lavfi=yadif:o="threads=2,thread_type=slice"'``
                forces a specific threading configuration.

``noise[=<strength>[:averaged][:pattern][:temporal][:uniform][:hq]``
    Adds noise.

    ``strength``
        Set the noise for all components. If you want different strength
        values for luma and chroma, use libavfilter's noise filter directly
        (using ``--vf=lavfi=[noise=...]``), or tell the libavfilter developers
        to stop being stupid.

    ``averaged``
        averaged temporal noise (smoother, but a lot slower)

    ``pattern``
        mix random noise with a (semi)regular pattern

    ``temporal``
        temporal noise (noise pattern changes between frames)

    ``uniform``
        uniform noise (Gaussian otherwise)

``hqdn3d[=luma_spatial:chroma_spatial:luma_tmp:chroma_tmp]``
    This filter aims to reduce image noise producing smooth images and making
    still images really still (This should enhance compressibility.).

    ``<luma_spatial>``
        spatial luma strength (default: 4)
    ``<chroma_spatial>``
        spatial chroma strength (default: 3)
    ``<luma_tmp>``
        luma temporal strength (default: 6)
    ``<chroma_tmp>``
        chroma temporal strength (default:
        ``luma_tmp*chroma_spatial/luma_spatial``)

``eq[=gamma:contrast:brightness:saturation:rg:gg:bg:weight]``
    Software equalizer that uses lookup tables (slow), allowing gamma correction
    in addition to simple brightness and contrast adjustment. The parameters are
    given as floating point values.

    ``<0.1-10>``
        initial gamma value (default: 1.0)
    ``<-2-2>``
        initial contrast, where negative values result in a negative image
        (default: 1.0)
    ``<-1-1>``
        initial brightness (default: 0.0)
    ``<0-3>``
        initial saturation (default: 1.0)
    ``<0.1-10>``
        gamma value for the red component (default: 1.0)
    ``<0.1-10>``
        gamma value for the green component (default: 1.0)
    ``<0.1-10>``
        gamma value for the blue component (default: 1.0)
    ``<0-1>``
        The weight parameter can be used to reduce the effect of a high gamma
        value on bright image areas, e.g. keep them from getting overamplified
        and just plain white. A value of 0.0 turns the gamma correction all
        the way down while 1.0 leaves it at its full strength (default: 1.0).

``unsharp[=lx:ly:la:cx:cy:ca]``
    unsharp mask / Gaussian blur

    ``l`` is for the luma component, ``c`` for the chroma component. ``x``/``y``
    is the filter size. ``a`` is the amount.

    ``lx``, ``ly``, ``cx``, ``cy``
        width and height of the matrix, odd sized in both directions (min =
        3:3, max = 13:11 or 11:13, usually something between 3:3 and 7:7)

    ``la``, ``ca``
        Relative amount of sharpness/blur to add to the image (a sane range
        should be -1.5-1.5).

        :<0: blur
        :>0: sharpen

``pullup[=jl:jr:jt:jb:sb:mp]``
    Pulldown reversal (inverse telecine) filter, capable of handling mixed
    hard-telecine, 24000/1001 fps progressive, and 30000/1001 fps progressive
    content. The ``pullup`` filter makes use of future context in making its
    decisions. It is stateless in the sense that it does not lock onto a pattern
    to follow, but it instead looks forward to the following fields in order to
    identify matches and rebuild progressive frames.

    ``jl``, ``jr``, ``jt``, and ``jb``
        These options set the amount of "junk" to ignore at the left, right,
        top, and bottom of the image, respectively. Left/right are in units of
        8 pixels, while top/bottom are in units of 2 lines. The default is 8
        pixels on each side.

    ``sb`` (strict breaks)
        Setting this option to 1 will reduce the chances of ``pullup``
        generating an occasional mismatched frame, but it may also cause an
        excessive number of frames to be dropped during high motion sequences.
        Conversely, setting it to -1 will make ``pullup`` match fields more
        easily. This may help processing of video where there is slight
        blurring between the fields, but may also cause there to be interlaced
        frames in the output.

    ``mp`` (metric plane)
        This option may be set to ``u`` or ``v`` to use a chroma plane instead of the
        luma plane for doing ``pullup``'s computations. This may improve accuracy
        on very clean source material, but more likely will decrease accuracy,
        especially if there is chroma noise (rainbow effect) or any grayscale
        video. The main purpose of setting ``mp`` to a chroma plane is to reduce
        CPU load and make pullup usable in realtime on slow machines.

``yadif=[mode[:enabled=yes|no]]``
    Yet another deinterlacing filter

    ``<mode>``
        :frame: Output 1 frame for each frame.
        :field: Output 1 frame for each field.
        :frame-nospatial: Like ``frame`` but skips spatial interlacing check.
        :field-nospatial: Like ``field`` but skips spatial interlacing check.

    ``<enabled>``
        :yes: Filter is active (default).
        :no:  Filter is not active, but can be activated with the ``D`` key
              (or any other key that toggles the ``deinterlace`` property).

    This filter, is automatically inserted when using the ``D`` key (or any
    other key that toggles the ``deinterlace`` property or when using the
    ``--deinterlace`` switch), assuming the video output does not have native
    deinterlacing support.

    If you just want to set the default mode, put this filter and its options
    into ``--vf-defaults`` instead, and enable deinterlacing with ``D`` or
    ``--deinterlace``.

    Also note that the ``D`` key is stupid enough to insert an interlacer twice
    when inserting yadif with ``--vf``, so using the above methods is
    recommended.

``delogo[=x:y:w:h:t:show]``
    Suppresses a TV station logo by a simple interpolation of the surrounding
    pixels. Just set a rectangle covering the logo and watch it disappear (and
    sometimes something even uglier appear - your mileage may vary).

    ``<x>,<y>``
        top left corner of the logo
    ``<w>,<h>``
        width and height of the cleared rectangle
    ``<t>``
        Thickness of the fuzzy edge of the rectangle (added to ``w`` and
        ``h``). When set to -1, a green rectangle is drawn on the screen to
        simplify finding the right ``x``,``y``,``w``,``h`` parameters.
    ``show``
        Draw a rectangle showing the area defined by x/y/w/h.

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

``stereo3d[=in:out]``
    Stereo3d converts between different stereoscopic image formats.

    ``<in>``
        Stereoscopic image format of input. Possible values:

        ``sbsl`` or ``side_by_side_left_first``
            side by side parallel (left eye left, right eye right)
        ``sbsr`` or ``side_by_side_right_first``
            side by side crosseye (right eye left, left eye right)
        ``abl`` or ``above_below_left_first``
            above-below (left eye above, right eye below)
        ``abr`` or ``above_below_right_first``
            above-below (right eye above, left eye below)
        ``ab2l`` or ``above_below_half_height_left_first``
            above-below with half height resolution (left eye above, right eye
            below)
        ``ab2r`` or ``above_below_half_height_right_first``
            above-below with half height resolution (right eye above, left eye
            below)

    ``<out>``
        Stereoscopic image format of output. Possible values are all the input
        formats as well as:

        ``arcg`` or ``anaglyph_red_cyan_gray``
            anaglyph red/cyan gray (red filter on left eye, cyan filter on
            right eye)
        ``arch`` or ``anaglyph_red_cyan_half_color``
            anaglyph red/cyan half colored (red filter on left eye, cyan filter
            on right eye)
        ``arcc`` or ``anaglyph_red_cyan_color``
            anaglyph red/cyan color (red filter on left eye, cyan filter on
            right eye)
        ``arcd`` or ``anaglyph_red_cyan_dubois``
            anaglyph red/cyan color optimized with the least-squares
            projection of Dubois (red filter on left eye, cyan filter on right
            eye)
        ``agmg`` or ``anaglyph_green_magenta_gray``
            anaglyph green/magenta gray (green filter on left eye, magenta
            filter on right eye)
        ``agmh`` or ``anaglyph_green_magenta_half_color``
            anaglyph green/magenta half colored (green filter on left eye,
            magenta filter on right eye)
        ``agmc`` or ``anaglyph_green_magenta_color``
            anaglyph green/magenta colored (green filter on left eye, magenta
            filter on right eye)
        ``aybg`` or ``anaglyph_yellow_blue_gray``
            anaglyph yellow/blue gray (yellow filter on left eye, blue filter
            on right eye)
        ``aybh`` or ``anaglyph_yellow_blue_half_color``
            anaglyph yellow/blue half colored (yellow filter on left eye, blue
            filter on right eye)
        ``aybc`` or ``anaglyph_yellow_blue_color``
            anaglyph yellow/blue colored (yellow filter on left eye, blue
            filter on right eye)
        ``irl`` or ``interleave_rows_left_first``
            Interleaved rows (left eye has top row, right eye starts on next
            row)
        ``irr`` or ``interleave_rows_right_first``
            Interleaved rows (right eye has top row, left eye starts on next
            row)
        ``ml`` or ``mono_left``
            mono output (left eye only)
        ``mr`` or ``mono_right``
            mono output (right eye only)

``gradfun[=strength[:radius|:size=<size>]]``
    Fix the banding artifacts that are sometimes introduced into nearly flat
    regions by truncation to 8-bit color depth. Interpolates the gradients that
    should go where the bands are, and dithers them.

    ``<strength>``
        Maximum amount by which the filter will change any one pixel. Also the
        threshold for detecting nearly flat regions (default: 1.5).

    ``<radius>``
        Neighborhood to fit the gradient to. Larger radius makes for smoother
        gradients, but also prevents the filter from modifying pixels near
        detailed regions (default: disabled).

    ``<size>``
        size of the filter in percent of the image diagonal size. This is
        used to calculate the final radius size (default: 1).


``dlopen=dll[:a0[:a1[:a2[:a3]]]]``
    Loads an external library to filter the image. The library interface
    is the ``vf_dlopen`` interface specified using ``libmpcodecs/vf_dlopen.h``.

    ``dll=<library>``
        Specify the library to load. This may require a full file system path
        in some cases. This argument is required.

    ``a0=<string>``
        Specify the first parameter to pass to the library.

    ``a1=<string>``
        Specify the second parameter to pass to the library.

    ``a2=<string>``
        Specify the third parameter to pass to the library.

    ``a3=<string>``
        Specify the fourth parameter to pass to the library.

``vapoursynth=file:buffered-frames:concurrent-frames``
    Loads a VapourSynth filter script. This is intended for streamed
    processing: mpv actually provides a source filter, instead of using a
    native VapourSynth video source. The mpv source will answer frame
    requests only within a small window of frames (the size of this window
    is controlled with the ``buffered-frames`` parameter), and requests outside
    of that will return errors. As such, you can't use the full power of
    VapourSynth, but you can use certain filters.

    If you just want to play video generated by a VapourSynth (i.e. using
    a native VapourSynth video source), it's better to use ``vspipe`` and a
    FIFO to feed the video to mpv. The same applies if the filter script
    requires random frame access (see ``buffered-frames`` parameter).

    This filter is experimental. If it turns out that it works well and is
    used, it will be ported to libavfilter. Otherwise, it will be just removed.

    ``file``
        Filename of the script source. Currently, this is always a python
        script. The variable ``video_in`` is set to the mpv video source,
        and it is expected that the script reads video from it. (Otherwise,
        mpv will decode no video, and the video packet queue will overflow,
        eventually leading to audio being stopped.) The script is also
        expected to pass through timestamps using the ``_DurationNum`` and
        ``_DurationDen`` frame properties.

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
        the script can requests backwards. E.g. if ``buffered-frames=5``, and
        the script just requested frame 15, it can still request frame 10, but
        frame 9 is not available anymore. If it requests frame 30, mpv will
        decode 15 more frames, and keep only frames 25-30.

        The actual number of buffered frames also depends on the value of the
        ``concurrent-frames`` option. Currently, both option values are
        multiplied to get the final buffer size.

        (Normally, VapourSynth source filters must provide random access, but
        mpv was made for playback, and does not provide frame-exact random
        access. The way this video filter works is a compromise to make simple
        filters work anyway.)

    ``concurrent-frames``
        Number of frames that should be requested in parallel. The
        level of concurrency depends on the filter and how quickly mpv can
        decode video to feed the filter. This value should probably be
        proportional to the number of cores on your machine. Most time,
        making it higher than the number of cores can actually make it
        slower.

        By default, this uses the special value ``auto``, which sets the option
        to the number of detected logical CPU cores.

    The following variables are defined by mpv:

    ``video_in``
        The mpv video source as vapoursynth clip. Note that this has no length
        set, which confuses many filters. Using ``Trim`` on the clip with a
        high dummy length can turn it into a finite clip.

    ``video_in_dw``, ``video_in_dh``
        Display size of the video. Can be different from video size if the
        video does not use square pixels (e.g. DVD).

    ``container_fps``
        FPS value as reported by file headers. This value can be wrong or
        completely broken (e.g. 0 or NaN). Even if the value is correct,
        if another filter changes the real FPS (by dropping or inserting
        frames), the value of this variable might not be useful. Note that
        the ``--fps`` command line option overrides this value.

        Useful for some filters which insist on having a FPS.

    ``display_fps``
        Refresh rate of the current display. Note that this value can be 0.

``vapoursynth-lazy``
    The same as ``vapoursynth``, but doesn't load Python scripts. Instead, a
    custom backend using Lua and the raw VapourSynth API is used. The syntax
    is completely different, and absolutely no conveniencve features are
    provided. There's no type checking either, and you can trigger crashes.

    .. admonition:: Example:

        ::

            video_out = invoke("morpho", "Open", {clip = video_in})

    The special variable ``video_in`` is the mpv video source, while the
    special variable ``video_out`` is used to read video from. The 1st argument
    is the plugin (queried with ``getPluginByNs``), the 2nd is the filter name,
    and the 3rd argument is a table with the arguments. Positional arguments
    are not supported. The types must match exactly. Since Lua is terrible and
    can't distinguish integers and floats, integer arguments must be prefixed
    with ``i_``, in which case the prefix is removed and the argument is cast
    to an integer. Should the argument's name start with ``i_``, you're out of
    luck.

    Clips (VSNodeRef) are passed as light userdata, so trying to pass any
    other userdata type will result in hard crashes.

``vavpp``
    VA-AP-API video post processing. Works with ``--vo=vaapi`` and ``--vo=opengl``
    only. Currently deinterlaces. This filter is automatically inserted if
    deinterlacing is requested (either using the ``D`` key, by default mapped to
    the command ``cycle deinterlace``, or the ``--deinterlace`` option).

    ``deint=<method>``
        Select the deinterlacing algorithm.

        no
            Don't perform deinterlacing.
        first-field
            Show only first field (going by ``--field-dominance``).
        bob
            bob deinterlacing (default).
        weave, motion-adaptive, motion-compensated
            Advanced deinterlacing algorithms. Whether these actually work
            depends on the GPU hardware, the GPU drivers, driver bugs, and
            mpv bugs.

``vdpaupp``
    VDPAU video post processing. Works with ``--vo=vdpau`` and ``--vo=opengl``
    only. This filter is automatically inserted if deinterlacing is requested
    (either using the ``D`` key, by default mapped to the command
    ``cycle deinterlace``, or the ``--deinterlace`` option). When enabling
    deinterlacing, it is always preferred over software deinterlacer filters
    if the ``vdpau`` VO is used, and also if ``opengl`` is used and hardware
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
        All modes respect ``--field-dominance``.

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
    ``hqscaling=<0-9>``
        0
            Use default VDPAU scaling (default).
        1-9
            Apply high quality VDPAU scaling (needs capable hardware).

``buffer=<num>``
    Buffer ``<num>`` frames in the filter chain. This filter is probably pretty
    useless, except for debugging. (Note that this won't help smoothing out
    latencies with decoding, because the filter will never output a frame if
    the buffer isn't full, except on EOF.)
