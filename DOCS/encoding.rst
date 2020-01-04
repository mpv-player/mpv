General usage
=============

::

  mpv infile -o outfile [-of outfileformat] [-ofopts formatoptions] [-orawts] \
    [(any other mpv options)] \
    -ovc outvideocodec [-ovcopts outvideocodecoptions] \
    -oac outaudiocodec [-oacopts outaudiocodecoptions]

Help for these options is provided if giving help as parameter, as in::

  mpv -ovc help

The suboptions of these generally are identical to ffmpeg's (as option parsing
is simply delegated to ffmpeg). The option -ocopyts enables copying timestamps
from the source as-is, instead of fixing them to match audio playback time
(note: this doesn't work with all output container formats); -orawts even turns
off discontinuity fixing.

Note that if neither -ofps nor -oautofps is specified, VFR encoding is assumed
and the time base is 24000fps. -oautofps sets -ofps to a guessed fps number
from the input video. Note that not all codecs and not all formats support VFR
encoding, and some which do have bugs when a target bitrate is specified - use
-ofps or -oautofps to force CFR encoding in these cases.

Of course, the options can be stored in a profile, like this .config/mpv/mpv.conf
section::

  [myencprofile]
  vf-add = scale=480:-2
  ovc = libx264
  ovcopts-add = preset=medium
  ovcopts-add = tune=fastdecode
  ovcopts-add = crf=23
  ovcopts-add = maxrate=1500k
  ovcopts-add = bufsize=1000k
  ovcopts-add = rc_init_occupancy=900k
  ovcopts-add = refs=2
  ovcopts-add = profile=baseline
  oac = aac
  oacopts-add = b=96k

It's also possible to define default encoding options by putting them into
the section named ``[encoding]``. (This behavior changed after mpv 0.3.x. In
mpv 0.3.x, config options in the default section / no section were applied
to encoding. This is not the case anymore.)

One can then encode using this profile using the command::

  mpv infile -o outfile.mp4 -profile myencprofile

Some example profiles are provided in a file
etc/encoding-profiles.conf; as for this, see below.


Encoding examples
=================

These are some examples of encoding targets this code has been used and tested
for.

Typical MPEG-4 Part 2 ("ASP", "DivX") encoding, AVI container::

  mpv infile -o outfile.avi \
    --vf=fps=25 \
    -ovc mpeg4 -ovcopts qscale=4 \
    -oac libmp3lame -oacopts ab=128k

Note: AVI does not support variable frame rate, so the fps filter must be used.
The frame rate should ideally match the input (25 for PAL, 24000/1001 or
30000/1001 for NTSC)

Typical MPEG-4 Part 10 ("AVC", "H.264") encoding, Matroska (MKV) container::

  mpv infile -o outfile.mkv \
    -ovc libx264 -ovcopts preset=medium,crf=23,profile=baseline \
    -oac libvorbis -oacopts qscale=3

Typical MPEG-4 Part 10 ("AVC", "H.264") encoding, MPEG-4 (MP4) container::

  mpv infile -o outfile.mp4 \
    -ovc libx264 -ovcopts preset=medium,crf=23,profile=baseline \
    -oac aac -oacopts ab=128k

Typical VP8 encoding, WebM (restricted Matroska) container::

  mpv infile -o outfile.mkv \
    -of webm \
    -ovc libvpx -ovcopts qmin=6,b=1000000k \
    -oac libvorbis -oacopts qscale=3


Device targets
==============

As the options for various devices can get complex, profiles can be used.

An example profile file for encoding is provided in
etc/encoding-profiles.conf in the source tree. This file is installed and loaded
by default. If you want to modify it, you can replace and it with your own copy
by doing::

  mkdir -p ~/.mpv
  cp /etc/mpv/encoding-profiles.conf ~/.mpv/encoding-profiles.conf

Keep in mind that the default profile is the playback one. If you want to add
options that apply only in encoding mode, put them into a ``[encoding]``
section.

Refer to the top of that file for more comments - in a nutshell, the following
options are added by it::

  -profile enc-to-dvdpal      DVD-Video PAL, use dvdauthor -v pal+4:3 -a ac3+en
  -profile enc-to-dvdntsc     DVD-Video NTSC, use dvdauthor -v ntsc+4:3 -a ac3+en
  -profile enc-to-bb-9000     MP4 for Blackberry Bold 9000
  -profile enc-to-nok-6300    3GP for Nokia 6300
  -profile enc-to-psp         MP4 for PlayStation Portable
  -profile enc-to-iphone      MP4 for iPhone
  -profile enc-to-iphone-4    MP4 for iPhone 4 (double res)
  -profile enc-to-iphone-5    MP4 for iPhone 5 (even larger res)

You can encode using these with a command line like::

  mpv infile -o outfile.mp4 -profile enc-to-bb-9000

Of course, you are free to override options set by these profiles by specifying
them after the -profile option.


What works
==========

* Encoding at variable frame rate (default)
* Encoding at constant frame rate using --vf=fps=RATE
* 2-pass encoding (specify flags=+pass1 in the first pass's -ovcopts, specify
  flags=+pass2 in the second pass)
* Hardcoding subtitles using vobsub, ass or srt subtitle rendering (just
  configure mpv for the subtitles as usual)
* Hardcoding any other mpv OSD (e.g. time codes, using -osdlevel 3 and -vf
  expand=::::1)
* Encoding directly from a DVD, network stream, webcam, or any other source
  mpv supports
* Using x264 presets/tunings/profiles (by using profile=, tune=, preset= in the
  -ovcopts)
* Deinterlacing/Inverse Telecine with any of mpv's filters for that
* Audio file converting: mpv -o outfile.mp3 infile.flac -no-video -oac
  libmp3lame -oacopts ab=320k

What does not work yet
======================

* 3-pass encoding (ensuring constant total size and bitrate constraints while
  having VBR audio; mencoder calls this "frameno")
* Direct stream copy
