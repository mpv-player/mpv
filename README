Welcome to MPlayer, The Movie Player. MPlayer can play most standard video
formats out of the box and almost all others with the help of external codecs.
MPlayer currently works best from the command line, but visual feedback for
many functions is available from its onscreen status display (OSD), which is
also used for displaying subtitles. MPlayer also has a GUI with skin support and
several unofficial alternative graphical frontends are available.

MEncoder is a command line video encoder for advanced users that can be built
from the MPlayer source tree. An unofficial graphical frontend exists but is
not included.

This document is for getting you started in a few minutes. It cannot answer all
of your questions. If you have problems, please read the documentation in
DOCS/HTML/en/index.html, which should help you solve most of your problems.
Also read the man page to learn how to use MPlayer.


Requirements:
- You need a working development environment that can compile programs.
  On popular Linux distributions, this means having the glibc development
  package(s) installed.
- To compile MPlayer with X11 support, you need to have the XFree86 development
  packages installed.
- For the GUI you need the libpng and GTK 1.2 development packages.


Before you start...
Unless you know what are you doing, consult DOCS/HTML/en/video.html to see
which driver to use with your video card to get the best quality and
performance. Most cards require special drivers not included with XFree86 to
drive their 2-D video acceleration features like YUV and scaling.

A quick and incomplete list of recommendations:
- ATI cards: Get the GATOS drivers for X11/Xv or use VIDIX.
- Matrox G200/G4x0/G550: Compile and use mga_vid for Linux, on BSD use VIDIX.
- 3dfx Voodoo3/Banshee: Get XFree86 4.2.0+ for Xv or use the tdfxfb driver.
- nVidia cards: Get the X11 driver from www.nvidia.com for Xv support.
- NeoMagic cards: Get an Xv capable driver from our homepage as described in
  DOCS/HTML/en/video.html.

Without accelerated video even an 800MHz P3 may be too slow to play DVDs.


______________________
STEP0: Getting MPlayer
~~~~~~~~~~~~~~~~~~~~~~

Official releases, prereleases and CVS snapshots, as well as fonts for the
OSD, codec packages and a number of different skins for the GUI are available
from the download section of our homepage at

  http://www.mplayerhq.hu/homepage/dload.html

A set of fonts is necessary for the OSD and subtitles unless you are using
TrueType fonts, the GUI needs at least one skin and codec packages add support
for some more video and audio formats. MPlayer does not come with any of these
by default, you have to download and install them separately.

You can also get MPlayer via anonymous CVS. Issue the following commands to get
the latest sources:

  cvs -d:pserver:anonymous@mplayerhq.hu:/cvsroot/mplayer login
  cvs -z3 -d:pserver:anonymous@mplayerhq.hu:/cvsroot/mplayer co -P main

When asked for a password, just hit enter. A directory named 'main' will be
created. You can later update your sources by saying

  cvs -z3 update -dPA

from within that directory.


_______________________________________________
STEP1: Installing FFmpeg libavcodec/libavformat
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are using an official (pre)release or a CVS snapshot, skip this step,
since official releases include libavcodec. CVS sources do not include
libavcodec. To verify if you do have libavcodec or not, check if a subdirectory
named 'libavcodec' exists in the MPlayer source tree.

The FFmpeg project provides libavcodec, a very portable codec collection (among
the supported formats is MPEG-4/DivX) with excellent quality and speed, that is
the preferred MPEG-4/DivX codec of MPlayer. You have to get libavcodec directly
from the FFmpeg CVS server.

To get the FFmpeg sources, use the following commands in a suitable directory
outside the MPlayer source directory:

cvs -d:pserver:anonymous@mplayerhq.hu:/cvsroot/ffmpeg login
cvs -z3 -d:pserver:anonymous@mplayerhq.hu:/cvsroot/ffmpeg co ffmpeg/libavcodec

When asked for a password, you can just hit enter. A directory named 'ffmpeg'
with a subdirectory named 'libavcodec' inside will be created. Copy (symbolic
linking does NOT suffice) this subdirectory into the MPlayer source tree.

In order to force automatic updates of libavcodec when you update MPlayer, add
the following line to main/CVS/Entries:

D/libavcodec////

FFmpeg also contains libavformat, a library to decode container formats that
can optionally be used to extend MPlayer's container format support. Get it
from FFmpeg CVS by the same steps outlined above for libavcodec, just
substitute libavcodec by libavformat everywhere.


_______________________________
STEP2: Installing Binary Codecs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MPlayer and libavcodec have builtin support for the most common audio and video
formats, but some formats require external codecs. Examples include Real, Indeo
and QuickTime audio formats. Support for Windows Media formats except WMV9
exists but still has some bugs, your mileage may vary. This step is not
mandatory, but recommended for getting MPlayer to play a broader range of
formats. Please note that most codecs only work on Intel x86 compatible PCs.

Unpack the codecs archives and put the contents in a directory where MPlayer
will find them. The default directory is /usr/local/lib/codecs/ (it used to be
/usr/local/lib/win32 in the past, this also works) but you can change that to
something else by using the '--with-codecsdir=DIR' option when you run
'./configure'.


__________________________
STEP3: Configuring MPlayer
~~~~~~~~~~~~~~~~~~~~~~~~~~

MPlayer can be adapted to all kinds of needs and hardware environments. Run

  ./configure

to configure MPlayer with the default options. The codecs you installed above
should be autodetected. GUI support has to be enabled separately, run

  ./configure --enable-gui

if you want to use the GUI.

If something does not work as expected, try

  ./configure --help

to see the available options and select what you need.

The configure script prints a summary of enabled and disabled options. If you
have something installed that configure fails to detect, check the file
configure.log for errors and reasons for the failure. Repeat this step until
you are satisfied with the enabled feature set.


________________________
STEP4: Compiling MPlayer
~~~~~~~~~~~~~~~~~~~~~~~~

Now you can start the compilation by typing

  make

You can install MPlayer with

  make install

provided that you have write permission in the installation directory.

If all went well, you can run MPlayer by typing 'mplayer'. A help screen with a
summary of the most common options and keyboard shortcuts should be displayed.

If you get 'unable to load shared library' or similar errors, run
'ldd ./mplayer' to check which libraries fail and go back to STEP 3 to fix it.
Sometimes running 'ldconfig' is enough to fix the problem.

NOTE: If you run Debian you can configure, compile and build a proper Debian
.deb package with only one command:

  fakeroot debian/rules binary

If you want to pass custom options to configure, you can set up the
DEB_BUILD_OPTIONS environment variable. For instance, if you want GUI
and OSD menu support you would use:

  DEB_BUILD_OPTIONS="--enable-gui --enable-menu" fakeroot debian/rules binary

You can also pass some variables to the Makefile. For example, if you want
to compile with gcc 3.4 even if it's not the default compiler:

  CC=gcc-3.4 DEB_BUILD_OPTIONS="--enable-gui" fakeroot debian/rules binary

To clean up the source tree run the following command:

  fakeroot debian/rules clean

____________________________________________
STEP5: Installing the onscreen display fonts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Unpack the archive and choose one of the available font sizes. Then copy the
font files of the corresponding size into /usr/local/share/mplayer/font/ or
~/.mplayer/font/ (or whatever you set with './configure --datadir=DIR').

Alternatively you can use a TrueType font installed on your system. Just
make a symbolic link from either /usr/local/share/mplayer/subfont.ttf or
~/.mplayer/subfont.ttf to your TrueType font.


____________________________
STEP6: Installing a GUI skin
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Unpack the archive and put the contents in /usr/local/share/mplayer/Skin/ or
~/.mplayer/Skin/. MPlayer will use the skin in the subdirectory named default
of /usr/local/share/mplayer/Skin/ or ~/.mplayer/Skin/ unless told otherwise via
the '-skin' switch. You should therefore rename your skin subdirectory or make
a suitable symbolic link.


__________________
STEP7: Let's play!
~~~~~~~~~~~~~~~~~~

That's it for the moment. To start playing movies, open a command line and try

  mplayer <moviefile>

or for the GUI

  gmplayer <moviefile>

gmplayer is a symbolic link to mplayer created by 'make install'.
Without <moviefile>, MPlayer will come up and you will be able to use the GUI
filepicker.

To play a VCD track or a DVD title, try:

  mplayer vcd://2 -cdrom-device /dev/hdc
  mplayer dvd://1 -alang en -slang hu -dvd-device /dev/hdd

See 'mplayer -help' and 'man mplayer' for further options.

'mplayer -vo help' will show you the available video output drivers. Experiment
with the '-vo' switch to see which one gives you the best performance.
If you get jerky playback or no sound, experiment with the '-ao' switch (see
'-ao help') to choose between different audio drivers. Note that jerky playback
is caused by buggy audio drivers or a slow processor and video card. With a
good audio and video driver combination, one can play DVDs and 720x576 DivX
files smoothly on a Celeron 366. Slower systems may need the '-framedrop'
option.

Questions you may have are probably answered in the rest of the documentation.
The places to start reading are the man page, DOCS/HTML/en/index.html and
DOCS/HTML/en/faq.html. If you find a bug, please report it, but first read
DOCS/HTML/en/bugreports.html.
