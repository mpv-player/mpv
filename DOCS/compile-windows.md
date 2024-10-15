# Compiling for Windows

Compiling for Windows is supported using GNU-like compilers (GCC/Clang). Clang
is compatible with both the ``w64-windows-gnu`` [MinGW-w64](https://www.mingw-w64.org/)
and ``pc-windows-msvc`` [Windows SDK](https://developer.microsoft.com/windows/downloads/windows-sdk)
targets. It supports the production of both 32-bit and 64-bit binaries and is
suitable for building on Windows as well as cross-compiling from Linux and Cygwin.

Although it is possible to build a complete MinGW-w64 toolchain yourself, there
are build environments and scripts available to simplify the process, such as
MSYS2 on Windows or a packaged toolchain provided by your favorite Linux
distribution. Note that MinGW-w64 environments included in Linux distributions
can vary in versions. As a general guideline, mpv only supports the MinGW-w64
toolchain version included in the latest Ubuntu LTS release.

mpv employs Meson for building, and the process is the same as any standard Meson
compilation.

For the most up-to-date reference on build scripts, you can refer to
[build.yml](https://github.com/mpv-player/mpv/blob/master/.github/workflows/build.yml),
which builds and tests all supported configurations: ``MinGW-w64``, ``Windows SDK``,
and ``MSYS2`` builds.

## Cross-compilation

When cross-compiling, it is recommended to use a Meson ``--cross-file`` to set up the
cross-compiling environment. For a basic example, please refer to
[Cross-compilation](https://mesonbuild.com/Cross-compilation.html).

Alternatively, consider using [mpv-winbuild-cmake](https://github.com/shinchiro/mpv-winbuild-cmake),
which bootstraps a MinGW-w64 toolchain and builds mpv along with its dependencies.

### Example with Meson

1. Create ``cross-file.txt`` with definitions for your toolchain and target platform.
   Refer to [x86_64-w64-mingw32.txt](https://mesonbuild.com/Cross-compilation.html)
   as a directly usable example.
   - **Important**: Beware of pkg-config usage. By default, it uses build machine
     files for dependency detection, even when ``--cross-file`` is used. It must
     be configured correctly. Refer to ``pkg_config_libdir`` and ``sys_root``
     in the [documentation](https://mesonbuild.com/Cross-compilation.html#defining-the-environment)
     for proper setup. **In this example pkg-config is not used/required.**
2. Initialize subprojects. This step is optional; other methods are also
   available to provide the necessary dependencies.

   ``` bash
   # Update the subprojects database from Meson's WrapDB.
   meson wrap update-db

   # Explicitly download wraps as nested projects may have older versions of them.
   meson wrap install expat
   meson wrap install harfbuzz
   meson wrap install libpng
   meson wrap install zlib

   # Add wraps for mpv's required dependencies
   mkdir -p subprojects

   cat <<EOF > subprojects/libplacebo.wrap
   [wrap-git]
   url = https://github.com/haasn/libplacebo
   revision = master
   depth = 1
   clone-recursive = true
   EOF

   cat <<EOF > subprojects/libass.wrap
   [wrap-git]
   revision = master
   url = https://github.com/libass/libass
   depth = 1
   EOF

   # For FFmpeg, use Meson's build system port; alternatively, you can compile
   # the upstream version yourself. See https://trac.ffmpeg.org/wiki/CompilationGuide
   cat <<EOF > subprojects/ffmpeg.wrap
   [wrap-git]
   url = https://gitlab.freedesktop.org/gstreamer/meson-ports/ffmpeg.git
   revision = meson-7.1
   depth = 1
   [provide]
   libavcodec = libavcodec_dep
   libavdevice = libavdevice_dep
   libavfilter = libavfilter_dep
   libavformat = libavformat_dep
   libavutil = libavutil_dep
   libswresample = libswresample_dep
   libswscale = libswscale_dep
   EOF
   ```

3. Build

   ``` bash
   meson setup -Ddefault_library=static -Dprefer_static=true \
               -Dc_link_args='-static' -Dcpp_link_args='-static' \
               --cross-file cross-file.txt build

   ninja -C build mpv.exe mpv.com
   ```

   This will produce fully portabiel, statically linked, ``mpv.exe`` and ``mpv.com``
   binaries.

#### Building libmpv

- To also build ``libmpv``, execute the following commands:

   ``` bash
   # For a static library
   meson configure build -Dlibmpv=true -Ddefault_library=static
   ninja -C build libmpv.a

   # For a shared library
   meson configure build -Dlibmpv=true -Ddefault_library=shared
   ninja -C build libmpv-2.dll
   ```

- Depending on the use case, you might want to use ``-Dgpl=false`` and review
  similar options for subprojects.
- If any of Meson's subprojects are not linked statically, you might need to
  specify options like the following example for ffmpeg:
  ``-Dffmpeg:default_library=static``.

#### Enabling more mpv features

The process above produces a basic mpv build. You can enable additional features.
Check the Meson
[WrapDB packages](https://mesonbuild.com/Wrapdb-projects.html) for available
dependencies or by providing them through other means.

Enhancing mpv with more features is as simple as adding more arguments to the
Meson setup command, for example:

``` bash
-Dlua=enabled -Djavascript=enabled -Dlcms2=enabled -Dlibplacebo:lcms=enabled
```

they will be automatically downloaded and built by Meson.

## Native Compilation with Clang (Windows SDK Build)

1. Install [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
   or the full [Visual Studio](https://visualstudio.microsoft.com/downloads/#visual-studio-community-2022):
   - From the installer, select the necessary components:
      - Clang compiler for Windows
      - C++ CMake tools for Windows
      - Windows SDK
   - Activate the developer shell:
      ```
      & "<Visual Studio Path>\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
      ```
2. Install Meson, as outlined in [Getting Meson](https://mesonbuild.com/Getting-meson.html):
3. The following build script utilizes the Meson subprojects system to build mpv and its dependencies.
   To make sure all dependency versions are up-to-date, update the subprojects database from Meson's WrapDB.
   Also explicitly download several wraps as some nested projects may pull older versions of them.
   ```
   meson wrap update-db

   meson wrap install expat
   meson wrap install harfbuzz
   meson wrap install libpng
   meson wrap install zlib
   ```
4. Set environment variables or use the `--native-file` option in Meson.
   ```powershell
   $env:CC = 'clang'
   $env:CXX = 'clang++'
   $env:CC_LD = 'lld'
   $env:CXX_LD = 'lld'
   $env:WINDRES = 'llvm-rc'
   ```
   Note that some dependencies (e.g. LuaJIT) may require `sed` to configure. Fortunately, it is already bundled in
   [Git for Windows](https://www.git-scm.com/download/win):
   ```powershell
   $env:PATH += ';<Git Path>\usr\bin'
   ```
5. Execute [ci\build-win32.ps1](https://github.com/mpv-player/mpv/blob/master/ci/build-win32.ps1). Refer to the script for more details.

This process will produce a fully static ``mpv.exe`` and ``mpv.com``, as well as
a static ``libmpv.a``.

## Native Compilation with MSYS2

For Windows developers seeking a quick setup, MSYS2 is an effective tool for
compiling mpv natively on a Windows machine. The MSYS2 repositories provide
binary packages for most of mpv's dependencies, simplifying the process to
primarily involve building mpv itself.

### Installing MSYS2

1. Follow the installation steps from [MSYS2](https://www.msys2.org/).
2. Initiate one of the [Environments](https://www.msys2.org/docs/environments/),
   with the CLANG64 (``clang64.exe``) being the recommended option.
   **Note:** This environment is distinct from the MSYS2 shell that opens
   automatically after the final installation dialog. You must close that
   initial shell and open a new one for the appropriate environment.

### Updating MSYS2

For guidance on updating MSYS2, please refer to the official documentation:
[Updating MSYS2](https://www.msys2.org/docs/updating/).

### Installing mpv Dependencies

``` bash
# Install pacboy and git
pacman -S pactoys git

# Install MSYS2 build dependencies and a MinGW-w64 compiler
pacboy -S python pkgconf cc meson

# Install key dependencies. libass and lcms2 are also included as dependencies
# of ffmpeg.
pacboy -S ffmpeg libjpeg-turbo libplacebo luajit vulkan-headers
```

### Building mpv

To compile and install mpv, execute the following commands.
The binaries will be installed in the directory ``/$MSYSTEM_PREFIX/bin``.

```bash
# Set up the build directory with the desired configuration
meson setup build -Dlibmpv=true --prefix=$MSYSTEM_PREFIX

# Compile
meson compile -C build

# Optionally, install the compiled binaries
meson install -C build
```

## Running mpv

Depending on your build configuration, ``mpv.exe`` may rely on external
libraries. To create a portable package, you will need to include these
dependencies as well. The quickest way to determine which libraries are needed
is to run ``mpv.exe`` and note any error messages that list the required ``.dll``
files. You can find these libraries in the sysroot used for compilation, such as
``/clang64/bin/`` in MSYS2.

## Linking libmpv with MSVC Programs

Building mpv/libmpv directly with the MSVC compiler (cl.exe) is not supported
due to differences in compiler flags. Our build system is designed specifically
for GNU-like compiler drivers. However, you can still build programs in
Visual Studio and link them with libmpv.

If the toolchain used generates a ``.lib`` file, it will be ready for use.
Otherwise, you will need to create an import library for the mpv DLL with the
following command:

```bash
lib /name:mpv-2.dll /out:mpv.lib /MACHINE:X64
```

Ensure that the string in the ``/name:`` parameter matches the filename of the
DLL, as this is the filename that the MSVC linker will reference.

**Note:** Static linking is only feasible with matching compilers. For instance,
if you build with Clang in Visual Studio, static linking is possible.
