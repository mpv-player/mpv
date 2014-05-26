Cross Compiling to Windows
==========================

Cross compiling mpv to Windows is supported with MinGW-w64. This can be used to
produce both 32 bit and 64 bit executables. MinGW-w64 is available from
http://mingw-w64.sourceforge.net.

You have to run mpv's configure with these arguments:

```bash
DEST_OS=win32 TARGET=i686-w64-mingw32 ./waf configure
```

While building a complete MinGW-w64 toolchain yourself is possible, people have
created scripts to help ease the process. These are the two recommended methods:

- Using [mingw-w64-cmake](https://github.com/lachs0r/mingw-w64-cmake) to setup
  a MinGW-w64 environment. We recommend you to try this first before MXE.
  mingw-w64-cmake will also build mpv and its dependencies.
- Alternatively, you can use MXE: http://mxe.cc. With MXE, you have to manually
  set the target to MinGW-w64 (even if you compile to 32 bit). A working example
  below.

**Warning**: the original MinGW (http://www.mingw.org) is unsupported.

Note that MinGW environments included in Linux distributions are often broken,
outdated and useless, and usually don't use MinGW-w64.

Example with MXE
----------------

```bash
# Download MXE. Note that compiling the required packages requires about 1.4 GB
# or more!

cd /opt
git clone https://github.com/mxe/mxe mingw
cd mingw

# Set build options.

# The JOBS environment variable controls threads to use when building. DO NOT
# use the regular `make -j4` option with MXE as it will slow down the build.
# Alternatively, you can set this in the make command by appending "JOBS=4"
# to the end of command:
echo "JOBS := 4" >> settings.mk

# The MXE_TARGET environment variable builds MinGW-w64 for 32 bit targets.
# Alternatively, you can specify this in the make command by appending
# "MXE_TARGETS=i686-w64-mingw32" to the end of command:
echo "MXE_TARGETS := i686-w64-mingw32" >> settings.mk

# If you want to build 64 bit version, use this:
# echo "MXE_TARGETS := x86_64-w64-mingw32" >> settings.mk

# Build required packages. The following provide a minimum required to build
# mpv.

make gcc ffmpeg libass jpeg pthreads

# Add MXE binaries to $PATH
export PATH=/opt/mingw/usr/bin/:$PATH

# Build mpv. The target will be used to automatically select the name of the
# build tools involved (e.g. it will use i686-w64-mingw32-gcc).

cd ..
git clone https://github.com/mpv-player/mpv.git
cd mpv
DEST_OS=win32 TARGET=i686-w64-mingw32 ./waf configure
# Or, if 64 bit version,
# DEST_OS=win32 TARGET=x86_64-w64-mingw32 ./waf configure
./waf build
```
