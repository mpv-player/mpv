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

build_options = [
    {
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
        'deps_neg': [ 'libmpv-shared' ],
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
        'deps': [ 'libdl' ],
        'default': 'disable',
        'func': check_cc(linkflags=['-Wl,-export-dynamic']),
    }, {
        'name': 'dlopen',
        'desc': 'dlopen',
        'deps_any': [ 'libdl', 'os-win32', 'os-cygwin' ],
        'func': check_true
    }, {
        'name': '--vf-dlopen-filters',
        'desc': 'compilation of default filters for vf_dlopen',
        'deps': [ 'dlopen' ],
        'default': 'disable',
        'func': check_true
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
        'deps': [ 'os-win32' ],
        'func': check_statement('stdlib.h', 'int x = __MINGW32__;'
                                            'int y = __MINGW64_VERSION_MAJOR'),
    }, {
        'name': 'posix',
        'desc': 'POSIX environment',
        # This should be good enough.
        'func': check_statement(['poll.h', 'unistd.h', 'sys/mman.h'],
            'struct pollfd pfd; poll(&pfd, 1, 0); fork(); int f[2]; pipe(f); munmap(f,0)'),
    }, {
        'name': 'posix-or-mingw',
        'desc': 'development environment',
        'deps_any': [ 'posix', 'mingw' ],
        'func': check_true,
        'req': True,
        'fmsg': 'Unable to find either POSIX or MinGW-w64 environment, ' \
                'or compiler does not work.',
    }, {
        'name': 'win32',
        'desc': 'win32',
        'deps_any': [ 'os-win32', 'os-cygwin' ],
        'func': check_cc(lib=['winmm', 'gdi32', 'ole32', 'uuid', 'avrt', 'dwmapi']),
    }, {
        'name': '--win32-internal-pthreads',
        'desc': 'internal pthread wrapper for win32 (Vista+)',
        'deps_neg': [ 'posix' ],
        'deps': [ 'win32' ],
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
        'name': 'atomic-builtins',
        'desc': 'compiler support for __atomic built-ins',
        'func': check_libs(['atomic'],
            check_statement('stdint.h',
                'int64_t test = 0;'
                'test = __atomic_add_fetch(&test, 1, __ATOMIC_SEQ_CST)')),
        'deps_neg': [ 'stdatomic' ],
    }, {
        'name': 'atomics',
        'desc': 'stdatomic.h support or emulation',
        'func': check_true,
        'req': True,
        'deps_any': ['stdatomic', 'atomic-builtins', 'gnuc'],
    }, {
        'name': 'c11-tls',
        'desc': 'C11 TLS support',
        'func': check_statement('stddef.h', 'static _Thread_local int x = 0'),
    }, {
        'name': 'gcc-tls',
        'desc': 'GCC TLS support',
        'func': check_statement('stddef.h', 'static __thread int x = 0'),
    }, {
        'name': 'librt',
        'desc': 'linking with -lrt',
        'deps': [ 'pthreads' ],
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
        'deps_any': [ 'os-win32', 'os-cygwin' ],
        'func': check_true
    }, {
        'name': '--termios',
        'desc': 'termios',
        'func': check_headers('termios.h', 'sys/termios.h'),
    }, {
        'name': '--shm',
        'desc': 'shm',
        'func': check_statement(['sys/types.h', 'sys/ipc.h', 'sys/shm.h'],
            'shmget(0, 0, 0); shmat(0, 0, 0); shmctl(0, 0, 0)')
    }, {
        'name': 'nanosleep',
        'desc': 'nanosleep',
        'func': check_statement('time.h', 'nanosleep(0,0)')
    }, {
        'name': 'posix-spawn',
        'desc': 'POSIX spawnp()/kill()',
        'func': check_statement(['spawn.h', 'signal.h'],
            'posix_spawnp(0,0,0,0,0,0); kill(0,0)'),
        'deps_neg': ['mingw'],
    }, {
        'name': 'subprocess',
        'desc': 'posix_spawnp() or MinGW',
        'func': check_true,
        'deps_any': ['posix-spawn', 'mingw'],
    }, {
        'name': 'glob',
        'desc': 'glob()',
        'func': check_statement('glob.h', 'glob("filename", 0, 0, 0)')
    }, {
        'name': 'glob-win32-replacement',
        'desc': 'glob() win32 replacement',
        'deps_neg': [ 'glob' ],
        'deps_any': [ 'os-win32', 'os-cygwin' ],
        'func': check_true
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
        'deps_neg': [ 'glibc-thread-name' ],
        'func': check_statement('pthread.h',
                                'pthread_setname_np("ducks")', use=['pthreads']),
    }, {
        'name': 'bsd-thread-name',
        'desc': 'BSD API for setting thread name',
        'deps_neg': [ 'glibc-thread-name', 'osx-thread-name' ],
        'func': check_statement(['pthread.h', 'pthread_np.h'],
                                'pthread_set_name_np(pthread_self(), "ducks")',
                                use=['pthreads']),
    }, {
        'name': 'netbsd-thread-name',
        'desc': 'NetBSD API for setting thread name',
        'deps_neg': [ 'glibc-thread-name', 'osx-thread-name', 'bsd-thread-name' ],
        'func': check_statement('pthread.h',
                                'pthread_setname_np(pthread_self(), "%s", (void *)"ducks")',
                                use=['pthreads']),
    }, {
        'name': 'bsd-fstatfs',
        'desc': "BSD's fstatfs()",
        'func': check_statement(['sys/param.h', 'sys/mount.h'],
                                'struct statfs fs; fstatfs(0, &fs); fs.f_fstypename')
    }, {
        'name': 'linux-fstatfs',
        'desc': "Linux's fstatfs()",
        'deps': [ 'os-linux' ],
        'func': check_statement('sys/vfs.h',
                                'struct statfs fs; fstatfs(0, &fs); fs.f_namelen')
    }, {
        'name': '--libsmbclient',
        'desc': 'Samba support',
        'deps': [ 'libdl' ],
        'func': check_pkg_config('smbclient'),
        'module': 'input',
    }, {
        'name' : '--lua',
        'desc' : 'Lua',
        'func': check_lua,
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
        'deps': [ 'libass' ],
        'func': check_true,
    }, {
        'name': 'dummy-osd',
        'desc': 'dummy OSD support',
        'deps_neg': [ 'libass-osd' ],
        'func': check_true,
    } , {
        'name': 'zlib',
        'desc': 'zlib',
        'func': check_libs(['z'],
                    check_statement('zlib.h', 'inflate(0, Z_NO_FLUSH)')),
        'req': True,
        'fmsg': 'Unable to find development files for zlib.'
    } , {
        'name' : '--encoding',
        'desc' : 'Encoding',
        'func': check_true,
    }, {
        'name': '--libbluray',
        'desc': 'Bluray support',
        'func': check_pkg_config('libbluray', '>= 0.3.0'),
    }, {
        'name': '--dvdread',
        'desc': 'dvdread support',
        'func': check_pkg_config('dvdread', '>= 4.1.0'),
    }, {
        'name': '--dvdnav',
        'desc': 'dvdnav support',
        'deps': [ 'dvdread' ],
        'func': check_pkg_config('dvdnav', '>= 4.2.0'),
    }, {
        'name': '--cdda',
        'desc': 'cdda support (libcdio)',
        'func': check_pkg_config('libcdio_paranoia'),
    }, {
        'name': '--uchardet',
        'desc': 'uchardet support',
        'deps': [ 'iconv' ],
        'func': check_pkg_config('uchardet'),
    }, {
        'name': '--rubberband',
        'desc': 'librubberband support',
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
        'deps': ['lua'],
        'func': check_pkg_config('vapoursynth',        '>= 24'),
    }, {
        'name': 'vapoursynth-core',
        'desc': 'VapourSynth filter bridge (core)',
        'deps_any': ['vapoursynth', 'vapoursynth-lazy'],
        'func': check_true,
    }, {
        'name': '--libarchive',
        'desc': 'libarchive wrapper for reading zip files and more',
        'func': check_pkg_config('libarchive >= 3.0.0'),
        'default': 'disable',
    }
]

ffmpeg_version = "3.2.2"
ffmpeg_pkg_config_checks = [
    'libavutil',     '>= 55.34.100',
    'libavcodec',    '>= 57.64.100',
    'libavformat',   '>= 57.56.100',
    'libswscale',    '>= 4.2.100',
    'libavfilter',   '>= 6.65.100',
    'libswresample', '>= 2.3.100',
]
libav_version = "12"
libav_pkg_config_checks = [
    'libavutil',     '>= 55.20.0',
    'libavcodec',    '>= 57.25.0',
    'libavformat',   '>= 57.7.0',
    'libswscale',    '>= 4.0.0',
    'libavfilter',   '>= 6.7.0',
    'libavresample', '>= 3.0.0',
]

libav_versions_string = "FFmpeg %s or Libav %s" % (ffmpeg_version, libav_version)

def check_ffmpeg_or_libav_versions():
    def fn(ctx, dependency_identifier, **kw):
        versions = ffmpeg_pkg_config_checks
        if ctx.dependency_satisfied('is_libav'):
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
        'name': 'is_ffmpeg',
        'desc': 'libav* is FFmpeg',
        # FFmpeg <=> LIBAVUTIL_VERSION_MICRO>=100
        'func': check_statement('libavcodec/version.h',
                                'int x[LIBAVCODEC_VERSION_MICRO >= 100 ? 1 : -1]',
                                use='libavcodec')
    }, {
        # This check should always result in the opposite of is_ffmpeg.
        # Run it to make sure is_ffmpeg didn't fail for some other reason than
        # the actual version check.
        'name': 'is_libav',
        'desc': 'libav* is Libav',
        # FFmpeg <=> LIBAVUTIL_VERSION_MICRO>=100
        'func': check_statement('libavcodec/version.h',
                                'int x[LIBAVCODEC_VERSION_MICRO >= 100 ? -1 : 1]',
                                use='libavcodec')
    }, {
        'name': 'libav',
        'desc': 'Libav/FFmpeg library versions',
        'deps_any': [ 'is_ffmpeg', 'is_libav' ],
        'func': check_ffmpeg_or_libav_versions(),
        'req': True,
        'fmsg': "Unable to find development files for some of the required \
FFmpeg/Libav libraries. You need at least {0}. Aborting.".format(libav_versions_string)
    }, {
        'name': '--libavdevice',
        'desc': 'libavdevice',
        'func': check_pkg_config('libavdevice', '>= 57.0.0'),
    }, {
        'name': 'avutil-imgcpy-uc',
        'desc': 'libavutil GPU memcpy for hardware decoding',
        'func': check_statement('libavutil/imgutils.h',
                                'av_image_copy_uc_from(0,0,0,0,0,0,0)',
                                use='libav'),
    },
]

audio_output_features = [
    {
        'name': '--sdl2',
        'desc': 'SDL2',
        'func': check_pkg_config('sdl2'),
        'default': 'disable'
    }, {
        'name': '--sdl1',
        'desc': 'SDL (1.x)',
        'deps_neg': [ 'sdl2' ],
        'func': check_pkg_config('sdl'),
        'default': 'disable'
    }, {
        'name': 'oss-audio-4front',
        'desc': 'OSS (implementation from opensound.com)',
        'func': check_oss_4front,
        'groups' : [ 'oss-audio' ]
    }, {
        'name': 'oss-audio-native',
        'desc': 'OSS (platform-specific OSS implementation)',
        'func': check_cc(header_name='sys/soundcard.h',
                         defines=['PATH_DEV_DSP="/dev/dsp"',
                                  'PATH_DEV_MIXER="/dev/mixer"'],
                         fragment=load_fragment('oss_audio.c')),
        'deps_neg': [ 'oss-audio-4front' ],
        'groups' : [ 'oss-audio' ]
    }, {
        'name': 'oss-audio-sunaudio',
        'desc': 'OSS (emulation on top of SunAudio)',
        'func': check_cc(header_name='soundcard.h',
                         lib='ossaudio',
                         defines=['PATH_DEV_DSP="/dev/sound"',
                                  'PATH_DEV_MIXER="/dev/mixer"'],
                         fragment=load_fragment('oss_audio_sunaudio.c')),
        'deps_neg': [ 'oss-audio-4front', 'oss-audio-native' ],
        'groups' : [ 'oss-audio' ]
    }, {
        'name': '--oss-audio',
        'desc': 'OSS audio output',
        'func': check_true,
        'deps_any': [ 'oss-audio-native', 'oss-audio-sunaudio',
                      'oss-audio-4front' ]
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
        'func': check_pkg_config('jack'),
    }, {
        'name': '--openal',
        'desc': 'OpenAL audio output',
        'func': check_openal,
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
        'deps': ['atomics'],
        'func': check_cc(
            fragment=load_fragment('audiounit.c'),
            framework_name=['Foundation', 'AudioToolbox'])
    }, {
        'name': '--wasapi',
        'desc': 'WASAPI audio output',
        'deps': ['win32'],
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
        'deps': [ 'vt.h' ],
        'func': check_pkg_config('libdrm'),
    }, {
        'name': '--gbm',
        'desc': 'GBM',
        'deps': [ 'gbm.h' ],
        'func': check_pkg_config('gbm'),
    } , {
        'name': '--wayland',
        'desc': 'Wayland',
        'func': check_pkg_config('wayland-client', '>= 1.6.0',
                                 'wayland-cursor', '>= 1.6.0',
                                 'xkbcommon',      '>= 0.3.0'),
    } , {
        'name': '--x11',
        'desc': 'X11',
        'func': check_pkg_config('x11'),
    } , {
        'name': '--xss',
        'desc': 'Xss screensaver extensions',
        'deps': [ 'x11' ],
        'func': check_pkg_config('xscrnsaver'),
    } , {
        'name': '--xext',
        'desc': 'X extensions',
        'deps': [ 'x11' ],
        'func': check_pkg_config('xext'),
    } , {
        'name': '--xv',
        'desc': 'Xv video output',
        'deps': [ 'x11' ],
        'func': check_pkg_config('xv'),
    } , {
        'name': '--xinerama',
        'desc': 'Xinerama',
        'deps': [ 'x11' ],
        'func': check_pkg_config('xinerama'),
    }, {
        'name': '--xrandr',
        'desc': 'Xrandr',
        'deps': [ 'x11' ],
        'func': check_pkg_config('xrandr', '>= 1.2.0'),
    } , {
        'name': '--gl-cocoa',
        'desc': 'OpenGL Cocoa Backend',
        'deps': [ 'cocoa' ],
        'groups': [ 'gl' ],
        'func': check_statement('IOSurface/IOSurface.h',
                                'IOSurfaceRef surface;',
                                framework='IOSurface')
    } , {
        'name': '--gl-x11',
        'desc': 'OpenGL X11 Backend',
        'deps': [ 'x11' ],
        'groups': [ 'gl' ],
        'func': check_libs(['GL', 'GL Xdamage'],
                   check_cc(fragment=load_fragment('gl_x11.c'),
                            use=['x11', 'libdl', 'pthreads']))
    } , {
        'name': '--egl-x11',
        'desc': 'OpenGL X11 EGL Backend',
        'deps': [ 'x11' ],
        'groups': [ 'gl' ],
        'func': check_pkg_config('egl', 'gl'),
    } , {
        'name': '--egl-drm',
        'desc': 'OpenGL DRM EGL Backend',
        'deps': [ 'drm', 'gbm' ],
        'groups': [ 'gl' ],
        'func': compose_checks(
            check_pkg_config('egl'),
            check_pkg_config_cflags('gl')
        )
    } , {
        'name': '--gl-wayland',
        'desc': 'OpenGL Wayland Backend',
        'deps': [ 'wayland' ],
        'groups': [ 'gl' ],
        'func': check_pkg_config('wayland-egl', '>= 9.0.0',
                                 'egl',         '>= 9.0.0')
    } , {
        'name': '--gl-win32',
        'desc': 'OpenGL Win32 Backend',
        'deps': [ 'win32' ],
        'groups': [ 'gl' ],
        'func': check_statement('windows.h', 'wglCreateContext(0)',
                                lib='opengl32')
    } , {
        'name': '--gl-dxinterop',
        'desc': 'OpenGL/DirectX Interop Backend',
        'deps': [ 'gl-win32' ],
        'groups': [ 'gl' ],
        'func': compose_checks(
            check_statement(['GL/gl.h', 'GL/wglext.h'], 'int i = WGL_ACCESS_WRITE_DISCARD_NV'),
            check_statement('d3d9.h', 'IDirect3D9Ex *d'))
    } , {
        'name': '--egl-angle',
        'desc': 'OpenGL Win32 ANGLE Backend',
        'deps_any': [ 'os-win32', 'os-cygwin' ],
        'groups': [ 'gl' ],
        'func': check_statement(['EGL/egl.h', 'EGL/eglext.h'],
                                'int x = EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE')
    } , {
        'name': '--egl-angle-lib',
        'desc': 'OpenGL Win32 ANGLE Library',
        'deps': [ 'egl-angle' ],
        'groups': [ 'gl' ],
        'func': check_statement(['EGL/egl.h'],
                                'eglCreateWindowSurface(0, 0, 0, 0)',
                                cflags="-DGL_APICALL= -DEGLAPI= -DANGLE_NO_ALIASES -DANGLE_EXPORT=",
                                lib=['EGL', 'GLESv2', 'dxguid', 'd3d9', 'gdi32', 'stdc++'])
    } , {
        'name': '--vdpau',
        'desc': 'VDPAU acceleration',
        'deps': [ 'x11' ],
        'func': check_pkg_config('vdpau', '>= 0.2'),
    } , {
        'name': '--vdpau-gl-x11',
        'desc': 'VDPAU with OpenGL/X11',
        'deps': [ 'vdpau', 'gl-x11' ],
        'func': check_true,
    }, {
        'name': '--vaapi',
        'desc': 'VAAPI acceleration',
        'deps': [ 'libdl' ],
        'deps_any': [ 'x11', 'wayland', 'egl-drm' ],
        'func': check_pkg_config('libva', '>= 0.36.0'),
    }, {
        'name': '--vaapi-x11',
        'desc': 'VAAPI (X11 support)',
        'deps': [ 'vaapi', 'x11' ],
        'func': check_pkg_config('libva-x11', '>= 0.36.0'),
    }, {
        'name': '--vaapi-wayland',
        'desc': 'VAAPI (Wayland support)',
        'deps': [ 'vaapi', 'gl-wayland' ],
        'func': check_pkg_config('libva-wayland', '>= 0.36.0'),
    }, {
        'name': '--vaapi-drm',
        'desc': 'VAAPI (DRM/EGL support)',
        'deps': [ 'vaapi', 'egl-drm' ],
        'func': check_pkg_config('libva-drm', '>= 0.36.0'),
    }, {
        'name': '--vaapi-glx',
        'desc': 'VAAPI GLX',
        'deps': [ 'vaapi-x11', 'gl-x11' ],
        'func': check_true,
    }, {
        'name': '--vaapi-x-egl',
        'desc': 'VAAPI EGL on X11',
        'deps': [ 'vaapi-x11', 'egl-x11' ],
        'func': check_true,
    }, {
        'name': 'vaapi-egl',
        'desc': 'VAAPI EGL',
        'deps_any': [ 'vaapi-x-egl', 'vaapi-wayland' ],
        'func': check_true,
    }, {
        'name': '--caca',
        'desc': 'CACA',
        'func': check_pkg_config('caca', '>= 0.99.beta18'),
    }, {
        'name': '--jpeg',
        'desc': 'JPEG support',
        'func': check_cc(header_name=['stdio.h', 'jpeglib.h'],
                         lib='jpeg', use='libm'),
    }, {
        'name': '--direct3d',
        'desc': 'Direct3D support',
        'deps': [ 'win32' ],
        'func': check_cc(header_name='d3d9.h'),
    }, {
        'name': '--android',
        'desc': 'Android support',
        'func': check_statement('android/api-level.h', '(void)__ANDROID__'),  # arbitrary android-specific header
    }, {
        'name': '--rpi',
        'desc': 'Raspberry Pi support',
        'func': check_rpi,
    }, {
        'name': '--standard-gl',
        'desc': 'Desktop standard OpenGL support',
        'func': compose_checks(
            check_statement('GL/gl.h', '(void)GL_RGB32F'),     # arbitrary OpenGL 3.0 symbol
            check_statement('GL/gl.h', '(void)GL_LUMINANCE16') # arbitrary OpenGL legacy-only symbol
        ),
    } , {
        'name': '--android-gl',
        'desc': 'Android OpenGL ES support',
        'deps': ['android'],
        'func': check_statement('GLES3/gl3.h', '(void)GL_RGB32F'),  # arbitrary OpenGL ES 3.0 symbol
    } , {
        'name': '--ios-gl',
        'desc': 'iOS OpenGL ES support',
        'func': check_statement('OpenGLES/ES3/glext.h', '(void)GL_RGB32F'),  # arbitrary OpenGL ES 3.0 symbol
    } , {
        'name': '--any-gl',
        'desc': 'Any OpenGL (ES) support',
        'deps_any': ['standard-gl', 'android-gl', 'ios-gl', 'cocoa'],
        'func': check_true
    } , {
        'name': '--plain-gl',
        'desc': 'OpenGL without platform-specific code (e.g. for libmpv)',
        'deps': ['any-gl'],
        'deps_any': [ 'libmpv-shared', 'libmpv-static' ],
        'func': check_true,
    }, {
        'name': '--mali-fbdev',
        'desc': 'MALI via Linux fbdev',
        'deps': ['standard-gl', 'libdl'],
        'func': compose_checks(
            check_cc(lib="EGL"),
            check_cc(lib="GLESv2"),
            check_statement('EGL/fbdev_window.h', 'struct fbdev_window test'),
            check_statement('linux/fb.h', 'struct fb_var_screeninfo test'),
        ),
    }, {
        'name': '--gl',
        'desc': 'OpenGL video outputs',
        'deps_any': [ 'gl-cocoa', 'gl-x11', 'egl-x11', 'egl-drm',
                      'gl-win32', 'gl-wayland', 'rpi', 'mali-fbdev',
                      'plain-gl' ],
        'func': check_true,
        'req': True,
        'fmsg': "Unable to find OpenGL header files for video output. " +
                "Aborting. If you really mean to compile without OpenGL " +
                "video outputs use --disable-gl."
    }, {
        'name': 'egl-helpers',
        'desc': 'EGL helper functions',
        'deps_any': [ 'egl-x11', 'mali-fbdev', 'rpi', 'gl-wayland', 'egl-drm' ],
        'func': check_true
    }
]

hwaccel_features = [
    {
        'name': '--vaapi-hwaccel',
        'desc': 'libavcodec VAAPI hwaccel',
        'deps': [ 'vaapi' ],
        'func': check_headers('libavcodec/vaapi.h', use='libav'),
    }, {
        'name': '--vaapi-hwaccel-new',
        'desc': 'libavcodec VAAPI hwaccel (new)',
        'deps': [ 'vaapi-hwaccel' ],
        'func': check_statement('libavcodec/version.h',
            'int x[(LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 26, 0) && '
            '       LIBAVCODEC_VERSION_MICRO < 100) ||'
            '      (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 74, 100) && '
            '       LIBAVCODEC_VERSION_MICRO >= 100)'
            '      ? 1 : -1]',
            use='libav'),
    }, {
        'name': '--vaapi-hwaccel-old',
        'desc': 'libavcodec VAAPI hwaccel (old)',
        'deps': [ 'vaapi-hwaccel' ],
        'deps_neg': [ 'vaapi-hwaccel-new' ],
        'func': check_true,
    }, {
        'name': '--videotoolbox-hwaccel',
        'desc': 'libavcodec videotoolbox hwaccel',
        'func': compose_checks(
            check_headers('VideoToolbox/VideoToolbox.h'),
            check_statement('libavcodec/videotoolbox.h',
                            'av_videotoolbox_alloc_context()',
                            use='libav')),
    } , {
        'name': '--videotoolbox-gl',
        'desc': 'Videotoolbox with OpenGL',
        'deps': [ 'gl-cocoa', 'videotoolbox-hwaccel' ],
        'func': check_true
    } , {
        'name': '--vdpau-hwaccel',
        'desc': 'libavcodec VDPAU hwaccel',
        'deps': [ 'vdpau' ],
        'func': check_statement('libavcodec/vdpau.h',
                                'av_vdpau_bind_context(0,0,0,AV_HWACCEL_FLAG_ALLOW_HIGH_DEPTH)',
                                use='libav'),
    }, {
        'name': '--d3d-hwaccel',
        'desc': 'libavcodec DXVA2 and D3D11VA hwaccel',
        'deps': [ 'win32' ],
        'func': compose_checks(
                    check_headers('libavcodec/dxva2.h',  use='libav'),
                    check_headers('libavcodec/d3d11va.h',  use='libav')),
    }, {
        'name': '--cuda-hwaccel',
        'desc': 'CUDA hwaccel',
        'deps': [ 'gl' ],
        'func': check_cc(fragment=load_fragment('cuda.c'),
                         use='libav'),
    }, {
        'name': 'sse4-intrinsics',
        'desc': 'GCC SSE4 intrinsics for GPU memcpy',
        'deps_any': [ 'd3d-hwaccel', 'vaapi-hwaccel-old' ],
        'func': check_cc(fragment=load_fragment('sse.c')),
    }
]

radio_and_tv_features = [
    {
        'name': '--tv',
        'desc': 'TV interface',
        'func': check_true,
    }, {
        'name': 'sys_videoio_h',
        'desc': 'videoio.h',
        'func': check_cc(header_name=['sys/time.h', 'sys/videoio.h'])
    }, {
        'name': 'videodev',
        'desc': 'videodev2.h',
        'func': check_cc(header_name=['sys/time.h', 'linux/videodev2.h']),
        'deps_neg': [ 'sys_videoio_h' ],
    }, {
        'name': '--tv-v4l2',
        'desc': 'Video4Linux2 TV interface',
        'deps': [ 'tv' ],
        'deps_any': [ 'sys_videoio_h', 'videodev' ],
        'func': check_true,
    }, {
        'name': '--libv4l2',
        'desc': 'libv4l2 support',
        'func': check_pkg_config('libv4l2'),
        'deps': [ 'tv-v4l2' ],
    }, {
        'name': '--audio-input',
        'desc': 'audio input support',
        'deps_any': [ 'tv-v4l2' ],
        'func': check_true
    } , {
        'name': '--dvbin',
        'desc': 'DVB input module',
        'func': check_cc(fragment=load_fragment('dvb.c')),
    }
]

standalone_features = [
    {
        'name': 'win32-executable',
        'desc': 'w32 executable',
        'deps_any': [ 'os-win32', 'os-cygwin'],
        'func': check_ctx_vars('WINDRES')
    }, {
        'name': '--apple-remote',
        'desc': 'Apple Remote support',
        'deps': [ 'cocoa' ],
        'func': check_true
    }
]

_INSTALL_DIRS_LIST = [
    ('bindir',  '${PREFIX}/bin',      'binary files'),
    ('libdir',  '${PREFIX}/lib',      'library files'),
    ('confdir', '${PREFIX}/etc/mpv',  'configuration files'),

    ('incdir',  '${PREFIX}/include',  'include files'),

    ('datadir', '${PREFIX}/share',    'data files'),
    ('mandir',  '${DATADIR}/man',     'man pages '),
    ('docdir',  '${DATADIR}/doc/mpv', 'documentation files'),
    ('htmldir', '${DOCDIR}',          'html documentation files'),
    ('zshdir',  '${DATADIR}/zsh/site-functions', 'zsh completion functions'),

    ('confloaddir', '${CONFDIR}', 'configuration files load directory'),
]

def options(opt):
    opt.load('compiler_c')
    opt.load('waf_customizations')
    opt.load('features')

    group = opt.get_option_group("build and install options")
    for ident, default, desc in _INSTALL_DIRS_LIST:
        group.add_option('--{0}'.format(ident),
            type    = 'string',
            dest    = ident,
            default = default,
            help    = 'directory for installing {0} [{1}]' \
                      .format(desc, default))

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

@conf
def is_optimization(ctx):
    return getattr(ctx.options, 'enable_optimize')

@conf
def is_debug_build(ctx):
    return getattr(ctx.options, 'enable_debug-build')

def configure(ctx):
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
    ctx.load('detections.compiler')
    ctx.load('detections.devices')

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

    ctx.parse_dependencies(standalone_features)

    ctx.define('HAVE_SYS_SOUNDCARD_H',
               '(HAVE_OSS_AUDIO_NATIVE || HAVE_OSS_AUDIO_4FRONT)',
               quote=False)

    ctx.define('HAVE_SOUNDCARD_H',
               'HAVE_OSS_AUDIO_SUNAUDIO',
               quote=False)

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
        ctx.env.LINKFLAGS += ['-Wl,-export-dynamic']

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
    __write_version__(ctx)
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
