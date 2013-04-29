.. _video_filters:

VIDEO FILTERS
=============

Video filters allow you to modify the video stream and its properties. The
syntax is:

--vf=<filter1[=parameter1:parameter2:...],filter2,...>
    Setup a chain of video filters.

Many parameters are optional and set to default values if omitted. To
explicitly use a default value set a parameter to '-1'. Parameters w:h means
width x height in pixels, x:y means x;y position counted from the upper left
corner of the bigger image.

*NOTE*: To get a full list of available video filters, see ``--vf=help``.

Video filters are managed in lists. There are a few commands to manage the
filter list.

--vf-add=<filter1[,filter2,...]>
    Appends the filters given as arguments to the filter list.

--vf-pre=<filter1[,filter2,...]>
    Prepends the filters given as arguments to the filter list.

--vf-del=<index1[,index2,...]>
    Deletes the filters at the given indexes. Index numbers start at 0,
    negative numbers address the end of the list (-1 is the last).

--vf-clr
    Completely empties the filter list.

With filters that support it, you can access parameters by their name.

--vf=<filter>=help
    Prints the parameter names and parameter value ranges for a particular
    filter.

--vf=<filter=named_parameter1=value1[:named_parameter2=value2:...]>
    Sets a named parameter to the given value. Use on and off or yes and no to
    set flag parameters.

Available filters are:

crop[=w:h:x:y]
    Crops the given part of the image and discards the rest. Useful to remove
    black bands from widescreen movies.

    <w>,<h>
        Cropped width and height, defaults to original width and height.
    <x>,<y>
        Position of the cropped picture, defaults to center.

expand[=w:h:x:y:aspect:round]
    Expands (not scales) movie resolution to the given value and places the
    unscaled original at coordinates x, y.

    <w>,<h>
        Expanded width,height (default: original width,height). Negative
        values for w and h are treated as offsets to the original size.

        *EXAMPLE*:

        `expand=0:-50:0:0`
            Adds a 50 pixel border to the bottom of the picture.

    <x>,<y>
        position of original image on the expanded image (default: center)

    <aspect>
        Expands to fit an aspect instead of a resolution (default: 0).

        *EXAMPLE*:

        `expand=800:::::4/3`
            Expands to 800x600, unless the source is higher resolution, in
            which case it expands to fill a 4/3 aspect.

    <round>
        Rounds up to make both width and height divisible by <r> (default: 1).

flip
    Flips the image upside down. See also ``--flip``.

mirror
    Mirrors the image on the Y axis.

rotate[=<0-7>]
    Rotates the image by 90 degrees and optionally flips it. For values
    between 4-7 rotation is only done if the movie geometry is portrait and
    not landscape.

    :0: Rotate by 90 degrees clockwise and flip (default).
    :1: Rotate by 90 degrees clockwise.
    :2: Rotate by 90 degrees counterclockwise.
    :3: Rotate by 90 degrees counterclockwise and flip.

scale[=w:h[:interlaced[:chr_drop[:par[:par2[:noup[:arnd]]]]]]]
    Scales the image with the software scaler (slow) and performs a YUV<->RGB
    colorspace conversion (see also ``--sws``).

    <w>,<h>
        scaled width/height (default: original width/height)

        :0:      scaled d_width/d_height
        :-1:     original width/height
        :-2:     Calculate w/h using the other dimension and the prescaled
                 aspect ratio.
        :-3:     Calculate w/h using the other dimension and the original
                 aspect ratio.
        :-(n+8): Like -n above, but rounding the dimension to the closest
                 multiple of 16.

    <interlaced>
        Toggle interlaced scaling.

        :0: off (default)
        :1: on

    <chr_drop>
        chroma skipping

        :0: Use all available input lines for chroma.
        :1: Use only every 2. input line for chroma.
        :2: Use only every 4. input line for chroma.
        :3: Use only every 8. input line for chroma.

    <par>[:<par2>] (see also ``--sws``)
        Set some scaling parameters depending on the type of scaler selected
        with ``--sws``.

        | --sws=2 (bicubic):  B (blurring) and C (ringing)
        |     0.00:0.60 default
        |     0.00:0.75 VirtualDub's "precise bicubic"
        |     0.00:0.50 Catmull-Rom spline
        |     0.33:0.33 Mitchell-Netravali spline
        |     1.00:0.00 cubic B-spline

        --sws=7 (gaussian): sharpness (0 (soft) - 100 (sharp))

        --sws=9 (lanczos):  filter length (1-10)

    <noup>
        Disallow upscaling past the original dimensions.

        :0: Allow upscaling (default).
        :1: Disallow upscaling if one dimension exceeds its original value.
        :2: Disallow upscaling if both dimensions exceed their original values.

    <arnd>
        Accurate rounding for the vertical scaler, which may be faster or
        slower than the default rounding.

        :0: Disable accurate rounding (default).
        :1: Enable accurate rounding.

dsize[=aspect|w:h:aspect-method:r]
    Changes the intended display size/aspect at an arbitrary point in the
    filter chain. Aspect can be given as a fraction (4/3) or floating point
    number (1.33). Alternatively, you may specify the exact display width and
    height desired. Note that this filter does *not* do any scaling itself; it
    just affects what later scalers (software or hardware) will do when
    auto-scaling to correct aspect.

    <w>,<h>
        New display width and height.

        Can also be these special values:

        :0:  original display width and height
        :-1: original video width and height (default)
        :-2: Calculate w/h using the other dimension and the original display
             aspect ratio.
        :-3: Calculate w/h using the other dimension and the original video
             aspect ratio.

        *EXAMPLE*:

        ``dsize=800:-2``
            Specifies a display resolution of 800x600 for a 4/3 aspect video,
            or 800x450 for a 16/9 aspect video.

    <aspect-method>
        Modifies width and height according to original aspect ratios.

        :-1: Ignore original aspect ratio (default).
        :0:  Keep display aspect ratio by using <w> and <h> as maximum
             resolution.
        :1:  Keep display aspect ratio by using <w> and <h> as minimum
             resolution.
        :2:  Keep video aspect ratio by using <w> and <h> as maximum
             resolution.
        :3:  Keep video aspect ratio by using <w> and <h> as minimum
             resolution.

        *EXAMPLE*:

        ``dsize=800:600:0``
            Specifies a display resolution of at most 800x600, or smaller, in
            order to keep aspect.

    <r>
        Rounds up to make both width and height divisible by <r> (default: 1).

format[=fmt[:outfmt]]
    Restricts the colorspace for the next filter without doing any conversion.
    Use together with the scale filter for a real conversion.

    *NOTE*: For a list of available formats see ``format=fmt=help``.

    <fmt>
        format name like rgb15, bgr24, 420p, etc (default: yuyv)
    <outfmt>
        Format name that should be substituted for the output. If this is not
        100% compatible with the <fmt> value it will crash.

        *EXAMPLE*

        ====================== =====================
        Valid                  Invalid (will crash)
        ====================== =====================
        ``format=rgb24:bgr24`` ``format=rgb24:420p``
        ``format=yuyv:uyvy``
        ====================== =====================

noformat[=fmt]
    Restricts the colorspace for the next filter without doing any conversion.
    Unlike the format filter, this will allow any colorspace except the one
    you specify.

    *NOTE*: For a list of available formats see ``noformat=fmt=help``.

    <fmt>
        format name like rgb15, bgr24, 420p, etc (default: 420p)

pp[=filter1[:option1[:option2...]]/[-]filter2...]
    Enables the specified chain of postprocessing subfilters. Subfilters must
    be separated by '/' and can be disabled by prepending a '-'. Each
    subfilter and some options have a short and a long name that can be used
    interchangeably, i.e. dr/dering are the same. All subfilters share common
    options to determine their scope:

    a/autoq
        Automatically switch the subfilter off if the CPU is too slow.
    c/chrom
        Do chrominance filtering, too (default).
    y/nochrom
        Do luminance filtering only (no chrominance).
    n/noluma
        Do chrominance filtering only (no luminance).

    *NOTE*: ``--pphelp`` shows a list of available subfilters.

    Available subfilters are:

    hb/hdeblock[:difference[:flatness]]
        horizontal deblocking filter

        :<difference>: Difference factor where higher values mean more
                       deblocking (default: 32).
        :<flatness>:   Flatness threshold where lower values mean more
                       deblocking (default: 39).

    vb/vdeblock[:difference[:flatness]]
        vertical deblocking filter

        :<difference>: Difference factor where higher values mean more
                       deblocking (default: 32).
        :<flatness>:   Flatness threshold where lower values mean more
                       deblocking (default: 39).

    ha/hadeblock[:difference[:flatness]]
        accurate horizontal deblocking filter

        :<difference>: Difference factor where higher values mean more
                       deblocking (default: 32).
        :<flatness>:   Flatness threshold where lower values mean more
                       deblocking (default: 39).

    va/vadeblock[:difference[:flatness]]
        accurate vertical deblocking filter

        :<difference>: Difference factor where higher values mean more
                       deblocking (default: 32).
        :<flatness>:   Flatness threshold where lower values mean more
                       deblocking (default: 39).

    The horizontal and vertical deblocking filters share the difference and
    flatness values so you cannot set different horizontal and vertical
    thresholds.

    h1/x1hdeblock
        experimental horizontal deblocking filter

    v1/x1vdeblock
        experimental vertical deblocking filter

    dr/dering
        deringing filter

    tn/tmpnoise[:threshold1[:threshold2[:threshold3]]]
        temporal noise reducer

        :<threshold1>: larger -> stronger filtering
        :<threshold2>: larger -> stronger filtering
        :<threshold3>: larger -> stronger filtering

    al/autolevels[:f/fullyrange]
        automatic brightness / contrast correction

        :f/fullyrange: Stretch luminance to (0-255).

    lb/linblenddeint
        Linear blend deinterlacing filter that deinterlaces the given block by
        filtering all lines with a (1 2 1) filter.

    li/linipoldeint
        Linear interpolating deinterlacing filter that deinterlaces the given
        block by linearly interpolating every second line.

    ci/cubicipoldeint
        Cubic interpolating deinterlacing filter deinterlaces the given block
        by cubically interpolating every second line.

    md/mediandeint
        Median deinterlacing filter that deinterlaces the given block by
        applying a median filter to every second line.

    fd/ffmpegdeint
        FFmpeg deinterlacing filter that deinterlaces the given block by
        filtering every second line with a (-1 4 2 4 -1) filter.

    l5/lowpass5
        Vertically applied FIR lowpass deinterlacing filter that deinterlaces
        the given block by filtering all lines with a (-1 2 6 2 -1) filter.

    fq/forceQuant[:quantizer]
        Overrides the quantizer table from the input with the constant
        quantizer you specify.

        :<quantizer>: quantizer to use

    de/default
        default pp filter combination (hb:a,vb:a,dr:a)

    fa/fast
        fast pp filter combination (h1:a,v1:a,dr:a)

    ac
        high quality pp filter combination (ha:a:128:7,va:a,dr:a)

    *EXAMPLE*:

    ``--vf=pp=hb/vb/dr/al``
        horizontal and vertical deblocking, deringing and automatic
        brightness/contrast

    ``--vf=pp=de/-al``
        default filters without brightness/contrast correction

    ``--vf=pp=default/tmpnoise:1:2:3``
        Enable default filters & temporal denoiser.

    ``--vf=pp=hb:y/vb:a``
        Horizontal deblocking on luminance only, and switch vertical
        deblocking on or off automatically depending on available CPU time.

lavfi=graph[:sws_flags]
    Filter video using ffmpeg's libavfilter.

    <graph>
        The libavfilter graph string. The filter must have a single video input
        pad and a single video output pad.

        See ``https://ffmpeg.org/ffmpeg-filters.html`` for syntax and available
        filters.

        *WARNING*: if you want to use the full filter syntax with this option,
        you have to quote the filter graph in order to prevent mpv's syntax
        and the filter graph syntax from clashing.

        *EXAMPLE*:

        ``-vf lavfi=[gradfun=20:30,vflip]``
            gradfun filter with nonsense parameters, followed by a vflip
            filter. (This demonstrates how libavfilter takes a graph and not
            just a single filter.) The filter graph string is quoted with
            ``[`` and ``]``. This requires no additional quoting or escaping
            with some shells (like bash), while others (like zsh) require
            additional ``"`` quotes around the option string.

        ``'--vf=lavfi="gradfun=20:30,vflip"'``
            same as before, but uses quoting that should be safe with all
            shells. The outer ``'`` quotes make sure that the shell doesn't
            remove the ``"`` quotes needed by mpv.

        ``'--vf=lavfi=graph="gradfun=radius=30:strength=20,vflip"'``
            same as before, but uses named parameters for everything.

    <sws_flags>
        If libavfilter inserts filters for pixel format conversion, this
        option gives the flags which should be passed to libswscale. This
        option is numeric and takes a bit-wise combination of ``SWS_`` flags.

        See ``http://git.videolan.org/?p=ffmpeg.git;a=blob;f=libswscale/swscale.h``.

noise[=luma[u][t|a][h][p]:chroma[u][t|a][h][p]]
    Adds noise.

    :<0-100>: luma noise
    :<0-100>: chroma noise
    :u:       uniform noise (gaussian otherwise)
    :t:       temporal noise (noise pattern changes between frames)
    :a:       averaged temporal noise (smoother, but a lot slower)
    :h:       high quality (slightly better looking, slightly slower)
    :p:       mix random noise with a (semi)regular pattern

hqdn3d[=luma_spatial:chroma_spatial:luma_tmp:chroma_tmp]
    This filter aims to reduce image noise producing smooth images and making
    still images really still (This should enhance compressibility.).

    <luma_spatial>
        spatial luma strength (default: 4)
    <chroma_spatial>
        spatial chroma strength (default: 3)
    <luma_tmp>
        luma temporal strength (default: 6)
    <chroma_tmp>
        chroma temporal strength (default:
        ``luma_tmp*chroma_spatial/luma_spatial``)

eq[=gamma:contrast:brightness:saturation:rg:gg:bg:weight]
    Software equalizer that uses lookup tables (slow),
    allowing gamma correction in addition to simple brightness and contrast
    adjustment. The parameters are given as floating point
    values.

    <0.1-10>
        initial gamma value (default: 1.0)
    <-2-2>
        initial contrast, where negative values result in a negative image
        (default: 1.0)
    <-1-1>
        initial brightness (default: 0.0)
    <0-3>
        initial saturation (default: 1.0)
    <0.1-10>
        gamma value for the red component (default: 1.0)
    <0.1-10>
        gamma value for the green component (default: 1.0)
    <0.1-10>
        gamma value for the blue component (default: 1.0)
    <0-1>
        The weight parameter can be used to reduce the effect of a high gamma
        value on bright image areas, e.g. keep them from getting overamplified
        and just plain white. A value of 0.0 turns the gamma correction all
        the way down while 1.0 leaves it at its full strength (default: 1.0).

ilpack[=mode]
    When interlaced video is stored in YUV 4:2:0 formats, chroma interlacing
    does not line up properly due to vertical downsampling of the chroma
    channels. This filter packs the planar 4:2:0 data into YUY2 (4:2:2) format
    with the chroma lines in their proper locations, so that in any given
    scanline, the luma and chroma data both come from the same field.

    <mode>
        Select the sampling mode.

        :0: nearest-neighbor sampling, fast but incorrect
        :1: linear interpolation (default)

unsharp[=l|cWxH:amount[:l|cWxH:amount]]
    unsharp mask / gaussian blur

    l
        Apply effect on luma component.

    c
        Apply effect on chroma components.

    <width>x<height>
        width and height of the matrix, odd sized in both directions (min =
        3x3, max = 13x11 or 11x13, usually something between 3x3 and 7x7)

    amount
        Relative amount of sharpness/blur to add to the image (a sane range
        should be -1.5-1.5).

        :<0: blur
        :>0: sharpen

swapuv
    Swap U & V plane.

pullup[=jl:jr:jt:jb:sb:mp]
    Third-generation pulldown reversal (inverse telecine) filter, capable of
    handling mixed hard-telecine, 24000/1001 fps progressive, and 30000/1001
    fps progressive content. The pullup filter is designed to be much more
    robust than detc or ivtc, by taking advantage of future context in making
    its decisions. Like ivtc, pullup is stateless in the sense that it does
    not lock onto a pattern to follow, but it instead looks forward to the
    following fields in order to identify matches and rebuild progressive
    frames. It is still under development, but believed to be quite accurate.

    jl, jr, jt, and jb
        These options set the amount of "junk" to ignore at the left, right,
        top, and bottom of the image, respectively. Left/right are in units of
        8 pixels, while top/bottom are in units of 2 lines. The default is 8
        pixels on each side.

    sb (strict breaks)
        Setting this option to 1 will reduce the chances of pullup generating
        an occasional mismatched frame, but it may also cause an excessive
        number of frames to be dropped during high motion sequences.
        Conversely, setting it to -1 will make pullup match fields more
        easily. This may help processing of video where there is slight
        blurring between the fields, but may also cause there to be interlaced
        frames in the output.

    mp (metric plane)
        This option may be set to 1 or 2 to use a chroma plane instead of the
        luma plane for doing pullup's computations. This may improve accuracy
        on very clean source material, but more likely will decrease accuracy,
        especially if there is chroma noise (rainbow effect) or any grayscale
        video. The main purpose of setting mp to a chroma plane is to reduce
        CPU load and make pullup usable in realtime on slow machines.

divtc[=options]
    Inverse telecine for deinterlaced video. If 3:2-pulldown telecined video
    has lost one of the fields or is deinterlaced using a method that keeps
    one field and interpolates the other, the result is a juddering video that
    has every fourth frame duplicated. This filter is intended to find and
    drop those duplicates and restore the original film framerate. Two
    different modes are available: One pass mode is the default and is
    straightforward to use, but has the disadvantage that any changes in the
    telecine phase (lost frames or bad edits) cause momentary judder until the
    filter can resync again. Two pass mode avoids this by analyzing the whole
    video beforehand so it will have forward knowledge about the phase changes
    and can resync at the exact spot. These passes do *not* correspond to pass
    one and two of the encoding process. You must run an extra pass using
    divtc pass one before the actual encoding throwing the resulting video
    away. Then use divtc pass two for the actual
    encoding. If you use multiple encoder passes, use divtc pass two for all
    of them. The options are:

    pass=1|2
        Use two pass mode.

    file=<filename>
        Set the two pass log filename (default: ``framediff.log``).

    threshold=<value>
        Set the minimum strength the telecine pattern must have for the filter
        to believe in it (default: 0.5). This is used to avoid recognizing
        false pattern from the parts of the video that are very dark or very
        still.

    window=<numframes>
        Set the number of past frames to look at when searching for pattern
        (default: 30). Longer window improves the reliability of the pattern
        search, but shorter window improves the reaction time to the changes
        in the telecine phase. This only affects the one pass mode. The two
        pass mode currently uses fixed window that extends to both future and
        past.

    phase=0|1|2|3|4
        Sets the initial telecine phase for one pass mode (default: 0). The
        two pass mode can see the future, so it is able to use the correct
        phase from the beginning, but one pass mode can only guess. It catches
        the correct phase when it finds it, but this option can be used to fix
        the possible juddering at the beginning. The first pass of the two
        pass mode also uses this, so if you save the output from the first
        pass, you get constant phase result.

    deghost=<value>
        Set the deghosting threshold (0-255 for one pass mode, -255-255 for
        two pass mode, default 0). If nonzero, deghosting mode is used. This
        is for video that has been deinterlaced by blending the fields
        together instead of dropping one of the fields. Deghosting amplifies
        any compression artifacts in the blended frames, so the parameter
        value is used as a threshold to exclude those pixels from deghosting
        that differ from the previous frame less than specified value. If two
        pass mode is used, then negative value can be used to make the filter
        analyze the whole video in the beginning of pass-2 to determine
        whether it needs deghosting or not and then select either zero or the
        absolute value of the parameter. Specify this option for pass-2, it
        makes no difference on pass-1.

phase[=t|b|p|a|u|T|B|A|U][:v]
    Delay interlaced video by one field time so that the field order changes.
    The intended use is to fix PAL movies that have been captured with the
    opposite field order to the film-to-video transfer. The options are:

    t
        Capture field order top-first, transfer bottom-first. Filter will
        delay the bottom field.

    b
        Capture bottom-first, transfer top-first. Filter will delay the top
        field.

    p
        Capture and transfer with the same field order. This mode only exists
        for the documentation of the other options to refer to, but if you
        actually select it, the filter will faithfully do nothing ;-)

    a
        Capture field order determined automatically by field flags, transfer
        opposite. Filter selects among t and b modes on a frame by frame basis
        using field flags. If no field information is available, then this
        works just like u.

    u
        Capture unknown or varying, transfer opposite. Filter selects among t
        and b on a frame by frame basis by analyzing the images and selecting
        the alternative that produces best match between the fields.

    T
        Capture top-first, transfer unknown or varying. Filter selects among t
        and p using image analysis.

    B
        Capture bottom-first, transfer unknown or varying. Filter selects
        among b and p using image analysis.

    A
        Capture determined by field flags, transfer unknown or varying. Filter
        selects among t, b and p using field flags and image analysis. If no
        field information is available, then this works just like U. This is
        the default mode.

    U
        Both capture and transfer unknown or varying. Filter selects among t,
        b and p using image analysis only.

    v
        Verbose operation. Prints the selected mode for each frame and the
        average squared difference between fields for t, b, and p
        alternatives.

yadif=[mode[:enabled=yes|no]]
    Yet another deinterlacing filter

    <mode>
        :0: Output 1 frame for each frame.
        :1: Output 1 frame for each field.
        :2: Like 0 but skips spatial interlacing check.
        :3: Like 1 but skips spatial interlacing check.

    <enabled>
        :yes: Filter is active (default).
        :no:  Filter is not active, but can be deactivated with the ``D`` key
              (or any other key that toggles the ``deinterlace`` property).

down3dright[=lines]
    Reposition and resize stereoscopic images. Extracts both stereo fields and
    places them side by side, resizing them to maintain the original movie
    aspect.

    <lines>
        number of lines to select from the middle of the image (default: 12)

delogo[=x:y:w:h:t]
    Suppresses a TV station logo by a simple interpolation of the surrounding
    pixels. Just set a rectangle covering the logo and watch it disappear (and
    sometimes something even uglier appear - your mileage may vary).

    <x>,<y>
        top left corner of the logo
    <w>,<h>
        width and height of the cleared rectangle
    <t>
        Thickness of the fuzzy edge of the rectangle (added to w and h). When
        set to -1, a green rectangle is drawn on the screen to simplify
        finding the right x,y,w,h parameters.
    file=<file>
        You can specify a text file to load the coordinates from.  Each line
        must have a timestamp (in seconds, and in ascending order) and the
        "x:y:w:h:t" coordinates (*t* can be omitted).

screenshot
    Optional filter for screenshot support. This is only needed if the video
    output doesn't provide working direct screenshot support. Note that it is
    not always safe to insert this filter by default. See the
    ``Taking screenshots`` section for details.

sub=[=bottom-margin:top-margin]
    Moves subtitle rendering to an arbitrary point in the filter
    chain, or force subtitle rendering in the video filter as opposed to using
    video output OSD support.


    <bottom-margin>
        Adds a black band at the bottom of the frame. The SSA/ASS renderer can
        place subtitles there (with ``--ass-use-margins``).
    <top-margin>
        Black band on the top for toptitles  (with ``--ass-use-margins``).


    *EXAMPLE*:

    ``--vf=sub,eq``
        Moves sub rendering before the eq filter. This will put both
        subtitle colors and video under the influence of the video equalizer
        settings.

stereo3d[=in:out]
    Stereo3d converts between different stereoscopic image formats.

    <in>
        Stereoscopic image format of input. Possible values:

        sbsl or side_by_side_left_first
            side by side parallel (left eye left, right eye right)
        sbsr or side_by_side_right_first
            side by side crosseye (right eye left, left eye right)
        abl or above_below_left_first
            above-below (left eye above, right eye below)
        abl or above_below_right_first
            above-below (right eye above, left eye below)
        ab2l or above_below_half_height_left_first
            above-below with half height resolution (left eye above, right eye
            below)
        ab2r or above_below_half_height_right_first
            above-below with half height resolution (right eye above, left eye
            below)

    <out>
        Stereoscopic image format of output. Possible values are all the input
        formats as well as:

        arcg or anaglyph_red_cyan_gray
            anaglyph red/cyan gray (red filter on left eye, cyan filter on
            right eye)
        arch or anaglyph_red_cyan_half_color
            anaglyph red/cyan half colored (red filter on left eye, cyan filter
            on right eye)
        arcc or anaglyph_red_cyan_color
            anaglyph red/cyan color (red filter on left eye, cyan filter on
            right eye)
        arcd or anaglyph_red_cyan_dubois
            anaglyph red/cyan color optimized with the least squares
            projection of dubois (red filter on left eye, cyan filter on right
            eye)
        agmg or anaglyph_green_magenta_gray
            anaglyph green/magenta gray (green filter on left eye, magenta
            filter on right eye)
        agmh or anaglyph_green_magenta_half_color
            anaglyph green/magenta half colored (green filter on left eye,
            magenta filter on right eye)
        agmc or anaglyph_green_magenta_color
            anaglyph green/magenta colored (green filter on left eye, magenta
            filter on right eye)
        aybg or anaglyph_yellow_blue_gray
            anaglyph yellow/blue gray (yellow filter on left eye, blue filter
            on right eye)
        aybh or anaglyph_yellow_blue_half_color
            anaglyph yellow/blue half colored (yellow filter on left eye, blue
            filter on right eye)
        aybc or anaglyph_yellow_blue_color
            anaglyph yellow/blue colored (yellow filter on left eye, blue
            filter on right eye)
        irl or interleave_rows_left_first
            Interleaved rows (left eye has top row, right eye starts on next
            row)
        irr or interleave_rows_right_first
            Interleaved rows (right eye has top row, left eye starts on next
            row)
        ml or mono_left
            mono output (left eye only)
        mr or mono_right
            mono output (right eye only)

gradfun[=strength[:radius]]
    Fix the banding artifacts that are sometimes introduced into nearly flat
    regions by truncation to 8bit colordepth. Interpolates the gradients that
    should go where the bands are, and dithers them.

    This filter is designed for playback only. Do not use it prior to lossy
    compression, because compression tends to lose the dither and bring back
    the bands.

    <strength>
        Maximum amount by which the filter will change any one pixel. Also the
        threshold for detecting nearly flat regions (default: 1.2).

    <radius>
        Neighborhood to fit the gradient to. Larger radius makes for smoother
        gradients, but also prevents the filter from modifying pixels near
        detailed regions (default: 16).

dlopen=dll[:a0[:a1[:a2[:a3]]]]
    Loads an external library to filter the image. The library interface
    is the vf_dlopen interface specified using libmpcodecs/vf_dlopen.h.

    dll=<library>
        Specify the library to load. This may require a full file system path
        in some cases! This argument is required.

    a0=<string>
        Specify the first parameter to pass to the library.

    a1=<string>
        Specify the second parameter to pass to the library.

    a2=<string>
        Specify the third parameter to pass to the library.

    a3=<string>
        Specify the fourth parameter to pass to the library.
