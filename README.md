![http://mpv.io/](https://raw.githubusercontent.com/mpv-player/mpv.io/master/source/images/mpv-logo-128.png)

## mpv

--------------


* [External links](#external-links)
* [Overview](#overview)
* [System requirements](#system-requirements)
* [Downloads](#downloads)
* [Changelog](#changelog)
* [Compilation](#compilation)
* [FFmpeg vs. Libav](#ffmpeg-vs-libav)
* [FFmpeg ABI compatibility](#ffmpeg-abi-compatibility)
* [Release cycle](#release-cycle)
* [Bug reports](#bug-reports)
* [Contributing](#contributing)
* [Relation to MPlayer and mplayer2](#relation-to-mplayer-and-mplayer2)
* [License](#license)
* [Contact](#contact)


## External links


* [Wiki](https://github.com/mpv-player/mpv/wiki)
* [FAQ](https://github.com/mpv-player/mpv/wiki/FAQ)
* [Manual](http://mpv.io/manual/master/)


## Overview


**mpv** is a media player based on MPlayer and mplayer2. It supports a wide
variety of video file formats, audio and video codecs, and subtitle types.

Releases can be found on the [release list][releases].

## System requirements

- A not too ancient Linux, Windows 7 or later, or OSX 10.8 or later.
- A somewhat capable CPU. Hardware decoding might help if the CPU is too slow to
  decode video in realtime, but must be explicitly enabled with the `--hwdec`
  option.
- A not too crappy GPU. mpv is not intended to be used with bad GPUs. There are
  many caveats with drivers or system compositors causing tearing, stutter,
  etc. On Windows, you might want to make sure the graphics drivers are
  current. In some cases, ancient fallback video output methods can help
  (such as `--vo=xv` on Linux), but this use is not recommended or supported.


## Downloads


For semi-official builds and third-party packages please see
[mpv.io/installation](http://mpv.io/installation/).

## Changelog


There is no complete changelog; however, changes to the player core interface
are listed in the [interface changelog][interface-changes].

Changes to the C API are documented in the [client API changelog][api-changes].

The [release list][releases] has a summary of most of the important changes
on every release.

Changes to the default key bindings are indicated in
[restore-old-bindings.conf][restore-old-bindings].

## Compilation


Compiling with full features requires development files for several
external libraries. Below is a list of some important requirements.

The mpv build system uses [waf](https://waf.io/), but we don't store it in the
repository. The `./bootstrap.py` script will download the latest version
of waf that was tested with the build system.

For a list of the available build options use `./waf configure --help`. If
you think you have support for some feature installed but configure fails to
detect it, the file `build/config.log` may contain information about the
reasons for the failure.

NOTE: To avoid cluttering the output with unreadable spam, `--help` only shows
one of the two switches for each option. If the option is autodetected by
default, the `--disable-***` switch is printed; if the option is disabled by
default, the `--enable-***` switch is printed. Either way, you can use
`--enable-***` or `--disable-**` regardless of what is printed by `--help`.

To build the software you can use `./waf build`: the result of the compilation
will be located in `build/mpv`. You can use `./waf install` to install mpv
to the *prefix* after it is compiled.

Example:

    ./bootstrap.py
    ./waf configure
    ./waf
    ./waf install

Essential dependencies (incomplete list):

- gcc or clang
- X development headers (xlib, xrandr, xext, xscrnsaver, xinerama, libvdpau,
  libGL, GLX, EGL, xv, ...)
- Audio output development headers (libasound/ALSA, pulseaudio)
- FFmpeg libraries (libavutil libavcodec libavformat libswscale libavfilter
  and either libswresample or libavresample)
- zlib
- iconv (normally provided by the system libc)
- libass (OSD, OSC, text subtitles)
- Lua (optional, required for the OSC pseudo-GUI and youtube-dl integration)
- libjpeg (optional, used for screenshots only)
- uchardet (optional, for subtitle charset detection)
- vdpau and vaapi libraries for hardware decoding on Linux (optional)

Libass dependencies:

- gcc or clang, yasm on x86 and x86_64
- fribidi, freetype, fontconfig development headers (for libass)
- harfbuzz (optional, required for correct rendering of combining characters,
  particularly for correct rendering of non-English text on OSX, and
  Arabic/Indic scripts on any platform)

FFmpeg dependencies:

- gcc or clang, yasm on x86 and x86_64
- OpenSSL or GnuTLS (have to be explicitly enabled when compiling FFmpeg)
- libx264/libmp3lame/libfdk-aac if you want to use encoding (have to be
  explicitly enabled when compiling FFmpeg)
- For native DASH playback, FFmpeg needs to be built with --enable-libxml2
  (although there are security implications).
- For good nvidia support on Linux, make sure nv-codec-headers is installed
  and can be found by configure.
- Libav support is broken. (See section below.)

Most of the above libraries are available in suitable versions on normal
Linux distributions. For ease of compiling the latest git master of everything,
you may wish to use the separately available build wrapper ([mpv-build][mpv-build])
which first compiles FFmpeg libraries and libass, and then compiles the player
statically linked against those.

If you want to build a Windows binary, you either have to use MSYS2 and MinGW,
or cross-compile from Linux with MinGW. See
[Windows compilation][windows_compilation].


## FFmpeg vs. Libav


Generally, mpv should work with the latest release as well as the git version
of FFmpeg. Libav support is currently broken, because they did not add certain
FFmpeg API changes which mpv relies on.


## FFmpeg ABI compatibility

mpv does not support linking against FFmpeg versions it was not built with, even
if the linked version is supposedly ABI-compatible with the version it was
compiled against. Expect malfunctions, crashes, and security issues if you
do it anyway.

The reason for not supporting this is because it creates far too much complexity
with little to no benefit, coupled with absurd and unusable FFmpeg API
artifacts.

Newer mpv versions will refuse to start if runtime and compile time FFmpeg
library versions mismatch.

## Release cycle

Every other month, an arbitrary git snapshot is made, and is assigned
a 0.X.0 version number. No further maintenance is done.

The goal of releases is to make Linux distributions happy. Linux distributions
are also expected to apply their own patches in case of bugs and security
issues.

Releases other than the latest release are unsupported and unmaintained.

See the [release policy document][release-policy] for more information.

## Bug reports


Please use the [issue tracker][issue-tracker] provided by GitHub to send us bug
reports or feature requests. Follow the template's instructions or the issue
will likely be ignored or closed as invalid.

Using the bug tracker as place for simple questions is fine but IRC is
recommended (see [Contact](#Contact) below).

## Contributing


Please read [contribute.md][contribute.md].

For small changes you can just send us pull requests through GitHub. For bigger
changes come and talk to us on IRC before you start working on them. It will
make code review easier for both parties later on.

You can check [the wiki](https://github.com/mpv-player/mpv/wiki/Stuff-to-do)
or the [issue tracker](https://github.com/mpv-player/mpv/issues?q=is%3Aopen+is%3Aissue+label%3A%22feature+request%22)
for ideas on what you could contribute with.

## Relation to MPlayer and mplayer2

mpv is a fork of MPlayer. Much has changed, and in general, mpv should be
considered a completely new program, rather than a MPlayer drop-in replacement.

For details see [FAQ entry](https://github.com/mpv-player/mpv/wiki/FAQ#How_is_mpv_related_to_MPlayer).

If you are wondering what's different from mplayer2 and MPlayer, an incomplete
and largely unmaintained list of changes is located [here][mplayer-changes].

## License

GPLv2 "or later" by default, LGPLv2.1 "or later" with `--enable-lgpl`.
See [details.](https://github.com/mpv-player/mpv/blob/master/Copyright)


## Contact


Most activity happens on the IRC channel and the github issue tracker.

 - **GitHub issue tracker**: [issue tracker][issue-tracker] (report bugs here)
 - **User IRC Channel**: `#mpv` on `irc.freenode.net`
 - **Developer IRC Channel**: `#mpv-devel` on `irc.freenode.net`

To contact the `mpv` team in private write to `mpv-team@googlegroups.com`. Use
only if discretion is required.

[releases]: https://github.com/mpv-player/mpv/releases
[mpv-build]: https://github.com/mpv-player/mpv-build
[homebrew-mpv]: https://github.com/mpv-player/homebrew-mpv
[issue-tracker]:  https://github.com/mpv-player/mpv/issues
[ffmpeg_vs_libav]: https://github.com/mpv-player/mpv/wiki/FFmpeg-versus-Libav
[release-policy]: https://github.com/mpv-player/mpv/blob/master/DOCS/release-policy.md
[windows_compilation]: https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md
[mplayer-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/mplayer-changes.rst
[interface-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/interface-changes.rst
[api-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/client-api-changes.rst
[restore-old-bindings]: https://github.com/mpv-player/mpv/blob/master/etc/restore-old-bindings.conf
[contribute.md]: https://github.com/mpv-player/mpv/blob/master/DOCS/contribute.md
