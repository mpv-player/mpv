# vi: ft=python

import sys, os, re
sys.path.insert(0, os.path.join(os.getcwd(), 'waftools'))
sys.path.insert(0, os.getcwd())
from waflib.Configure import conf
from waflib import Utils
from waftools.checks.generic import *
from waftools.checks.custom import *

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
        'name': '--pdf-build',
        'desc': 'pdf manual generation',
        'func': check_ctx_vars('RST2PDF'),
        'default': 'disable',
    }, {
        'name': 'libdl',
        'desc': 'dynamic loader',
        'func': check_libs(['dl'], check_statement('dlfcn.h', 'dlopen("", 0)'))
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
        'func': check_pkg_config('cmocka', '>= 0.4.1'),
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
        'func': check_statement('stddef.h', 'int x = __MINGW32__;'
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
        'func': check_cc(lib=['winmm', 'gdi32', 'ole32', 'uuid']),
    }, {
        'name': '--win32-internal-pthreads',
        'desc': 'internal pthread wrapper for win32 (Vista+)',
        'deps_neg': [ 'posix' ],
        'deps': [ 'win32' ],
        'func': check_true,
        'default': 'disable',
    }, {
        'name': 'pthreads',
        'desc': 'POSIX threads',
        'func': check_pthreads,
        'req': True,
        'fmsg': 'Unable to find pthreads support.'
    }, {
        'name': 'stdatomic',
        'desc': 'stdatomic.h',
        'func': check_libs(['atomic'],
            check_statement('stdatomic.h',
                'atomic_int_least64_t test = ATOMIC_VAR_INIT(123);'
                'int test2 = atomic_load(&test)'))
    }, {
        'name': 'atomic-builtins',
        'desc': 'compiler support for __atomic built-ins',
        'func': check_libs(['atomic'],
            check_statement('stdint.h',
                'int64_t test = 0;'
                'test = __atomic_add_fetch(&test, 1, __ATOMIC_SEQ_CST)')),
        'deps_neg': [ 'stdatomic' ],
    }, {
        'name': 'sync-builtins',
        'desc': 'compiler support for __sync built-ins',
        'func': check_statement('stdint.h',
                    'int64_t test = 0;'
                    '__typeof__(test) x = ({int a = 1; a;});'
                    'test = __sync_add_and_fetch(&test, 1)'),
        'deps_neg': [ 'stdatomic', 'atomic-builtins' ],
    }, {
        'name': 'atomics',
        'desc': 'compiler support for usable thread synchronization built-ins',
        'func': check_true,
        'deps_any': ['stdatomic', 'atomic-builtins', 'sync-builtins'],
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
        'name': '--waio',
        'desc': 'libwaio for win32',
        'deps': [ 'os-win32', 'mingw' ],
        'func': check_libs(['waio'],
                    check_statement('waio/waio.h', 'waio_alloc(0, 0, 0, 0)')),
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
        'name': '--libguess',
        'desc': 'libguess support',
        'func': check_pkg_config('libguess', '>= 1.0'),
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
        'name': '--enca',
        'desc': 'ENCA support',
        'func': check_statement('enca.h', 'enca_get_languages(NULL)', lib='enca'),
    }, {
        'name': '--ladspa',
        'desc': 'LADSPA plugin support',
        'func': check_statement('ladspa.h', 'LADSPA_Descriptor ld = {0}'),
    }, {
        'name': '--rubberband',
        'desc': 'librubberband support',
        'func': check_pkg_config('rubberband', '>= 1.8.0'),
    }, {
        'name': '--libbs2b',
        'desc': 'libbs2b audio filter support',
        'func': check_pkg_config('libbs2b'),
    }, {
        'name': '--lcms2',
        'desc': 'LCMS2 support',
        'func': check_pkg_config('lcms2', '>= 2.6'),
    }, {
        'name': 'vapoursynth-core',
        'desc': 'VapourSynth filter bridge (core)',
        'func': check_pkg_config('vapoursynth >= 24'),
    }, {
        'name': '--vapoursynth',
        'desc': 'VapourSynth filter bridge (Python)',
        'deps': ['vapoursynth-core'],
        'func': check_pkg_config('vapoursynth-script >= 23'),
    }, {
        'name': '--vapoursynth-lazy',
        'desc': 'VapourSynth filter bridge (Lazy Lua)',
        'deps': ['vapoursynth-core', 'lua'],
        'func': check_true,
    }
]

libav_pkg_config_checks = [
    'libavutil',   '>= 54.02.0',
    'libavcodec',  '>= 56.1.0',
    'libavformat', '>= 56.01.0',
    'libswscale',  '>= 2.1.3'
]
libav_versions_string = "FFmpeg 2.4 or Libav 11"

libav_dependencies = [
    {
        'name': 'libav',
        'desc': 'libav/ffmpeg',
        'func': check_pkg_config(*libav_pkg_config_checks),
        'req': True,
        'fmsg': "Unable to find development files for some of the required \
FFmpeg/Libav libraries. You need at least {0}. Aborting.".format(libav_versions_string)
    }, {
        'name': '--libswresample',
        'desc': 'libswresample',
        'func': check_pkg_config('libswresample', '>= 1.1.100'),
    }, {
        'name': '--libavresample',
        'desc': 'libavresample',
        'func': check_pkg_config('libavresample',  '>= 2.1.0'),
        'deps_neg': ['libswresample'],
    }, {
        'name': 'resampler',
        'desc': 'usable resampler found',
        'deps_any': [ 'libavresample', 'libswresample' ],
        'func': check_true,
        'req':  True,
        'fmsg': 'No resampler found. Install libavresample or libswresample (FFmpeg).'
    }, {
        'name': '--libavfilter',
        'desc': 'libavfilter',
        'func': check_pkg_config('libavfilter', '>= 5.0.0'),
    }, {
        'name': '--libavdevice',
        'desc': 'libavdevice',
        'func': check_pkg_config('libavdevice', '>= 55.0.0'),
    }, {
        'name': 'avcodec-chroma-pos-api',
        'desc': 'libavcodec avcodec_enum_to_chroma_pos API',
        'func': check_statement('libavcodec/avcodec.h', """int x, y;
            avcodec_enum_to_chroma_pos(&x, &y, AVCHROMA_LOC_UNSPECIFIED)""",
            use='libav')
    }, {
        'name': 'avframe-metadata',
        'desc': 'libavutil AVFrame metadata',
        'func': check_statement('libavutil/frame.h',
                                'av_frame_get_metadata(NULL)',
                                use='libav')
    }, {
        'name': 'avframe-skip-samples',
        'desc': 'libavutil AVFrame skip samples metadata',
        'func': check_statement('libavutil/frame.h',
                                'enum AVFrameSideDataType type = AV_FRAME_DATA_SKIP_SAMPLES',
                                use='libav')
    }, {
        'name': 'av-pix-fmt-mmal',
        'desc': 'libavutil AV_PIX_FMT_MMAL',
        'func': check_statement('libavutil/pixfmt.h',
                                'int x = AV_PIX_FMT_MMAL',
                                use='libav'),
    }
]

audio_output_features = [
    {
        'name': '--sdl2',
        'desc': 'SDL2',
        'deps': ['atomics'],
        'func': check_pkg_config('sdl2'),
        'default': 'disable'
    }, {
        'name': '--sdl1',
        'desc': 'SDL (1.x)',
        'deps': ['atomics'],
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
        'deps': ['atomics'],
        'func': check_pkg_config('jack'),
    }, {
        'name': '--openal',
        'desc': 'OpenAL audio output',
        'func': check_pkg_config('openal', '>= 1.13'),
        'default': 'disable'
    }, {
        'name': '--alsa',
        'desc': 'ALSA audio output',
        'func': check_pkg_config('alsa', '>= 1.0.18'),
    }, {
        'name': '--coreaudio',
        'desc': 'CoreAudio audio output',
        'deps': ['atomics'],
        'func': check_cc(
            fragment=load_fragment('coreaudio.c'),
            framework_name=['CoreFoundation', 'CoreAudio', 'AudioUnit', 'AudioToolbox'])
    }, {
        'name': '--dsound',
        'desc': 'DirectSound audio output',
        'func': check_cc(header_name='dsound.h'),
    }, {
        'name': '--wasapi',
        'desc': 'WASAPI audio output',
        'deps': ['win32', 'atomics'],
        'func': check_cc(fragment=load_fragment('wasapi.c')),
    }
]

video_output_features = [
    {
        'name': '--cocoa',
        'desc': 'Cocoa',
        'func': check_cocoa
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
        'func': check_true
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
        'default': 'disable',
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
        'deps': [ 'x11', 'libdl' ],
        'func': check_pkg_config(
            'libva', '>= 0.32.0', 'libva-x11', '>= 0.32.0'),
    }, {
        'name': '--vaapi-vpp',
        'desc': 'VAAPI VPP',
        'deps': [ 'vaapi' ],
        'func': check_pkg_config('libva', '>= 0.34.0'),
    }, {
        'name': '--vaapi-glx',
        'desc': 'VAAPI GLX',
        'deps': [ 'vaapi', 'gl-x11' ],
        'func': check_true,
    }, {
        'name': '--caca',
        'desc': 'CACA',
        'func': check_pkg_config('caca', '>= 0.99.beta18'),
    }, {
        'name': '--drm',
        'desc': 'DRM',
        'deps': [ 'vt.h' ],
        'func': check_pkg_config('libdrm'),
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
        # We need MMAL/bcm_host/dispmanx APIs. Also, most RPI distros require
        # every project to hardcode the paths to the include directories. Also,
        # these headers are so broken that they spam tons of warnings by merely
        # including them (compensate with -isystem and -fgnu89-inline).
        'name': '--rpi',
        'desc': 'Raspberry Pi support',
        'func':
            check_cc(cflags="-isystem/opt/vc/include/ "+
                            "-isystem/opt/vc/include/interface/vcos/pthreads " +
                            "-isystem/opt/vc/include/interface/vmcs_host/linux " +
                            "-fgnu89-inline",
                     linkflags="-L/opt/vc/lib",
                     header_name="bcm_host.h",
                     lib=['mmal_core', 'mmal_util', 'mmal_vc_client', 'bcm_host']),
    }, {
        'name': '--rpi-gles',
        'desc': 'GLES on Raspberry Pi',
        'groups': [ 'gl' ],
        'deps': ['rpi'],
        # We still need all OpenGL symbols, because the vo_opengl code is
        # generic and supports anything from GLES2/OpenGL 2.1 to OpenGL 4 core.
        'func': compose_checks(
            check_cc(lib="EGL"),
            check_cc(lib="GLESv2"),
            check_statement('GL/gl.h', '(void)GL_RGB32F'),     # arbitrary OpenGL 3.0 symbol
            check_statement('GL/gl.h', '(void)GL_LUMINANCE16') # arbitrary OpenGL legacy-only symbol
            ),
    } , {
        'name': '--gl',
        'desc': 'OpenGL video outputs',
        'deps_any': [ 'gl-cocoa', 'gl-x11', 'gl-win32', 'gl-wayland', 'rpi-gles' ],
        'func': check_true
    }
]

hwaccel_features = [
    {
        'name': '--vaapi-hwaccel',
        'desc': 'libavcodec VAAPI hwaccel',
        'deps': [ 'vaapi' ],
        'func': check_headers('libavcodec/vaapi.h', use='libav'),
    } , {
        'name': '--vda-hwaccel',
        'desc': 'libavcodec VDA hwaccel',
        'func': compose_checks(
            check_headers('VideoDecodeAcceleration/VDADecoder.h'),
            check_statement('libavcodec/vda.h',
                            'av_vda_alloc_context()',
                            framework='IOSurface',
                            use='libav')),
    }, {
        'name': '--vda-gl',
        'desc': 'VDA with OpenGL',
        'deps': [ 'gl-cocoa', 'vda-hwaccel' ],
        'func': check_true
    }, {
        'name': '--vdpau-hwaccel',
        'desc': 'libavcodec VDPAU hwaccel',
        'deps': [ 'vdpau' ],
        'func': check_statement('libavcodec/vdpau.h',
                                'av_vdpau_alloc_context()',
                                use='libav'),
    }, {
        'name': '--dxva2-hwaccel',
        'desc': 'libavcodec DXVA2 hwaccel',
        'deps': [ 'win32' ],
        'func': check_headers('libavcodec/dxva2.h', use='libav'),
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
        'name': '--pvr',
        'desc': 'Video4Linux2 MPEG PVR interface',
        'func': check_cc(fragment=load_fragment('pvr.c')),
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
    ('zshdir',  '${DATADIR}/zsh/site-functions', 'zsh completion functions'),
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
        help    = "select Lua package which should be autodetected. Choices: 51 51deb 51fbsd 52 52deb 52fbsd luajit")

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
    ctx.find_program('perl',      var='BIN_PERL')
    ctx.find_program('rst2man',   var='RST2MAN',   mandatory=False)
    ctx.find_program('rst2pdf',   var='RST2PDF',   mandatory=False)
    ctx.find_program(windres,     var='WINDRES',   mandatory=False)

    for ident, _, _ in _INSTALL_DIRS_LIST:
        varname = ident.upper()
        ctx.env[varname] = getattr(ctx.options, ident)

        # keep substituting vars, until the paths are fully expanded
        while re.match('\$\{([^}]+)\}', ctx.env[varname]):
            ctx.env[varname] = Utils.subst_vars(ctx.env[varname], ctx.env)

    ctx.load('compiler_c')
    ctx.load('waf_customizations')
    ctx.load('dependencies')
    ctx.load('detections.compiler')
    ctx.load('detections.devices')

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

    ctx.store_dependencies_lists()

def build(ctx):
    if ctx.options.variant not in ctx.all_envs:
        from waflib import Errors
        raise Errors.WafError(
            'The project was not configured: run "waf --variant={0} configure" first!'
                .format(ctx.options.variant))
    ctx.unpack_dependencies_lists()
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
