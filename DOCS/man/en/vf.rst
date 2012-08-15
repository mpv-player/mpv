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

scale[=w:h[:interlaced[:chr_drop[:par[:par2[:presize[:noup[:arnd]]]]]]]]
    Scales the image with the software scaler (slow) and performs a YUV<->RGB
    colorspace conversion (see also ``--sws``).

    <w>,<h>
        scaled width/height (default: original width/height)

        *NOTE*: If ``--zoom`` is used, and underlying filters (including
        libvo) are incapable of scaling, it defaults to d_width/d_height!

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

    <presize>
        Scale to preset sizes.

        :qntsc: 352x240 (NTSC quarter screen)
        :qpal:  352x288 (PAL quarter screen)
        :ntsc:  720x480 (standard NTSC)
        :pal:   720x576 (standard PAL)
        :sntsc: 640x480 (square pixel NTSC)
        :spal:  768x576 (square pixel PAL)

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

yvu9
    Forces software YVU9 to YV12 colorspace conversion. Deprecated in favor of
    the software scaler.

yuvcsp
    Clamps YUV color values to the CCIR 601 range without doing real
    conversion.

palette
    RGB/BGR 8 -> 15/16/24/32bpp colorspace conversion using palette.

format[=fourcc[:outfourcc]]
    Restricts the colorspace for the next filter without doing any conversion.
    Use together with the scale filter for a real conversion.

    *NOTE*: For a list of available formats see ``format=fmt=help``.

    <fourcc>
        format name like rgb15, bgr24, yv12, etc (default: yuy2)
    <outfourcc>
        Format name that should be substituted for the output. If this is not
        100% compatible with the <fourcc> value it will crash.

        *EXAMPLE*

        ====================== =====================
        Valid                  Invalid (will crash)
        ====================== =====================
        ``format=rgb24:bgr24`` ``format=rgb24:yv12``
        ``format=yuyv:yuy2``
        ====================== =====================

noformat[=fourcc]
    Restricts the colorspace for the next filter without doing any conversion.
    Unlike the format filter, this will allow any colorspace except the one
    you specify.

    *NOTE*: For a list of available formats see ``noformat=fmt=help``.

    <fourcc>
        format name like rgb15, bgr24, yv12, etc (default: yv12)

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

spp[=quality[:qp[:mode]]]
    Simple postprocessing filter that compresses and decompresses the image at
    several (or - in the case of quality level 6 - all) shifts and averages
    the results.

    <quality>
        0-6 (default: 3)

    <qp>
        Force quantization parameter (default: 0, use QP from video).

    <mode>

        :0: hard thresholding (default)
        :1: soft thresholding (better deringing, but blurrier)
        :4: like 0, but also use B-frames' QP (may cause flicker)
        :5: like 1, but also use B-frames' QP (may cause flicker)

uspp[=quality[:qp]]
    Ultra simple & slow postprocessing filter that compresses and decompresses
    the image at several (or - in the case of quality level 8 - all) shifts
    and averages the results.

    The way this differs from the behavior of spp is that uspp actually
    encodes & decodes each case with libavcodec Snow, whereas spp uses a
    simplified intra only 8x8 DCT similar to MJPEG.

    <quality>
        0-8 (default: 3)

    <qp>
        Force quantization parameter (default: 0, use QP from video).

fspp[=quality[:qp[:strength[:bframes]]]]
    faster version of the simple postprocessing filter

    <quality>
        4-5 (equivalent to spp; default: 4)

    <qp>
        Force quantization parameter (default: 0, use QP from video).

    <-15-32>
        Filter strength, lower values mean more details but also more
        artifacts, while higher values make the image smoother but also
        blurrier (default: 0 - PSNR optimal).

    <bframes>
        0: do not use QP from B-frames (default)
        1: use QP from B-frames too (may cause flicker)

pp7[=qp[:mode]]
    Variant of the spp filter, similar to spp=6 with 7 point DCT where only
    the center sample is used after IDCT.

    <qp>
        Force quantization parameter (default: 0, use QP from video).

    <mode>
        :0: hard thresholding
        :1: soft thresholding (better deringing, but blurrier)
        :2: medium thresholding (default, good results)

qp=equation
    quantization parameter (QP) change filter

    <equation>
        some equation like ``2+2*sin(PI*qp)``

geq=equation
    generic equation change filter

    <equation>
        Some equation, e.g. ``p(W-X\,Y)`` to flip the image horizontally. You
        can use whitespace to make the equation more readable. There are a
        couple of constants that can be used in the equation:

        :PI:      the number pi
        :E:       the number e
        :X / Y:   the coordinates of the current sample
        :W / H:   width and height of the image
        :SW / SH: width/height scale depending on the currently filtered plane,
                  e.g. 1,1 and 0.5,0.5 for YUV 4:2:0.
        :p(x,y):  returns the value of the pixel at location x/y of the current
                  plane.

test
    Generate various test patterns.

rgbtest[=width:height]
    Generate an RGB test pattern useful for detecting RGB vs BGR issues. You
    should see a red, green and blue stripe from top to bottom.

    <width>
        Desired width of generated image (default: 0). 0 means width of input
        image.

    <height>
        Desired height of generated image (default: 0). 0 means height of
        input image.

lavc[=quality:fps]
    Fast software YV12 to MPEG-1 conversion with libavcodec for use with
    DVB/DXR3/IVTV/V4L2.

    <quality>
        :1-31: fixed qscale
        :32-:  fixed bitrate in kbits

    <fps>
        force output fps (float value) (default: 0, autodetect based on height)

dvbscale[=aspect]
    Set up optimal scaling for DVB cards, scaling the x axis in hardware and
    calculating the y axis scaling in software to keep aspect. Only useful
    together with expand and scale.

    <aspect>
        Control aspect ratio, calculate as ``DVB_HEIGHT*ASPECTRATIO`` (default:
        ``576*4/3=768``), set it to ``576*(16/9)=1024`` for a 16:9 TV.

    *EXAMPLE*:

    ``--vf=dvbscale,scale=-1:0,expand=-1:576:-1:-1:1,lavc``
        FIXME: Explain what this does.

noise[=luma[u][t|a][h][p]:chroma[u][t|a][h][p]]
    Adds noise.

    :<0-100>: luma noise
    :<0-100>: chroma noise
    :u:       uniform noise (gaussian otherwise)
    :t:       temporal noise (noise pattern changes between frames)
    :a:       averaged temporal noise (smoother, but a lot slower)
    :h:       high quality (slightly better looking, slightly slower)
    :p:       mix random noise with a (semi)regular pattern

denoise3d[=luma_spatial:chroma_spatial:luma_tmp:chroma_tmp]
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

hqdn3d[=luma_spatial:chroma_spatial:luma_tmp:chroma_tmp]
    High precision/quality version of the denoise3d filter. Parameters and
    usage are the same.

ow[=depth[:luma_strength[:chroma_strength]]]
    Overcomplete Wavelet denoiser.

    <depth>
        Larger depth values will denoise lower frequency components more, but
        slow down filtering (default: 8).
    <luma_strength>
        luma strength (default: 1.0)
    <chroma_strength>
        chroma strength (default: 1.0)

eq[=brightness:contrast] (OBSOLETE)
    Software equalizer with interactive controls just like the hardware
    equalizer, for cards/drivers that do not support brightness and contrast
    controls in hardware.

    <-100-100>
        initial brightness
    <-100-100>
        initial contrast

eq2[=gamma:contrast:brightness:saturation:rg:gg:bg:weight]
    Alternative software equalizer that uses lookup tables (very slow),
    allowing gamma correction in addition to simple brightness and contrast
    adjustment. Note that it uses the same MMX optimized code as ``--vf=eq``
    if all gamma values are 1.0. The parameters are given as floating point
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

hue[=hue:saturation]
    Software equalizer with interactive controls just like the hardware
    equalizer, for cards/drivers that do not support hue and saturation
    controls in hardware.

    <-180-180>
        initial hue (default: 0.0)
    <-100-100>
        initial saturation, where negative values result in a negative chroma
        (default: 1.0)

halfpack[=f]
    Convert planar YUV 4:2:0 to half-height packed 4:2:2, downsampling luma
    but keeping all chroma samples. Useful for output to low-resolution
    display devices when hardware downscaling is poor quality or is not
    available. Can also be used as a primitive luma-only deinterlacer with
    very low CPU usage.

    <f>
        By default, halfpack averages pairs of lines when downsampling. Any
        value different from 0 or 1 gives the default (averaging) behavior.

        :0: Only use even lines when downsampling.
        :1: Only use odd lines when downsampling.

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

decimate[=max:hi:lo:frac]
    Drops frames that do not differ greatly from the previous frame in order
    to reduce framerate. The main use of this filter is for very-low- bitrate
    encoding (e.g. streaming over dialup modem), but it could in theory be
    used for fixing movies that were inverse-telecined incorrectly.

    <max>
        Sets the maximum number of consecutive frames which can be dropped (if
        positive), or the minimum interval between dropped frames (if
        negative).
    <hi>,<lo>,<frac>
        A frame is a candidate for dropping if no 8x8 region differs by more
        than a threshold of <hi>, and if not more than <frac> portion (1
        meaning the whole image) differs by more than a threshold of <lo>.
        Values of <hi> and <lo> are for 8x8 pixel blocks and represent actual
        pixel value differences, so a threshold of 64 corresponds to 1 unit of
        difference for each pixel, or the same spread out differently over the
        block.

dint[=sense:level]
    The drop-deinterlace (dint) filter detects and drops the first from a set
    of interlaced video frames.

    <0.0-1.0>
        relative difference between neighboring pixels (default: 0.1)
    <0.0-1.0>
        What part of the image has to be detected as interlaced to drop the
        frame (default: 0.15).

lavcdeint (OBSOLETE)
    FFmpeg deinterlacing filter, same as ``--vf=pp=fd``

kerndeint[=thresh[:map[:order[:sharp[:twoway]]]]]
    Donald Graft's adaptive kernel deinterlacer. Deinterlaces parts of a video
    if a configurable threshold is exceeded.

    <0-255>
        threshold (default: 10)
    <map>
        :0: Ignore pixels exceeding the threshold (default).
        :1: Paint pixels exceeding the threshold white.

    <order>
        :0: Leave fields alone (default).
        :1: Swap fields.

    <sharp>
        :0: Disable additional sharpening (default).
        :1: Enable additional sharpening.

    <twoway>
        :0: Disable twoway sharpening (default).
        :1: Enable twoway sharpening.

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

il[=d|i][s][:[d|i][s]]
    (De)interleaves lines. The goal of this filter is to add the ability to
    process interlaced images pre-field without deinterlacing them. You can
    filter your interlaced DVD and play it on a TV without breaking the
    interlacing. While deinterlacing (with the postprocessing filter) removes
    interlacing permanently (by smoothing, averaging, etc) deinterleaving
    splits the frame into 2 fields (so called half pictures), so you can
    process (filter) them independently and then re-interleave them.

    :d: deinterleave (placing one above the other)
    :i: interleave
    :s: swap fields (exchange even & odd lines)

fil[=i|d]
    (De)interleaves lines. This filter is very similar to the il filter but
    much faster, the main disadvantage is that it does not always work.
    Especially if combined with other filters it may produce randomly messed
    up images, so be happy if it works but do not complain if it does not for
    your combination of filters.

    :d: Deinterleave fields, placing them side by side.
    :i: Interleave fields again (reversing the effect of fil=d).

field[=n]
    Extracts a single field from an interlaced image using stride arithmetic
    to avoid wasting CPU time. The optional argument n specifies whether to
    extract the even or the odd field (depending on whether n is even or odd).

detc[=var1=value1:var2=value2:...]
    Attempts to reverse the 'telecine' process to recover a clean,
    non-interlaced stream at film framerate. This was the first and most
    primitive inverse telecine filter to be added to MPlayer. It works by
    latching onto the telecine 3:2 pattern and following it as long as
    possible. This makes it suitable for perfectly-telecined material, even in
    the presence of a fair degree of noise, but it will fail in the presence
    of complex post-telecine edits. Development on this filter is no longer
    taking place, as ivtc, pullup, and filmdint are better for most
    applications. The following arguments (see syntax above) may be used to
    control detc's behavior:

    <dr>
        Set the frame dropping mode.

        :0: Do not drop frames to maintain fixed output framerate (default).
        :1: Always drop a frame when there have been no drops or telecine
            merges in the past 5 frames.
        :2: Always maintain exact 5:4 input to output frame ratio.

    <am>
        Analysis mode.

        :0: Fixed pattern with initial frame number specified by <fr>.
        :1: aggressive search for telecine pattern (default)

    <fr>
        Set initial frame number in sequence. 0-2 are the three clean
        progressive frames; 3 and 4 are the two interlaced frames. The
        default, -1, means 'not in telecine sequence'. The number specified
        here is the type for the imaginary previous frame before the movie
        starts.

    <t0>, <t1>, <t2>, <t3>
        Threshold values to be used in certain modes.

ivtc[=1]
    Experimental 'stateless' inverse telecine filter. Rather than trying to
    lock on to a pattern like the detc filter does, ivtc makes its decisions
    independently for each frame. This will give much better results for
    material that has undergone heavy editing after telecine was applied, but
    as a result it is not as forgiving of noisy input, for example TV capture.
    The optional parameter (ivtc=1) corresponds to the dr=1 option for the
    detc filter, and should not be used with MPlayer. Further development on
    ivtc has stopped, as the pullup and filmdint filters appear to be much
    more accurate.

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

filmdint[=options]
    Inverse telecine filter, similar to the pullup filter above. It is
    designed to handle any pulldown pattern, including mixed soft and hard
    telecine and limited support for movies that are slowed down or sped up
    from their original framerate for TV. Only the luma plane is used to find
    the frame breaks. If a field has no match, it is deinterlaced with simple
    linear approximation. If the source is MPEG-2, this must be the first
    filter to allow access to the field-flags set by the MPEG-2 decoder.
    Depending on the source MPEG, you may be fine ignoring this advice, as
    long as you do not see lots of "Bottom-first field" warnings. With no
    options it does normal inverse telecine. When this filter is used with
    MPlayer, it will result in an uneven framerate during playback, but it is
    still generally better than using pp=lb or no deinterlacing at all.
    Multiple options can be specified separated by /.

    crop=<w>:<h>:<x>:<y>
        Just like the crop filter, but faster, and works on mixed hard and
        soft telecined content as well as when y is not a multiple of 4. If x
        or y would require cropping fractional pixels from the chroma planes,
        the crop area is extended. This usually means that x and y must be
        even.

    io=<ifps>:<ofps>
        For each ifps input frames the filter will output ofps frames. This
        could be used to filter movies that are broadcast on TV at a frame
        rate different from their original framerate.

    luma_only=<n>
        If n is nonzero, the chroma plane is copied unchanged. This is useful
        for YV12 sampled TV, which discards one of the chroma fields.

    mmx2=<n>
        On x86, if n=1, use MMX2 optimized functions, if n=2, use 3DNow!
        optimized functions, otherwise, use plain C. If this option is not
        specified, MMX2 and 3DNow! are auto-detected, use this option to
        override auto-detection.

    fast=<n>
        The larger n will speed up the filter at the expense of accuracy. The
        default value is n=3. If n is odd, a frame immediately following a
        frame marked with the REPEAT_FIRST_FIELD MPEG flag is assumed to be
        progressive, thus filter will not spend any time on soft-telecined
        MPEG-2 content. This is the only effect of this flag if MMX2 or 3DNow!
        is available. Without MMX2 and 3DNow, if n=0 or 1, the same
        calculations will be used as with n=2 or 3. If n=2 or 3, the number of
        luma levels used to find the frame breaks is reduced from 256 to 128,
        which results in a faster filter without losing much accuracy. If n=4
        or 5, a faster, but much less accurate metric will be used to find the
        frame breaks, which is more likely to misdetect high vertical detail
        as interlaced content.

    verbose=<n>
        If n is nonzero, print the detailed metrics for each frame. Useful for
        debugging.

    dint_thres=<n>
        Deinterlace threshold. Used during de-interlacing of unmatched frames.
        Larger value means less deinterlacing, use n=256 to completely turn
        off deinterlacing. Default is n=8.

    comb_thres=<n>
        Threshold for comparing a top and bottom fields. Defaults to 128.

    diff_thres=<n>
        Threshold to detect temporal change of a field. Default is 128.

    sad_thres=<n>
        Sum of Absolute Difference threshold, default is 64.

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
    away. Use ``--nosound --ovc=raw -o /dev/null`` to avoid wasting CPU power
    for this pass. You may add something like ``crop=2:2:0:0`` after divtc to
    speed things up even more. Then use divtc pass two for the actual
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

telecine[=start]
    Apply 3:2 'telecine' process to increase framerate by 20%. This most
    likely will not work correctly with MPlayer. The optional start parameter
    tells the filter where in the telecine pattern to start (0-3).

tinterlace[=mode]
    Temporal field interlacing - merge pairs of frames into an interlaced
    frame, halving the framerate. Even frames are moved into the upper field,
    odd frames to the lower field. This can be used to fully reverse the
    effect of the tfields filter (in mode 0). Available modes are:

    :0: Move odd frames into the upper field, even into the lower field,
        generating a full-height frame at half framerate.
    :1: Only output odd frames, even frames are dropped; height unchanged.
    :2: Only output even frames, odd frames are dropped; height unchanged.
    :3: Expand each frame to full height, but pad alternate lines with black;
        framerate unchanged.
    :4: Interleave even lines from even frames with odd lines from odd frames.
        Height unchanged at half framerate.

tfields[=mode[:field_dominance]]
    Temporal field separation - split fields into frames, doubling the output
    framerate.

    <mode>
        :0: Leave fields unchanged (will jump/flicker).
        :1: Interpolate missing lines. (The algorithm used might not be so
            good.)
        :2: Translate fields by 1/4 pixel with linear interpolation (no jump).
        :4: Translate fields by 1/4 pixel with 4tap filter (higher quality)
            (default).

    <field_dominance> (DEPRECATED)
        :-1: auto (default) Only works if the decoder exports the appropriate
             information and no other filters which discard that information
             come before tfields in the filter chain, otherwise it falls back
             to 0 (top field first).
        :0:  top field first
        :1:  bottom field first

        *NOTE*: This option will possibly be removed in a future version. Use
        ``--field-dominance`` instead.

yadif=[mode[:field_dominance]]
    Yet another deinterlacing filter

    <mode>
        :0: Output 1 frame for each frame.
        :1: Output 1 frame for each field.
        :2: Like 0 but skips spatial interlacing check.
        :3: Like 1 but skips spatial interlacing check.

    <field_dominance> (DEPRECATED)
        Operates like tfields.

        *NOTE*: This option will possibly be removed in a future version. Use
        ``--field-dominance`` instead.

mcdeint=[mode[:parity[:qp]]]
    Motion compensating deinterlacer. It needs one field per frame as input
    and must thus be used together with tfields=1 or yadif=1/3 or equivalent.

    <mode>
        :0: fast
        :1: medium
        :2: slow, iterative motion estimation
        :3: extra slow, like 2 plus multiple reference frames

    <parity>
        0 or 1 selects which field to use (note: no autodetection yet!).

    <qp>
        Higher values should result in a smoother motion vector field but less
        optimal individual vectors.

boxblur=radius:power[:radius:power]
    box blur

    <radius>
        blur filter strength
    <power>
        number of filter applications

sab=radius:pf:colorDiff[:radius:pf:colorDiff]
    shape adaptive blur

    <radius>
        blur filter strength (~0.1-4.0) (slower if larger)
    <pf>
        prefilter strength (~0.1-2.0)
    <colorDiff>
        maximum difference between pixels to still be considered (~0.1-100.0)

smartblur=radius:strength:threshold[:radius:strength:threshold]
    smart blur

    <radius>
        blur filter strength (~0.1-5.0) (slower if larger)
    <strength>
        blur (0.0-1.0) or sharpen (-1.0-0.0)
    <threshold>
        filter all (0), filter flat areas (0-30) or filter edges (-30-0)

perspective=x0:y0:x1:y1:x2:y2:x3:y3:t
    Correct the perspective of movies not filmed perpendicular to the screen.

    <x0>,<y0>,...
        coordinates of the top left, top right, bottom left, bottom right
        corners
    <t>
        linear (0) or cubic resampling (1)

2xsai
    Scale and smooth the image with the 2x scale and interpolate algorithm.

1bpp
    1bpp bitmap to YUV/BGR 8/15/16/32 conversion

down3dright[=lines]
    Reposition and resize stereoscopic images. Extracts both stereo fields and
    places them side by side, resizing them to maintain the original movie
    aspect.

    <lines>
        number of lines to select from the middle of the image (default: 12)

bmovl=hidden:opaque:fifo
    The bitmap overlay filter reads bitmaps from a FIFO and displays them on
    top of the movie, allowing some transformations on the image. See also
    ``TOOLS/bmovl-test.c`` for a small bmovl test program.

    <hidden>
        Set the default value of the 'hidden' flag (0=visible, 1=hidden).
    <opaque>
        Set the default value of the 'opaque' flag (0=transparent, 1=opaque).
    <fifo>
        path/filename for the FIFO (named pipe connecting ``mplayer
        --vf=bmovl`` to the controlling application)

    FIFO commands are:

    RGBA32 width height xpos ypos alpha clear
        followed by width*height*4 Bytes of raw RGBA32 data.
    ABGR32 width height xpos ypos alpha clear
        followed by width*height*4 Bytes of raw ABGR32 data.
    RGB24 width height xpos ypos alpha clear
        followed by width*height*3 Bytes of raw RGB24 data.
    BGR24 width height xpos ypos alpha clear
        followed by width*height*3 Bytes of raw BGR24 data.
    ALPHA width height xpos ypos alpha
        Change alpha transparency of the specified area.
    CLEAR width height xpos ypos
        Clear area.
    OPAQUE
        Disable all alpha transparency. Send "ALPHA 0 0 0 0 0" to enable it
        again.
    HIDE
        Hide bitmap.
    SHOW
        Show bitmap.

    Arguments are:

    <width>, <height>
        image/area size
    <xpos>, <ypos>
        Start blitting at position x/y.
    <alpha>
        Set alpha difference. If you set this to -255 you can then send a
        sequence of ALPHA-commands to set the area to -225, -200, -175 etc for
        a nice fade-in-effect! ;)

        :0:    same as original
        :255:  Make everything opaque.
        :-255: Make everything transparent.

    <clear>
        Clear the framebuffer before blitting.

        :0: The image will just be blitted on top of the old one, so you do
            not need to send 1.8MB of RGBA32 data every time a small part of
            the screen is updated.
        :1: clear

framestep=I|[i]step
    Renders only every nth frame or every intra frame (keyframe).

    If you call the filter with I (uppercase) as the parameter, then *only*
    keyframes are rendered. For DVDs it generally means one in every 15/12
    frames (IBBPBBPBBPBBPBB), for AVI it means every scene change or every
    keyint value.

    When a keyframe is found, an 'I!' string followed by a newline character
    is printed, leaving the current line of MPlayer output on the screen,
    because it contains the time (in seconds) and frame number of the keyframe
    (You can use this information to split the AVI.).

    If you call the filter with a numeric parameter 'step' then only one in
    every 'step' frames is rendered.

    If you put an 'i' (lowercase) before the number then an 'I!' is printed
    (like the I parameter).

    If you give only the i then nothing is done to the frames, only I! is
    printed.

tile=xtiles:ytiles:output:start:delta
    Tile a series of images into a single, bigger image. If you omit a
    parameter or use a value less than 0, then the default value is used. You
    can also stop when you are satisfied (``... --vf=tile=10:5 ...``). It is
    probably a good idea to put the scale filter before the tile :-)

    The parameters are:

    <xtiles>
        number of tiles on the x axis (default: 5)
    <ytiles>
        number of tiles on the y axis (default: 5)
    <output>
        Render the tile when 'output' number of frames are reached, where
        'output' should be a number less than xtile * ytile. Missing tiles are
        left blank. You could, for example, write an 8 * 7 tile every 50
        frames to have one image every 2 seconds @ 25 fps.
    <start>
        outer border thickness in pixels (default: 2)
    <delta>
        inner border thickness in pixels (default: 4)

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

remove-logo=/path/to/logo_bitmap_file_name.pgm
    Suppresses a TV station logo, using a PGM or PPM image file to determine
    which pixels comprise the logo. The width and height of the image file
    must match those of the video stream being processed. Uses the filter
    image and a circular blur algorithm to remove the logo.

    ``/path/to/logo_bitmap_file_name.pgm``
        [path] + filename of the filter image.

screenshot
    Optional filter for screenshot support. This is only needed if the video
    output doesn't provide working direct screenshot support. Note that it is
    not always safe to insert this filter by default. See the
    ``Taking screenshots`` section for details.

ass
    Moves SSA/ASS subtitle rendering to an arbitrary point in the filter
    chain, or force subtitle rendering in the video filter as opposed to using
    video output EOSD support. See the ``--ass`` option.

    *EXAMPLE*:

    ``--vf=ass,eq``
        Moves SSA/ASS rendering before the eq filter. This will put both
        subtitle colors and video under the influence of the video equalizer
        settings.

blackframe[=amount:threshold]
    Detect frames that are (almost) completely black. Can be useful to detect
    chapter transitions or commercials. Output lines consist of the frame
    number of the detected frame, the percentage of blackness, the frame type
    and the frame number of the last encountered keyframe.

    <amount>
        Percentage of the pixels that have to be below the threshold (default:
        98).

    <threshold>
        Threshold below which a pixel value is considered black (default: 32).

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

fixpts[=options]
    Fixes the presentation timestamps (PTS) of the frames. By default, the PTS
    passed to the next filter is dropped, but the following options can change
    that:

    print
        Print the incoming PTS.

    fps=<fps>
        Specify a frame per second value.

    start=<pts>
        Specify an initial value for the PTS.

    autostart=<n>
        Uses the *n*\th incoming PTS as the initial PTS. All previous PTS are
        kept, so setting a huge value or -1 keeps the PTS intact.

    autofps=<n>
        Uses the *n*\th incoming PTS after the end of autostart to determine
        the framerate.

    *EXAMPLE*:

    ``--vf=fixpts=fps=24000/1001,ass,fixpts``
        Generates a new sequence of PTS, uses it for ASS subtitles, then drops
        it. Generating a new sequence is useful when the timestamps are reset
        during the program; this is frequent on DVDs. Dropping it may be
        necessary to avoid confusing encoders.

    *NOTE*: Using this filter together with any sort of seeking (including
    ``--ss``) may make demons fly out of your nose.
