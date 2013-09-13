mpv
===

[![Build Status](https://api.travis-ci.org/mpv-player/mpv.png)](https://travis-ci.org/mpv-player/mpv)

Overview
--------

**mpv** is a movie player based on MPlayer and mplayer2. It supports a wide
variety of video file formats, audio and video codecs, and subtitle types.

If you are wondering what's different from mplayer2 and MPlayer you can read
more about the [changes][changes].

Compilation
-----------

Compiling with full features requires development files for several
external libraries. Below is a list of some important requirements. For
more information see the output of `./configure --help` for a list of options,
or look at the list of enabled and disabled features printed after running
`./configure`. If you think you have support for some feature installed
but configure fails to detect it, the file `config.log` may contain
information about the reasons for the failure.

Essential dependencies (incomplete list):

- gcc or clang
- X development headers (xlib, X extensions, libvdpau, libGL, libXv, ...)
- Audio output development headers (libasound, pulseaudio)
- fribidi, freetype, fontconfig development headers (for libass)
- libass
- FFmpeg libraries (libavutil libavcodec libavformat libswscale libpostproc)
- libjpeg
- libquvi if you want to play Youtube videos directly
- libx264/libmp3lame/libfdk-aac if you want to use encoding (has to be
  explicitly enabled when compiling ffmpeg)

Most of the above libraries are available in suitable versions on normal
Linux distributions. However FFmpeg is an exception (distro versions may be
too old to work at all or work well). For that reason you may want to use
the separately available build wrapper ([mpv-build][mpv-build]) that first compiles FFmpeg
libraries and libass, and then compiles the player statically linked against
those.

If you are running Mac OSX and using homebrew we provide [homebrew-mpv][homebrew-mpv], an up
to date formula that compiles mpv with sensible dependencies and defaults for
OSX.

configure --enable-* parameters
-------------------------------

The --enable-* parameters unconditionally force options on, completely
skipping autodetection. This behavior is unlike what you may be used to from
autoconf-based configure scripts that can decide to override you. This greater
level of control comes at a price. You may have to provide the correct compiler
and linker flags yourself.
If you used one of these options and experience a compilation or
linking failure, make sure you have passed the necessary compiler/linker flags
to configure.

Bug reports
-----------

Please use the [issue tracker][issue-tracker] provided by GitHub to send us bug
reports or feature requests.

Contributing
------------

For small changes you can just send us pull requests through GitHub. For bigger
changes come and talk to us on IRC before you start working on them. It will
make code review easier for both parties later on.

Contacts
--------

These forms of contact are meant to ask questions about mpv usage, give
feedback on mpv and discuss it's development.

If possible, please avoid posting bugs here and use the [issue tracker][issue-tracker]
instead.

 - **Users IRC Channel**: `#mpv-player` on `irc.freenode.net`
 - **Users Mailing List**: `mpv-users@googlegroups.com` ([Archive / Subscribe][mpv-users]).
 - **Devel Mailing List**: `mpv-devel@googlegroups.com` ([Archive / Subscribe][mpv-devel])

To contact the `mpv` team in private write to `mpv-team@googlegroups.com`.

[changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/man/en/changes.rst
[mpv-build]: https://github.com/mpv-player/mpv-build
[homebrew-mpv]: https://github.com/mpv-player/homebrew-mpv
[issue-tracker]:  https://github.com/mpv-player/mpv/issues
[mpv-users]: https://groups.google.com/forum/?hl=en#!forum/mpv-users
[mpv-devel]: https://groups.google.com/forum/?hl=en#!forum/mpv-devel
