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
        'func': check_ctx_vars('RST2LATEX', 'PDFLATEX'),
        'default': 'disable'
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
        'name': '--macosx-bundle',
        'desc': 'compilation of a Mac OS X Application bundle',
        'deps': [ 'os-darwin' ],
        'default': 'disable',
        'func': check_true
    }, {
        'name': 'win32-executable',
        'desc': 'w32 executable',
        'deps_any': [ 'os-win32', 'os-cygwin'],
        'func': check_ctx_vars('WINDRES')
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
    }, {
        'name': 'ebx_available',
        'desc': 'ebx availability',
        'func': check_cc(fragment=load_fragment('ebx.c'))
    } , {
        'name': 'libm',
        'desc': '-lm',
        'func': check_cc(lib='m')
    }, {
        'name': 'nanosleep',
        'desc': 'nanosleep',
        'func': check_statement('time.h', 'nanosleep(0,0)')
    }, {
        'name': 'sys-mman-h',
        'desc': 'mman.h',
        'func': check_statement('sys/mman.h', 'mmap(0, 0, 0, 0, 0, 0)')
    }, {
        'name': 'pthreads',
        'desc': 'POSIX threads',
        'func': check_pthreads,
        'req': True,
        'fmsg': 'Unable to find pthreads support.'
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
        'name': 'priority',
        'desc': 'w32 priority API',
        'deps_any': [ 'os-win32', 'os-cygwin'],
        'func': check_true
    }, {
        'name': 'videoio',
        'desc': 'videoio.h',
        'func': check_headers('sys/videoio.h')
    }, {
        'name': '--terminfo',
        'desc': 'terminfo',
        'func': check_libs(['ncurses', 'ncursesw'],
            check_statement('term.h', 'setupterm(0, 1, 0)')),
    }, {
        'name': '--termcap',
        'desc': 'termcap',
        'deps_neg': ['terminfo'],
        'func': check_libs(['ncurses', 'tinfo', 'termcap'],
            check_statement('term.h', 'tgetent(0, 0)')),
    }, {
        'name': '--termios',
        'desc': 'termios',
        'func': check_headers('termios.h', 'sys/termios.h'),
    }, {
        'name': '--shm',
        'desc': 'shm',
        'func': check_statement('sys/shm.h',
            'shmget(0, 0, 0); shmat(0, 0, 0); shmctl(0, 0, 0)')
    }, {
        'name': 'posix-select',
        'desc': 'POSIX select()',
        'func': check_statement('sys/select.h', """
            int rc;
            rc = select(0, (fd_set *)(0), (fd_set *)(0), (fd_set *)(0),
                        (struct timeval *)(0))""")
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
        'name': 'setmode',
        'desc': 'setmode()',
        'func': check_statement('io.h', 'setmode(0, 0)')
    }, {
        'name': 'sys-sysinfo-h',
        'desc': 'sys/sysinfo.h',
        'func': check_statement('sys/sysinfo.h',
            'struct sysinfo s_info; s_info.mem_unit=0; sysinfo(&s_info)')
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
        'name': '--libquvi4',
        'desc': 'libquvi 0.4.x support',
        'groups': [ 'libquvi' ],
        'func': check_pkg_config('libquvi', '>= 0.4.1'),
    }, {
        'name': '--libquvi9',
        'desc': 'libquvi 0.9.x support',
        'groups': [ 'libquvi' ],
        'deps_neg': [ 'libquvi4' ],
        'func': check_pkg_config('libquvi-0.9', '>= 0.9.0'),
    }, {
        'name': '--libquvi',
        'desc': 'libquvi support',
        'deps_any': [ 'libquvi4', 'libquvi9' ],
        'func': check_true
    }, {
        'name': '--libass',
        'desc': 'SSA/ASS support',
        'func': check_pkg_config('libass'),
        'req': True,
        'fmsg': "Unable to find development files for libass. Aborting. \
If you really mean to compile without libass support use --disable-libass."
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
        'name' : '--joystick',
        'desc' : 'joystick',
        'func': check_cc(header_name='linux/joystick.h'),
        'default': 'disable'
    }, {
        'name' : '--lirc',
        'desc' : 'lirc',
        'func': check_cc(header_name='lirc/lirc_client.h', lib='lirc_client'),
    }, {
        'name' : '--vcd',
        'desc' : 'VCD support',
        'deps_any': [ 'os-linux', 'os-freebsd', 'os-netbsd', 'os-openbsd', 'os-darwin' ],
        'func': check_true,
        'os_specific_checks': {
            'os-win32': {
                'func': check_cc(fragment=load_fragment('vcd_windows.c'))
            }
        }
    }, {
        'name': '--libbluray',
        'desc': 'Bluray support',
        'func': check_pkg_config('libbluray', '>= 0.2.1'),
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
        'name': '--mpg123',
        'desc': 'mpg123 support',
        'func': check_pkg_config('libmpg123', '>= 1.2.0'),
    }, {
        'name': '--ladspa',
        'desc': 'LADSPA plugin support',
        'func': check_statement('ladspa.h', 'LADSPA_Descriptor ld = {0}'),
    }, {
        'name': '--libbs2b',
        'desc': 'libbs2b audio filter support',
        'func': check_pkg_config('libbs2b'),
    }, {
        'name': '--lcms2',
        'desc': 'LCMS2 support',
        'func': check_pkg_config('lcms2'),
    }
]

libav_pkg_config_checks = [
    'libavutil',   '>= 52.3.0',
    'libavcodec',  '> 54.34.0',
    'libavformat', '> 54.19.0',
    'libswscale',  '>= 2.0.0'
]

libav_dependencies = [
    {
        'name': 'libav',
        'desc': 'libav/ffmpeg',
        'func': check_pkg_config(*libav_pkg_config_checks),
        'req': True,
        'fmsg': "Unable to find development files for some of the required \
Libav libraries ({0}). Aborting.".format(" ".join(libav_pkg_config_checks))
    }, {
        'name': '--libavresample',
        'desc': 'libavresample',
        'func': check_pkg_config('libavresample',  '>= 1.0.0'),
    }, {
        'name': 'avresample-set-channel-mapping',
        'desc': 'libavresample channel mapping API',
        'deps': [ 'libavresample' ],
        'func': check_statement('libavresample/avresample.h',
                                'avresample_set_channel_mapping(NULL, NULL)',
                                use='libavresample'),
    }, {
        'name': '--libswresample',
        'desc': 'libswresample',
        'func': check_pkg_config('libswresample', '>= 0.17.102'),
    }, {
        'name': 'resampler',
        'desc': 'usable resampler found',
        'deps_any': [ 'libavresample', 'libswresample' ],
        'func': check_true,
        'req':  True,
        'fmsg': 'No resampler found. Install libavresample or libswresample (FFmpeg).'
    }, {
        'name': 'avcodec-new-vdpau-api',
        'desc': 'libavcodec new vdpau API',
        'func': check_statement('libavutil/pixfmt.h',
                                'int x = AV_PIX_FMT_VDPAU',
                                use='libav'),
    }, {
        'name': 'avcodec-chroma-pos-api',
        'desc': 'libavcodec avcodec_enum_to_chroma_pos API',
        'func': check_statement('libavcodec/avcodec.h', """int x, y;
            avcodec_enum_to_chroma_pos(&x, &y, AVCHROMA_LOC_UNSPECIFIED)""",
            use='libav')
    }, {
        'name': 'avutil-qp-api',
        'desc': 'libavutil QP API',
        'func': check_statement('libavutil/frame.h',
                                'av_frame_get_qp_table(NULL, NULL, NULL)',
                                use='libav')
    }, {
        'name': 'avutil-refcounting',
        'desc': 'libavutil ref-counting API',
        'func': check_statement('libavutil/frame.h', 'av_frame_unref(NULL)',
                                use='libav'),
    } , {
        'name': 'av-opt-set-int-list',
        'desc': 'libavutil av_opt_set_int_list() API',
        'func': check_statement('libavutil/opt.h',
                                'av_opt_set_int_list(0,0,(int*)0,0,0)',
                                use='libav')
    }, {
        'name': '--libavfilter',
        'desc': 'libavfilter',
        'func': compose_checks(
            check_pkg_config('libavfilter'),
            check_cc(fragment=load_fragment('libavfilter.c'),
                     use='libavfilter')),
    }, {
        'name': '--vf-lavfi',
        'desc': 'using libavfilter through vf_lavfi',
        'deps': [ 'libavfilter', 'avutil-refcounting' ],
        'func': check_true
    }, {
        'name': '--af-lavfi',
        'desc': 'using libavfilter through af_lavfi',
        'deps': [ 'libavfilter', 'av-opt-set-int-list' ],
        'func': check_true
    }, {
        'name': '--libavdevice',
        'desc': 'libavdevice',
        'func': check_pkg_config('libavdevice', '>= 54.0.0'),
    }, {
        'name': '--libpostproc',
        'desc': 'libpostproc',
        'func': check_pkg_config('libpostproc', '>= 52.0.0'),
    }
]

audio_output_features = [
    {
        'name': '--sdl2',
        'desc': 'SDL2',
        'func': check_pkg_config('sdl2')
    }, {
        'name': '--sdl',
        'desc': 'SDL (1.x)',
        'deps_neg': [ 'sdl2' ],
        'func': check_pkg_config('sdl')
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
        'name': '--audio-select',
        'desc': 'audio select()',
        'deps': [ 'posix-select', 'oss-audio' ],
        'func': check_true,
    }, {
        'name': '--rsound',
        'desc': 'RSound audio output',
        'func': check_statement('rsound.h', 'rsd_init(NULL)', lib='rsound')
    }, {
        'name': '--sndio',
        'desc': 'sndio audio input/output',
        'func': check_statement('sndio.h',
            'struct sio_par par; sio_initpar(&par); const char *s = SIO_DEVANY', lib='sndio')
    }, {
        'name': '--pulse',
        'desc': 'PulseAudio audio output',
        'func': check_pkg_config('libpulse', '>= 0.9')
    }, {
        'name': '--portaudio',
        'desc': 'PortAudio audio output',
        'deps': [ 'pthreads' ],
        'func': check_pkg_config('portaudio-2.0', '>= 19'),
    }, {
        'name': '--jack',
        'desc': 'JACK audio output',
        'func': check_pkg_config('jack'),
    }, {
        'name': '--openal',
        'desc': 'OpenAL audio output',
        'func': check_pkg_config('openal', '>= 1.13'),
        'default': 'disable'
    }, {
        'name': '--alsa',
        'desc': 'ALSA audio output',
        'func': check_pkg_config('alsa'),
    }, {
        'name': '--coreaudio',
        'desc': 'CoreAudio audio output',
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
        'func': check_cc(fragment=load_fragment('wasapi.c'), lib='ole32'),
    }
]

video_output_features = [
    {
        'name': '--cocoa',
        'desc': 'Cocoa',
        'func': check_cc(
            fragment=load_fragment('cocoa.m'),
            compile_filename='test.m',
            framework_name=['Cocoa', 'IOKit', 'OpenGL'],
            linkflags='-fobjc-arc')
    } , {
        'name': 'gdi',
        'desc': 'GDI',
        'func': check_cc(lib='gdi32')
    } , {
        'name': '--wayland',
        'desc': 'Wayland',
        'func': check_pkg_config('wayland-client', '>= 1.2.0',
                                 'wayland-cursor', '>= 1.2.0',
                                 'xkbcommon',      '>= 0.3.0'),
    } , {
        'name': '--x11',
        'desc': 'X11',
        'func': check_pkg_config('x11'),
    } , {
        'name': '--xss',
        'desc': 'Xss screensaver extensions',
        'deps': [ 'x11' ],
        'func': check_statement('X11/extensions/scrnsaver.h',
            'XScreenSaverSuspend(NULL, True)', use='x11', lib='Xss'),
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
        'name': '--xf86vm',
        'desc': 'Xxf86vm',
        'deps': [ 'x11' ],
        'func': check_cc(fragment=load_fragment('xf86vm.c'),
                         lib='Xxf86vm', use='x11')
    } , {
        'name': '--xf86xk',
        'desc': 'XF86keysym',
        'deps': [ 'x11' ],
        'func': check_cc(fragment=load_fragment('xf86xk.c'))
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
        'name': '--gl-wayland',
        'desc': 'OpenGL Wayland Backend',
        'deps': [ 'wayland' ],
        'groups': [ 'gl' ],
        'func': check_pkg_config('wayland-egl', '>= 9.0.0',
                                 'egl',         '>= 9.0.0')
    } , {
        'name': '--gl-win32',
        'desc': 'OpenGL Win32 Backend',
        'deps': [ 'gdi' ],
        'groups': [ 'gl' ],
        'func': check_statement('windows.h', 'wglCreateContext(0)',
                                lib='opengl32')
    } , {
        'name': '--gl',
        'desc': 'OpenGL video outputs',
        'deps_any': [ 'gl-cocoa', 'gl-x11', 'gl-win32', 'gl-wayland' ],
        'func': check_true
    } , {
        'name': '--corevideo',
        'desc': 'CoreVideo',
        'deps': [ 'gl', 'gl-cocoa' ],
        'func': check_statement('QuartzCore/CoreVideo.h',
            'CVOpenGLTextureCacheCreate(0, 0, 0, 0, 0, 0)',
            framework_name=['QuartzCore'])
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
        'func': check_pkg_config('libva-glx', '>= 0.32.0'),
    }, {
        'name': '--caca',
        'desc': 'CACA',
        'func': check_pkg_config('caca', '>= 0.99.beta18'),
    }, {
        'name': '--dvb',
        'desc': 'DVB',
        'func': check_cc(fragment=load_fragment('dvb.c')),
    } , {
        'name': '--dvbin',
        'desc': 'DVB input module',
        'deps': [ 'dvb' ],
        'func': check_true,
    }, {
        'name': '--jpeg',
        'desc': 'JPEG support',
        'func': check_cc(header_name=['stdio.h', 'jpeglib.h'],
                         lib='jpeg', use='libm'),
    }, {
        'name': '--direct3d',
        'desc': 'Direct3D support',
        'deps': [ 'gdi' ],
        'func': check_cc(header_name='d3d9.h'),
    }
]

hwaccel_features = [
    {
        'name': '--vaapi-hwaccel',
        'desc': 'libavcodec VAAPI hwaccel',
        'deps': [ 'vaapi' ],
        'func': check_true,
    } , {
        'name': '--vda-hwaccel',
        'desc': 'libavcodec VDA hwaccel',
        'deps': [ 'corevideo', 'avutil-refcounting'],
        'func': compose_checks(
            check_headers('VideoDecodeAcceleration/VDADecoder.h'),
            check_statement('libavcodec/vda.h',
                            'ff_vda_create_decoder(NULL, NULL, NULL)',
                            framework='IOSurface',
                            use='libav')),
    } , {
        'name': 'vda-libavcodec-refcounting',
        'desc': "libavcodec VDA ref-counted CVPixelBuffers",
        'deps': [ 'vda-hwaccel' ],
        'func': check_statement ('libavcodec/vda.h',
            """struct vda_context a = (struct vda_context) {
                   .use_ref_buffer = 1 }""", use='libav')
    }, {
        'name': '--vda-gl',
        'desc': 'VDA with OpenGL',
        'deps': [ 'gl-cocoa', 'vda-hwaccel' ],
        'func': check_true
    }, {
        'name': '--vdpau-decoder',
        'desc': 'VDPAU decoder (old)',
        'deps': [ 'vdpau' ],
        'deps_neg': ['avcodec-new-vdpau-api'],
        'func': check_true,
    }, {
        'name': '--vdpau-hwaccel',
        'desc': 'libavcodec VDPAU hwaccel (new)',
        'deps': [ 'vdpau', 'avcodec-new-vdpau-api' ],
        'func': check_true,
    }
]

radio_and_tv_features = [
    {
        'name': '--radio',
        'desc': 'Radio interface',
        'func': check_true,
        'default': 'disable'
    }, {
        'name': '--radio-capture',
        'desc': 'Radio capture (through PCI/line-in)',
        'func': check_true,
        'deps': [ 'radio' ],
        'deps_any': [ 'alsa', 'oss-audio', 'sndio'],
    }, {
        'name': '--radio-v4l2',
        'desc': 'Video4Linux2 radio interface',
        'func': check_cc(header_name='linux/videodev2.h'),
        'default': 'disable'
    }, {
        'name': '--tv',
        'desc': 'TV interface',
        'func': check_true,
    }, {
        'name': '--tv-v4l2',
        'desc': 'Video4Linux2 TV interface',
        'func': check_cc(header_name=['sys/time.h', 'linux/videodev2.h'])
    }, {
        'name': '--libv4l2',
        'desc': 'libv4l2 support',
        'func': check_pkg_config('libv4l2'),
    }, {
        'name': '--pvr',
        'desc': 'Video4Linux2 MPEG PVR interface',
        'func': check_cc(fragment=load_fragment('pvr.c')),
    }, {
        'name': '--audio-input',
        'desc': 'audio input support',
        'deps_any': [ 'radio-capture', 'tv-v4l2' ],
        'func': check_true
    }
]

scripting_features = [
    {
        'name' : '--lua',
        'desc' : 'Lua',
        'func': check_lua,
    }
]

_INSTALL_DIRS_LIST = [
    ('bindir',  '${PREFIX}/bin',      'binary files'),
    ('libdir',  '${PREFIX}/lib',      'library files'),
    ('confdir', '${PREFIX}/etc/mpv',  'configuration files'),

    ('datadir', '${PREFIX}/share',    'data files'),
    ('mandir',  '${DATADIR}/man',     'man pages '),
    ('docdir',  '${DATADIR}/doc/mpv', 'documentation files'),
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

    opt.parse_features('build and install options', build_options)
    optional_features = main_dependencies + libav_dependencies
    opt.parse_features('optional feaures',  optional_features)
    opt.parse_features('audio outputs',     audio_output_features)
    opt.parse_features('video outputs',     video_output_features)
    opt.parse_features('hwaccels',          hwaccel_features)
    opt.parse_features('radio/tv features', radio_and_tv_features)
    opt.parse_features('scripting',         scripting_features)

    group = opt.get_option_group("scripting")
    group.add_option('--lua',
        type    = 'string',
        dest    = 'LUA_VER',
        help    = "select Lua package which should be autodetected. Choices: 51 51deb 52 52deb luajit")

@conf
def is_debug_build(ctx):
    return getattr(ctx.options, 'enable_debug-build')

def configure(ctx):
    ctx.check_waf_version(mini='1.7.13')
    target = os.environ.get('TARGET')
    (cc, pkg_config, windres) = ('cc', 'pkg-config', 'windres')

    if target:
        cc         = '-'.join([target, 'gcc'])
        pkg_config = '-'.join([target, pkg_config])
        windres    = '-'.join([target, windres])

    ctx.find_program(cc,          var='CC')
    ctx.find_program(pkg_config,  var='PKG_CONFIG')
    ctx.find_program('perl',      var='BIN_PERL')
    ctx.find_program('rst2man',   var='RST2MAN',   mandatory=False)
    ctx.find_program('rst2latex', var='RST2LATEX', mandatory=False)
    ctx.find_program('pdflatex',  var='PDFLATEX',  mandatory=False)
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
    ctx.load('detections.cpu')
    ctx.load('detections.devices')

    if ctx.env.DEST_OS in ('freebsd', 'openbsd'):
        ctx.env.CFLAGS += ['-I/usr/local/include']
        ctx.env.LINKFLAGS += ['-L/usr/local/lib']

    if ctx.env.DEST_OS == 'netbsd':
        ctx.env.CFLAGS += ['-I/usr/pkg/include']
        ctx.env.LINKFLAGS += ['-L/usr/pkg/lib']

    ctx.parse_dependencies(build_options)
    ctx.parse_dependencies(main_dependencies)
    ctx.parse_dependencies(audio_output_features)
    ctx.parse_dependencies(video_output_features)
    ctx.parse_dependencies(libav_dependencies)
    ctx.parse_dependencies(hwaccel_features)
    ctx.parse_dependencies(radio_and_tv_features)

    if ctx.options.LUA_VER:
        ctx.options.enable_lua = True

    ctx.parse_dependencies(scripting_features)

    ctx.define('HAVE_SYS_SOUNDCARD_H',
               '(HAVE_OSS_AUDIO_NATIVE || HAVE_OSS_AUDIO_4FRONT)',
               quote=False)

    ctx.define('HAVE_SOUNDCARD_H',
               'HAVE_OSS_AUDIO_SUNAUDIO',
               quote=False)

    ctx.load('generators.headers')

    if not ctx.dependency_satisfied('build-date'):
        ctx.env.CFLAGS += ['-DNO_BUILD_TIMESTAMPS']

    ctx.store_dependencies_lists()

def build(ctx):
    ctx.unpack_dependencies_lists()
    ctx.load('wscript_build')
