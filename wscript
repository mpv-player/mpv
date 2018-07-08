# vi: ft=python

import sys, os, re
sys.path.insert(0, os.path.join(os.getcwd(), 'waftools'))
sys.path.insert(0, os.getcwd())
from waflib.Configure import conf
from waflib.Tools import c_preproc
from waflib import Utils
from waftools.checks.generic import *
from waftools.checks.custom import *

c_preproc.go_absolute=True # enable system folders
c_preproc.standard_includes.append('/usr/local/include')

APPNAME = 'mpv'

"""
Dependency identifiers (for win32 vs. Unix):
    wscript / C source                  meaning
    --------------------------------------------------------------------------
    posix / HAVE_POSIX:                 defined on Linux, OSX, Cygwin
                                        (Cygwin emulates POSIX APIs on Windows)
    mingw / __MINGW32__:                defined if posix is not defined
                                        (Windows without Cygwin)
    os-win32 / _WIN32:                  defined if basic windows.h API is available
    win32-desktop / HAVE_WIN32_DESKTOP: defined if desktop windows.h API is available
    uwp / HAVE_UWP:                     defined if building for UWP (basic Windows only)
"""

build_options = [
    {
        'name': '--lgpl',
        'desc': 'LGPL (version 2.1 or later) build',
        'default': 'disable',
        'func': check_true,
    }, {
        'name': 'gpl',
        'desc': 'GPL (version 2 or later) build',
        'deps': '!lgpl',
        'func': check_true,
    }, {
        'name': 'libaf',
        'desc': 'internal audio filter chain',
        'deps': 'gpl',
        'func': check_true,
    }, {
        'name': '--cplayer',
        'desc': 'mpv CLI player',
        'default': 'enable',
        'func': check_true
    }, {
        'name': '--libmpv-shared',
        'desc': 'shared library',
        'default': 'disable',
        'func': check_true
    }, {
        'name': '--libmpv-static',
        'desc': 'static library',
        'default': 'disable',
        'deps': '!libmpv-shared',
        'func': check_true
    }, {
        'name': '--static-build',
        'desc': 'static build',
        'default': 'disable',
        'func': check_true
    }, {
        'name': '--build-date',
        'desc': 'whether to include binary compile time',
        'default': 'enable',
        'func': check_true
    }, {
        'name': '--optimize',
        'desc': 'whether to optimize',
        'default': 'enable',
        'func': check_true
    }, {
        'name': '--debug-build',
        'desc': 'whether to compile-in debugging information',
        'default': 'enable',
        'func': check_true
    }, {
        'name': '--manpage-build',
        'desc': 'manpage generation',
        'func': check_ctx_vars('RST2MAN')
    }, {
        'name': '--html-build',
        'desc': 'html manual generation',
        'func': check_ctx_vars('RST2HTML'),
        'default': 'disable',
    }, {
        'name': '--pdf-build',
        'desc': 'pdf manual generation',
        'func': check_ctx_vars('RST2PDF'),
        'default': 'disable',
    }, {
        'name': 'libdl',
        'desc': 'dynamic loader',
        'func': check_libs(['dl'], check_statement('dlfcn.h', 'dlopen("", 0)'))
    }, {
        'name': '--cplugins',
        'desc': 'C plugins',
        'deps': 'libdl && !os-win32',
        'func': check_cc(linkflags=['-rdynamic']),
    }, {
        'name': '--zsh-comp',
        'desc': 'zsh completion',
        'func': check_ctx_vars('BIN_PERL'),
        'func': check_true,
        'default': 'disable',
    }, {
        # does nothing - left for backward and forward compatibility
        'name': '--asm',
        'desc': 'inline assembly (currently without effect)',
        'default': 'enable',
        'func': check_true,
    }, {
        'name': '--test',
        'desc': 'test suite (using cmocka)',
        'func': check_pkg_config('cmocka', '>= 1.0.0'),
        'default': 'disable',
    }, {
        'name': '--clang-database',
        'desc': 'generate a clang compilation database',
        'func': check_true,
        'default': 'disable',
    }
]

main_dependencies = [
    {
        'name': 'noexecstack',
        'desc': 'compiler support for noexecstack',
        'func': check_cc(linkflags='-Wl,-z,noexecstack')
    }, {
        'name': 'noexecstack',
        'desc': 'linker support for --nxcompat --no-seh --dynamicbase',
        'func': check_cc(linkflags=['-Wl,--nxcompat', '-Wl,--no-seh', '-Wl,--dynamicbase'])
    } , {
        'name': 'libm',
        'desc': '-lm',
        'func': check_cc(lib='m')
    }, {
        'name': 'mingw',
        'desc': 'MinGW',
        'deps': 'os-win32',
        'func': check_statement('stdlib.h', 'int x = __MINGW32__;'
                                            'int y = __MINGW64_VERSION_MAJOR'),
    }, {
        'name': 'posix',
        'desc': 'POSIX environment',
        # This should be good enough.
        'func': check_statement(['poll.h', 'unistd.h', 'sys/mman.h'],
            'struct pollfd pfd; poll(&pfd, 1, 0); fork(); int f[2]; pipe(f); munmap(f,0)'),
    }, {
        'name': '--android',
        'desc': 'Android environment',
        'func': compose_checks(
            check_statement('android/api-level.h', '(void)__ANDROID__'),  # arbitrary android-specific header
            check_cc(lib="android"),
            check_cc(lib="EGL"),
        )
    }, {
        'name': 'posix-or-mingw',
        'desc': 'development environment',
        'deps': 'posix || mingw',
        'func': check_true,
        'req': True,
        'fmsg': 'Unable to find either POSIX or MinGW-w64 environment, ' \
                'or compiler does not work.',
    }, {
        'name': '--swift',
        'desc': 'macOS Swift build tools',
        'deps': 'os-darwin',
        'func': check_swift,
    }, {
        'name': '--uwp',
        'desc': 'Universal Windows Platform',
        'default': 'disable',
        'deps': 'os-win32 && mingw && !cplayer',
        'func': check_cc(lib=['windowsapp']),
    }, {
        'name': 'win32-desktop',
        'desc': 'win32 desktop APIs',
        'deps': '(os-win32 || os-cygwin) && !uwp',
        'func': check_cc(lib=['winmm', 'gdi32', 'ole32', 'uuid', 'avrt', 'dwmapi', 'version']),
    }, {
        'name': '--win32-internal-pthreads',
        'desc': 'internal pthread wrapper for win32 (Vista+)',
        'deps': 'os-win32 && !posix',
        'func': check_true,
    }, {
        'name': 'pthreads',
        'desc': 'POSIX threads',
        'func': check_pthreads,
        'req': True,
        'fmsg': 'Unable to find pthreads support.'
    }, {
        'name': 'gnuc',
        'desc': 'GNU C extensions',
        'func': check_statement([], "__GNUC__"),
    }, {
        'name': 'stdatomic',
        'desc': 'stdatomic.h',
        'func': check_libs(['atomic'],
            check_statement('stdatomic.h',
                'atomic_int_least64_t test = ATOMIC_VAR_INIT(123);'
                'atomic_fetch_add(&test, 1)'))
    }, {
        'name': 'atomics',
        'desc': 'stdatomic.h support or slow emulation',
        'func': check_true,
        'req': True,
        'deps': 'stdatomic || gnuc',
    }, {
        'name': 'librt',
        'desc': 'linking with -lrt',
        'deps': 'pthreads',
        'func': check_cc(lib='rt')
    }, {
        'name': '--iconv',
        'desc': 'iconv',
        'func': check_iconv,
        'req': True,
        'fmsg': "Unable to find iconv which should be part of a standard \
compilation environment. Aborting. If you really mean to compile without \
iconv support use --disable-iconv.",
    }, {
        'name': 'dos-paths',
        'desc': 'w32/dos paths',
        'deps': 'os-win32 || os-cygwin',
        'func': check_true
    }, {
        'name': 'posix-spawn-native',
        'desc': 'spawnp()/kill() POSIX support',
        'func': check_statement(['spawn.h', 'signal.h'],
            'posix_spawnp(0,0,0,0,0,0); kill(0,0)'),
        'deps': '!mingw',
    }, {
        'name': 'posix-spawn-android',
        'desc': 'spawnp()/kill() Android replacement',
        'func': check_true,
        'deps': 'android && !posix-spawn-native',
    },{
        'name': 'posix-spawn',
        'desc': 'any spawnp()/kill() support',
        'deps': 'posix-spawn-native || posix-spawn-android',
        'func': check_true,
    }, {
        'name': 'win32-pipes',
        'desc': 'Windows pipe support',
        'func': check_true,
        'deps': 'win32-desktop && !posix',
    }, {
        'name': 'glob-posix',
        'desc': 'glob() POSIX support',
        'deps': '!(os-win32 || os-cygwin)',
        'func': check_statement('glob.h', 'glob("filename", 0, 0, 0)'),
    }, {
        'name': 'glob-win32',
        'desc': 'glob() win32 replacement',
        'deps': '!posix && (os-win32 || os-cygwin)',
        'func': check_true
    }, {
        'name': 'glob',
        'desc': 'any glob() support',
        'deps': 'glob-posix || glob-win32',
        'func': check_true,
    }, {
        'name': 'fchmod',
        'desc': 'fchmod()',
        'func': check_statement('sys/stat.h', 'fchmod(0, 0)'),
    }, {
        'name': 'vt.h',
        'desc': 'vt.h',
        'func': check_statement(['sys/vt.h', 'sys/ioctl.h'],
                                'int m; ioctl(0, VT_GETMODE, &m)'),
    }, {
        'name': 'gbm.h',
        'desc': 'gbm.h',
        'func': check_cc(header_name=['stdio.h', 'gbm.h']),
    }, {
        'name': 'glibc-thread-name',
        'desc': 'GLIBC API for setting thread name',
        'func': check_statement('pthread.h',
                                'pthread_setname_np(pthread_self(), "ducks")',
                                use=['pthreads']),
    }, {
        'name': 'osx-thread-name',
        'desc': 'OSX API for setting thread name',
        'deps': '!glibc-thread-name',
        'func': check_statement('pthread.h',
                                'pthread_setname_np("ducks")', use=['pthreads']),
    }, {
        'name': 'bsd-thread-name',
        'desc': 'BSD API for setting thread name',
        'deps': '!(glibc-thread-name || osx-thread-name)',
        'func': check_statement('pthread.h',
                                'pthread_set_name_np(pthread_self(), "ducks")',
                                use=['pthreads']),
    }, {
        'name': 'bsd-fstatfs',
        'desc': "BSD's fstatfs()",
        'func': check_statement(['sys/param.h', 'sys/mount.h'],
                                'struct statfs fs; fstatfs(0, &fs); fs.f_fstypename')
    }, {
        'name': 'linux-fstatfs',
        'desc': "Linux's fstatfs()",
        'deps': 'os-linux',
        'func': check_statement('sys/vfs.h',
                                'struct statfs fs; fstatfs(0, &fs); fs.f_namelen')
    }, {
        'name': '--libsmbclient',
        'desc': 'Samba support (makes mpv GPLv3)',
        'deps': 'libdl && gpl',
        'func': check_pkg_config('smbclient'),
        'default': 'disable',
        'module': 'input',
    }, {
        'name' : '--lua',
        'desc' : 'Lua',
        'func': check_lua,
    }, {
        'name' : '--javascript',
        'desc' : 'Javascript (MuJS backend)',
        'func': check_pkg_config('mujs', '>= 1.0.0'),
    }, {
        'name': '--libass',
        'desc': 'SSA/ASS support',
        'func': check_pkg_config('libass', '>= 0.12.1'),
        'req': True,
        'fmsg': "Unable to find development files for libass, or the version " +
                "found is too old. Aborting. If you really mean to compile " +
                "without libass support use --disable-libass."
    }, {
        'name': '--libass-osd',
        'desc': 'libass OSD support',
        'deps': 'libass',
        'func': check_true,
    }, {
        'name': 'dummy-osd',
        'desc': 'dummy OSD support',
        'deps': '!libass-osd',
        'func': check_true,
    } , {
        'name': '--zlib',
        'desc': 'zlib',
        'func': check_libs(['z'],
                    check_statement('zlib.h', 'inflate(0, Z_NO_FLUSH)')),
        'req': True,
        'fmsg': 'Unable to find development files for zlib.'
    }, {
        'name': '--libbluray',
        'desc': 'Bluray support',
        'func': check_pkg_config('libbluray', '>= 0.3.0'),
        #'default': 'disable',
    }, {
        'name': '--dvdread',
        'desc': 'dvdread support',
        'deps': 'gpl',
        'func': check_pkg_config('dvdread', '>= 4.1.0'),
        'default': 'disable',
    }, {
        'name': '--dvdnav',
        'desc': 'dvdnav support',
        'deps': 'gpl',
        'func': check_pkg_config('dvdnav',  '>= 4.2.0',
                                 'dvdread', '>= 4.1.0'),
        'default': 'disable',
    }, {
        'name': 'dvdread-common',
        'desc': 'DVD/IFO support',
        'deps': 'gpl && (dvdread || dvdnav)',
        'func': check_true,
    }, {
        'name': '--cdda',
        'desc': 'cdda support (libcdio)',
        'deps': 'gpl',
        'func': check_pkg_config('libcdio_paranoia'),
        'default': 'disable',
    }, {
        'name': '--uchardet',
        'desc': 'uchardet support',
        'deps': 'iconv',
        'func': check_pkg_config('uchardet'),
    }, {
        'name': '--rubberband',
        'desc': 'librubberband support',
        'deps': 'libaf',
        'func': check_pkg_config('rubberband', '>= 1.8.0'),
    }, {
        'name': '--lcms2',
        'desc': 'LCMS2 support',
        'func': check_pkg_config('lcms2', '>= 2.6'),
    }, {
        'name': '--vapoursynth',
        'desc': 'VapourSynth filter bridge (Python)',
        'func': check_pkg_config('vapoursynth',        '>= 24',
                                 'vapoursynth-script', '>= 23'),
    }, {
        'name': '--vapoursynth-lazy',
        'desc': 'VapourSynth filter bridge (Lazy Lua)',
        'deps': 'lua',
        'func': check_pkg_config('vapoursynth',        '>= 24'),
    }, {
        'name': 'vapoursynth-core',
        'desc': 'VapourSynth filter bridge (core)',
        'deps': 'vapoursynth || vapoursynth-lazy',
        'func': check_true,
    }, {
        'name': '--libarchive',
        'desc': 'libarchive wrapper for reading zip files and more',
        'func': check_pkg_config('libarchive >= 3.0.0'),
    }
]

ffmpeg_pkg_config_checks = [
    'libavutil',     '>= 56.12.100',
    'libavcodec',    '>= 58.16.100',
    'libavformat',   '>= 58.9.100',
    'libswscale',    '>= 5.0.101',
    'libavfilter',   '>= 7.14.100',
    'libswresample', '>= 3.0.100',
]
libav_pkg_config_checks = [
    'libavutil',     '>= 56.6.0',
    'libavcodec',    '>= 58.8.0',
    'libavformat',   '>= 58.1.0',
    'libswscale',    '>= 5.0.0',
    'libavfilter',   '>= 7.0.0',
    'libavresample', '>= 4.0.0',
]

def check_ffmpeg_or_libav_versions():
    def fn(ctx, dependency_identifier, **kw):
        versions = ffmpeg_pkg_config_checks
        if ctx.dependency_satisfied('libav'):
            versions = libav_pkg_config_checks
        return check_pkg_config(*versions)(ctx, dependency_identifier, **kw)
    return fn

libav_dependencies = [
    {
        'name': 'libavcodec',
        'desc': 'FFmpeg/Libav present',
        'func': check_pkg_config('libavcodec'),
        'req': True,
        'fmsg': "FFmpeg/Libav development files not found.",
    }, {
        'name': 'ffmpeg',
        'desc': 'libav* is FFmpeg',
        # FFmpeg <=> LIBAVUTIL_VERSION_MICRO>=100
        'func': check_statement('libavcodec/version.h',
                                'int x[LIBAVCODEC_VERSION_MICRO >= 100 ? 1 : -1]',
                                use='libavcodec'),
    }, {
        # This check should always result in the opposite of ffmpeg-*.
        # Run it to make sure is_ffmpeg didn't fail for some other reason than
        # the actual version check.
        'name': 'libav',
        'desc': 'libav* is Libav',
        # FFmpeg <=> LIBAVUTIL_VERSION_MICRO>=100
        'func': check_statement('libavcodec/version.h',
                                'int x[LIBAVCODEC_VERSION_MICRO >= 100 ? -1 : 1]',
                                use='libavcodec')
    }, {
        'name': 'libav-any',
        'desc': 'Libav/FFmpeg library versions',
        'deps': 'ffmpeg || libav',
        'func': check_ffmpeg_or_libav_versions(),
        'req': True,
        'fmsg': "Unable to find development files for some of the required \
FFmpeg/Libav libraries. Git master is recommended."
    }, {
        'name': '--libavdevice',
        'desc': 'libavdevice',
        'func': check_pkg_config('libavdevice', '>= 57.0.0'),
    }
]

audio_output_features = [
    {
        'name': '--sdl2',
        'desc': 'SDL2',
        'func': check_pkg_config('sdl2'),
        'default': 'disable'
    }, {
        'name': '--oss-audio',
        'desc': 'OSS',
        'func': check_cc(header_name='sys/soundcard.h'),
        'deps': 'posix && gpl',
    }, {
        'name': '--rsound',
        'desc': 'RSound audio output',
        'func': check_statement('rsound.h', 'rsd_init(NULL)', lib='rsound')
    }, {
        'name': '--sndio',
        'desc': 'sndio audio input/output',
        'func': check_statement('sndio.h',
            'struct sio_par par; sio_initpar(&par); const char *s = SIO_DEVANY', lib='sndio'),
        'default': 'disable'
    }, {
        'name': '--pulse',
        'desc': 'PulseAudio audio output',
        'func': check_pkg_config('libpulse', '>= 1.0')
    }, {
        'name': '--jack',
        'desc': 'JACK audio output',
        'deps': 'gpl',
        'func': check_pkg_config('jack'),
    }, {
        'name': '--openal',
        'desc': 'OpenAL audio output',
        'func': check_pkg_config('openal', '>= 1.13'),
        'default': 'disable'
    }, {
        'name': '--opensles',
        'desc': 'OpenSL ES audio output',
        'func': check_statement('SLES/OpenSLES.h', 'slCreateEngine', lib="OpenSLES"),
    }, {
        'name': '--alsa',
        'desc': 'ALSA audio output',
        'func': check_pkg_config('alsa', '>= 1.0.18'),
    }, {
        'name': '--coreaudio',
        'desc': 'CoreAudio audio output',
        'func': check_cc(
            fragment=load_fragment('coreaudio.c'),
            framework_name=['CoreFoundation', 'CoreAudio', 'AudioUnit', 'AudioToolbox'])
    }, {
        'name': '--audiounit',
        'desc': 'AudioUnit output for iOS',
        'deps': 'atomics',
        'func': check_cc(
            fragment=load_fragment('audiounit.c'),
            framework_name=['Foundation', 'AudioToolbox'])
    }, {
        'name': '--wasapi',
        'desc': 'WASAPI audio output',
        'deps': 'os-win32 || os-cygwin',
        'func': check_cc(fragment=load_fragment('wasapi.c')),
    }
]

video_output_features = [
    {
        'name': '--cocoa',
        'desc': 'Cocoa',
        'func': check_cocoa
    }, {
        'name': '--drm',
        'desc': 'DRM',
        'deps': 'vt.h',
        'func': check_pkg_config('libdrm'),
    }, {
        'name': '--drmprime',
        'desc': 'DRM Prime ffmpeg support',
        'func': check_statement('libavutil/pixfmt.h',
                                'int i = AV_PIX_FMT_DRM_PRIME')
    }, {
        'name': '--gbm',
        'desc': 'GBM',
        'deps': 'gbm.h',
        'func': check_pkg_config('gbm'),
    } , {
        'name': '--wayland-scanner',
        'desc': 'wayland-scanner',
        'func': check_program('wayland-scanner', 'WAYSCAN')
    } , {
        'name': '--wayland-protocols',
        'desc': 'wayland-protocols',
        'func': check_wl_protocols
    } , {
        'name': '--wayland',
        'desc': 'Wayland',
        'deps': 'wayland-protocols && wayland-scanner',
        'func': check_pkg_config('wayland-client', '>= 1.6.0',
                                 'wayland-cursor', '>= 1.6.0',
                                 'xkbcommon',      '>= 0.3.0'),
    } , {
        'name': '--x11',
        'desc': 'X11',
        'deps': 'gpl',
        'func': check_pkg_config('x11',         '>= 1.0.0',
                                 'xscrnsaver',  '>= 1.0.0',
                                 'xext',        '>= 1.0.0',
                                 'xinerama',    '>= 1.0.0',
                                 'xrandr',      '>= 1.2.0'),
    } , {
        'name': '--xv',
        'desc': 'Xv video output',
        'deps': 'x11',
        'func': check_pkg_config('xv'),
    } , {
        'name': '--gl-cocoa',
        'desc': 'OpenGL Cocoa Backend',
        'deps': 'cocoa',
        'groups': [ 'gl' ],
        'func': check_statement('IOSurface/IOSurface.h',
                                'IOSurfaceRef surface;',
                                framework='IOSurface')
    } , {
        'name': '--gl-x11',
        'desc': 'OpenGL X11 Backend',
        'deps': 'x11',
        'groups': [ 'gl' ],
        'func': check_libs(['GL', 'GL Xdamage'],
                   check_cc(fragment=load_fragment('gl_x11.c'),
                            use=['x11', 'libdl', 'pthreads']))
    } , {
        'name': '--egl-x11',
        'desc': 'OpenGL X11 EGL Backend',
        'deps': 'x11',
        'groups': [ 'gl' ],
        'func': check_pkg_config('egl'),
    } , {
        'name': '--egl-drm',
        'desc': 'OpenGL DRM EGL Backend',
        'deps': 'drm && gbm',
        'groups': [ 'gl' ],
        'func': check_pkg_config('egl'),
    } , {
        'name': '--gl-wayland',
        'desc': 'OpenGL Wayland Backend',
        'deps': 'wayland',
        'groups': [ 'gl' ],
        'func': check_pkg_config('wayland-egl', '>= 9.0.0',
                                 'egl',         '>= 9.0.0')
    } , {
        'name': '--gl-win32',
        'desc': 'OpenGL Win32 Backend',
        'deps': 'win32-desktop',
        'groups': [ 'gl' ],
        'func': check_statement('windows.h', 'wglCreateContext(0)',
                                lib='opengl32')
    } , {
        'name': '--gl-dxinterop',
        'desc': 'OpenGL/DirectX Interop Backend',
        'deps': 'gl-win32',
        'groups': [ 'gl' ],
        'func': compose_checks(
            check_statement(['GL/gl.h', 'GL/wglext.h'], 'int i = WGL_ACCESS_WRITE_DISCARD_NV'),
            check_statement('d3d9.h', 'IDirect3D9Ex *d'))
    } , {
        'name': '--egl-angle',
        'desc': 'OpenGL ANGLE headers',
        'deps': 'os-win32 || os-cygwin',
        'groups': [ 'gl' ],
        'func': check_statement(['EGL/egl.h', 'EGL/eglext.h'],
                                'int x = EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE')
    } , {
        'name': '--egl-angle-lib',
        'desc': 'OpenGL Win32 ANGLE Library',
        'deps': 'egl-angle',
        'groups': [ 'gl' ],
        'func': check_statement(['EGL/egl.h'],
                                'eglCreateWindowSurface(0, 0, 0, 0)',
                                cflags=['-DGL_APICALL=', '-DEGLAPI=',
                                        '-DANGLE_NO_ALIASES', '-DANGLE_EXPORT='],
                                lib=['EGL', 'GLESv2', 'dxguid', 'd3d9',
                                     'gdi32', 'stdc++'])
    }, {
        'name': '--egl-angle-win32',
        'desc': 'OpenGL Win32 ANGLE Backend',
        'deps': 'egl-angle && win32-desktop',
        'groups': [ 'gl' ],
        'func': check_true,
    } , {
        'name': '--vdpau',
        'desc': 'VDPAU acceleration',
        'deps': 'x11',
        'func': check_pkg_config('vdpau', '>= 0.2'),
    } , {
        'name': '--vdpau-gl-x11',
        'desc': 'VDPAU with OpenGL/X11',
        'deps': 'vdpau && gl-x11',
        'func': check_true,
    }, {
        'name': '--vaapi',
        'desc': 'VAAPI acceleration',
        'deps': 'libdl && (x11 || wayland || egl-drm)',
        'func': check_pkg_config('libva', '>= 0.36.0'),
    }, {
        'name': '--vaapi-x11',
        'desc': 'VAAPI (X11 support)',
        'deps': 'vaapi && x11',
        'func': check_pkg_config('libva-x11', '>= 0.36.0'),
    }, {
        'name': '--vaapi-wayland',
        'desc': 'VAAPI (Wayland support)',
        'deps': 'vaapi && gl-wayland',
        'func': check_pkg_config('libva-wayland', '>= 0.36.0'),
    }, {
        'name': '--vaapi-drm',
        'desc': 'VAAPI (DRM/EGL support)',
        'deps': 'vaapi && egl-drm',
        'func': check_pkg_config('libva-drm', '>= 0.36.0'),
    }, {
        'name': '--vaapi-glx',
        'desc': 'VAAPI GLX',
        'deps': 'gpl && vaapi-x11 && gl-x11',
        'func': check_true,
    }, {
        'name': '--vaapi-x-egl',
        'desc': 'VAAPI EGL on X11',
        'deps': 'vaapi-x11 && egl-x11',
        'func': check_true,
    }, {
        'name': 'vaapi-egl',
        'desc': 'VAAPI EGL',
        'deps': 'vaapi-x-egl || vaapi-wayland',
        'func': check_true,
    }, {
        'name': '--caca',
        'desc': 'CACA',
        'deps': 'gpl',
        'func': check_pkg_config('caca', '>= 0.99.beta18'),
    }, {
        'name': '--jpeg',
        'desc': 'JPEG support',
        'func': check_cc(header_name=['stdio.h', 'jpeglib.h'],
                         lib='jpeg', use='libm'),
    }, {
        'name': '--direct3d',
        'desc': 'Direct3D support',
        'deps': 'win32-desktop && gpl',
        'func': check_cc(header_name='d3d9.h'),
    }, {
        'name': 'shaderc-shared',
        'desc': 'libshaderc SPIR-V compiler (shared library)',
        'deps': '!static-build',
        'groups': ['shaderc'],
        'func': check_cc(header_name='shaderc/shaderc.h', lib='shaderc_shared'),
    }, {
        'name': 'shaderc-static',
        'desc': 'libshaderc SPIR-V compiler (static library)',
        'deps': '!shaderc-shared',
        'groups': ['shaderc'],
        'func': check_cc(header_name='shaderc/shaderc.h',
                         lib=['shaderc_combined', 'stdc++']),
    }, {
        'name': '--shaderc',
        'desc': 'libshaderc SPIR-V compiler',
        'deps': 'shaderc-shared || shaderc-static',
        'func': check_true,
    }, {
        'name': '--crossc',
        'desc': 'libcrossc SPIR-V translator',
        'func': check_pkg_config('crossc'),
    }, {
        'name': '--d3d11',
        'desc': 'Direct3D 11 video output',
        'deps': 'win32-desktop && shaderc && crossc',
        'func': check_cc(header_name=['d3d11_1.h', 'dxgi1_2.h']),
    }, {
        # We need MMAL/bcm_host/dispmanx APIs. Also, most RPI distros require
        # every project to hardcode the paths to the include directories. Also,
        # these headers are so broken that they spam tons of warnings by merely
        # including them (compensate with -isystem and -fgnu89-inline).
        'name': '--rpi',
        'desc': 'Raspberry Pi support',
        'func': compose_checks(
            check_cc(cflags=["-isystem/opt/vc/include",
                             "-isystem/opt/vc/include/interface/vcos/pthreads",
                             "-isystem/opt/vc/include/interface/vmcs_host/linux",
                             "-fgnu89-inline"],
                     linkflags="-L/opt/vc/lib",
                     header_name="bcm_host.h",
                     lib=['mmal_core', 'mmal_util', 'mmal_vc_client', 'bcm_host']),
            # We still need all OpenGL symbols, because the vo_gpu code is
            # generic and supports anything from GLES2/OpenGL 2.1 to OpenGL 4 core.
            check_cc(lib="EGL", linkflags="-lGLESv2"),
            check_cc(lib="GLESv2"),
        ),
    } , {
        'name': '--ios-gl',
        'desc': 'iOS OpenGL ES hardware decoding interop support',
        'func': check_statement('OpenGLES/ES3/glext.h', '(void)GL_RGB32F'),  # arbitrary OpenGL ES 3.0 symbol
    } , {
        'name': '--plain-gl',
        'desc': 'OpenGL without platform-specific code (e.g. for libmpv)',
        'deps': 'libmpv-shared || libmpv-static',
        'func': check_true,
    }, {
        'name': '--mali-fbdev',
        'desc': 'MALI via Linux fbdev',
        'deps': 'libdl',
        'func': compose_checks(
            check_cc(lib="EGL"),
            check_statement('EGL/fbdev_window.h', 'struct fbdev_window test'),
            check_statement('linux/fb.h', 'struct fb_var_screeninfo test'),
        ),
    }, {
        'name': '--gl',
        'desc': 'OpenGL context support',
        'deps': 'gl-cocoa || gl-x11 || egl-x11 || egl-drm || '
                 + 'gl-win32 || gl-wayland || rpi || mali-fbdev || '
                 + 'plain-gl',
        'func': check_true,
        'req': True,
        'fmsg': "No OpenGL video output found or enabled. " +
                "Aborting. If you really mean to compile without OpenGL " +
                "video outputs use --disable-gl.",
    }, {
        'name': '--vulkan',
        'desc':  'Vulkan context support',
        'func': check_pkg_config('vulkan'),
    }, {
        'name': 'egl-helpers',
        'desc': 'EGL helper functions',
        'deps': 'egl-x11 || mali-fbdev || rpi || gl-wayland || egl-drm || ' +
                'egl-angle-win32 || android',
        'func': check_true
    }
]

hwaccel_features = [
    {
        'name': 'videotoolbox-hwaccel',
        'desc': 'libavcodec videotoolbox hwaccel',
        'deps': 'gl-cocoa || ios-gl',
        'func': check_true,
    }, {
        'name': '--videotoolbox-gl',
        'desc': 'Videotoolbox with OpenGL',
        'deps': 'gl-cocoa && videotoolbox-hwaccel',
        'func': check_true
    }, {
        'name': '--d3d-hwaccel',
        'desc': 'D3D11VA hwaccel',
        'deps': 'os-win32',
        'func': check_true,
    }, {
        'name': '--d3d9-hwaccel',
        'desc': 'DXVA2 hwaccel',
        'deps': 'd3d-hwaccel',
        'func': check_true,
    }, {
        'name': '--gl-dxinterop-d3d9',
        'desc': 'OpenGL/DirectX Interop Backend DXVA2 interop',
        'deps': 'gl-dxinterop && d3d9-hwaccel',
        'groups': [ 'gl' ],
        'func': check_true,
    }, {
        'name': 'ffnvcodec',
        'desc': 'CUDA Headers and dynamic loader',
        'func': check_pkg_config('ffnvcodec >= 8.1.24.1'),
    }, {
        'name': '--cuda-hwaccel',
        'desc': 'CUDA hwaccel',
        'deps': 'gl && ffnvcodec',
        'func': check_true,
    }
]

radio_and_tv_features = [
    {
        'name': '--tv',
        'desc': 'TV interface',
        'deps': 'gpl',
        'func': check_true,
        'default': 'disable',
    }, {
        'name': 'sys_videoio_h',
        'desc': 'videoio.h',
        'func': check_cc(header_name=['sys/time.h', 'sys/videoio.h']),
        'deps': 'tv',
    }, {
        'name': 'videodev',
        'desc': 'videodev2.h',
        'func': check_cc(header_name=['sys/time.h', 'linux/videodev2.h']),
        'deps': 'tv && !sys_videoio_h',
    }, {
        'name': '--tv-v4l2',
        'desc': 'Video4Linux2 TV interface',
        'deps': 'tv && (sys_videoio_h || videodev)',
        'func': check_true,
    }, {
        'name': '--libv4l2',
        'desc': 'libv4l2 support',
        'func': check_pkg_config('libv4l2'),
        'deps': 'tv-v4l2',
    }, {
        'name': '--audio-input',
        'desc': 'audio input support',
        'deps': 'tv-v4l2',
        'func': check_true
    } , {
        'name': '--dvbin',
        'desc': 'DVB input module',
        'deps': 'gpl',
        'func': check_true,
        'default': 'disable',
    }
]

standalone_features = [
    {
        'name': 'win32-executable',
        'desc': 'w32 executable',
        'deps': 'os-win32 || !(!(os-cygwin))',
        'func': check_ctx_vars('WINDRES')
    }, {
        'name': '--apple-remote',
        'desc': 'Apple Remote support',
        'deps': 'cocoa',
        'func': check_true
    }, {
        'name': '--macos-touchbar',
        'desc': 'macOS Touch Bar support',
        'deps': 'cocoa',
        'func': check_cc(
            fragment=load_fragment('touchbar.m'),
            framework_name=['AppKit'],
            compile_filename='test-touchbar.m',
            linkflags='-fobjc-arc')
     }, {
        'name': '--macos-cocoa-cb',
        'desc': 'macOS opengl-cb backend',
        'deps': 'cocoa  && swift',
        'func': check_true
    }
]

_INSTALL_DIRS_LIST = [
    ('confdir', '${SYSCONFDIR}/mpv',  'configuration files'),
    ('zshdir',  '${DATADIR}/zsh/site-functions', 'zsh completion functions'),
    ('confloaddir', '${CONFDIR}', 'configuration files load directory'),
]

def options(opt):
    opt.load('compiler_c')
    opt.load('waf_customizations')
    opt.load('features')
    opt.load('gnu_dirs')

    #remove unused options from gnu_dirs
    opt.parser.remove_option("--sbindir")
    opt.parser.remove_option("--libexecdir")
    opt.parser.remove_option("--sharedstatedir")
    opt.parser.remove_option("--localstatedir")
    opt.parser.remove_option("--oldincludedir")
    opt.parser.remove_option("--infodir")
    opt.parser.remove_option("--localedir")
    opt.parser.remove_option("--dvidir")
    opt.parser.remove_option("--pdfdir")
    opt.parser.remove_option("--psdir")

    libdir = opt.parser.get_option('--libdir')
    if libdir:
        # Replace any mention of lib64 as we keep the default
        # for libdir the same as before the waf update.
        libdir.help = libdir.help.replace('lib64', 'lib')

    group = opt.get_option_group("Installation directories")
    for ident, default, desc in _INSTALL_DIRS_LIST:
        group.add_option('--{0}'.format(ident),
            type    = 'string',
            dest    = ident,
            default = default,
            help    = 'directory for installing {0} [{1}]' \
                      .format(desc, default.replace('${','').replace('}','')))

    group = opt.get_option_group("build and install options")
    group.add_option('--variant',
        default = '',
        help    = 'variant name for saving configuration and build results')

    opt.parse_features('build and install options', build_options)
    optional_features = main_dependencies + libav_dependencies
    opt.parse_features('optional features', optional_features)
    opt.parse_features('audio outputs',     audio_output_features)
    opt.parse_features('video outputs',     video_output_features)
    opt.parse_features('hwaccels',          hwaccel_features)
    opt.parse_features('tv features',       radio_and_tv_features)
    opt.parse_features('standalone app',    standalone_features)

    group = opt.get_option_group("optional features")
    group.add_option('--lua',
        type    = 'string',
        dest    = 'LUA_VER',
        help    = "select Lua package which should be autodetected. Choices: 51 51deb 51obsd 51fbsd 52 52deb 52arch 52fbsd luajit")
    group.add_option('--swift-flags',
        type    = 'string',
        dest    = 'SWIFT_FLAGS',
        help    = "Optional Swift compiler flags")

@conf
def is_optimization(ctx):
    return getattr(ctx.options, 'enable_optimize')

@conf
def is_debug_build(ctx):
    return getattr(ctx.options, 'enable_debug-build')

def configure(ctx):
    from waflib import Options
    ctx.resetenv(ctx.options.variant)
    ctx.check_waf_version(mini='1.8.4')
    target = os.environ.get('TARGET')
    (cc, pkg_config, ar, windres) = ('cc', 'pkg-config', 'ar', 'windres')

    if target:
        cc         = '-'.join([target, 'gcc'])
        pkg_config = '-'.join([target, pkg_config])
        ar         = '-'.join([target, ar])
        windres    = '-'.join([target, windres])

    ctx.find_program(cc,          var='CC')
    ctx.find_program(pkg_config,  var='PKG_CONFIG')
    ctx.find_program(ar,          var='AR')
    ctx.find_program('rst2html',  var='RST2HTML',  mandatory=False)
    ctx.find_program('rst2man',   var='RST2MAN',   mandatory=False)
    ctx.find_program('rst2pdf',   var='RST2PDF',   mandatory=False)
    ctx.find_program(windres,     var='WINDRES',   mandatory=False)
    ctx.find_program('perl',      var='BIN_PERL',  mandatory=False)

    ctx.add_os_flags('LIBRARY_PATH')

    ctx.load('compiler_c')
    ctx.load('waf_customizations')
    ctx.load('dependencies')
    ctx.load('detections.compiler_swift')
    ctx.load('detections.compiler')
    ctx.load('detections.devices')
    ctx.load('gnu_dirs')

    # if libdir is not set in command line options,
    # override the gnu_dirs default in order to
    # always have `lib/` as the library directory.
    if not getattr(Options.options, 'LIBDIR', None):
        ctx.env['LIBDIR'] = Utils.subst_vars(os.path.join('${EXEC_PREFIX}', 'lib'), ctx.env)

    for ident, _, _ in _INSTALL_DIRS_LIST:
        varname = ident.upper()
        ctx.env[varname] = getattr(ctx.options, ident)

        # keep substituting vars, until the paths are fully expanded
        while re.match('\$\{([^}]+)\}', ctx.env[varname]):
            ctx.env[varname] = Utils.subst_vars(ctx.env[varname], ctx.env)

    ctx.parse_dependencies(build_options)
    ctx.parse_dependencies(main_dependencies)
    ctx.parse_dependencies(audio_output_features)
    ctx.parse_dependencies(video_output_features)
    ctx.parse_dependencies(libav_dependencies)
    ctx.parse_dependencies(hwaccel_features)
    ctx.parse_dependencies(radio_and_tv_features)

    if ctx.options.LUA_VER:
        ctx.options.enable_lua = True

    if ctx.options.SWIFT_FLAGS:
        ctx.env.SWIFT_FLAGS += ' ' + ctx.options.SWIFT_FLAGS

    ctx.parse_dependencies(standalone_features)

    ctx.load('generators.headers')

    if not ctx.dependency_satisfied('build-date'):
        ctx.env.CFLAGS += ['-DNO_BUILD_TIMESTAMPS']

    if ctx.dependency_satisfied('clang-database'):
        ctx.load('clang_compilation_database')

    if ctx.dependency_satisfied('cplugins'):
        # We need to export the libmpv symbols, since the mpv binary itself is
        # not linked against libmpv. The C plugin needs to be able to pick
        # up the libmpv symbols from the binary. We still restrict the set
        # of exported symbols via mpv.def.
        ctx.env.LINKFLAGS += ['-rdynamic']

    ctx.store_dependencies_lists()

def __write_version__(ctx):
    ctx.env.VERSIONH_ST = '--versionh="%s"'
    ctx.env.CWD_ST = '--cwd="%s"'
    ctx.env.VERSIONSH_CWD = [ctx.srcnode.abspath()]

    ctx(
        source = 'version.sh',
        target = 'version.h',
        rule   = 'sh ${SRC} ${CWD_ST:VERSIONSH_CWD} ${VERSIONH_ST:TGT}',
        always = True,
        update_outputs = True)

def build(ctx):
    if ctx.options.variant not in ctx.all_envs:
        from waflib import Errors
        raise Errors.WafError(
            'The project was not configured: run "waf --variant={0} configure" first!'
                .format(ctx.options.variant))
    ctx.unpack_dependencies_lists()
    ctx.add_group('versionh')
    ctx.add_group('sources')

    ctx.set_group('versionh')
    __write_version__(ctx)
    ctx.set_group('sources')
    ctx.load('wscript_build')

def init(ctx):
    from waflib.Build import BuildContext, CleanContext, InstallContext, UninstallContext
    for y in (BuildContext, CleanContext, InstallContext, UninstallContext):
        class tmp(y):
            variant = ctx.options.variant

    # This is needed because waf initializes the ConfigurationContext with
    # an arbitrary setenv('') which would rewrite the previous configuration
    # cache for the default variant if the configure step finishes.
    # Ideally ConfigurationContext should just let us override this at class
    # level like the other Context subclasses do with variant
    from waflib.Configure import ConfigurationContext
    class cctx(ConfigurationContext):
        def resetenv(self, name):
            self.all_envs = {}
            self.setenv(name)
