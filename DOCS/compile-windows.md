Compiling for Windows
=====================

Compiling for Windows is supported with MinGW-w64. This can be used to produce
both 32-bit and 64-bit executables, and it works for building on Windows and
cross-compiling from Linux and Cygwin. MinGW-w64 is available from:
http://mingw-w64.sourceforge.net.

While building a complete MinGW-w64 toolchain yourself is possible, there are a
few build environments and scripts to help ease the process, such as MSYS2 and
MXE. Note that MinGW environments included in Linux distributions are often
broken, outdated and useless, and usually don't use MinGW-w64.

**Warning**: the original MinGW (http://www.mingw.org) is unsupported.

Cross-compilation
=================

When cross-compiling, you have to run mpv's configure with these arguments:

```bash
DEST_OS=win32 TARGET=i686-w64-mingw32 ./waf configure
```

[MXE](http://mxe.cc) makes it very easy to bootstrap a complete MingGW-w64
environment from a Linux machine. See a working example below.

Alternatively, you can try [mingw-w64-cmake](https://github.com/lachs0r/mingw-w64-cmake),
which bootstraps a MinGW-w64 environment and builds mpv and dependencies.

Example with MXE
----------------

```bash
# Before starting, make sure you install MXE prerequisites. MXE will download
# and build all target dependencies, but no host dependencies. For example,
# you need a working compiler, or MXE can't build the crosscompiler.
#
# Refer to
#
#    http://mxe.cc/#requirements
#
# Scroll down for disto/OS-specific instructions to install them.

# Download MXE. Note that compiling the required packages requires about 1.4 GB
# or more!

cd /opt
git clone https://github.com/mxe/mxe mxe
cd mxe

# Set build options.

# The JOBS environment variable controls threads to use when building. DO NOT
# use the regular `make -j4` option with MXE as it will slow down the build.
# Alternatively, you can set this in the make command by appending "JOBS=4"
# to the end of command:
echo "JOBS := 4" >> settings.mk

# The MXE_TARGET environment variable builds MinGW-w64 for 32 bit targets.
# Alternatively, you can specify this in the make command by appending
# "MXE_TARGETS=i686-w64-mingw32" to the end of command:
echo "MXE_TARGETS := i686-w64-mingw32.static" >> settings.mk

# If you want to build 64 bit version, use this:
# echo "MXE_TARGETS := x86_64-w64-mingw32.static" >> settings.mk

# Build required packages. The following provide a minimum required to build
# a reasonable mpv binary (though not an absolute minimum).

make gcc ffmpeg libass jpeg lua

# Add MXE binaries to $PATH
export PATH=/opt/mxe/usr/bin/:$PATH

# Build mpv. The target will be used to automatically select the name of the
# build tools involved (e.g. it will use i686-w64-mingw32.static-gcc).

cd ..
git clone https://github.com/mpv-player/mpv.git
cd mpv
python ./bootstrap.py
DEST_OS=win32 TARGET=i686-w64-mingw32.static ./waf configure
# Or, if 64 bit version,
# DEST_OS=win32 TARGET=x86_64-w64-mingw32.static ./waf configure
./waf build
```

Native compilation with MSYS2
=============================

For Windows developers looking to get started quickly, MSYS2 can be used to
compile mpv natively on a Windows machine. The MSYS2 repositories have binary
packages for most of mpv's dependencies, so the process should only involve
building mpv itself.

To build 64-bit mpv on Windows:

Installing MSYS2
----------------

1. Download an installer from https://msys2.github.io/

   It doesn't matter whether the i686 or the x86_64 version is used. Both can
   build 32-bit and 64-bit binaries when running on a 64-bit version of Windows.

2. Add ``C:\msys64\mingw64\bin`` to your ``%PATH%``. mpv will depend on several
   DLLs in this folder.

3. Start a MinGW-w64 shell (``mingw64_shell.bat``). For a 32-bit build, use
   ``mingw32_shell.bat``.

Updating MSYS2
--------------

To prevent errors during post-install, ``msys2-runtime`` and ``pacman`` must be
updated first.

```bash
# Check for updates to msys2-runtime and pacman. If there were updates, restart
# MSYS2 before continuing.
pacman -Sy --needed msys2-runtime pacman

# Update everything else
pacman -Su
```

Installing mpv dependencies
---------------------------

```bash
# Install MSYS2 build dependencies and a MinGW-w64 compiler
pacman -S git pkg-config python3 mingw-w64-x86_64-gcc

# Install the most important MinGW-w64 dependencies. libass, libbluray and
# lcms2 are also pulled in as dependencies of ffmpeg.
pacman -S mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-lua51

# Install additional (optional) dependencies
pacman -S mingw-w64-x86_64-libdvdnav mingw-w64-x86_64-libguess
```

For a 32-bit build, install ``mingw-w64-i686-*`` packages instead.

Building mpv
------------

Clone the latest mpv from git and install waf:

```bash
git clone https://github.com/mpv-player/mpv.git && cd mpv
./bootstrap.py
```

Finally, compile and install mpv. Binaries will be installed to
``/mingw64/bin``.

```bash
# For a 32-bit build, use --prefix=/mingw32 instead
./waf configure CC=gcc --prefix=/mingw64
./waf install
```

Or, compile and install both libmpv and mpv:

```bash
./waf configure CC=gcc --enable-libmpv-shared --prefix=/mingw64
./waf install

# waf installs libmpv to the wrong directory, so fix it up
mv -f /mingw64/bin/pkgconfig/mpv.pc /mingw64/lib/pkgconfig/
mv -f /mingw64/bin/libmpv.dll.a /mingw64/lib/
sed -i 's_/mingw64/bin_/mingw64/lib_' /mingw64/lib/pkgconfig/mpv.pc
rmdir /mingw64/bin/pkgconfig
```
