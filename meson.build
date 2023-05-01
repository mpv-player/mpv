project('mpv',
        'c',
        license: ['GPL2+', 'LGPL2.1+'],
        version: files('./VERSION'),
        meson_version: '>=0.62.0',
        default_options: [
            'buildtype=debugoptimized',
            'b_lundef=false',
            'c_std=c11',
            'warning_level=2',
        ]
)

build_root = meson.project_build_root()
source_root = meson.project_source_root()
python = find_program('python3')

# ffmpeg
libavcodec = dependency('libavcodec', version: '>= 58.134.100')
libavfilter = dependency('libavfilter', version: '>= 7.110.100')
libavformat = dependency('libavformat', version: '>= 58.76.100')
libavutil = dependency('libavutil', version: '>= 56.70.100')
libswresample = dependency('libswresample', version: '>= 3.9.100')
libswscale = dependency('libswscale', version: '>= 5.9.100')

libplacebo = dependency('libplacebo', version: '>=6.338.2',
                default_options: ['default_library=static', 'demos=false'])

libass = dependency('libass', version: '>= 0.12.2')

# the dependency order of libass -> ffmpeg is necessary due to
# static linking symbol resolution between fontconfig and MinGW
dependencies = [libass,
                libavcodec,
                libavfilter,
                libavformat,
                libavutil,
                libplacebo,
                libswresample,
                libswscale]

# Keeps track of all enabled/disabled features
features = {
    'debug': get_option('debug'),
    'ffmpeg': true,
    'gpl': get_option('gpl'),
    'jpegxl': libavformat.version().version_compare('>= 59.27.100'),
    'avif-muxer': libavformat.version().version_compare('>= 59.24.100'),
    'libass': true,
    'libplacebo': true,
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
    'misc/io_utils.c',
    'misc/json.c',
    'misc/language.c',
    'misc/natural_sort.c',
    'misc/node.c',
    'misc/path_utils.c',
    'misc/random.c',
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
    'video/out/vo_kitty.c',
    'video/out/win_state.c',
    'video/repack.c',
    'video/sws_utils.c',

    ## libplacebo
    'video/out/placebo/ra_pl.c',
    'video/out/placebo/utils.c',
    'video/out/vo_gpu_next.c',
    'video/out/gpu_next/context.c',

    ## osdep
    'osdep/io.c',
    'osdep/semaphore-mac.c',
    'osdep/subprocess.c',
    'osdep/timer.c',

    ## tree_allocator
    'ta/ta.c',
    'ta/ta_talloc.c',
    'ta/ta_utils.c'
)


# compiler stuff
cc = meson.get_compiler('c')

flags = ['-D_FILE_OFFSET_BITS=64']
link_flags = []

test_flags = ['-Werror=implicit-function-declaration',
              '-Wno-missing-field-initializers',
              '-Wno-sign-compare',
              '-Wno-unused-parameter',
              '-Wno-cast-function-type',
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
    flags += ['-Wformat', '-Werror=format-security']
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

features += {'darwin': darwin}
features += {'posix': posix}
features += {'dos-paths': win32, 'win32': win32}

mswin_flags = ['-D_WIN32_WINNT=0x0602', '-DWINVER=0x0602', '-DUNICODE',
               '-DCOBJMACROS', '-DINITGUID', '-U__STRICT_ANSI__', '-DNOMINMAX',
               '-D_USE_MATH_DEFINES', '-DWIN32_LEAN_AND_MEAN']

if host_machine.system() == 'windows'
    flags += [mswin_flags]
endif

if host_machine.system() == 'cygwin'
    flags += [mswin_flags, '-mwin32']
endif

if posix
    flags += ['-D_GNU_SOURCE']
endif

if cc.has_link_argument('-Wl,-z,noexecstack')
    link_flags += '-Wl,-z,noexecstack'
endif

if cc.has_link_argument('-Wl,--nxcompat,--no-seh,--dynamicbase')
    link_flags += '-Wl,--nxcompat,--no-seh,--dynamicbase'
endif

features += {'build-date': get_option('build-date')}
if not features['build-date']
    flags += '-DNO_BUILD_TIMESTAMPS'
endif

features += {'ta-leak-report': get_option('ta-leak-report')}

libdl = dependency('dl', required: false)
features += {'libdl': libdl.found()}
if features['libdl']
    dependencies += libdl
endif

# C11 atomics are mandatory but linking to the library is not always required.
dependencies += cc.find_library('atomic', required: false)

cplugins = get_option('cplugins').require(
    win32 or (features['libdl'] and cc.has_link_argument('-rdynamic')),
    error_message: 'cplugins not supported by the os or compiler!',
)
features += {'cplugins': cplugins.allowed()}
if features['cplugins'] and not win32
    link_flags += '-rdynamic'
endif

win32_threads = get_option('win32-threads').require(win32)

features += {'win32-threads': win32_threads.allowed()}
if not features['win32-threads']
    pthreads = dependency('threads')
    sources += files('osdep/threads-posix.c')
    features += {'pthread-condattr-setclock':
                    cc.has_header_symbol('pthread.h',
                                         'pthread_condattr_setclock',
                                         args: '-D_GNU_SOURCE',
                                         dependencies: pthreads)}
    dependencies += pthreads
endif

pthread_debug = get_option('pthread-debug').require(
    win32_threads.disabled(),
    error_message: 'win32-threads was found!',
)
features += {'pthread-debug': pthread_debug.allowed()}

add_project_arguments(flags, language: 'c')
add_project_arguments(['-Wno-unused-parameter'], language: 'objc')
add_project_link_arguments(link_flags, language: ['c', 'objc'])


# osdep
cocoa = dependency('appleframeworks', modules: ['Cocoa', 'IOKit', 'QuartzCore'],
                   required: get_option('cocoa'))
features += {'cocoa': cocoa.found()}
if features['cocoa']
    dependencies += cocoa
    sources += files('osdep/language-mac.c',
                     'osdep/path-mac.m',
                     'osdep/utils-mac.c',
                     'osdep/mac/app_bridge.m')
    main_fn_source = files('osdep/main-fn-mac.c')
endif

if posix
    path_source = files('osdep/path-unix.c')
    if cc.has_function('fork', prefix : '#include <unistd.h>')
        subprocess_source = files('osdep/subprocess-posix.c')
    else
        subprocess_source = files('osdep/subprocess-dummy.c')
    endif
    sources += files('input/ipc-unix.c',
                     'osdep/poll_wrapper.c',
                     'osdep/terminal-unix.c',
                     'sub/filter_regex.c')
endif

if posix and not features['cocoa']
    sources += files('osdep/language-posix.c')
    main_fn_source = files('osdep/main-fn-unix.c')
endif

if darwin
    path_source = files('osdep/path-darwin.c')
    timer_source = files('osdep/timer-darwin.c')
endif

if posix and not darwin
    timer_source = files('osdep/timer-linux.c')
endif

features += {'ppoll': cc.has_function('ppoll', args: '-D_GNU_SOURCE',
                                      prefix: '#include <poll.h>')}

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

features += {'android': host_machine.system() == 'android'}
if features['android']
    dependencies += cc.find_library('android')
    sources += files('audio/out/ao_audiotrack.c',
                     'misc/jni.c',
                     'osdep/android/strnlen.c',
                     'video/out/android_common.c',
                     'video/out/vo_mediacodec_embed.c')
endif

uwp_opt = get_option('uwp').require(
    not get_option('cplayer'),
    error_message: 'cplayer is not false!',
)
uwp = cc.find_library('windowsapp', required: uwp_opt)
features += {'uwp': uwp.found()}
if features['uwp']
    dependencies += uwp
    path_source = files('osdep/path-uwp.c')
    subprocess_source = []
endif

features += {'win32-executable': win32 and get_option('cplayer')}
if win32
    timer_source = files('osdep/timer-win32.c')
    sources += files('osdep/w32_keyboard.c',
                     'osdep/windows_utils.c')
endif

features += {'win32-desktop': win32 and not uwp.found()}
if features['win32-desktop']
    win32_desktop_libs = [cc.find_library('avrt'),
                          cc.find_library('dwmapi'),
                          cc.find_library('gdi32'),
                          cc.find_library('imm32'),
                          cc.find_library('ntdll'),
                          cc.find_library('ole32'),
                          cc.find_library('uuid'),
                          cc.find_library('uxtheme'),
                          cc.find_library('version')]
    dependencies += win32_desktop_libs
    path_source = files('osdep/path-win.c')
    subprocess_source = files('osdep/subprocess-win.c')
    sources += files('input/ipc-win.c',
                     'osdep/language-win.c',
                     'osdep/terminal-win.c',
                     'video/out/w32_common.c',
                     'video/out/win32/displayconfig.c',
                     'video/out/win32/droptarget.c',
                     'video/out/win32/menu.c')
    main_fn_source = files('osdep/main-fn-win.c')
endif

if not posix and not features['win32-desktop']
    subprocess_source = files('osdep/subprocess-dummy.c')
    sources += files('input/ipc-dummy.c',
                     'osdep/terminal-dummy.c')
endif

features += {'glob-posix': cc.has_function('glob', prefix: '#include <glob.h>')}

features += {'glob-win32': win32 and not features['glob-posix']}
if features['glob-win32']
    sources += files('osdep/glob-win.c')
endif

features += {'glob': features['glob-posix'] or features['glob-win32']}

features += {'vt.h': cc.has_header_symbol('sys/vt.h', 'VT_GETMODE')}

features += {'consio.h': not features['vt.h'] and cc.has_header_symbol('sys/consio.h', 'VT_GETMODE')}

# macOS's pthread_setname_np is a special snowflake and differs from literally every other platform.
features += {'mac-thread-name': darwin}

features += {'glibc-thread-name': false}
if not features['mac-thread-name']
    features += {'glibc-thread-name': posix and cc.has_function('pthread_setname_np', args: '-D_GNU_SOURCE',
                                                                dependencies: pthreads, prefix: '#include <pthread.h>')}
endif

features += {'bsd-thread-name': false}
if not features['mac-thread-name'] and not features['glibc-thread-name']
    features += {'bsd-thread-name': posix and cc.has_function('pthread_set_name_np', dependencies: pthreads,
                                                              prefix: '#include <pthread.h>\n#include <pthread_np.h>')}
endif

features += {'bsd-fstatfs': cc.has_function('fstatfs', prefix: '#include <sys/mount.h>\n#include <sys/param.h>')}

features += {'linux-fstatfs': cc.has_function('fstatfs', prefix: '#include <sys/vfs.h>')}

vector_attribute = '''int main() {
float v __attribute__((vector_size(32)));
}
'''
vector = get_option('vector').require(
    cc.compiles(vector_attribute, name: 'vector check'),
    error_message: 'the compiler does not support gcc vectors!',
)
features += {'vector': vector.allowed()}

sources += path_source + subprocess_source + timer_source


# various file generations
tools_directory = join_paths(source_root, 'TOOLS')
docutils_wrapper = find_program(join_paths(tools_directory, 'docutils-wrapper.py'))
file2string = find_program(join_paths(tools_directory, 'file2string.py'))
matroska = find_program(join_paths(tools_directory, 'matroska.py'))

ebml_defs = custom_target('ebml_defs.inc',
    output: 'ebml_defs.inc',
    command: [matroska, '--generate-definitions', '@OUTPUT@'],
)

ebml_types = custom_target('ebml_types.h',
    output: 'ebml_types.h',
    command: [matroska, '--generate-header', '@OUTPUT@'],
)

sources += [ebml_defs, ebml_types]

subdir('common')
subdir('etc')
subdir('player')
subdir('sub')

if darwin
    subdir(join_paths('TOOLS', 'osxbundle'))
endif


# misc dependencies
features += {'av-channel-layout': libavutil.version().version_compare('>= 57.24.100')}
if features['av-channel-layout']
    sources += files('audio/chmap_avchannel.c')
endif

cdda_opt = get_option('cdda').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
cdio = dependency('libcdio', version: '>= 0.90', required: cdda_opt)
cdda = dependency('libcdio_paranoia', required: cdda_opt)
features += {'cdda': cdda.found() and cdio.found()}
if features['cdda']
    dependencies += [cdda, cdio]
    sources += files('stream/stream_cdda.c')
endif

dvbin_opt = get_option('dvbin').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
dvbin = cc.has_header('linux/dvb/frontend.h', required: dvbin_opt)
features += {'dvbin': dvbin}
if features['dvbin']
    sources += files('stream/dvb_tune.c',
                     'stream/stream_dvb.c')
endif

dvdnav_opt = get_option('dvdnav').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
dvdnav = dependency('dvdnav', version: '>= 4.2.0', required: dvdnav_opt)
dvdread = dependency('dvdread', version: '>= 4.1.0', required: dvdnav_opt)
features += {'dvdnav': dvdnav.found() and dvdread.found()}
if features['dvdnav']
    dependencies += [dvdnav, dvdread]
    sources += files('stream/stream_dvdnav.c')
endif

iconv = dependency('iconv', required: get_option('iconv'))
features += {'iconv': iconv.found()}
if features['iconv']
    dependencies += iconv
endif

javascript = dependency('mujs', version: '>= 1.0.0', required: get_option('javascript'))
features += {'javascript': javascript.found()}
if features['javascript']
    dependencies += javascript
    sources += files('player/javascript.c',
                     'sub/filter_jsre.c')
endif

lcms2 = dependency('lcms2', version: '>= 2.6', required: get_option('lcms2'))
features += {'lcms2': lcms2.found()}
if features['lcms2']
    dependencies += lcms2
endif

libarchive = dependency('libarchive', version: '>= 3.4.0', required: get_option('libarchive'))
features += {'libarchive': libarchive.found()}
if features['libarchive']
    dependencies += libarchive
    sources += files('demux/demux_libarchive.c',
                     'stream/stream_libarchive.c')
endif

libavdevice = dependency('libavdevice', version: '>= 58.13.100', required: get_option('libavdevice'))
features += {'libavdevice': libavdevice.found()}
if features['libavdevice']
    dependencies += libavdevice
endif

libbluray = dependency('libbluray', version: '>= 0.3.0', required: get_option('libbluray'))
features += {'libbluray': libbluray.found()}
if features['libbluray']
    dependencies += libbluray
    sources += files('stream/stream_bluray.c')
endif

libm = cc.find_library('m', required: false)
if libm.found()
    dependencies += libm
endif

librt = cc.find_library('rt', required: false)
if librt.found()
    dependencies += librt
endif

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

features += {'lua': lua.found()}
lua_version = lua.name()
if features['lua']
    dependencies += lua
    sources += files('player/lua.c')
endif
if not features['lua'] and lua_opt == 'enabled'
     error('Lua enabled but no suitable Lua version could be found!')
endif

rubberband = dependency('rubberband', version: '>= 1.8.0', required: get_option('rubberband'))
features += {'rubberband': rubberband.found()}
if features['rubberband']
    features += {'rubberband-3': rubberband.version().version_compare('>= 3.0.0')}
    dependencies += rubberband
    sources += files('audio/filter/af_rubberband.c')
endif

sdl2 = dependency('sdl2', required: get_option('sdl2'))
features += {'sdl2': sdl2.found()}
if features['sdl2']
    dependencies += sdl2
endif

sdl2_gamepad = get_option('sdl2-gamepad').require(
    features['sdl2'],
    error_message: 'sdl2 was not found!',
)
features += {'sdl2-gamepad': sdl2_gamepad.allowed()}
if features['sdl2-gamepad']
    sources += files('input/sdl_gamepad.c')
endif

uchardet_opt = get_option('uchardet').require(
    features['iconv'],
    error_message: 'iconv was not found!',
)
uchardet = dependency('uchardet', required: uchardet_opt)
features += {'uchardet': uchardet.found()}
if features['uchardet']
    dependencies += uchardet
endif

features += {'lavu-uuid': libavutil.version().version_compare('>= 57.27.100')}
if not features['lavu-uuid']
    sources += files('misc/uuid.c')
endif

vapoursynth = dependency('vapoursynth', version: '>= 26', required: get_option('vapoursynth'))
vapoursynth_script = dependency('vapoursynth-script', version: '>= 26',
                                required: get_option('vapoursynth'))
features += {'vapoursynth': vapoursynth.found() and vapoursynth_script.found()}
if features['vapoursynth']
    dependencies += [vapoursynth, vapoursynth_script]
    sources += files('video/filter/vf_vapoursynth.c')
endif

zimg = dependency('zimg', version: '>= 2.9', required: get_option('zimg'))
features += {'zimg': zimg.found()}
if features['zimg']
    dependencies += zimg
    sources += files('video/filter/vf_fingerprint.c',
                     'video/zimg.c')
    features += {'zimg-st428': zimg.version().version_compare('>= 3.0.5')}
endif

zlib = dependency('zlib', required: get_option('zlib'))
features += {'zlib': zlib.found()}
if features['zlib']
    dependencies += zlib
endif


# audio output dependencies
alsa = dependency('alsa', version: '>= 1.0.18', required: get_option('alsa'))
features += {'alsa': alsa.found()}
if features['alsa']
    dependencies += alsa
    sources += files('audio/out/ao_alsa.c')
endif

audiounit_opt = get_option('audiounit').require(darwin)
audiounit = {
    'deps': dependency('appleframeworks', modules: ['Foundation', 'AudioToolbox'],
                       required: audiounit_opt),
    'symbol': cc.has_header_symbol('AudioToolbox/AudioToolbox.h', 'kAudioUnitSubType_RemoteIO',
                                   required: audiounit_opt),
}
features += {'audiounit': audiounit['deps'].found() and audiounit['symbol']}
if features['audiounit']
    dependencies += audiounit['deps']
    sources += files('audio/out/ao_audiounit.m')
endif

avfoundation = dependency('appleframeworks', modules: ['CoreMedia', 'AVFoundation'],
                          required: get_option('avfoundation'))
features += {'avfoundation': avfoundation.found()}
if features['avfoundation']
    dependencies += avfoundation
    sources += files('audio/out/ao_avfoundation.m')
endif

coreaudio = dependency('appleframeworks', modules: ['CoreFoundation', 'CoreAudio',
                       'AudioUnit', 'AudioToolbox'], required: get_option('coreaudio'))
features += {'coreaudio': coreaudio.found()}
if features['coreaudio']
    dependencies += coreaudio
    sources += files('audio/out/ao_coreaudio.c',
                     'audio/out/ao_coreaudio_exclusive.c',
                     'audio/out/ao_coreaudio_properties.c')
endif

if features['audiounit'] or features['coreaudio'] or features['avfoundation']
    sources += files('audio/out/ao_coreaudio_chmap.c',
                     'audio/out/ao_coreaudio_utils.c',
                     'audio/out/ao_coreaudio_properties.c')
endif

jack_opt = get_option('jack').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
jack = dependency('jack', required: jack_opt)
features += {'jack': jack.found()}
if features['jack']
    dependencies += jack
    sources += files('audio/out/ao_jack.c')
endif

openal = dependency('openal', version: '>= 1.13', required: get_option('openal'))
features += {'openal': openal.found()}
if features['openal']
    dependencies += openal
    sources += files('audio/out/ao_openal.c')
endif

opensles = cc.find_library('OpenSLES', required: get_option('opensles'))
features += {'opensles': opensles.found()}
if features['opensles']
    dependencies += opensles
    sources += files('audio/out/ao_opensles.c')
endif

oss_opt = get_option('oss-audio').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
features += {'oss-audio': cc.has_header_symbol('sys/soundcard.h', 'SNDCTL_DSP_HALT',
                                               required: oss_opt)}
if features['oss-audio']
    sources += files('audio/out/ao_oss.c')
endif

pipewire = dependency('libpipewire-0.3', version: '>= 0.3.48', required: get_option('pipewire'))
features += {'pipewire': pipewire.found()}
if features['pipewire']
    dependencies += pipewire
    sources += files('audio/out/ao_pipewire.c')
endif

pulse = dependency('libpulse', version: '>= 1.0', required: get_option('pulse'))
features += {'pulse': pulse.found()}
if features['pulse']
    dependencies += pulse
    sources += files('audio/out/ao_pulse.c')
endif

sdl2_audio = get_option('sdl2-audio').require(
    features['sdl2'],
    error_message: 'sdl2 was not found!',
)
features += {'sdl2-audio': sdl2_audio.allowed()}
if features['sdl2-audio']
    sources += files('audio/out/ao_sdl.c')
endif

sndio = dependency('sndio', required: get_option('sndio'))
features += {'sndio': sndio.found()}
features += {'sndio-1-9': sndio.version().version_compare('>= 1.9.0')}
if features['sndio']
    dependencies += sndio
    sources += files('audio/out/ao_sndio.c')
endif

wasapi = cc.has_header_symbol('audioclient.h', 'IAudioClient', required:
    get_option('wasapi').require(win32))
features += {'wasapi': wasapi}
if features['wasapi']
    sources += files('audio/out/ao_wasapi.c',
                     'audio/out/ao_wasapi_changenotify.c',
                     'audio/out/ao_wasapi_utils.c')
endif


# video output dependencies
caca_opt = get_option('caca').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
caca = dependency('caca', version: '>= 0.99.beta18', required: caca_opt)
features += {'caca': caca.found()}
if features['caca']
    dependencies += caca
    sources += files('video/out/vo_caca.c')
endif

direct3d_opt = get_option('direct3d').require(
    get_option('gpl') and features['win32-desktop'],
    error_message: 'the build is not GPL or this is not a win32 desktop!',
)
direct3d = cc.has_header('d3d9.h', required: direct3d_opt)
features += {'direct3d': direct3d}
if features['direct3d']
    sources += files('video/out/vo_direct3d.c')
endif

drm = dependency('libdrm', version: '>= 2.4.105', required: get_option('drm'))
features += {'drm': drm.found() and (features['vt.h'] or features['consio.h'])}
if features['drm']
    dependencies += drm
    sources += files('video/drmprime.c',
                     'video/out/drm_atomic.c',
                     'video/out/drm_common.c',
                     'video/out/drm_prime.c',
                     'video/out/hwdec/hwdec_drmprime.c',
                     'video/out/hwdec/hwdec_drmprime_overlay.c',
                     'video/out/vo_drm.c')
endif

gbm_opt = get_option('gbm').require(
    features['drm'],
    error_message: 'drm was not found!'
)
gbm = dependency('gbm', version: '>= 17.1.0', required: gbm_opt)
features += {'gbm': gbm.found()}
if features['gbm']
    dependencies += gbm
endif

jpeg = dependency('libjpeg', required: get_option('jpeg'))
features += {'jpeg': jpeg.found()}
if features['jpeg']
    dependencies += jpeg
endif

sdl2_video = get_option('sdl2-video').require(
    features['sdl2'],
    error_message: 'sdl2 was not found!',
)
features += {'sdl2-video': sdl2_video.allowed()}
if features['sdl2-video']
    sources += files('video/out/vo_sdl.c')
endif

shaderc = dependency('shaderc', required:
    get_option('shaderc').require(features['win32-desktop']))
features += {'shaderc': shaderc.found()}
if features['shaderc']
    dependencies += shaderc
    sources += files('video/out/gpu/spirv_shaderc.c')
endif

sixel = dependency('libsixel', version: '>= 1.5', required: get_option('sixel'))
features += {'sixel': sixel.found()}
if features['sixel']
    dependencies += sixel
    sources += files('video/out/vo_sixel.c')
endif

features += {'posix-shm': false}
if features['posix']
    features += {'posix-shm': cc.has_function('shm_open', prefix: '#include <sys/mman.h>')}
endif

spirv_cross = dependency('spirv-cross-c-shared', required:
    get_option('spirv-cross').require(features['win32-desktop']))
features += {'spirv-cross': spirv_cross.found()}
if features['spirv-cross']
    dependencies += spirv_cross
endif

d3d11 = get_option('d3d11').require(
    features['win32-desktop'] and features['shaderc'] and features['spirv-cross'],
    error_message: 'Either is not a win32 desktop or shaderc nor spirv-cross were found!',
)
features += {'d3d11': d3d11.allowed()}
if features['d3d11']
    sources += files('video/out/d3d11/context.c',
                     'video/out/d3d11/ra_d3d11.c')
    features += {'dxgi-debug-d3d11': cc.has_header_symbol('d3d11sdklayers.h', 'DXGI_DEBUG_D3D11')}
endif

wayland = {
    'deps': [dependency('wayland-client', version: '>= 1.20.0', required: get_option('wayland')),
             dependency('wayland-cursor', version: '>= 1.20.0', required: get_option('wayland')),
             dependency('wayland-protocols', version: '>= 1.25', required: get_option('wayland')),
             dependency('xkbcommon', version: '>= 0.3.0', required: get_option('wayland'))],
    'header': cc.has_header('linux/input-event-codes.h', required: get_option('wayland'),
                            # Pass CFLAGS from a related package as a hint for non-Linux
                            dependencies: dependency('wayland-client', required: get_option('wayland'))),
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
    subdir(join_paths('video', 'out'))
endif

features += {'memfd-create': false}
if features['wayland']
    features += {'memfd-create': cc.has_function('memfd_create',
                                                 prefix: '#define _GNU_SOURCE\n#include <sys/mman.h>')}
endif
if features['wayland'] and features['memfd-create']
    sources += files('video/out/vo_wlshm.c')
endif

dmabuf_wayland = get_option('dmabuf-wayland').require(
    features['drm'] and features['memfd-create'] and features['wayland'],
    error_message: 'drm, memfd-create or wayland was not found!',
)
features += {'dmabuf-wayland': dmabuf_wayland.allowed()}
if features['dmabuf-wayland']
    sources += files('video/out/vo_dmabuf_wayland.c')
    sources += files('video/out/hwdec/dmabuf_interop_wl.c')
    sources += files('video/out/wldmabuf/context_wldmabuf.c')
    sources += files('video/out/wldmabuf/ra_wldmabuf.c')
endif

x11_opt = get_option('x11').require(
    get_option('gpl'),
    error_message: 'the build is not GPL!',
)
x11 = {
    'deps': [dependency('x11', version: '>= 1.0.0', required: x11_opt),
             dependency('xscrnsaver', version: '>= 1.0.0', required: x11_opt),
             dependency('xext', version: '>= 1.0.0', required: x11_opt),
             dependency('xpresent', version: '>= 1.0.0', required: x11_opt),
             dependency('xrandr', version: '>= 1.4.0', required: x11_opt)],
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
features += {'xv': xv.found()}
if features['xv']
    dependencies += xv
    sources += files('video/out/vo_xv.c')
endif

if features['wayland'] or features['x11'] or features['drm']
    sources += ('video/out/present_sync.c')
endif


# OpenGL feature checking
gl_allowed = get_option('gl').allowed()
features += {'gl': false}

GL = dependency('', required: false)
if darwin
    GL = dependency('appleframeworks', modules: 'OpenGL', required: get_option('gl-cocoa'))
elif features['win32-desktop']
    GL = dependency('GL', required: get_option('gl-win32'))
elif features['x11']
    GL = dependency('GL', required: get_option('gl-x11'))
endif

gl_cocoa = get_option('gl-cocoa').require(
    features['cocoa'] and GL.found() and gl_allowed,
    error_message: 'cocoa and GL were not found!',
)
features += {'gl-cocoa': gl_cocoa.allowed()}
if features['gl-cocoa']
    dependencies += GL
    features += {'gl': true}
endif

gl_win32 = get_option('gl-win32').require(
    GL.found() and gl_allowed and features['win32-desktop'],
    error_message: 'GL and win32 desktop were not found!',
)
features += {'gl-win32': gl_win32.allowed()}
if features['gl-win32']
    dependencies += GL
    features += {'gl': true}
    sources += files('video/out/opengl/context_win.c')
endif

gl_x11 = get_option('gl-x11').require(
    GL.found() and gl_allowed and features['x11'],
    error_message: 'GL and x11 were not found!',
)
features += {'gl-x11': gl_x11.allowed()}
if features['gl-x11']
    dependencies += GL
    features += {'gl': true}
    sources += files('video/out/opengl/context_glx.c')
endif

gl_dxinterop = get_option('gl-dxinterop').require(
    features['gl-win32'] and
        cc.has_header_symbol('GL/wglext.h', 'WGL_ACCESS_READ_ONLY_NV',
                             prefix: '#include <GL/gl.h>') and
        cc.has_header_symbol('d3d9.h', 'IDirect3D9Ex'),
    error_message: 'gl-dxinterop could not be found!',
)
features += {'gl-dxinterop': gl_dxinterop.allowed()}
if features['gl-dxinterop']
    sources += files('video/out/opengl/context_dxinterop.c')
endif

egl_angle = get_option('egl-angle').require(
    features['gl-win32'] and
        cc.has_header_symbol('EGL/eglext.h',
                             'EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE',
                             prefix: '#include <EGL/egl.h>') and
        cc.has_header_symbol('EGL/eglext_angle.h',
                             'PFNEGLCREATEDEVICEANGLEPROC',
                             # TODO: change to list when meson 1.0.0 is required
                             prefix: '#include <EGL/egl.h>\n#include <EGL/eglext.h>'),
    error_message: 'egl-angle could not be found!',
)
features += {'egl-angle': egl_angle.allowed()}
if features['egl-angle']
    sources += files('video/out/opengl/angle_dynamic.c')
endif

egl_dep = cc.find_library('EGL', required:
    get_option('egl-angle-lib').require(features['egl-angle']))
egl_angle_lib = get_option('egl-angle-lib').require(
    egl_dep.found() and cc.has_function('eglCreateWindowSurface',
                                               dependencies: egl_dep,
                                               prefix: '#include <EGL/egl.h>'),
    error_message: 'egl-angle-lib could not be found!',
)
features += {'egl-angle-lib': egl_angle_lib.allowed()}
if features['egl-angle-lib']
    dependencies += egl_dep
endif

egl_angle_win32 = get_option('egl-angle-win32').require(
    features['egl-angle'] and features['win32-desktop'],
    error_message: 'either this is not a win32 desktop or egl-angle was not found!',
)
features += {'egl-angle-win32': egl_angle_win32.allowed()}
if features['egl-angle-win32']
    sources += files('video/out/opengl/context_angle.c')
endif

if features['d3d11'] or features['egl-angle-win32']
    sources += files('video/out/gpu/d3d11_helpers.c')
endif

egl = dependency('egl', version: '>= 1.4.0', required: get_option('egl'))
features += {'egl': egl.found() and gl_allowed}
if features['egl']
    dependencies += egl
endif

egl_android_opt = get_option('egl-android').require(
    features['android'] and gl_allowed,
    error_message: 'the OS is not android!',
)
egl_android = cc.find_library('EGL', required: egl_android_opt)
features += {'egl-android': egl_android.found()}
if features['egl-android']
    dependencies += egl_android
    features += {'gl': true}
    sources += files('video/out/opengl/context_android.c')
endif

egl_drm = get_option('egl-drm').require(
    features['drm'] and features['egl'] and features['gbm'] and gl_allowed,
    error_message: 'either drm, egl, or gbm could not be found!',
)
features += {'egl-drm': egl_drm.allowed()}
if features['egl-drm']
    features += {'gl': true}
    sources += files('video/out/opengl/context_drm_egl.c')
endif

egl_wayland_opt = get_option('egl-wayland').require(
    features['egl'] and features['wayland'] and gl_allowed,
    error_message: 'either egl or wayland could not be found!',
)
egl_wayland = dependency('wayland-egl', version: '>= 9.0.0', required: egl_wayland_opt)
features += {'egl-wayland': egl_wayland.found()}
if features['egl-wayland']
    dependencies += egl_wayland
    features += {'gl': true}
    sources += files('video/out/opengl/context_wayland.c')
endif

egl_x11 = get_option('egl-x11').require(
    features['egl'] and gl_allowed and features['x11'],
    error_message: 'either egl or x11 could not be found!',
)
features += {'egl-x11': egl_x11.allowed()}
if features['egl-x11']
    features += {'gl': true}
    sources += files('video/out/opengl/context_x11egl.c')
endif

plain_gl = get_option('plain-gl').require(
    gl_allowed,
    error_message: 'gl was not enabled!',
)
if plain_gl.allowed()
    features += {'gl': true}
endif

if features['egl'] or features['egl-android'] or features['egl-angle-win32']
    sources += files('video/out/opengl/egl_helpers.c')
endif

if features['gl'] and features['egl']
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
    libplacebo.get_variable('pl_has_vulkan', default_value: '0') == '1',
    error_message: 'libplacebo compiled without vulkan support!',
)
vulkan = dependency('vulkan', version: '>= 1.1.70', required: vulkan_opt)
features += {'vulkan': vulkan.found()}
if features['vulkan']
    dependencies += vulkan
    sources += files('video/out/vulkan/context.c',
                     'video/out/vulkan/utils.c')
endif

if features['vulkan'] and features['android']
    sources += files('video/out/vulkan/context_android.c')
endif

if features['vulkan'] and features['wayland']
    sources += files('video/out/vulkan/context_wayland.c')
endif

if features['vulkan'] and features['win32-desktop']
    sources += files('video/out/vulkan/context_win.c')
endif

if features['vulkan'] and features['x11']
     sources += files('video/out/vulkan/context_xlib.c')
endif

features += {'vk-khr-display': cc.has_function('vkCreateDisplayPlaneSurfaceKHR', prefix: '#include <vulkan/vulkan_core.h>',
                                               dependencies: [vulkan])}
if features['vk-khr-display']
    sources += files('video/out/vulkan/context_display.c')
endif


# hwaccel
ffnvcodec = dependency('ffnvcodec', version: '>= 11.1.5.1', required: false)
features += {'ffnvcodec': ffnvcodec.found()}
if features['ffnvcodec']
    dependencies += ffnvcodec
    sources += files('video/cuda.c')
endif

cuda_hwaccel = get_option('cuda-hwaccel').require(
    features['ffnvcodec'],
    error_message: 'ffnvcodec was not found!',
)
features += {'cuda-hwaccel': cuda_hwaccel.allowed()}
if features['cuda-hwaccel']
    sources += files('video/out/hwdec/hwdec_cuda.c')
endif

cuda_interop = get_option('cuda-interop').require(
    features['cuda-hwaccel'] and (features['gl'] or features['vulkan']),
    error_message: 'cuda-hwaccel and either gl or vulkan were not found!',
)
features += {'cuda-interop': cuda_interop.allowed()}
if features['cuda-interop'] and features['gl']
    sources += files('video/out/hwdec/hwdec_cuda_gl.c')
endif
if features['cuda-interop'] and features['vulkan']
    sources += files('video/out/hwdec/hwdec_cuda_vk.c')
endif

android_media_ndk = get_option('android-media-ndk').require(
    features['android'] and cc.has_header_symbol('media/NdkImageReader.h', 'AIMAGE_FORMAT_PRIVATE')
)
features += {'android-media-ndk': android_media_ndk.allowed()}
if features['android-media-ndk']
    # header only, library is dynamically loaded
    sources += files('video/out/hwdec/hwdec_aimagereader.c')
endif

vulkan_interop = get_option('vulkan-interop').require(
    features['vulkan'] and vulkan.version().version_compare('>=1.3.238') and
    libavutil.version().version_compare('>=58.11.100'),
    error_message: 'Vulkan Interop requires vulkan headers >= 1.3.238, and libavutil >= 58.11.100',
)
features += {'vulkan-interop': vulkan_interop.allowed()}
if features['vulkan-interop']
    sources += files('video/out/hwdec/hwdec_vulkan.c')
endif

d3d_hwaccel = get_option('d3d-hwaccel').require(
    win32,
    error_message: 'the os is not win32!',
)
features += {'d3d-hwaccel': d3d_hwaccel.allowed()}
if features['d3d-hwaccel']
    sources += files('video/d3d.c',
                     'video/filter/vf_d3d11vpp.c')
endif

if features['d3d-hwaccel'] and features['egl-angle']
    sources += files('video/out/opengl/hwdec_d3d11egl.c')
endif
if features['d3d-hwaccel'] and features['d3d11']
    sources += files('video/out/d3d11/hwdec_d3d11va.c')
endif

d3d9_hwaccel = get_option('d3d9-hwaccel').require(
    features['d3d-hwaccel'],
    error_message: 'd3d-hwaccel was not found!',
)
features += {'d3d9-hwaccel': d3d9_hwaccel.allowed()}
if features['d3d9-hwaccel'] and features['egl-angle']
    sources += files('video/out/opengl/hwdec_dxva2egl.c')
endif
if features['d3d9-hwaccel'] and features['d3d11']
    sources += files('video/out/d3d11/hwdec_dxva2dxgi.c')
endif

gl_dxinterop_d3d9 = get_option('gl-dxinterop-d3d9').require(
    features['gl-dxinterop'] and features['d3d9-hwaccel'],
    error_message: 'gl-dxinterop and d3d9-hwaccel were not found!',
)
features += {'gl-dxinterop-d3d9': gl_dxinterop_d3d9.allowed()}
if features['gl-dxinterop-d3d9']
    sources += files('video/out/opengl/hwdec_dxva2gldx.c')
endif

# this is an arbitrary GLES 3.x symbol
ios_gl = cc.has_header_symbol('OpenGLES/ES3/glext.h', 'GL_RGB32F', required:
    get_option('ios-gl').require(darwin))
features += {'ios-gl': ios_gl}
if features['ios-gl']
    sources += files('video/out/hwdec/hwdec_ios_gl.m')
endif

libva = dependency('libva', version: '>= 1.1.0', required: get_option('vaapi'))

vaapi_drm = dependency('libva-drm', version: '>= 1.1.0', required:
    get_option('vaapi-drm').require(libva.found() and features['drm']))
features += {'vaapi-drm': vaapi_drm.found()}
if features['vaapi-drm']
    dependencies += vaapi_drm
endif

vaapi_wayland = dependency('libva-wayland', version: '>= 1.1.0', required:
    get_option('vaapi-wayland').require(libva.found() and features['wayland']))
features += {'vaapi-wayland': vaapi_wayland.found()}
if features['vaapi-wayland']
    dependencies += vaapi_wayland
endif

vaapi_x11 = dependency('libva-x11', version: '>= 1.1.0', required:
    get_option('vaapi-x11').require(libva.found() and features['x11']))
features += {'vaapi-x11': vaapi_x11.found()}
if features['vaapi-x11']
    dependencies += vaapi_x11
    sources += files('video/out/vo_vaapi.c')
endif

vaapi_win32 = dependency('libva-win32', required:
    get_option('vaapi-win32').require(libva.found() and win32))
features += {'vaapi-win32': vaapi_win32.found()}
if features['vaapi-win32']
    dependencies += vaapi_win32
endif

vaapi = get_option('vaapi').require(libva.found() and (features['vaapi-drm'] or
                                    features['vaapi-wayland'] or features['vaapi-x11'] or
                                    features['vaapi-win32']))
features += {'vaapi': vaapi.allowed()}

if features['vaapi']
    dependencies += libva
    sources += files('video/filter/vf_vavpp.c',
                     'video/vaapi.c',
                     'video/out/hwdec/hwdec_vaapi.c',
                     'video/out/hwdec/dmabuf_interop_pl.c')
endif

features += {'dmabuf-interop-gl': features['egl'] and features['drm']}
if features['dmabuf-interop-gl']
    sources += files('video/out/hwdec/dmabuf_interop_gl.c')
endif

vdpau_opt = get_option('vdpau').require(
    features['x11'],
    error_message: 'x11 was not found!',
)
vdpau = dependency('vdpau', version: '>= 0.2', required: vdpau_opt)
features += {'vdpau': vdpau.found()}
if features['vdpau']
    dependencies += vdpau
    sources += files('video/filter/vf_vdpaupp.c',
                     'video/out/vo_vdpau.c',
                     'video/vdpau.c',
                     'video/vdpau_mixer.c')
endif

features += {'vdpau-gl-x11': features['vdpau'] and gl_x11.allowed()}
if features['vdpau'] and features['vdpau-gl-x11']
    sources += files('video/out/opengl/hwdec_vdpau.c')
endif

videotoolbox_gl = get_option('videotoolbox-gl').require(
    features['gl-cocoa'] or features['ios-gl'],
    error_message: 'gl-cocoa nor ios-gl could be found!',
)
features += {'videotoolbox-gl': videotoolbox_gl.allowed()}
corevideo = dependency('appleframeworks', modules: ['CoreVideo'], required:
    get_option('videotoolbox-pl').require(darwin))
videotoolbox_pl = get_option('videotoolbox-pl').require(
    features['vulkan'] and corevideo.found(),
    error_message: 'vulkan or CV metal support could be found!',
)
features += {'videotoolbox-pl': videotoolbox_pl.allowed()}
if features['videotoolbox-gl'] or features['videotoolbox-pl'] or features['ios-gl']
    sources += files('video/out/hwdec/hwdec_vt.c')
endif
if features['videotoolbox-gl']
    sources += files('video/out/hwdec/hwdec_mac_gl.c')
endif
if features['videotoolbox-pl']
    dependencies += corevideo
    sources += files('video/out/hwdec/hwdec_vt_pl.m')
endif


# macOS features
macos_sdk_version_py = ''
if darwin
    macos_sdk_version_py = find_program(join_paths(source_root, 'TOOLS', 'macos-sdk-version.py'),
                                                   required: true)
endif

macos_sdk_path = ''
macos_sdk_version = '0.0'
if darwin and macos_sdk_version_py.found()
    macos_sdk_info = run_command(macos_sdk_version_py, check: true).stdout().split(',')
    macos_sdk_path = macos_sdk_info[0]
    macos_sdk_version = macos_sdk_info[1]
endif

if macos_sdk_path != ''
    message('Detected macOS sdk path: ' + macos_sdk_path)
endif

if macos_sdk_version != '0.0'
    message('Detected macOS SDK: ' + macos_sdk_version)
    add_languages('objc')
    objc_link_flags = ['-isysroot', macos_sdk_path, '-L/usr/lib', '-L/usr/local/lib']
    add_project_link_arguments(objc_link_flags, language: ['c', 'objc'])
endif

xcrun = find_program('xcrun', required: get_option('swift-build').require(darwin))
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
    darwin and macos_sdk_version.version_compare('>= 10.15') and swift_ver.version_compare('>= 4.1'),
    error_message: 'A suitable macos sdk version or swift version could not be found!',
)
features += {'swift': swift.allowed()}

swift_sources = []
if features['cocoa'] and features['swift']
    swift_sources += files('osdep/mac/application.swift',
                           'osdep/mac/app_hub.swift',
                           'osdep/mac/event_helper.swift',
                           'osdep/mac/input_helper.swift',
                           'osdep/mac/libmpv_helper.swift',
                           'osdep/mac/log_helper.swift',
                           'osdep/mac/menu_bar.swift',
                           'osdep/mac/option_helper.swift',
                           'osdep/mac/precise_timer.swift',
                           'osdep/mac/presentation.swift',
                           'osdep/mac/swift_compat.swift',
                           'osdep/mac/swift_extensions.swift',
                           'osdep/mac/type_helper.swift',
                           'video/out/mac/common.swift',
                           'video/out/mac/title_bar.swift',
                           'video/out/mac/view.swift',
                           'video/out/mac/window.swift')
endif

macos_cocoa_cb = get_option('macos-cocoa-cb').require(
    features['cocoa'] and features['gl-cocoa'] and features['swift'],
    error_message: 'Either cocoa, gl-cocoa or swift could not be found!',
)
features += {'macos-cocoa-cb': macos_cocoa_cb.allowed()}
if features['macos-cocoa-cb']
    swift_sources += files('video/out/cocoa_cb_common.swift',
                           'video/out/mac/gl_layer.swift')
endif
if features['cocoa'] and features['vulkan'] and features['swift']
    swift_sources += files('video/out/mac_common.swift',
                           'video/out/mac/metal_layer.swift')
    sources += files('video/out/vulkan/context_mac.m')
endif

macos_media_player = get_option('macos-media-player').require(
    features['swift'],
    error_message: 'Swift was not found!',
)
features += {'macos-media-player': macos_media_player.allowed()}
if features['macos-media-player']
    swift_sources += files('osdep/mac/remote_command_center.swift')
endif

macos_touchbar = get_option('macos-touchbar').require(
    features['cocoa'] and cc.has_header('AppKit/NSTouchBar.h'),
    error_message: 'Either cocoa could not be found or AppKit/NSTouchBar.h could not be found!',
)
features += {'macos-touchbar': macos_touchbar.allowed()}
if features['macos-touchbar']
    swift_sources += files('osdep/mac/touch_bar.swift')
endif

if features['swift'] and swift_sources.length() > 0
    subdir('osdep/mac')
endif


# manpages
manpage = 'DOCS/man/mpv.rst'
rst2man = find_program('rst2man', 'rst2man.py', required: get_option('manpage-build'))
if rst2man.found()
    mandir = get_option('mandir')
    custom_target('manpages',
        input: manpage,
        output: 'mpv.1',
        command: [
          docutils_wrapper, rst2man,
          '--record-dependencies', '@DEPFILE@',
          '--strip-elements-with-class=contents',
          '@INPUT@', '@OUTPUT@'],
        depfile: 'mpv.1.dep',
        install: true,
        install_dir: join_paths(mandir, 'man1')
    )
endif

rst2html = find_program('rst2html', 'rst2html.py', required: get_option('html-build'))
if rst2html.found()
    datadir = get_option('datadir')
    custom_target('html-manpages',
        input: manpage,
        output: 'mpv.html',
        command: [
          docutils_wrapper, rst2html,
          '--record-dependencies', '@DEPFILE@',
          '@INPUT@', '@OUTPUT@'],
        depfile: 'mpv.html.dep',
        install: true,
        install_dir: join_paths(datadir, 'doc', 'mpv')
    )
endif

rst2pdf = find_program('rst2pdf', required: get_option('pdf-build'))
if rst2pdf.found()
    dependency_file = rst2pdf.version().version_compare('>=0.100')
    datadir = get_option('datadir')
    custom_target('pdf-manpages',
        input: manpage,
        output: 'mpv.pdf',
        command: [
            docutils_wrapper, rst2pdf,
            '-c', '-b', '1', '--repeat-table-rows',
            dependency_file ? ['--record-dependencies', '@DEPFILE@'] : [],
            '@INPUT@', '-o', '@OUTPUT@'],
        depfile: 'mpv.pdf.dep',
        install: true,
        install_dir: join_paths(datadir, 'doc', 'mpv')
    )
endif


if meson.version().version_compare('>= 1.1.0')
    configuration = meson.build_options()
else
    # Arbitrary hardcoded things to pass if the meson version is too
    # old to have the build_options method.
    configuration = 'meson configure build ' + '-Dprefix=' + get_option('prefix') + \
                    ' -Dbuildtype=' + get_option('buildtype') + \
                    ' -Doptimization=' + get_option('optimization')
endif


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
conf_data.set_quoted('MPV_CONFDIR', join_paths(get_option('prefix'), get_option('sysconfdir'), 'mpv'))
conf_data.set_quoted('PLATFORM', host_machine.system())
configure_file(output : 'config.h', configuration : conf_data)
message('List of enabled features: ' + feature_str)

# These are intentionally not added to conf_data.
features += {'cplayer': get_option('cplayer')}
features += {'libmpv-' + get_option('default_library'): get_option('libmpv')}


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


client_h_define = cc.get_define('MPV_CLIENT_API_VERSION', prefix: '#include "libmpv/client.h"',
                                include_directories: include_directories('.'))
major = client_h_define.split('|')[0].split('<<')[0].strip('() ')
minor = client_h_define.split('|')[1].strip('() ')
client_api_version = major + '.' + minor + '.0'

libmpv = library('mpv', sources, dependencies: dependencies, gnu_symbol_visibility: 'hidden',
                 link_args: cc.get_supported_link_arguments(['-Wl,-Bsymbolic']),
                 version: client_api_version, install: get_option('libmpv'),
                 build_by_default: get_option('libmpv'))


if get_option('libmpv')
    pkg = import('pkgconfig')
    pkg.generate(libmpv, version: client_api_version,
                 description: 'mpv media player client library')

    headers = ['libmpv/client.h', 'libmpv/render.h',
               'libmpv/render_gl.h', 'libmpv/stream_cb.h']
    install_headers(headers, subdir: 'mpv')

    # Allow projects to build with libmpv by cloning into ./subprojects/mpv
    libmpv_dep = declare_dependency(link_with: libmpv)
    meson.override_dependency('mpv', libmpv_dep)
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
    install_data('etc/mpv.metainfo.xml', install_dir: join_paths(datadir, 'metainfo'))
    install_data('etc/encoding-profiles.conf', install_dir: join_paths(confdir, 'mpv'))

    foreach size: ['16x16', '32x32', '64x64', '128x128']
        icon_dir = join_paths(datadir, 'icons', 'hicolor', size, 'apps')
        install_data('etc/mpv-icon-8bit-' + size + '.png', install_dir: icon_dir, rename: 'mpv.png')
    endforeach

    hicolor_dir = join_paths(datadir, 'icons', 'hicolor')
    install_data('etc/mpv-gradient.svg', install_dir: join_paths(hicolor_dir, 'scalable', 'apps'),
                 rename: 'mpv.svg')
    install_data('etc/mpv-symbolic.svg', install_dir: join_paths(hicolor_dir, 'symbolic', 'apps'))

    mpv = executable('mpv', main_fn_source, objects: libmpv.extract_all_objects(recursive: true), dependencies: dependencies,
                     win_subsystem: 'windows,6.0', install: true)

    # Older meson versions generate this in the player subdirectory.
    if win32 and meson.version().version_compare('>= 1.3.0')
        wrapper_sources= 'osdep/win32-console-wrapper.c'
        executable('mpv', wrapper_sources, c_args: '-municode', link_args: '-municode',
                   name_suffix: 'com', install: true)
    endif

    if darwin
        osxbundle = find_program(join_paths(tools_directory, 'osxbundle.py'), required: true)
        custom_target('macos-bundle',
            output: 'mpv.app',
            command: [osxbundle, mpv.full_path(), '@SOURCE_ROOT@'],
        )
    endif
endif

if get_option('tests')
    subdir('test')
endif

summary({'d3d11': features['d3d11'],
         'javascript': features['javascript'],
         'libmpv': get_option('libmpv'),
         'lua': features['lua'],
         'opengl': features['gl'],
         'vulkan': features['vulkan'],
         'wayland': features['wayland'],
         'x11': features['x11']},
         bool_yn: true)
