Cross Compiling to Windows
==========================

Cross compiling mpv to Windows is supported with MinGW-w64. This can be used to
produce both 32 bit and 64 bit executables. MinGW-w64 is available from
http://mingw-w64.sourceforge.net.

You have to run mpv's configure with these arguments:
    DEST_OS=win32 TARGET=i686-w64-mingw32 ./waf configure

Using mingw-w64-cmake to setup a MinGW-w64 environment is recommended (this will
also build mpv and its dependencies): https://github.com/lachs0r/mingw-w64-cmake

Alternatively, use MXE: http://mxe.cc
With MXE, you have to modify the file settings.mk to target MinGW-w64 (even if
you compile to 32 bit).

Warning: the original MinGW (http://www.mingw.org) is unsupported.

Note that MinGW environments included in Linux distributions are often broken,
outdated and useless, and usually don't use MinGW-w64.

Example with MXE
----------------

```bash
# Download MXE. Note that compiling the required packages requires about 1 GB
# or more!

cd /opt
git clone https://github.com/mxe/mxe mingw
cd mingw

# Edit the MXE target, so that MinGW-w64 for 32 bit targets is built.

echo "MXE_TARGETS := i686-w64-mingw32" > settings.mk

# Build required packages. The following provide a minimum required to build
# mpv. (Not all of the following packages are strictly required.)

make gcc
make ffmpeg
make libass
make jpeg
make pthreads

# Build mpv. The target will be used to automatically select the name of the
# build tools involved (e.g. it will use i686-w64-mingw32-gcc).

git clone https://github.com/mpv-player/mpv.git
cd mpv
export PATH=/opt/mingw/usr/bin/:$PATH
DEST_OS=win32 TARGET=i686-w64-mingw32 ./waf configure
./waf build
```
