project('mpv',
        'c',
        license: ['GPL2+', 'LGPL2.1+'],
        version: files('./VERSION'),
        meson_version: '>=0.60.3',
        default_options: [
            'buildtype=debugoptimized',
            'b_lundef=false',
            'c_std=c11',
            'warning_level=1',
        ]
)

build_root = meson.project_build_root()
source_root = meson.project_source_root()
python = find_program('python3')

# ffmpeg
libavcodec = dependency('libavcodec', version: '>= 58.12.100')
libavfilter = dependency('libavfilter', version: '>= 7.14.100')
libavformat = dependency('libavformat', version: '>= 58.9.100')
libavutil = dependency('libavutil', version: '>= 56.12.100')
libswresample = dependency('libswresample', version: '>= 3.0.100')
libswscale = dependency('libswscale', version: '>= 5.0.101')

libass = dependency('libass', version: '>= 0.12.2')
pthreads = dependency('threads')

# the dependency order of libass -> ffmpeg is necessary due to
# static linking symbol resolution between fontconfig and MinGW
dependencies = [libass,
                libavcodec,
                libavfilter,
                libavformat,
                libavutil,
                libswresample,
                libswscale,
                pthreads]

# Keeps track of all enabled/disabled features
features = {
    'debug': get_option('debug'),
    'ffmpeg': true,
    'gpl': get_option('gpl'),
    'jpegxl': libavformat.version().version_compare('>= 59.27.100'),
    'libass': true,
    'optimize': get_option('optimization') != '0',
    'threads': true,
}


# generic sources
sources = files(
    ## Audio
    'audio/aframe.c',
    'audio/chmap.c',
    'audio/chmap_sel.c',
    'audio/decode/ad_lavc.c',
    'audio/decode/ad_spdif.c',
    'audio/filter/af_drop.c',
    'audio/filter/af_format.c',
    'audio/filter/af_lavcac3enc.c',
    'audio/filter/af_scaletempo.c',
    'audio/filter/af_scaletempo2.c',
    'audio/filter/af_scaletempo2_internals.c',
    'audio/fmt-conversion.c',
    'audio/format.c',
    'audio/out/ao.c',
    'audio/out/ao_lavc.c',
    'audio/out/ao_null.c',
    'audio/out/ao_pcm.c',
    'audio/out/buffer.c',

    ## Core
    'common/av_common.c',
    'common/av_log.c',
    'common/codecs.c',
    'common/common.c',
    'common/encode_lavc.c',
    'common/msg.c',
    'common/playlist.c',
    'common/recorder.c',
    'common/stats.c',
    'common/tags.c',
    'common/version.c',

    ## Demuxers
    'demux/codec_tags.c',
    'demux/cue.c',
    'demux/cache.c',
    'demux/demux.c',
    'demux/demux_cue.c',
    'demux/demux_disc.c',
    'demux/demux_edl.c',
    'demux/demux_lavf.c',
    'demux/demux_mf.c',
    'demux/demux_mkv.c',
    'demux/demux_mkv_timeline.c',
    'demux/demux_null.c',
    'demux/demux_playlist.c',
    'demux/demux_raw.c',
    'demux/demux_timeline.c',
    'demux/ebml.c',
    'demux/packet.c',
    'demux/timeline.c',

    ## Filters
    'filters/f_async_queue.c',
    'filters/f_autoconvert.c',
    'filters/f_auto_filters.c',
    'filters/f_decoder_wrapper.c',
    'filters/f_demux_in.c',
    'filters/f_hwtransfer.c',
    'filters/f_lavfi.c',
    'filters/f_output_chain.c',
    'filters/f_swresample.c',
    'filters/f_swscale.c',
    'filters/f_utils.c',
    'filters/filter.c',
    'filters/frame.c',
    'filters/user_filters.c',

    ## Input
    'input/cmd.c',
    'input/event.c',
    'input/input.c',
    'input/ipc.c',
    'input/keycodes.c',

    ## Misc
    'misc/bstr.c',
    'misc/charset_conv.c',
    'misc/dispatch.c',
    'misc/json.c',
    'misc/natural_sort.c',
    'misc/node.c',
    'misc/rendezvous.c',
    'misc/thread_pool.c',
    'misc/thread_tools.c',

    ## Options
    'options/m_config_core.c',
    'options/m_config_frontend.c',
    'options/m_option.c',
    'options/m_property.c',
    'options/options.c',
    'options/parse_commandline.c',
    'options/parse_configfile.c',
    'options/path.c',

    ## Player
    'player/audio.c',
    'player/client.c',
    'player/command.c',
    'player/configfiles.c',
    'player/external_files.c',
    'player/loadfile.c',
    'player/main.c',
    'player/misc.c',
    'player/osd.c',
    'player/playloop.c',
    'player/screenshot.c',
    'player/scripting.c',
    'player/sub.c',
    'player/video.c',

    ## Streams
    'stream/cookies.c',
    'stream/stream.c',
    'stream/stream_avdevice.c',
    'stream/stream_cb.c',
    'stream/stream_concat.c',
    'stream/stream_edl.c',
    'stream/stream_file.c',
    'stream/stream_lavf.c',
    'stream/stream_memory.c',
    'stream/stream_mf.c',
    'stream/stream_null.c',
    'stream/stream_slice.c',

    ## Subtitles
    'sub/ass_mp.c',
    'sub/dec_sub.c',
    'sub/draw_bmp.c',
    'sub/filter_sdh.c',
    'sub/img_convert.c',
    'sub/lavc_conv.c',
    'sub/osd.c',
    'sub/osd_libass.c',
    'sub/sd_ass.c',
    'sub/sd_lavc.c',

    ## Video
    'video/csputils.c',
    'video/decode/vd_lavc.c',
    'video/filter/refqueue.c',
    'video/filter/vf_format.c',
    'video/filter/vf_sub.c',
    'video/fmt-conversion.c',
    'video/hwdec.c',
    'video/image_loader.c',
    'video/image_writer.c',
    'video/img_format.c',
    'video/mp_image.c',
    'video/mp_image_pool.c',
    'video/out/aspect.c',
    'video/out/bitmap_packer.c',
    'video/out/dither.c',
    'video/out/dr_helper.c',
    'video/out/filter_kernels.c',
    'video/out/gpu/context.c',
    'video/out/gpu/error_diffusion.c',
    'video/out/gpu/hwdec.c',
    'video/out/gpu/lcms.c',
    'video/out/gpu/libmpv_gpu.c',
    'video/out/gpu/osd.c',
    'video/out/gpu/ra.c',
    'video/out/gpu/shader_cache.c',
    'video/out/gpu/spirv.c',
    'video/out/gpu/user_shaders.c',
    'video/out/gpu/utils.c',
    'video/out/gpu/video.c',
    'video/out/gpu/video_shaders.c',
    'video/out/libmpv_sw.c',
    'video/out/vo.c',
    'video/out/vo_gpu.c',
    'video/out/vo_image.c',
    'video/out/vo_lavc.c',
    'video/out/vo_libmpv.c',
    'video/out/vo_null.c',
    'video/out/vo_tct.c',
    'video/out/win_state.c',
    'video/repack.c',
    'video/sws_utils.c',

    ## osdep
    'osdep/io.c',
    'osdep/semaphore_osx.c',
    'osdep/subprocess.c',
    'osdep/threads.c',
    'osdep/timer.c',

    ## tree_allocator
    'ta/ta.c',
    'ta/ta_talloc.c',
    'ta/ta_utils.c'
)


# compiler stuff
cc = meson.get_compiler('c')

flags = ['-D_ISOC99_SOURCE', '-D_GNU_SOURCE',
         '-D_FILE_OFFSET_BITS=64']
link_flags = []

test_flags = ['-Werror=implicit-function-declaration',
              '-Wno-error=deprecated-declarations',
              '-Wno-error=unused-function',
              '-Wempty-body',
              '-Wdisabled-optimization',
              '-Wstrict-prototypes',
              '-Wno-format-zero-length',
              '-Wno-redundant-decls',
              '-Wvla',
              '-Wno-format-truncation',
              '-Wimplicit-fallthrough',
              '-fno-math-errno']

flags += cc.get_supported_arguments(test_flags)

if cc.has_multi_arguments('-Wformat', '-Werror=format-security')
    flags += '-Werror=format-security'
endif

if cc.get_id() == 'gcc'
    gcc_flags = ['-Wundef', '-Wmissing-prototypes', '-Wshadow',
                 '-Wno-switch', '-Wparentheses', '-Wpointer-arith',
                 '-Wno-pointer-sign',
                 # GCC bug 66425
                 '-Wno-unused-result']
    flags += gcc_flags
endif

if cc.get_id() == 'clang'
    clang_flags = ['-Wno-logical-op-parentheses', '-Wno-switch',
                   '-Wno-tautological-compare', '-Wno-pointer-sign',
                   '-Wno-tautological-constant-out-of-range-compare']
    flags += clang_flags
endif

darwin = host_machine.system() == 'darwin'
win32 = host_machine.system() == 'cygwin' or host_machine.system() == 'windows'
posix = not win32
features += {'posix': posix}
features += {'dos-paths': win32, 'win32': win32}

mswin_flags = ['-D_WIN32_WINNT=0x0602', '-DUNICODE', '-DCOBJMACROS',
               '-DINITGUID', '-U__STRICT_ANSI__']

if host_machine.system() == 'windows'
    flags += [mswin_flags, '-D__USE_MINGW_ANSI_STDIO=1']
endif

if host_machine.system() == 'cygwin'
    flags += [mswin_flags, '-mwin32']
endif

noexecstack = false
if cc.has_link_argument('-Wl,-z,noexecstack')
    link_flags += '-Wl,-z,noexecstack'
    noexecstack = true
endif

if cc.has_link_argument('-Wl,--nxcompat,--no-seh,--dynamicbase')
    link_flags += '-Wl,--nxcompat,--no-seh,--dynamicbase'
    noexecstack = true
endif

features += {'noexecstack': noexecstack}

if not get_option('build-date')
    flags += '-DNO_BUILD_TIMESTAMPS'
endif
features += {'build-date': get_option('build-date')}

features += {'ta-leak-report': get_option('ta-leak-report')}

libdl_dep = cc.find_library('libdl', required: false)
libdl = cc.has_function('dlopen', dependencies: libdl_dep, prefix: '#include <dlfcn.h>')
if libdl
    dependencies += libdl_dep
endif
features += {'libdl': libdl}

cplugins = get_option('cplugins').require(
    libdl and not win32 and cc.has_link_argument('-rdynamic'),
    error_message: 'cplugins not supported by the os or compiler!',
)
if cplugins.allowed()
    link_flags += '-rdynamic'
endif
features += {'cplugins': cplugins.allowed()}

if get_option('tests')
    sources += files('test/chmap.c',
                     'test/gl_video.c',
                     'test/img_format.c',
                     'test/json.c',
                     'test/linked_list.c',
                     'test/paths.c',
                     'test/scale_sws.c',
                     'test/scale_test.c',
                     'test/tests.c')
endif
features += {'tests': get_option('tests')}

# Note: this include is only used for windows pthreads and
# must be accompanied immediately by the following flags.
# This currently works because these are the last flags set
# in the build for windows. Adding any new flags after this
# will probably break something.
includedir = []
win32_pthreads = get_option('win32-internal-pthreads').require(
    win32 and not posix,
    error_message: 'the os is not win32!',
)
if win32_pthreads.allowed()
    flags += ['-isystem', '-I', '-DIN_WINPTHREAD']
    # Note: Adding this include causes POSIX_TIMERS to be defined for
    # unclear reasons (some confusion with <pthread.h> probably).
    # Hack around it by using HAVE_WIN32_INTERNAL_PTHREADS.
    includedir += include_directories('osdep/win32/include')
    sources += files('osdep/win32/pthread.c')
endif
features += {'win32-internal-pthreads': win32_pthreads.allowed()}

pthread_debug = get_option('pthread-debug').require(
    win32_pthreads.disabled(),
    error_message: 'win32-internal-pthreads was found!',
)
if pthread_debug.allowed()
    flags += '-DMP_PTHREAD_DEBUG'
endif
features += {'pthread-debug': pthread_debug.allowed()}

add_project_arguments(flags, language: 'c')
add_project_link_arguments(link_flags, language: ['c', 'objc'])


# osdep
cocoa = dependency('appleframeworks', modules: ['Cocoa', 'IOKit', 'QuartzCore'],
                   required: get_option('cocoa'))
if cocoa.found()
    dependencies += cocoa
    sources += files('osdep/macosx_application.m',
                     'osdep/macosx_events.m',
                     'osdep/macosx_menubar.m',
                     'osdep/main-fn-cocoa.c',
                     'osdep/path-macosx.m',
                     'video/out/cocoa_common.m',
                     'video/out/cocoa/events_view.m',
                     'video/out/cocoa/video_view.m',
                     'video/out/cocoa/window.m')
endif
features += {'cocoa': cocoa.found()}

if posix
    sources += files('input/ipc-unix.c',
                     'osdep/path-unix.c',
                     'osdep/polldev.c',
                     'osdep/subprocess-posix.c',
                     'osdep/terminal-unix.c',
                     'sub/filter_regex.c')
endif

if posix and not cocoa.found()
    sources += files('osdep/main-fn-unix.c')
endif

if darwin
    sources += files('osdep/timer-darwin.c')
endif

if posix and not darwin
    sources += files('osdep/timer-linux.c')
endif

cd_devices = {
    'windows': 'D:',
    'cygwin': 'D:',
    'darwin': '/dev/disk1',
    'freebsd': '/dev/cd0',
    'openbsd': '/dev/rcd0c',
    'linux': '/dev/sr0',
}
if host_machine.system() in cd_devices
    cd_device = cd_devices[host_machine.system()]
else
    cd_device = '/dev/cdrom'
endif

dvd_devices = {
    'windows': 'D:',
    'cygwin': 'D:',
    'darwin': '/dev/diskN',
    'freebsd': '/dev/cd0',
    'openbsd': '/dev/rcd0c',
    'linux': '/dev/sr0',
}
if host_machine.system() in cd_devices
    dvd_device = dvd_devices[host_machine.system()]
else
    dvd_device = '/dev/dvd'
endif

android = host_machine.system() == 'android'
if android
    dependencies += cc.find_library('android')
    sources += files('audio/out/ao_audiotrack.c',
                     'misc/jni.c',
                     'osdep/android/strnlen.c',
                     'video/out/android_common.c',
                     'video/out/vo_mediacodec_embed.c')
endif
features += {'android': android}

uwp_opt = get_option('uwp').require(
    not get_option('cplayer'),
    error_message: 'cplayer is not false!',
)
uwp = cc.find_library('windowsapp', required: uwp_opt)
if uwp.found()
    dependencies += uwp
    sources += files('osdep/path-uwp.c')
endif
features += {'uwp': uwp.found()}

if win32
    sources += files('osdep/timer-win2.c',
                     'osdep/w32_keyboard.c',
                     'osdep/windows_utils.c')
endif
features += {'win32-executable': win32 and get_option('cplayer')}

win32_desktop = win32 and not uwp.found()
if win32_desktop
    win32_desktop_libs = [cc.find_library('avrt'),
                          cc.find_library('dwmapi'),
                          cc.find_library('gdi32'),
                          cc.find_library('ole32'),
                          cc.find_library('uuid'),
                          cc.find_library('version'),
                          cc.find_library('winmm')]
    dependencies += win32_desktop_libs
    sources += files('input/ipc-win.c',
                     'osdep/main-fn-win.c',
                     'osdep/path-win.c',
                     'osdep/subprocess-win.c',
                     'osdep/terminal-win.c',
                     'video/out/w32_common.c',
                     'video/out/win32/displayconfig.c',
                     'video/out/win32/droptarget.c')
endif
features += {'win32-desktop': win32_desktop}

if not posix and not win32_desktop
    sources += files('input/ipc-dummy.c',
                     'osdep/subprocess-dummy.c',
                     'osdep/terminal-dummy.c')
endif

glob_posix = cc.has_function('glob', prefix: '#include <glob.h>')
features += {'glob-posix': glob_posix}

glob_win32 = win32 and not posix
if glob_win32
    sources += files('osdep/glob-win.c')
endif
features += {'glob-win32': glob_win32}

glob = glob_posix or glob_win32
features += {'glob': glob}

vt_h = cc.has_header_symbol('sys/vt.h', 'VT_GETMODE')
features += {'vt.h': vt_h}

consio_h = not vt_h and cc.has_header_symbol('sys/consio.h', 'VT_GETMODE')
features += {'consio.h': consio_h}

fragments = join_paths(source_root, 'waftools', 'fragments')

glibc_thread_name = cc.compiles(files(join_paths(fragments, 'glibc_thread_name.c')),
                                name: 'glibc-thread-name check')
features += {'glibc-thread-name': glibc_thread_name and posix}

osx_thread_name = false
if not glibc_thread_name
    osx_thread_name = cc.compiles(files(join_paths(fragments, 'osx_thread_name.c')),
                                  name: 'osx-thread-name check')
endif
features += {'osx-thread-name': osx_thread_name}

bsd_thread_name = false
if not osx_thread_name and not glibc_thread_name
    bsd_thread_name = cc.compiles(files(join_paths(fragments, 'bsd_thread_name.c')),
                                  name: 'bsd-thread-name check')
endif
features += {'bsd-thread-name': bsd_thread_name}

bsd_fstatfs = cc.has_function('fstatfs', prefix: '#include <sys/mount.h>\n#include <sys/param.h>')
features += {'bsd-fstatfs': bsd_fstatfs}

linux_fstatfs = cc.has_function('fstatfs', prefix: '#include <sys/vfs.h>')
features += {'linux-fstatfs': linux_fstatfs}

vector = get_option('vector').require(
    cc.compiles(files(join_paths(fragments, 'vector.c')), name: 'vector check'),
    error_message: 'the compiler does not support gcc vectors!',
)
features += {'vector': vector.allowed()}


# various file generations
tools_directory = join_paths(source_root, 'TOOLS')
file2string = find_program(join_paths(tools_directory, 'file2string.py'))
matroska = find_program(join_paths(tools_directory, 'matroska.py'))
version_py = find_program(join_paths(source_root, 'version.py'))

subdir('generated')
subdir(join_paths('generated', 'etc'))
subdir(join_paths('generated', 'sub'))

if darwin
    subdir(join_paths('generated', 'TOOLS', 'osxbundle', 'mpv.app', 'Contents', 'Resources'))
endif


# misc dependencies
av_ch_layout_available = libavutil.version().version_compare('>= 57.24.100')
if av_ch_layout_available
    sources += files('audio/chmap_avchannel.c')
endif
features += {'av-channel-layout': av_ch_layout_available}

cdda_opt = get_option('cdda').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
cdda = dependency('libcdio_paranoia', required: cdda_opt)
if cdda.found()
    dependencies += cdda
    sources += files('stream/stream_cdda.c')
endif
features += {'cdda': cdda.found()}

dvbin = get_option('dvbin').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
if dvbin.allowed()
    sources += files('stream/dvb_tune.c',
                     'stream/stream_dvb.c')
endif
features += {'dvbin': dvbin.allowed()}

dvdnav_opt = get_option('dvdnav').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
dvdnav = dependency('dvdnav', version: '>= 4.2.0', required: dvdnav_opt)
dvdread = dependency('dvdread', version: '>= 4.1.0', required: dvdnav_opt)
if dvdnav.found() and dvdread.found()
    dependencies += [dvdnav, dvdread]
    sources += files('stream/stream_dvdnav.c')
endif
features += {'dvdnav': dvdnav.found() and dvdread.found()}

iconv = dependency('iconv', required: get_option('iconv'))
if iconv.found()
    dependencies += iconv
endif
features += {'iconv': iconv.found()}

javascript = dependency('mujs', version: '>= 1.0.0', required: get_option('javascript'))
if javascript.found()
    dependencies += javascript
    sources += files('player/javascript.c',
                     'sub/filter_jsre.c')
    subdir(join_paths('generated', 'player', 'javascript'))
endif
features += {'javascript': javascript.found()}

lcms2 = dependency('lcms2', version: '>= 2.6', required: get_option('lcms2'))
if lcms2.found()
    dependencies += lcms2
endif
features += {'lcms2': lcms2.found()}

libarchive = dependency('libarchive', version: '>= 3.4.0', required: get_option('libarchive'))
if libarchive.found()
    dependencies += libarchive
    sources += files('demux/demux_libarchive.c',
                     'stream/stream_libarchive.c')
endif
features += {'libarchive': libarchive.found()}

libavdevice = dependency('libavdevice', version: '>= 57.0.0', required: get_option('libavdevice'))
if libavdevice.found()
    dependencies += libavdevice
endif
features += {'libavdevice': libavdevice.found()}

libbluray = dependency('libbluray', version: '>= 0.3.0', required: get_option('libbluray'))
if libbluray.found()
    dependencies += libbluray
    sources += files('stream/stream_bluray.c')
endif
features += {'libbluray': libbluray.found()}

libm = cc.find_library('m', required: false)
if libm.found()
    dependencies += libm
endif
features += {'libm': libm.found()}

librt = cc.find_library('rt', required: false)
if librt.found()
    dependencies += librt
endif
features += {'librt': librt.found()}

lua = dependency('', required: false)
lua_opt = get_option('lua')
if lua_opt != 'disabled'
    lua_version = [['lua', ['>=5.1.0', '<5.3.0']], # generic lua.pc
                   ['lua52', '>= 5.2.0'],
                   ['lua5.2', '>= 5.2.0'],
                   ['lua-5.2', '>= 5.2.0'],
                   ['luajit', '>= 2.0.0'],
                   ['lua51', '>= 5.1.0'],
                   ['lua5.1', '>= 5.1.0'],
                   ['lua-5.1', '>= 5.1.0']]
    foreach version : lua_version
        if lua_opt == 'auto' or lua_opt == 'enabled'
            lua = dependency(version[0], version: version[1], required: false)
            if lua.found()
                break
            endif
        elif lua_opt == version[0]
            lua = dependency(version[0], version: version[1])
            if lua.found()
                break
            endif
        endif
    endforeach
endif

if lua.found()
    dependencies += lua
    sources += files('player/lua.c')
    subdir(join_paths('generated', 'player', 'lua'))
endif
if not lua.found() and lua_opt == 'enabled'
     error('lua enabled but no suitable lua version could be found!')
endif
features += {'lua': lua.found()}
lua_version = lua.name()

rubberband = dependency('rubberband', version: '>= 1.8.0', required: get_option('rubberband'))
if rubberband.found()
    dependencies += rubberband
    sources += files('audio/filter/af_rubberband.c')
endif
features += {'rubberband': rubberband.found()}
features += {'rubberband-3': rubberband.version().version_compare('>= 3.0.0')}

sdl2 = dependency('sdl2', required: get_option('sdl2'))
if sdl2.found()
    dependencies += sdl2
endif
features += {'sdl2': sdl2.found()}

sdl2_gamepad = get_option('sdl2-gamepad').require(
    sdl2.found(),
    error_message: 'sdl2 was not found!',
)
if sdl2_gamepad.allowed()
    sources += files('input/sdl_gamepad.c')
endif
features += {'sdl2-gamepad': sdl2_gamepad.allowed()}

stdatomic = cc.find_library('atomic', required: get_option('stdatomic'))
if stdatomic.found()
    dependencies += stdatomic
endif
features += {'stdatomic': stdatomic.found()}

uchardet_opt = get_option('uchardet').require(
    iconv.found(),
    error_message: 'iconv was not found!',
)
uchardet = dependency('uchardet', required: uchardet_opt)
if uchardet.found()
    dependencies += uchardet
endif
features += {'uchardet': uchardet.found()}

vapoursynth = dependency('vapoursynth', version: '>= 24', required: get_option('vapoursynth'))
vapoursynth_script = dependency('vapoursynth-script', version: '>= 23',
                                required: get_option('vapoursynth'))
features += {'vapoursynth': vapoursynth.found() and vapoursynth_script.found()}
if features['vapoursynth']
    dependencies += [vapoursynth, vapoursynth_script]
    sources += files('video/filter/vf_vapoursynth.c')
endif

zimg = dependency('zimg', version: '>= 2.9', required: get_option('zimg'))
if zimg.found()
    dependencies += zimg
    sources += files('video/filter/vf_fingerprint.c',
                     'video/zimg.c')
    if get_option('tests')
        sources += files('test/repack.c',
                         'test/scale_zimg.c')
    endif
endif
features += {'zimg': zimg.found()}

zlib = dependency('zlib', required: get_option('zlib'))
if zlib.found()
    dependencies += zlib
endif
features += {'zlib': zlib.found()}


# audio output dependencies
alsa = dependency('alsa', version: '>= 1.0.18', required: get_option('alsa'))
if alsa.found()
    dependencies += alsa
    sources += files('audio/out/ao_alsa.c')
endif
features += {'alsa': alsa.found()}

audiounit = {
    'deps': dependency('appleframeworks', modules: ['Foundation', 'AudioToolbox'],
                       required: get_option('audiounit')),
    'symbol': cc.has_header_symbol('AudioToolbox/AudioToolbox.h', 'kAudioUnitSubType_RemoteIO',
                                   required: get_option('audiounit')),
}
if audiounit['deps'].found() and audiounit['symbol']
    dependencies += audiounit['deps']
    sources += files('audio/out/ao_audiounit.m')
endif
features += {'audiounit': audiounit['deps'].found() and audiounit['symbol']}

coreaudio = dependency('appleframeworks', modules: ['CoreFoundation', 'CoreAudio',
                       'AudioUnit', 'AudioToolbox'], required: get_option('coreaudio'))
if coreaudio.found()
    dependencies += coreaudio
    sources += files('audio/out/ao_coreaudio.c',
                     'audio/out/ao_coreaudio_exclusive.c',
                     'audio/out/ao_coreaudio_properties.c')
endif
features += {'coreaudio': coreaudio.found()}

if audiounit['deps'].found() and audiounit['symbol'] or coreaudio.found()
    sources += files('audio/out/ao_coreaudio_chmap.c',
                     'audio/out/ao_coreaudio_utils.c')
endif

jack_opt = get_option('jack').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
jack = dependency('jack', required: jack_opt)
if jack.found()
    dependencies += jack
    sources += files('audio/out/ao_jack.c')
endif
features += {'jack': jack.found()}

openal = dependency('openal', version: '>= 1.13', required: get_option('openal'))
if openal.found()
    dependencies += openal
    sources += files('audio/out/ao_openal.c')
endif
features += {'openal': openal.found()}

opensles = cc.find_library('OpenSLES', required: get_option('opensles'))
if opensles.found()
    dependencies += opensles
    sources += files('audio/out/ao_opensles.c')
endif
features += {'opensles': opensles.found()}

oss_opt = get_option('oss-audio').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
oss = cc.has_header_symbol('sys/soundcard.h', 'SNDCTL_DSP_SETPLAYVOL',
                           required: oss_opt)
if oss
    sources += files('audio/out/ao_oss.c')
endif
features += {'oss-audio': oss}

pipewire = dependency('libpipewire-0.3', version: '>= 0.3', required: get_option('pipewire'))
if pipewire.found()
    dependencies += pipewire
    sources += files('audio/out/ao_pipewire.c')
endif
features += {'pipewire': pipewire.found()}

pulse = dependency('libpulse', version: '>= 1.0', required: get_option('pulse'))
if pulse.found()
    dependencies += pulse
    sources += files('audio/out/ao_pulse.c')
endif
features += {'pulse': pulse.found()}

sdl2_audio = get_option('sdl2-audio').require(
    sdl2.found(),
    error_message: 'sdl2 was not found!',
)
if sdl2_audio.allowed()
    sources += files('audio/out/ao_sdl.c')
endif
features += {'sdl2-audio': sdl2_audio.allowed()}

sndio = dependency('sndio', required: get_option('sndio'))
if sndio.found()
    dependencies += sndio
    sources += files('audio/out/ao_sndio.c')
endif
features += {'sndio': sndio.found()}

wasapi = cc.has_header_symbol('audioclient.h', 'IAudioClient', required: get_option('wasapi'))
if wasapi
    sources += files('audio/out/ao_wasapi.c',
                     'audio/out/ao_wasapi_changenotify.c',
                     'audio/out/ao_wasapi_utils.c')
endif
features += {'wasapi': wasapi}


# video output dependencies
caca_opt = get_option('caca').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
caca = dependency('caca', version: '>= 0.99.beta18', required: caca_opt)
if caca.found()
    dependencies += caca
    sources += files('video/out/vo_caca.c')
endif
features += {'caca': caca.found()}

direct3d_opt = get_option('direct3d').require(
    get_option('gpl') and win32_desktop,
    error_message: 'the build is not GPL or this is not a win32 desktop!',
)
direct3d = cc.check_header('d3d9.h', required: direct3d_opt)
if direct3d
    sources += files('video/out/vo_direct3d.c')
endif
features += {'direct3d': direct3d}

drm = {
    'deps': dependency('libdrm', version: '>= 2.4.75', required: get_option('drm')),
    'header': vt_h or consio_h,
}
features += {'drm': drm['deps'].found() and drm['header']}
if features['drm']
    dependencies += drm['deps']
    sources += files('video/drmprime.c',
                     'video/out/drm_atomic.c',
                     'video/out/drm_common.c',
                     'video/out/drm_prime.c',
                     'video/out/opengl/hwdec_drmprime_drm.c',
                     'video/out/hwdec/hwdec_drmprime.c',
                     'video/out/vo_drm.c')
endif

# This can be removed roughly when Debian 12 is released.
drm_is_kms = features['drm'] and drm['deps'].version().version_compare('>= 2.4.105')
features += {'drm-is-kms': drm_is_kms}

gbm = dependency('gbm', version: '>=17.1.0', required: get_option('gbm'))
if gbm.found()
    dependencies += gbm
endif
features += {'gbm': gbm.found()}

jpeg = dependency('libjpeg', required: get_option('jpeg'))
if jpeg.found()
    dependencies += jpeg
endif
features += {'jpeg': jpeg.found()}

libplacebo_next = false
libplacebo = dependency('libplacebo', version: '>=4.157.0', required: get_option('libplacebo'))
if libplacebo.found()
    dependencies += libplacebo
    sources += files('video/out/placebo/ra_pl.c',
                     'video/out/placebo/utils.c')
    pl_api_ver = libplacebo.version().split('.')[1]
    if pl_api_ver.version_compare('>=202')
        libplacebo_next = true
        message('libplacebo v4.202+ found! Enabling vo_gpu_next.')
        sources += files('video/out/vo_gpu_next.c',
                         'video/out/gpu_next/context.c')
    else
        message('libplacebo v4.202+ not found! Disabling vo_gpu_next.')
    endif
endif
features += {'libplacebo': libplacebo.found()}
features += {'libplacebo-next': libplacebo_next}

sdl2_video = get_option('sdl2-video').require(
    sdl2.found(),
    error_message: 'sdl2 was not found!',
)
if sdl2_video.allowed()
    sources += files('video/out/vo_sdl.c')
endif
features += {'sdl2-video': sdl2_video.allowed()}

shaderc = dependency('shaderc', required: get_option('shaderc'))
if shaderc.found()
    dependencies += shaderc
    sources += files('video/out/gpu/spirv_shaderc.c')
endif
features += {'shaderc': shaderc.found()}

sixel = dependency('libsixel', version: '>= 1.5', required: get_option('sixel'))
if sixel.found()
    dependencies += sixel
    sources += files('video/out/vo_sixel.c')
endif
features += {'sixel': sixel.found()}

spirv_cross = dependency('spirv-cross-c-shared', required: get_option('spirv-cross'))
if spirv_cross.found()
    dependencies += spirv_cross
endif
features += {'spirv-cross': spirv_cross.found()}

d3d11 = get_option('d3d11').require(
    win32_desktop and shaderc.found() and spirv_cross.found(),
    error_message: 'Either is not a win32 desktop or shaderc nor spirv-cross were found!',
)
if d3d11.allowed()
    sources += files('video/out/d3d11/context.c',
                     'video/out/d3d11/ra_d3d11.c')
endif
features += {'d3d11': d3d11.allowed()}

wayland = {
    'deps': [dependency('wayland-client', version: '>= 1.15.0', required: get_option('wayland')),
             dependency('wayland-cursor', version: '>= 1.15.0', required: get_option('wayland')),
             dependency('wayland-protocols', version: '>= 1.15', required: get_option('wayland')),
             dependency('xkbcommon', version: '>= 0.3.0', required: get_option('wayland'))],
    'header': cc.check_header('linux/input-event-codes.h', required: get_option('wayland')),
    'scanner': find_program('wayland-scanner', required: get_option('wayland')),
}
wayland_deps = true
foreach dep: wayland['deps']
    if not dep.found()
        wayland_deps = false
        break
    endif
endforeach
features += {'wayland': wayland_deps and wayland['header'] and wayland['scanner'].found()}

if features['wayland']
    subdir(join_paths('generated', 'wayland'))
endif

memfd_create = false
if features['wayland']
    memfd_create = cc.has_function('memfd_create',
                                   prefix: '#define _GNU_SOURCE\n#include <sys/mman.h>')
endif
if features['wayland'] and memfd_create
    sources += files('video/out/vo_wlshm.c')
endif
features += {'memfd_create': features['wayland'] and memfd_create}

x11_opt = get_option('x11').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
x11 = {
    'deps': [dependency('x11', version: '>= 1.0.0', required: x11_opt),
             dependency('xscrnsaver', version: '>= 1.0.0', required: x11_opt),
             dependency('xext', version: '>= 1.0.0', required: x11_opt),
             dependency('xinerama', version: '>= 1.0.0', required: x11_opt),
             dependency('xpresent', version: '>= 1.0.0', required: x11_opt),
             dependency('xrandr', version: '>= 1.2.0', required: x11_opt)],
}
x11_deps = true
foreach dep: x11['deps']
    if not dep.found()
        x11_deps = false
        break
    endif
endforeach
features += {'x11': x11_deps}

if features['x11']
    dependencies += x11['deps']
    sources += files('video/out/vo_x11.c',
                     'video/out/x11_common.c')
endif

xv_opt = get_option('xv').require(
    features['x11'],
    error_message: 'x11 could not be found!',
)
xv = dependency('xv', required: xv_opt)
if xv.found()
    dependencies += xv
    sources += files('video/out/vo_xv.c')
endif
features += {'xv': xv.found()}

if features['wayland'] or features['x11']
    sources += ('video/out/present_sync.c')
endif


# OpenGL feature checking
gl_allowed = get_option('gl').allowed()

GL = dependency('', required: false)
if darwin
    GL = dependency('appleframeworks', modules: 'OpenGL', required: get_option('gl-cocoa'))
elif win32_desktop
    GL = dependency('GL', required: get_option('gl-win32'))
elif features['x11']
    GL = dependency('GL', required: get_option('gl-x11'))
endif

gl_cocoa = get_option('gl-cocoa').require(
    cocoa.found() and GL.found() and gl_allowed,
    error_message: 'cocoa and GL were not found!',
)
if gl_cocoa.allowed()
    dependencies += GL
    features += {'gl': true}
    sources += files('video/out/opengl/context_cocoa.c')
endif
features += {'gl-cocoa': gl_cocoa.allowed()}

gl_win32 = get_option('gl-win32').require(
    GL.found() and gl_allowed and win32_desktop,
    error_message: 'GL and win32 desktop were not found!',
)
if gl_win32.allowed()
    dependencies += GL
    features += {'gl': true}
    sources += files('video/out/opengl/context_win.c')
endif
features += {'gl-win32': gl_win32.allowed()}

gl_x11 = get_option('gl-x11').require(
    GL.found() and gl_allowed and features['x11'],
    error_message: 'GL and x11 were not found!',
)
if gl_x11.allowed()
    dependencies += GL
    features += {'gl': true}
    sources += files('video/out/opengl/context_glx.c')
endif
features += {'gl-x11': gl_x11.allowed()}

gl_dxinterop_d3d = gl_win32.allowed() and \
                   cc.has_header_symbol('GL/wglext.h', 'WGL_ACCESS_READ_ONLY_NV',
                                        prefix: '#include <GL/gl.h>')
gl_dxinterop_gl = gl_win32.allowed() and cc.has_header_symbol('d3d9.h', 'IDirect3D9Ex')
gl_dxinterop = get_option('gl-dxinterop').require(
    gl_dxinterop_d3d and gl_dxinterop_gl and gl_win32.allowed(),
    error_message: 'gl-dxinterop could not be found!',
)
if gl_dxinterop.allowed()
    sources += files('video/out/opengl/context_dxinterop.c')
endif
features += {'gl-dxinterop': gl_dxinterop.allowed()}

egl_angle = get_option('egl-angle').require(
    gl_win32.allowed() and cc.has_header_symbol('EGL/eglext.h',
                                                'EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE',
                                                prefix: '#include <EGL/egl.h>'),
    error_message: 'egl-angle could not be found!',
)
if egl_angle.allowed()
    sources += files('video/out/opengl/angle_dynamic.c')
endif
features += {'egl-angle': egl_angle.allowed()}

egl_angle_lib = get_option('egl-angle-lib').require(
    egl_angle.allowed() and cc.has_function('eglCreateWindowSurface',
                                            prefix: '#include <EGL/egl.h>'),
    error_message: 'egl-angle-lib could not be found!',
)
features += {'egl-angle-lib': egl_angle_lib.allowed()}

egl_angle_win32 = get_option('egl-angle-win32').require(
    egl_angle.allowed() and win32_desktop,
    error_message: 'either this is not a win32 desktop or egl-angle was not found!',
)
if egl_angle_win32.allowed()
    sources += files('video/out/opengl/context_angle.c')
endif
features += {'egl-angle-win32': egl_angle_win32.allowed()}

if d3d11.allowed() or egl_angle_win32.allowed()
    sources += files('video/out/gpu/d3d11_helpers.c')
endif

egl = dependency('egl', version: '> 1.4.0', required: get_option('egl'))
features += {'egl': egl.found() and gl_allowed}
if features['egl']
    dependencies += egl
endif

egl_android_opt = get_option('egl-android').require(
    android and gl_allowed,
    error_message: 'the OS is not android!',
)
egl_android = cc.find_library('EGL', required: egl_android_opt)
if egl_android.found()
    dependencies += egl_android
    features += {'gl': true}
    sources += files('video/out/opengl/context_android.c')
endif
features += {'egl-android': egl_android.found()}

egl_drm = get_option('egl-drm').require(
    features['drm'] and features['egl'] and gbm.found() and gl_allowed,
    error_message: 'either drm, egl, or gbm could not be found!',
)
if egl_drm.allowed()
    features += {'gl': true}
    sources += files('video/out/opengl/context_drm_egl.c')
endif
features += {'egl-drm': egl_drm.allowed()}

egl_wayland = dependency('wayland-egl', version: '>= 9.0.0', required: get_option('egl-wayland'))
features += {'gl-wayland': features['egl'] and egl_wayland.found() and gl_allowed and features['wayland']}
if features['gl-wayland']
    dependencies += egl_wayland
    features += {'gl': true}
    sources += files('video/out/opengl/context_wayland.c')
endif

egl_x11 = get_option('egl-x11').require(
    features['egl'] and gl_allowed and features['x11'],
    error_message: 'either egl or x11 could not be found!',
)
if egl_x11.allowed()
    features += {'gl': true}
    sources += files('video/out/opengl/context_x11egl.c')
endif
features += {'egl-x11': egl_x11.allowed()}

plain_gl = get_option('plain-gl').require(
    get_option('libmpv') and gl_allowed,
    error_message: 'libmpv was not enabled!',
)
if plain_gl.allowed()
    features += {'gl': true}
endif
features += {'plain-gl': plain_gl.allowed()}

rpi = dependency('/opt/vc/lib/pkgconfig/brcmegl.pc', 'brcmegl', required: get_option('rpi'))
features += {'rpi': gl_allowed and rpi.found()}
if features['rpi']
    dependencies += rpi
    features += {'gl': true}
    sources += files('video/out/opengl/context_rpi.c')
endif

egl_helpers = features['egl'] or egl_android.found() or egl_angle_win32.allowed() or features['rpi']
if egl_helpers
    sources += files('video/out/opengl/egl_helpers.c')
endif
features += {'egl-helpers': egl_helpers}

if features['egl'] and egl_helpers
    sources += files('video/filter/vf_gpu.c')
endif

if features['gl']
    sources += files('video/out/opengl/common.c',
                     'video/out/opengl/context.c',
                     'video/out/opengl/formats.c',
                     'video/out/opengl/libmpv_gl.c',
                     'video/out/opengl/ra_gl.c',
                     'video/out/opengl/utils.c')
elif not features['gl'] and get_option('gl').enabled()
    error('gl enabled but no OpenGL video output could be found!')
endif


# vulkan
vulkan_opt = get_option('vulkan').require(
    libplacebo.found(),
    error_message: 'libplacebo could not be found!',
)
vulkan = dependency('vulkan', required: vulkan_opt)
if vulkan.found()
    dependencies += vulkan
    sources += files('video/out/vulkan/context.c',
                     'video/out/vulkan/context_display.c',
                     'video/out/vulkan/utils.c')
endif
features += {'vulkan': vulkan.found()}

if vulkan.found() and android
    sources += files('video/out/vulkan/context_android.c')
endif

if vulkan.found() and features['wayland']
    sources += files('video/out/vulkan/context_wayland.c')
endif

if vulkan.found() and win32_desktop
    sources += files('video/out/vulkan/context_win.c')
endif

if vulkan.found() and features['x11']
     sources += files('video/out/vulkan/context_xlib.c')
endif


# hwaccel
ffnvcodec = dependency('ffnvcodec', version: '>= 8.2.15.7', required: false)
if ffnvcodec.found()
    dependencies += ffnvcodec
    sources += files('video/cuda.c')
endif
features += {'ffnvcodec': ffnvcodec.found()}

cuda_hwaccel = get_option('cuda-hwaccel').require(
    ffnvcodec.found(),
    error_message: 'ffnvcodec was not found!',
)
if cuda_hwaccel.allowed()
    sources += files('video/out/hwdec/hwdec_cuda.c')
endif
features += {'cuda-hwaccel': cuda_hwaccel.allowed()}

cuda_interop = get_option('cuda-interop').require(
    cuda_hwaccel.allowed() and (features['gl'] or vulkan.found()),
    error_message: 'cuda-hwaccel and either gl or vulkan were not found!',
)
if cuda_interop.allowed() and features['gl']
    sources += files('video/out/hwdec/hwdec_cuda_gl.c')
endif
if cuda_interop.allowed() and vulkan.found()
    sources += files('video/out/hwdec/hwdec_cuda_vk.c')
endif
features += {'cuda-interop': cuda_interop.allowed() and (features['gl'] or vulkan.found())}

d3d_hwaccel = get_option('d3d-hwaccel').require(
    win32,
    error_message: 'the os is not win32!',
)
if d3d_hwaccel.allowed()
    sources += files('video/d3d.c',
                     'video/filter/vf_d3d11vpp.c')
endif
features += {'d3d-hwaccel': d3d_hwaccel.allowed()}

if d3d_hwaccel.allowed() and egl_angle.allowed()
    sources += files('video/out/opengl/hwdec_d3d11egl.c')
endif
if d3d_hwaccel.allowed() and d3d11.allowed()
    sources += files('video/out/d3d11/hwdec_d3d11va.c')
endif

d3d9_hwaccel = get_option('d3d9-hwaccel').require(
    d3d_hwaccel.allowed(),
    error_message: 'd3d-hwaccel was not found!',
)
if d3d9_hwaccel.allowed() and egl_angle.allowed()
    sources += files('video/out/opengl/hwdec_dxva2egl.c')
endif
if d3d9_hwaccel.allowed() and d3d11.allowed()
    sources += files('video/out/d3d11/hwdec_dxva2dxgi.c')
endif
features += {'d3d9-hwaccel': d3d9_hwaccel.allowed()}

gl_dxinterop_d3d9 = get_option('gl-dxinterop-d3d9').require(
    gl_dxinterop.allowed() and d3d9_hwaccel.allowed(),
    error_message: 'gl-dxinterop and d3d9-hwaccel were not found!',
)
if gl_dxinterop_d3d9.allowed()
    sources += files('video/out/opengl/hwdec_dxva2gldx.c')
endif
features += {'gl-dxinterop-d3d9': gl_dxinterop_d3d9.allowed()}

ios_gl = cc.has_header_symbol('OpenGLES/ES3/glext.h', 'GL_RGB32F', required: get_option('ios-gl'))
if ios_gl
    sources += files('video/out/opengl/hwdec_ios.m')
endif
features += {'ios-gl': ios_gl}

rpi_mmal_opt = get_option('rpi-mmal').require(
    features['rpi'],
    error_message: 'rpi was not found!',
)
rpi_mmal = dependency('/opt/vc/lib/pkgconfig/mmal.pc', 'mmal', required: rpi_mmal_opt)
if rpi_mmal.found()
    dependencies += rpi_mmal
    sources += files('video/out/opengl/hwdec_rpi.c',
                     'video/out/vo_rpi.c')
endif
features += {'rpi-mmal': rpi_mmal.found()}

vaapi = dependency('libva', version: '>= 1.1.0', required: get_option('vaapi'))
features += {'vaapi': vaapi.found() and libdl and (features['x11'] or
             features['wayland'] or egl_drm.allowed())}

if features['vaapi']
    dependencies += vaapi
    sources += files('video/filter/vf_vavpp.c',
                     'video/vaapi.c')
endif

vaapi_drm = dependency('libva-drm', version: '>= 1.1.0', required: get_option('vaapi-drm'))
features += {'vaapi-drm': features['vaapi'] and egl_drm.allowed() and vaapi_drm.found()}
if features['vaapi-drm']
    dependencies += vaapi_drm
endif

vaapi_wayland = dependency('libva-wayland', version: '>= 1.1.0', required: get_option('vaapi-wayland'))
features += {'vaapi-wayland': features['vaapi'] and features['gl-wayland'] and vaapi_wayland.found()}
if features['vaapi-wayland']
    dependencies += vaapi_wayland
endif

features += {'vaapi-wayland-memfd': features['vaapi-wayland'] and memfd_create}
if features['vaapi-wayland-memfd']
    sources += files('video/out/vo_vaapi_wayland.c')
endif

vaapi_x11 = dependency('libva-x11', version: '>= 1.1.0', required: get_option('vaapi-x11'))
features += {'vaapi-x11': features['vaapi'] and features['x11'] and vaapi_x11.found()}
if features['vaapi-x11']
    dependencies += vaapi_x11
    sources += files('video/out/vo_vaapi.c')
endif

features += {'vaapi-x-egl': features['vaapi-x11'] and egl_x11.allowed()}
features += {'vaapi-egl': features['vaapi-x11'] or features['vaapi-wayland'] or features['drm']}
features += {'vaapi-libplacebo': features['vaapi'] and libplacebo.found()}

if features['vaapi-egl'] or features['vaapi-libplacebo']
    sources += files('video/out/hwdec/hwdec_vaapi.c')
endif

dmabuf_interop_gl = features['egl'] and features['drm']
if dmabuf_interop_gl
    sources += files('video/out/hwdec/dmabuf_interop_gl.c')
endif
features += {'dmabuf-interop-gl': dmabuf_interop_gl}

dmabuf_interop_pl = features['vaapi-libplacebo']
if dmabuf_interop_pl
    sources += files('video/out/hwdec/dmabuf_interop_pl.c')
endif
features += {'dmabuf-interop-pl': dmabuf_interop_pl}

vdpau_opt = get_option('vdpau').require(
    features['x11'],
    error_message: 'x11 was not found!',
)
vdpau = dependency('vdpau', version: '>= 0.2', required: vdpau_opt)
if vdpau.found()
    dependencies += vdpau
    sources += files('video/filter/vf_vdpaupp.c',
                     'video/out/vo_vdpau.c',
                     'video/vdpau.c',
                     'video/vdpau_mixer.c')
endif
if vdpau.found() and gl_x11.allowed()
    features += 'vdpau-gl-x11'
    sources += files('video/out/opengl/hwdec_vdpau.c')
endif
features += {'vdpau': vdpau.found()}
features += {'vdpau-gl-x11': vdpau.found() and gl_x11.allowed()}

videotoolbox_gl = get_option('videotoolbox-gl').require(
    gl_cocoa.allowed() or ios_gl,
    error_message: 'gl-cocoa nor ios-gl could be found!',
)
if videotoolbox_gl.allowed()
    sources += files('video/out/opengl/hwdec_osx.c')
endif
features += {'videotoolbox-gl': videotoolbox_gl.allowed()}


# macOS features
macos_sdk_version_py = find_program(join_paths(source_root, 'TOOLS',
                                    'macos-sdk-version.py'))
macos_sdk_info = ['', '0.0']
if darwin
    macos_sdk_info = run_command(macos_sdk_version_py, check: true).stdout().split(',')
endif

macos_sdk_path = macos_sdk_info[0].strip()
macos_sdk_version = macos_sdk_info[1]
if macos_sdk_path != ''
    message('Detected macOS sdk path: ' + macos_sdk_path)
endif

if macos_sdk_version != '0.0'
    message('Detected macOS SDK: ' + macos_sdk_version)
    add_languages('objc')
    objc_link_flags = ['-isysroot', macos_sdk_path, '-L/usr/lib', '-L/usr/local/lib']
    add_project_link_arguments(objc_link_flags, language: ['c', 'objc'])
endif

macos_10_11_features = get_option('macos-10-11-features').require(
    macos_sdk_version.version_compare('>=10.11'),
    error_message: 'a suitable macos sdk version could not be found!',
)

macos_10_12_2_features = get_option('macos-10-12-2-features').require(
    macos_sdk_version.version_compare('>=10.12.2'),
    error_message: 'a suitable macos sdk version could not be found!',
)

macos_10_14_features = get_option('macos-10-14-features').require(
    macos_sdk_version.version_compare('>=10.14'),
    error_message: 'a suitable macos sdk version could not be found!',
)

xcrun = find_program('xcrun', required: get_option('swift-build'))
swift_ver = '0.0'
if xcrun.found()
    swift_prog = find_program(run_command(xcrun, '-find', 'swift', check: true).stdout().strip())
    swift_ver_string = run_command(swift_prog, '-version', check: true).stdout()
    verRe = '''
#!/usr/bin/env python3
import re
import sys
verRe = re.compile("(?i)version\s?([\d.]+)")
swift_ver = verRe.search(sys.argv[1]).group(1)
sys.stdout.write(swift_ver)
'''
    swift_ver = run_command(python, '-c', verRe, swift_ver_string, check: true).stdout()
    message('Detected Swift version: ' + swift_ver)
endif

swift = get_option('swift-build').require(
    darwin and macos_sdk_version.version_compare('>=10.10') and swift_ver.version_compare('>=4.1'),
    error_message: 'A suitable macos sdk version or swift version could not be found!',
)

swift_sources = []
if cocoa.found() and swift.allowed()
    swift_sources += files('osdep/macos/libmpv_helper.swift',
                           'osdep/macos/log_helper.swift',
                           'osdep/macos/mpv_helper.swift',
                           'osdep/macos/swift_compat.swift',
                           'osdep/macos/swift_extensions.swift',
                           'video/out/mac/common.swift',
                           'video/out/mac/title_bar.swift',
                           'video/out/mac/view.swift',
                           'video/out/mac/window.swift')
endif

macos_cocoa_cb = get_option('macos-cocoa-cb').require(
    cocoa.found() and swift.allowed(),
    error_message: 'Either cocoa or swift could not be found!',
)
if macos_cocoa_cb.allowed()
    swift_sources += files('video/out/cocoa_cb_common.swift',
                           'video/out/mac/gl_layer.swift')
endif
features += {'macos-cocoa-cb': macos_cocoa_cb.allowed()}

macos_media_player = get_option('macos-media-player').require(
    macos_10_12_2_features.allowed() and swift.allowed(),
    error_message: 'Either the macos sdk version is not at least 10.12.2 or swift was not found!',
)
if macos_media_player.allowed()
    swift_sources += files('osdep/macos/remote_command_center.swift')
endif
features += {'macos-media-player': macos_media_player.allowed()}

if swift.allowed()
    subdir(join_paths('generated', 'osdep'))
endif

macos_touchbar = {
    'name': 'macos-touchbar',
    'deps': dependency('appleframeworks', modules: 'AppKit',
                       required: get_option('macos-touchbar')),
    'fragment': files(join_paths(fragments, 'touchbar.m')),
}
features += {'macos-touchbar': cc.compiles(macos_touchbar['fragment'], name: 'macos-touchbar check')}
if features['macos-touchbar']
    sources += files('osdep/macosx_touchbar.m')
elif get_option('macos-touchbar').enabled()
    error('macos-touchbar enabled but it could not be found!')
endif


# manpages
manpage = 'DOCS/man/mpv.rst'
rst2man = find_program('rst2man', 'rst2man.py', required: get_option('manpage-build'))
if rst2man.found()
    mandir = get_option('mandir')
    custom_target('manpages',
        input: manpage,
        output: 'mpv.1',
        command: [rst2man, '--strip-elements-with-class=contents', '@INPUT@', '@OUTPUT@'],
        install: true,
        install_dir: join_paths(mandir, 'man1')
    )
endif
features += {'manpage-build': rst2man.found()}

rst2html = find_program('rst2html', required: get_option('html-build'))
if rst2html.found()
    datadir = get_option('datadir')
    custom_target('html-manpages',
        input: manpage,
        output: 'mpv.html',
        command: [rst2html, '@INPUT@', '@OUTPUT@'],
        install: true,
        install_dir: join_paths(datadir, 'doc', 'mpv')
    )
endif
features += {'html-build': rst2html.found()}

rst2pdf = find_program('rst2pdf', required: get_option('pdf-build'))
if rst2pdf.found()
    datadir = get_option('datadir')
    custom_target('pdf-manpages',
        input: manpage,
        output: 'mpv.pdf',
        command: [rst2pdf, '-c', '-b', '1', '--repeat-table-rows', '@INPUT@', '-o', '@OUTPUT@'],
        install: true,
        install_dir: join_paths(datadir, 'doc', 'mpv')
    )
endif
features += {'pdf-build': rst2pdf.found()}


# We can't easily get every single thing a user might have passed on the cli,
# but we might as well add prefix (even if it's not specifically set) since
# it's highly relevant and useful.
configuration = 'meson build '
configuration += '-Dprefix=' + get_option('prefix')

features += {'cplayer': get_option('cplayer')}
features += {'libmpv-' + get_option('default_library'): get_option('libmpv')}


# Set config.h
conf_data = configuration_data()
conf_data.set_quoted('CONFIGURATION', configuration)
conf_data.set_quoted('DEFAULT_DVD_DEVICE', dvd_device)
conf_data.set_quoted('DEFAULT_CDROM_DEVICE', cd_device)

# Loop over all features in the build, create a define and add them to config.h
feature_keys = []
foreach feature, allowed: features
    define = 'HAVE_@0@'.format(feature.underscorify().to_upper())
    conf_data.set10(define, allowed)
    # special handling for lua
    if feature == 'lua' and allowed
        feature_keys += lua_version
        continue
    endif
    if allowed
        feature_keys += feature
    endif
endforeach


# Script to sort the feature_keys object.
feature_sort = '''
#!/usr/bin/env python3
import sys
features = " ".join(sorted(sys.argv[1:]))
sys.stdout.write(features)
'''
feature_str = run_command(python, '-c', feature_sort, feature_keys, check: true).stdout()
conf_data.set_quoted('FULLCONFIG', feature_str)
conf_data.set_quoted('MPV_CONFDIR', join_paths(get_option('sysconfdir'), 'mpv'))
configure_file(output : 'config.h', configuration : conf_data)
message('List of enabled features: ' + feature_str)


# build targets
if win32
    windows = import('windows')
    res_flags = ['--codepage=65001']

    # Unintuitively, this compile operates out of the osdep subdirectory.
    # Hence, these includes are needed.
    res_includes = [source_root, build_root]

    resources = ['etc/mpv-icon-8bit-16x16.png',
                 'etc/mpv-icon-8bit-32x32.png',
                 'etc/mpv-icon-8bit-64x64.png',
                 'etc/mpv-icon-8bit-128x128.png',
                 'etc/mpv-icon.ico',
                 'osdep/mpv.exe.manifest']

    sources += windows.compile_resources('osdep/mpv.rc', args: res_flags, depend_files: resources,
                                         depends: version_h, include_directories: res_includes)
endif


if get_option('libmpv')
    client_h_define = cc.get_define('MPV_CLIENT_API_VERSION', prefix: '#include "libmpv/client.h"',
                                    include_directories: include_directories('.'))
    major = client_h_define.split('|')[0].split('<<')[0].strip('() ')
    minor = client_h_define.split('|')[1].strip('() ')
    client_api_version = major + '.' + minor + '.0'

    libmpv = library('mpv', sources, dependencies: dependencies, gnu_symbol_visibility: 'hidden',
                     version: client_api_version, include_directories: includedir, install: true)
    pkg = import('pkgconfig')
    pkg.generate(libmpv, version: client_api_version,
                 description: 'mpv media player client library')

    headers = ['libmpv/client.h', 'libmpv/render.h',
               'libmpv/render_gl.h', 'libmpv/stream_cb.h']
    install_headers(headers, subdir: 'mpv')
endif

if get_option('cplayer')
    datadir = get_option('datadir')
    confdir = get_option('sysconfdir')

    conf_files = ['etc/mpv.conf', 'etc/input.conf',
                  'etc/mplayer-input.conf', 'etc/restore-old-bindings.conf']
    install_data(conf_files, install_dir: join_paths(datadir, 'doc', 'mpv'))

    bash_install_dir = join_paths(datadir, 'bash-completion', 'completions')
    install_data('etc/mpv.bash-completion', install_dir: bash_install_dir, rename: 'mpv')

    zsh_install_dir = join_paths(datadir, 'zsh', 'site-functions')
    install_data('etc/_mpv.zsh', install_dir: zsh_install_dir, rename: '_mpv')

    install_data('etc/mpv.desktop', install_dir: join_paths(datadir, 'applications'))
    install_data('etc/encoding-profiles.conf', install_dir: join_paths(confdir, 'mpv'))

    foreach size: ['16x16', '32x32', '64x64', '128x128']
        icon_dir = join_paths(datadir, 'icons', 'hicolor', size, 'apps')
        install_data('etc/mpv-icon-8bit-' + size + '.png', install_dir: icon_dir, rename: 'mpv.png')
    endforeach

    hicolor_dir = join_paths(datadir, 'icons', 'hicolor')
    install_data('etc/mpv-gradient.svg', install_dir: join_paths(hicolor_dir, 'scalable', 'apps'),
                 rename: 'mpv.svg')
    install_data('etc/mpv-symbolic.svg', install_dir: join_paths(hicolor_dir, 'symbolic', 'apps'))

    executable('mpv', sources, dependencies: dependencies, win_subsystem: 'windows,6.0',
               include_directories: includedir, install: true)
endif

summary({'d3d11': d3d11.allowed(),
         'gpu-next': libplacebo_next,
         'javascript': javascript.found(),
         'libmpv': get_option('libmpv'),
         'lua': lua.found(),
         'opengl': features['gl'],
         'vulkan': vulkan.found(),
         'wayland': features['wayland'],
         'x11': features['x11']},
         bool_yn: true)
