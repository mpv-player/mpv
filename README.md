![http://mpv.io/](https://raw.githubusercontent.com/mpv-player/mpv.io/master/source/images/mpv-logo-128.png)

## mpv

--------------

* [#Overview](#overview)
* [#System requirements](#system-requirements)
* [#Downloads](#downloads)
* [#Changelogs](#changelogs)
* [#Compilation](#compilation)
* [#FFmpeg vs. Libav](#ffmpeg-vs-libav)
* [#Release cycle](#release-cycle)
* [#Bug reports](#bug-reports)
* [#Contributing](#contributing)
* [#Relation to MPlayer and mplayer2](#relation-to-mplayer-and-mplayer2)
* [#Contact](#contact)
* [#License](#license)
* [Wiki](https://github.com/mpv-player/mpv/wiki)
* [FAQ](https://github.com/mpv-player/mpv/wiki/FAQ)
* [Man pages](http://mpv.io/manual/master/)

## Overview


**mpv** is a media player based on MPlayer and mplayer2. It supports a wide
variety of video file formats, audio and video codecs, and subtitle types.

Releases can be found on the [release list][releases].

## System requirements


- A not too ancient Linux, Windows Vista or later, or OSX 10.8 or later.
- A somewhat capable CPU. Hardware decoding might help if the CPU is too
  slow to decode video in realtime, but must be explicitly enabled with
  the `--hwdec` option.
- A not too crappy GPU. mpv is not intended to be used with bad GPUs. There are
  many caveats with drivers or system compositors causing tearing, stutter,
  etc. On Windows, you might want to make sure the graphics drivers are
  current. In some cases, ancient fallback video output methods can help
  (such as `--vo=xv` on Linux), but this use is not recommended or supported.


## Downloads


For semi-official builds and third-party packages please see
[mpv.io/installation](http://mpv.io/installation/).

## Changelogs


Changes to the core player interface are listed in the [interface changelog][interface-changes].

Changes to the C API are documented in the [client API changelog][api-changes].

The major changes of releases are documented in the [release list][releases].

Changes to the default key bindings are indicated in [restore-old-bindings.conf][restore-old-bindings].

## Compilation


Compiling with all features requires the development files of several
external libraries. Below is a list of some important requirements.

The mpv build system uses [Waf](https://waf.io/), but we don't store it in the
source tree. Run the `bootstrap.py` script to download the latest version of
Waf that was tested with the build system.

For a list of the available build options see `./waf configure --help`. If
configure doesn't recognize some features you have installed see `build/config.log`.

NOTE: To avoid cluttering the output, `--help` only shows the switch to change the
default, but you can always use `--enable-*` and `--disable-*`.

To build the software you can use `./waf build`: the result of the compilation
will be located in `build/mpv`. You can use `./waf install` to install mpv
to the *prefix* after it is compiled.

Essential dependencies (incomplete):

- gcc or clang
- X development headers (xlib, xrandr, xext, xscrnsaver, xinerama, libvdpau,
  libGL, GLX, EGL, xv, ...)
- Audio output development headers (libasound/ALSA, pulseaudio)
- FFmpeg libraries (libavutil libavcodec libavformat libswscale libavfilter
  and either libswresample or libavresample)
  At least FFmpeg 3.2.2 or Libav 12 is required.
  For hardware decoding with vaapi and vdpau, FFmpeg 3.3 or Libav git is
  required.
- zlib
- iconv (normally provided by the system libc)
- libass (OSD, OSC, text subtitles)
- Lua (optional, required for the OSC pseudo-GUI and youtube-dl integration)
- libjpeg (optional, used for screenshots only)
- uchardet (optional, for subtitle charset detection)
- vdpau and vaapi libraries for hardware decoding on Linux (optional)
  (FFmpeg 3.3 or Libav git is also required.)

Libass dependencies:

- gcc or clang, yasm on x86 and x86_64
- fribidi, freetype, fontconfig development headers (for libass)
- harfbuzz (optional, required for correct rendering of combining characters,
  particularly for correct rendering of non-English text on OSX, and
  Arabic/Indic scripts on any platform)

FFmpeg dependencies:

- gcc or clang, yasm on x86 and x86_64
- OpenSSL (has to be explicitly enabled when compiling ffmpeg)
- libx264/libmp3lame/libfdk-aac if you want to use encoding (has to be
  explicitly enabled when compiling ffmpeg)
- Libav also works, but some features will not work. (See section below.)

Most of the above libraries are available in suitable versions on normal
Linux distributions. However FFmpeg is an exception (distro versions may be
too old to work at all or work well). For that reason you may want to use
the separately available build wrapper ([mpv-build][mpv-build]) that first
compiles FFmpeg libraries and libass, and then compiles the player statically
linked against those.

If you want to build a Windows binary, you either have to use MSYS2 and MinGW,
or cross-compile from Linux with MinGW. See
[Windows compilation][windows_compilation].


## FFmpeg vs. Libav


Generally, mpv should work with the latest release as well as the git version
of both FFmpeg and Libav. But FFmpeg is preferred, and some mpv features work
with FFmpeg only (subtitle formats in particular).


## Preferred FFmpeg version


Using the latest FFmpeg release (or FFmpeg git master) is strongly recommended.
Older versions are unsupported, even if the build system still happens to
accept them. The main reason mpv still builds with older FFmpeg versions is to
evade arguing with people (users, distros) who insist on using older FFmpeg
versions for no rational reason.

If you want to use a stable FFmpeg release, use the latest release, which has
most likely the best maintenance out of all stable releases. Older releases
are for distros, and at best receive basic changes like fixing critical security
issues or build fixes, and at worst are completely abandoned.

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
reports or feature requests.

## Contributing


For small changes you can just send us pull requests through GitHub. For bigger
changes come and talk to us on IRC before you start working on them. It will
make code review easier for both parties later on.

## Relation to MPlayer and mplayer2


mpv is based on mplayer2, which in turn is based on the original MPlayer
(also called mplayer, mplayer-svn, mplayer1). Many changes have been made, a
large part of which is incompatible or completely changes how the player
behaves. Although there are still many similarities to its ancestors, mpv
should generally be treated as a completely different program.

mpv was forked because we wanted to modernize MPlayer. This includes
removing cruft (including features which stopped making sense 10 years ago),
and of course adding modern features. Such huge and intrusive changes made it
infeasible to work directly with MPlayer, which is mostly focused on
preservation, so a fork had to be made. (Actually, mpv is based on mplayer2,
which already started this process of removing cruft.)

In general, mpv should be considered a completely new program, rather than a
MPlayer drop-in replacement.

If you are wondering what's different from mplayer2 and MPlayer, an incomplete
and now unmaintained list of changes is located [here][mplayer-changes].

## Contact


Most activity happens on the IRC channel and the github issue tracker. The
mailing lists are mostly unused.

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

## License


Mostly GPLv2 or later. See [details.](https://github.com/mpv-player/mpv/blob/master/Copyright)
