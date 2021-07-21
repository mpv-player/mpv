project('mpv',
        'c',
        license: ['GPL2+', 'LGPL2.1+'],
        version: '0.33',
        default_options: [
            'buildtype=debugoptimized',
            'c_std=c11',
            'warning_level=1',
        ]
)

cc = meson.get_compiler('c')


# essential dependencies
ffmpeg = {
    'name': 'ffmpeg',
    'deps': [dependency('libavutil', version: '>= 56.12.100'),
             dependency('libavcodec', version: '>= 58.12.100'),
             dependency('libavformat', version: '>= 58.9.100'),
             dependency('libswscale', version: '>= 5.0.101'),
             dependency('libavfilter', version: '>= 7.14.100'),
             dependency('libswresample', version: '>= 3.0.100')],
}

libass = dependency('libass', version: '>= 0.12.2')

pthreads = dependency('threads')

zlib = cc.find_library('z')

dependencies = [ffmpeg['deps'],
                libass,
                pthreads,
                zlib]

iconv = find_program('iconv', required: get_option('iconv'))
if not iconv.found() and not get_option('iconv').disabled()
    error('''Unable to find iconv which should be part of a standard
          compilation environment. Aborting. If you really mean to compile
          without iconv support use -Diconv=disabled.''')
endif


# compiler/linker flags
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
              '-Werror=format-security',
              '-Wno-redundant-decls',
              '-Wvla',
              '-Wno-format-truncation',
              '-Wimplicit-fallthrough',
              '-fno-math-errno']

flags += cc.get_supported_arguments(test_flags)

if cc.get_id() == 'gcc'
    gcc_flags = ['-Wundef', '-Wmissing-prototypes', '-Wshadow',
                 '-Wno-switch', '-Wparentheses', '-Wpointer-arith',
                 '-Wno-pointer-sign',
                 # GCC bug 66425
                 '-Wno-unused-result']
    flags += gcc_flags
endif

if cc.get_id() == 'clang'
    clang_flags = ['-Wno-logical-op-parentheses', '-fcolor-diagnostics',
                   '-Wno-tautological-compare',
                   '-Wno-tautological-constant-out-of-range-compare']
    flags += clang_flags
endif

vector = {
    'name': 'vector',
    'statement': run_command('cat', 'waftools/fragments/vector.c').stdout(),
    'use': false,
}
vector += {'use': cc.compiles(vector['statement'], name: 'vector check')}

win32 = host_machine.system() == 'cygwin' or host_machine.system() == 'windows'
mingw = win32 # assume true for now but this is probably wrong
posix = false
if not win32
    posix = true
endif

mswin_flags = ['-D_WIN32_WINNT=0x0602', '-DUNICODE', '-DCOBJMACROS',
               '-DINITGUID', '-U__STRICT_ANSI__']

mswin_linkflags = ['-Wl,--major-os-version=6,--minor-os-version=0',
                   '-Wl,--major-subsystem-version=6,--minor-subsystem-version=0']

if host_machine.system() == 'windows'
    flags += [mswin_flags, '-D__USE_MINGW_ANSI_STDIO=1']
    link_flags += [mswin_linkflags, '-mwindows']
endif

if host_machine.system() == 'cygwin'
    flags += [mswin_flags, '-mwin32']
    link_flags += mswin_linkflags
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

if get_option('build-date').disabled()
    flags += '-DNO_BUILD_TIMESTAMPS'
endif

if get_option('pthread-debug').enabled()
    if cc.has_argument('-DMP_PTHREAD_DEBUG')
        flags += '-DMP_PTHREAD_DEBUG'
    endif
endif

libdl = {
    'name': 'libdl',
    'deps': cc.find_library('libdl', required: false),
    'use' : false,
}
if libdl['deps'].found()
    libdl_fragment = run_command('cat', 'waftools/fragments/libdl.c').stdout()
    libdl += {'use': cc.compiles(libdl_fragment, dependencies: libdl['deps'], name: 'libdl check')}
endif
if libdl['use']
    dependencies += libdl['deps']
endif

cplugins = {
    'name': 'cplugins',
    'use': libdl['use'] and not win32 and not get_option('cplugins').disabled()
}
if cplugins['use']
    if cc.has_link_argument('-rdynamic')
        link_flags += '-rdynamic'
    endif
endif

add_project_arguments(flags, language: 'c')
add_project_link_arguments(link_flags, language: 'c')


# various file generators
tools_directory = join_paths(meson.source_root(), 'TOOLS')
file2string = join_paths(tools_directory, 'file2string.py')
generated_dir = join_paths(meson.build_root(), 'generated')

run_command('mkdir', '-p', join_paths(generated_dir, 'etc'))
run_command('mkdir', '-p', join_paths(generated_dir, 'sub'))
run_command('mkdir', '-p', join_paths(generated_dir, 'TOOLS', 'osxbundle', 'mpv.app', 'Contents', 'Resources'))
run_command('mkdir', '-p', join_paths(generated_dir, 'player', 'lua'))
run_command('mkdir', '-p', join_paths(generated_dir, 'player', 'javascript'))

osxbundle = ['icon.icns']
foreach file: osxbundle
    input = join_paths(meson.source_root(), 'TOOLS', 'osxbundle', 'mpv.app', 'Contents', 'Resources', file)
    output = join_paths(generated_dir, 'TOOLS', 'osxbundle', 'mpv.app', 'Contents', 'Resources', file + '.inc')
    command = 'python '+file2string+' '+input+' > '+output
    run_command('sh', '-c', command)
endforeach

icons = ['16', '32', '64', '128']
foreach size: icons
    name = 'mpv-icon-8bit-'+size+'x'+size+'.png'
    input = join_paths(meson.source_root(), 'etc', name)
    output = join_paths(generated_dir, 'etc', name + '.inc')
    command = 'python ' + file2string + ' ' + input + ' > ' + output
    run_command('sh', '-c', command)
endforeach

etc_files = ['input.conf', 'builtin.conf']
foreach file: etc_files
    input = join_paths(meson.source_root(), 'etc', file)
    output = join_paths(generated_dir, 'etc', file + '.inc')
    command = 'python ' + file2string + ' ' + input + ' > ' + output
    run_command('sh', '-c', command)
endforeach

sub_file = ['osd_font.otf']
foreach file: sub_file
    input = join_paths(meson.source_root(), 'sub', file)
    output = join_paths(generated_dir, 'sub', file + '.inc')
    command = 'python ' + file2string + ' ' + input + ' > ' + output
    run_command('sh', '-c', command)
endforeach

lua_files = ['defaults.lua', 'assdraw.lua', 'options.lua', 'osc.lua',
             'ytdl_hook.lua', 'stats.lua', 'console.lua', 'auto_profiles.lua']
foreach file: lua_files
    input = join_paths(meson.source_root(), 'player', 'lua', file)
    output = join_paths(generated_dir, 'player', 'lua', file + '.inc')
    command = 'python ' + file2string + ' ' + input + ' > ' + output
    run_command('sh', '-c', command)
endforeach

javascript_file = ['defaults.js']
foreach file: javascript_file
    input = join_paths(meson.source_root(), 'player', 'javascript', file)
    output = join_paths(generated_dir, 'player', 'javascript', file + '.inc')
    command = 'python '+ file2string + ' ' + input + ' > ' + output
    run_command('sh', '-c', command)
endforeach

matroska = join_paths(tools_directory, 'matroska.py')
ebml_defs = join_paths(generated_dir, 'ebml_defs.c')
ebml_types = join_paths(generated_dir, 'ebml_types.h')
run_command('sh', '-c', matroska+' --generate-definitions > ' + ebml_defs)
run_command('sh', '-c', matroska+' --generate-header > ' + ebml_types)

version_h = join_paths(meson.source_root(), 'version.sh')
version_h_out = join_paths(generated_dir, 'version.h')
run_command('sh', version_h, '--versionh_abs=' + version_h_out)


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
    'osdep/threads.c',
    'osdep/timer.c',

    'osdep/semaphore_osx.c',
    'osdep/subprocess.c',

    ## tree_allocator
    'ta/ta.c',
    'ta/ta_talloc.c',
    'ta/ta_utils.c'
)


# osdep
if posix
    sources += ['input/ipc-unix.c',
                'osdep/main-fn-unix.c',
                'osdep/path-unix.c',
                'osdep/polldev.c',
                'osdep/subprocess-posix.c',
                'osdep/terminal-unix.c',
                'sub/filter_regex.c']
endif

if posix and host_machine.system() != 'darwin'
    sources += 'osdep/timer-linux.c'
endif

if host_machine.system() == 'darwin'
    sources += 'osdep/timer-darwin.c'
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

android = {
    'name': 'android',
    'use': host_machine.system() == 'android' and not get_option('android').disabled(),
}
if android['use']
    android = true
    sources += ['audio/out/ao_audiotrack.c',
                'video/out/android_common.c',
                'video/out/vo_mediacodec_embed.c']
endif

tvos = {
    'name': 'tvos',
    'statement': run_command('cat', 'waftools/fragments/tvos.c').stdout(),
    'use': false,
}
if not get_option('tvos').disabled()
    tvos += {'use': cc.compiles(tvos['statement'], name: 'tvos check')}
endif

##TODO: swift shit here

uwp = cc.find_library('windowsapp', required: get_option('uwp'))
if uwp.found()
    dependencies += uwp
    sources += 'osdep/path-uwp.c'
endif

if win32
    sources += ['osdep/timer-win2.c',
                'osdep/w32_keyboard.c',
                'osdep/windows_utils.c']
endif

win32_desktop = win32 and not uwp.found()
if win32_desktop
    sources += ['input/ipc-win.c',
                'osdep/main-fn-win.c',
                'osdep/path-win.c',
                'osdep/subprocess-win.c',
                'osdep/terminal-win.c',
                'video/out/w32_common.c',
                'video/out/win32/displayconfig.c',
                'video/out/win32/droptarget.c']
endif

win32_pthreads = host_machine.system() == 'windows' and not posix
if not get_option('win32-internal-pthreads').disabled() and win32_pthreads
    sources += 'osdep/win32/pthread.c'
endif

glob_posix = {
    'name': 'glob_posix',
    'statement': run_command('cat', 'waftools/fragments/glob_posix.c').stdout(),
    'use': false,
}
glob_posix += {'use': cc.compiles(glob_posix['statement'], name: 'glob-posix check')}

glob_win32 = win32 and not posix
if glob_win32
    sources += 'osdep/glob-win.c'
endif

if glob_posix['use'] or glob_win32
    glob = true
else
    glob = false
endif

vt_h = {
    'name': 'vt.h',
    'statement': run_command('cat', 'waftools/fragments/vt_h.c').stdout(),
    'use': false,
}
vt_h += {'use': cc.compiles(vt_h['statement'], name: 'vt.h check')}

consio_h = {
    'name': 'consio.h',
    'statement': run_command('cat', 'waftools/fragments/consio_h.c').stdout(),
    'use': false,
}
if not vt_h['use']
    consio_h += {'use': cc.compiles(consio_h['statement'], name: 'consio.h check')}
endif

glibc_thread_name = {
    'name': 'glibc-thread-name',
    'statement': run_command('cat', 'waftools/fragments/glibc_thread_name.c').stdout(),
    'use': false,
}
glibc_thread_name += {'use': cc.compiles(glibc_thread_name['statement'], dependencies: pthreads, name: 'glibc-thread-name check')}

osx_thread_name = {
    'name': 'osx-thread-name',
    'statement': run_command('cat', 'waftools/fragments/osx_thread_name.c').stdout(),
    'use': false,
}
if not glibc_thread_name['use']
    osx_thread_name += {'use': cc.compiles(osx_thread_name['statement'], dependencies: pthreads, name: 'osx-thread-name check')}
endif

bsd_thread_name = {
    'name': 'bsd-thread-name',
    'statement': run_command('cat', 'waftools/fragments/bsd_thread_name.c').stdout(),
    'use': false,
}
if not glibc_thread_name['use'] and not osx_thread_name['use']
    bsd_thread_name += {'use': cc.compiles(bsd_thread_name['statement'], dependencies: pthreads, name: 'bsd-thread-name check')}
endif

bsd_fstatfs = {
    'name': 'bsd-fstatfs',
    'statement': run_command('cat', 'waftools/fragments/bsd_fstatfs.c').stdout(),
    'use': false,
}
bsd_fstatfs += {'use': cc.compiles(bsd_fstatfs['statement'], name: 'bsd-fstatfs check')}

linux_fstatfs = {
    'name': 'linux-fstatfs',
    'statement': run_command('cat', 'waftools/fragments/linux_fstatfs.c').stdout(),
    'use': false,
}
linux_fstatfs += {'use': cc.compiles(linux_fstatfs['statement'], name: 'linux-fstatfs check')}


# misc pkg-config dependencies
cdda = dependency('libcdio_paranoia', required: get_option('cdda'))
if cdda.found()
    dependencies += cdda
    sources += 'stream/stream_cdda.c'
endif

dvdnav = dependency('dvdnav', version: '>= 4.2.0', required: get_option('dvdnav'))
dvdread = dependency('dvdread', version: '>= 4.1.0', required: get_option('dvdnav'))
if dvdnav.found() and dvdread.found()
    dependencies += [dvdnav, dvdread]
    sources += 'stream/stream_dvdnav.c'
endif

javascript = dependency('mujs', version: '>= 1.0.0', required: get_option('javascript'))
if javascript.found()
    dependencies += javascript
    sources += 'player/javascript.c'
endif

lcms2 = dependency('lcms2', version: '>= 2.6', required: get_option('lcms2'))
if lcms2.found()
    dependencies += lcms2
endif

libarchive = dependency('libarchive', version: '>= 3.4.0', required: get_option('libarchive'))
if libarchive.found()
    dependencies += libarchive
    sources += ['demux/demux_libarchive.c',
                'stream/stream_libarchive.c']
endif

libavdevice = dependency('libavdevice', version: '>= 57.0.0', required: get_option('libavdevice'))
if libavdevice.found()
    dependencies += libavdevice
endif

libbluray = dependency('libbluray', version: '>= 0.3.0', required: get_option('libbluray'))
if libbluray.found()
    dependencies += libbluray
    sources += 'stream/stream_bluray.c'
endif

lua_version = [['52', '>= 5.2.0, < 5.3.0'],
               ['lua52', '>= 5.2.0'],
               ['lua5.2', '>= 5.2.0'],
               ['lua-5.2', '>= 5.2.0'],
               ['luajit', '>= 2.0.0'],
               ['51', '>= 5.1.0, < 5.2.0'],
               ['lua51', '>= 5.1.0'],
               ['lua5.1', '>= 5.1.0'],
               ['lua-5.1', '>= 5.1.0']]
foreach version : lua_version
    lua = dependency(version[0], version: version[1], required: false)
    if lua.found()
        break
    endif
endforeach
if lua.found()
    dependencies += lua
    sources += 'player/lua.c'
endif
if not lua.found() and get_option('lua').enabled()
    error('lua enabled but no suitable lua version could be found!')
endif

rubberband = dependency('rubberband', version: '>= 1.8.0', required: get_option('rubberband'))
if rubberband.found()
    dependencies += rubberband
    sources += 'audio/filter/af_rubberband.c'
endif

sdl2 = dependency('sdl2', required: get_option('sdl2'))
if sdl2.found()
    dependencies += sdl2
    sources += ['audio/out/ao_sdl.c',
                'input/sdl_gamepad.c',
                'video/out/vo_sdl.c']
endif

uchardet = dependency('uchardet', required: get_option('uchardet'))
if uchardet.found()
    dependencies += uchardet
endif

vapoursynth = dependency('vapoursynth', version: '>= 24', required: get_option('vapoursynth'))
vapoursynth_script = dependency('vapoursynth-script', version: '>= 23', required: get_option('vapoursynth'))
if vapoursynth.found() and vapoursynth_script.found()
    dependencies += [vapoursynth, vapoursynth_script]
    sources += 'video/filter/vf_vapoursynth.c'
endif

zimg = dependency('zimg', version: '>= 2.9', required: get_option('zimg'))
if zimg.found()
    dependencies += zimg
    sources += ['video/filter/vf_fingerprint.c',
                'video/zimg.c']
endif


# misc other dependencies
dvbin = get_option('gpl').enabled() and not get_option('dvbin').disabled()
if dvbin
    sources += ['stream/dvb_tune.c',
                'stream/stream_dvb.c']
endif

libm = cc.find_library('m', required: false)
if libm.found()
    dependencies += libm
endif

librt = cc.find_library('rt', required: false)
if librt.found()
    dependencies += librt
endif

stdatomic = {
    'name': 'stdatomic',
    'deps': cc.find_library('atomic', required: get_option('stdatomic')),
    'statement': run_command('cat', 'waftools/fragments/stdatomic.c').stdout(),
    'use' : false,
}
if stdatomic['deps'].found()
    stdatomic += {'use': cc.compiles(stdatomic['statement'], dependencies: stdatomic['deps'], name: 'stdatomic check')}
endif
if stdatomic['use']
    dependencies += stdatomic['deps']
endif


# audio output dependencies
alsa = dependency('alsa', version: '>= 1.0.18', required: get_option('alsa'))
if alsa.found()
    dependencies += alsa
    sources += 'audio/out/ao_alsa.c'
endif

audiounit = {
    'name': 'audiounit',
    'statement': run_command('cat', 'waftools/fragments/audiounit.c').stdout(),
    'use': false,
}
if not get_option('audiounit').disabled()
    audiounit += {'use': cc.compiles(audiounit['statement'], name: 'audiounit check')}
endif
if audiounit['use']
    sources += 'audio/out/ao_audiounit.m'
endif

coreaudio = {
    'name': 'coreaudio',
    'statement': run_command('cat', 'waftools/fragments/coreaudio.c').stdout(),
    'use': false,
}
if not get_option('coreaudio').disabled()
    coreaudio += {'use': cc.compiles(coreaudio['statement'], name: 'coreaudio check')}
endif
if coreaudio['use']
    sources += ['audio/out/ao_coreaudio.c',
                'audio/out/ao_coreaudio_exclusive.c',
                'audio/out/ao_coreaudio_properties.c']
endif

if coreaudio['use'] or audiounit['use']
    sources += ['audio/out/ao_coreaudio_chmap.c',
                'audio/out/ao_coreaudio_utils.c']
endif

jack = dependency('jack', required: get_option('jack'))
if jack.found() and get_option('gpl').enabled()
    dependencies += jack
    sources += 'audio/out/ao_jack.c'
endif

openal = dependency('openal', version: '>= 1.13', required: get_option('openal'))
if openal.found()
    dependencies += openal
    sources += 'audio/out/ao_openal.c'
endif

opensles = {
    'name': 'opensles',
    'deps': cc.find_library('OpenSLES', required: get_option('opensles')),
    'statement': run_command('cat', 'waftools/fragments/opensles.c').stdout(),
    'use': false,
}
if opensles['deps'].found()
    opensles += {'use': cc.compiles(opensles['statement'], dependencies: opensles, name: 'opensles check')}
endif
if opensles['use']
    dependencies += opensles['deps']
    sources += 'audio/out/ao_opensles.c'
endif

oss = {
    'name': 'oss',
    'statement': run_command('cat', 'waftools/fragments/oss.c').stdout(),
    'use': false,
}
if not get_option('oss-audio').disabled()
    oss += {'use': cc.compiles(oss['statement'], name: 'oss check')}
endif
if oss['use']
    sources += 'audio/out/ao_oss.c'
endif

pulse = dependency('libpulse', version: '>= 1.0', required: get_option('pulse'))
if pulse.found()
    dependencies += pulse
    sources += 'audio/out/ao_pulse.c'
endif

wasapi = {
    'name': 'wasapi',
    'statement': run_command('cat', 'waftools/fragments/wasapi.c').stdout(),
    'use': false,
}
if not get_option('wasapi').disabled() and win32
    wasapi += {'use': cc.compiles(wasapi['statement'], name: 'wasapi check')}
endif
if wasapi['use']
    sources += ['audio/out/ao_wasapi.c',
                'audio/out/ao_wasapi_changenotify.c',
                'audio/out/ao_wasapi_utils.c']
endif


# video output dependencies
caca = dependency('caca', version: '>= 0.99.beta18', required: get_option('caca'))
if caca.found()
    dependencies += caca
    sources += 'video/out/vo_caca.c'
endif

drm = {
    'name': 'drm',
    'deps': dependency('libdrm', version: '>= 2.4.74', required: get_option('drm')),
    'header': vt_h['use'] or consio_h['use'],
}
drm += {'use': drm['deps'].found() and drm['header']}
if drm['use']
    dependencies += drm['deps']
    sources += ['video/out/drm_atomic.c',
                'video/out/drm_common.c',
                'video/out/drm_prime.c',
                'video/out/opengl/hwdec_drmprime_drm.c',
                'video/out/vo_drm.c']
endif

gbm = {
    'name': 'gbm',
    'deps': dependency('gbm', required: get_option('gbm')),
    'header': cc.check_header('stdio.h') and cc.check_header('gbm.h'),
}
gbm += {'use': gbm['deps'].found() and gbm['header']}
if gbm['use']
    dependencies += gbm['deps']
endif

jpeg = cc.find_library('jpeg', required: get_option('jpeg'))
if jpeg.found()
    dependencies += jpeg
endif

libplacebo_project = subproject(
    'libplacebo',
    required: false,
    version: '>3.120.3'
)
libplacebo = dependency('libplacebo', fallback: ['libplacebo', 'libplacebo'], version: '>=2.72.0', required: get_option('libplacebo'))
if libplacebo.found()
    dependencies += libplacebo
    sources += ['video/out/placebo/ra_pl.c',
                'video/out/placebo/utils.c']
    version = libplacebo.version()
    version_split = version.split('.')
    libplacebo_apiver = version_split[1]
else
    libplacebo_apiver = 0
endif

shaderc_shared = {
    'name': 'shaderc-shared',
    'deps': dependency('shaderc', required: get_option('shaderc')),
    'header': cc.check_header('shaderc/shaderc.h') and get_option('default_library') == 'shared',
}
shaderc_shared += {'use': shaderc_shared['deps'].found() and shaderc_shared['header']}
if shaderc_shared['use']
    dependencies += shaderc_shared['deps']
endif

shaderc_static = {
    'name': 'shaderc-static',
    'deps': dependency('shaderc_combined', required: get_option('shaderc')),
    'header': cc.check_header('shaderc/shaderc.h') and not shaderc_shared['use'],
}
shaderc_static += {'use': shaderc_static['deps'].found() and shaderc_static['header']}
if shaderc_static['use']
    dependencies += shaderc_static['deps']
endif

shaderc = {
    'name': 'shaderc',
    'use': shaderc_shared['use'] or shaderc_static['use']
}
if shaderc['use']
    sources += 'video/out/gpu/spirv_shaderc.c'
endif

sixel = dependency('sixel', version: '>= 1.5', required: get_option('sixel'))
if sixel.found()
    dependencies += sixel
    sources += 'video/out/vo_sixel.c'
endif

spirv_cross_shared = {
    'name': 'spirv-cross-shared',
    'deps': dependency('spirv-cross-c-shared', required: get_option('spirv-cross')),
}
spirv_cross_shared += {'use': spirv_cross_shared['deps'].found() and get_option('default_library') == 'shared'}

spirv_cross_static = {
    'name': 'spirv-cross-static',
    'deps': dependency('spirv-cross', required: get_option('spirv-cross')),
}
spirv_cross_static += {'use': spirv_cross_static['deps'].found() and not spirv_cross_shared['use']}

spirv_cross = {
    'name': 'spirv-cross',
    'use': spirv_cross_shared['use'] or spirv_cross_static['use']
}

d3d11 = {
    'name': 'd3d11',
    'use': win32_desktop and shaderc['use'] and spirv_cross['use'] and not get_option('d3d11').disabled(),
}

wayland = {
    'name': 'wayland',
    'deps': [dependency('wayland-client', version: '>= 1.15.0', required: get_option('wayland')),
             dependency('wayland-cursor', version: '>= 1.15.0', required: get_option('wayland')),
             dependency('wayland-protocols', required: get_option('wayland')),
             dependency('xkbcommon', version: '>= 0.3.0', required: get_option('wayland'))],
    'header': cc.check_header('linux/input-event-codes.h'),
    'scanner': find_program('wayland-scanner', required: get_option('wayland')),
    'use': false,
}
wayland_deps = true
foreach dep: wayland['deps']
    if not dep.found()
        wayland_deps = false
        break
    endif
endforeach

if wayland_deps and wayland['header'] and wayland['scanner'].found()
    wayland += {'use': true}
endif

if wayland['use']
    wl_protocol_dir = wayland['deps'][2].get_pkgconfig_variable('pkgdatadir')
    protocols = [[wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
                 [wl_protocol_dir, 'stable/presentation-time/presentation-time.xml'],
                 [wl_protocol_dir, 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'],
                 [wl_protocol_dir, 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml']]
    wl_protocols_source = []
    wl_protocols_headers = []

    wayland_dir = join_paths(generated_dir, 'wayland')
    run_command('mkdir', '-p', wayland_dir)
    foreach p: protocols
        xml = join_paths(p)
        protocol_name = run_command('basename', xml, '.xml')
        protocol_code = join_paths(wayland_dir, protocol_name.stdout().strip() + '.c')
        protocol_header = join_paths(wayland_dir, protocol_name.stdout().strip() + '.h')
        run_command('wayland-scanner', 'private-code', xml, protocol_code)
        run_command('wayland-scanner', 'client-header', xml, protocol_header)
        wl_protocols_source += protocol_code
        wl_protocols_headers += protocol_header
    endforeach

    lib_client_protocols = static_library('protocols',
                                          wl_protocols_source + wl_protocols_headers,
                                          dependencies: wayland['deps'][0])

    client_protocols = declare_dependency(link_with: lib_client_protocols,
                                          sources: wl_protocols_headers)

    dependencies += [client_protocols, wayland['deps']]

    sources += ['video/out/wayland_common.c']
endif

memfd_create = {
    'name': 'memfd_create',
    'statement': run_command('cat', 'waftools/fragments/memfd_create.c').stdout(),
    'use': false,
}
if wayland['use']
    memfd_create += {'use': cc.compiles(memfd_create['statement'], name: 'memfd_create check')}
endif
if wayland['use'] and memfd_create['use']
      sources += 'video/out/vo_wlshm.c'
endif

x11 = {
    'name': 'x11',
    'deps': [dependency('x11', version: '>= 1.0.0', required: get_option('x11')),
             dependency('xscrnsaver', version: '>= 1.0.0', required: get_option('x11')),
             dependency('xext', version: '>= 1.0.0', required: get_option('x11')),
             dependency('xinerama', version: '>= 1.0.0', required: get_option('x11')),
             dependency('xrandr', version: '>= 1.2.0', required: get_option('x11'))],
}
foreach dep: x11['deps']
    if not dep.found()
        x11 += {'use': false}
        break
    endif
    if dep == x11['deps'][4]
       x11 += {'use': true}
    endif
endforeach
if x11['use']
    dependencies += x11['deps']
    sources += ['video/out/vo_x11.c',
                'video/out/x11_common.c']
endif

xv = dependency('xv', required: get_option('xv'))
if xv.found()
    dependencies += xv
    sources += 'video/out/vo_xv.c'
endif


# Opengl dependencies
gl = {
    'name': 'gl',
    'use': false,
}
if not get_option('gl').disabled()
    sources += ['video/out/opengl/common.c',
                'video/out/opengl/context.c',
                'video/out/opengl/formats.c',
                'video/out/opengl/libmpv_gl.c',
                'video/out/opengl/ra_gl.c',
                'video/out/opengl/utils.c']
    plain_gl = {
        'name': 'plain-gl',
        'use': not get_option('plain-gl').disabled(),
    }
    if plain_gl['use']
        gl += {'use': true}
    endif

    egl = {
        'name': 'egl',
        'deps': dependency('egl', version: '> 1.4.0', required: get_option('egl')),
        'use' : false
    }
    if egl['deps'].found()
        dependencies += egl['deps']
        egl += {'use': true}
    endif

    egl_drm = {
        'name': 'egl-drm',
        'use': drm['use'] and gbm['use'] and egl['use'] and not get_option('egl-drm').disabled(),
    }
    if egl_drm['use']
        gl += {'use': true}
        sources += 'video/out/opengl/context_drm_egl.c'
    endif

    egl_wayland = {
        'name': 'egl-wayland',
        'deps': dependency('wayland-egl', version: '>= 9.0.0', required: get_option('egl-wayland')),
    }
    egl_wayland += {'use': wayland['use'] and egl['use'] and egl_wayland['deps'].found()}
    if egl_wayland['use']
        dependencies += egl_wayland['deps']
        gl += {'use': true}
        sources += 'video/out/opengl/context_wayland.c'
    endif

    gl_x11 = {
        'name': 'gl_x11',
        'deps': cc.find_library('GL', required: get_option('gl-x11')),
        'use': false,
    }
    if x11['use']
        if gl_x11['deps'].found()
            gl_x11 += {'use': true}
        endif
    endif
    if gl_x11['use']
        dependencies += gl_x11['deps']
        gl += {'use': true}
        sources += 'video/out/opengl/context_glx.c'
    endif

    egl_x11 = {
        'name': 'egl-x11',
        'use': x11['use'] and egl['use'] and not get_option('egl-x11').disabled(),
    }
    if egl_x11['use']
        gl += {'use': true}
        sources += 'video/out/opengl/context_x11egl.c'
    endif

    if egl_x11['use'] or gl_x11['use']
        sources += 'video/out/opengl/oml_sync.c'
    endif

    gl_win32 = {
        'name': 'gl-win32',
        'deps': cc.find_library('opengl32', required: get_option('gl-win32')),
        'statement': run_command('cat', 'waftools/fragments/gl_win32.c').stdout(),
        'use': false,
    }
    if gl_win32['deps'].found() and win32
        gl_win32 += {'use': cc.compiles(gl_win32['statement'], dependencies: gl_win32['deps'], name: 'gl-win32 check')}
    endif
    if gl_win32['use']
        dependencies += gl_win32['deps']
        gl += {'use': true}
        sources += 'video/out/opengl/context_win.c'
    endif

    gl_dxinterop = {
        'name': 'gl-dxinterop',
        'd3d_statement': run_command('cat', 'waftools/fragments/gl_dxinterop_d3d.c').stdout(),
        'gl_statement': run_command('cat', 'waftools/fragments/gl_dxinterop_gl.c').stdout(),
        'use': false,
    }
    if gl_win32['use'] and not get_option('gl-dxinterop').disabled()
        gl_dxinterop_d3d = cc.compiles(gl_dxinterop['d3d_statement'], name: 'gl-dxinterop d3d check')
        gl_dxinterop_gl = cc.compiles(gl_dxinterop['gl_statement'], name: 'gl-dxinterop gl check')
        gl_dxinterop += {'use': gl_dxinterop_d3d and gl_dxinterop_gl}
    endif
    if gl_dxinterop['use']
        sources += 'video/out/opengl/context_dxinterop.c'
    endif

    egl_angle = {
        'name': 'egl-angle',
        'statement': run_command('cat', 'waftools/fragments/egl_angle.c').stdout(),
        'use': false,
    }
    if win32 and not get_option('egl-angle').disabled()
        egl_angle += {'use': cc.compiles(egl_angle['statement'], name: 'egl-angle check')}
    endif
    if egl_angle['use']
        'video/out/opengl/angle_dynamic.c'
    endif

    #TODO: make this nightmareish egl angle lib check work
    egl_angle_lib = {
        'name': 'egl-angle-lib',
        'statement': run_command('cat', 'waftools/fragments/egl_angle_lib.c').stdout(),
        'use': false,
    }
    if not get_option('egl-angle-lib').disabled()
        angle_flags = ['-DGL_APICALL=', '-DEGLAPI=',
                       '-DANGLE_NO_ALIASES', '-DANGLE_EXPORT=']
        angle_deps = [egl['deps'],
                      cc.find_library('GLESv2', required: false),
                      cc.find_library('dxguid', required: false),
                      cc.find_library('d3d9', required: false),
                      cc.find_library('gdi32', required: false)]
        egl_angle_lib += {'use': cc.compiles(egl_angle_lib['statement'], args: angle_flags, dependencies: angle_deps)}
        # angle_libs = [egl, glesv2, dxguid, d3d9, gdi32, stdc++]
    endif

    egl_angle_win32 = {
        'name': 'egl-angle-win32',
        'use': win32_desktop and egl_angle['use'] and not get_option('egl-angle-win32').disabled(),
    }
    if egl_angle_win32['use']
        sources += 'video/out/opengl/context_angle.c'
    endif

    egl_android = {
        'name': 'egl-android',
        'use': android['use'] and egl['deps'].found() and not get_option('egl-android').disabled()
    }
    if egl_android['use']
        sources += 'video/out/opengl/context_android.c'
        egl_android['use']
    endif

    egl_helpers = {
        'name': 'egl-helpers',
        'use': egl['use'] or egl_android['use'] or egl_angle_win32['use'] # or rpi['use']
    }
    if egl_helpers['use']
        sources += 'video/out/opengl/egl_helpers.c'
    endif

    if egl['use'] and egl_helpers['use']
        sources += 'video/filter/vf_gpu.c'
    endif

    if not gl['use']
        error('No OpenGL video output found or enabled. If you really mean to compile without OpenGL video outputs use -Dgl=disabled.')
    endif
endif


vulkan = dependency('vulkan', required: get_option('vulkan'))
if vulkan.found() and libplacebo.found()
    dependencies += vulkan
    sources += ['video/out/vulkan/context.c',
                'video/out/vulkan/context_display.c',
                'video/out/vulkan/utils.c']
endif

if vulkan.found() and libplacebo.found() and x11['use']
     sources += 'video/out/vulkan/context_xlib.c'
endif

if vulkan.found() and libplacebo.found() and wayland['use']
    sources += 'video/out/vulkan/context_wayland.c'
endif


# hwaccel
vdpau = dependency('vdpau', version: '>= 0.2', required: get_option('vdpau'))
if vdpau.found() and x11['use']
    dependencies += vdpau
    sources += ['video/filter/vf_vdpaupp.c',
                'video/out/vo_vdpau.c',
                'video/vdpau.c',
                'video/vdpau_mixer.c']
endif
if vdpau.found() and gl_x11['use']
    sources += 'video/out/opengl/hwdec_vdpau.c'
endif

vaapi = {
    'name': 'vaapi',
    'deps': dependency('libva', version: '>= 1.1.0', required: get_option('vaapi')),
}
vaapi += {'use': vaapi['deps'].found() and libdl['use'] and (x11['use'] or wayland['use'] or egl_drm['use'])}
if vaapi['use']
    dependencies += vaapi['deps']
    sources += ['video/filter/vf_vavpp.c',
                'video/vaapi.c']
endif

vaapi_x11 = {
    'name': 'vaapi-x11',
    'deps': dependency('libva-x11', version: '>= 1.1.0', required: get_option('vaapi-x11')),
}
vaapi_x11 += {'use': vaapi['use'] and x11['use'] and vaapi_x11['deps'].found()}
if vaapi_x11['use'] and get_option('gpl').enabled()
    dependencies += vaapi_x11['deps']
    sources += 'video/out/vo_vaapi.c'
endif

vaapi_wayland = {
    'name': 'vaapi-wayland',
    'deps': dependency('libva-wayland', version: '>= 1.1.0', required: get_option('vaapi-wayland')),
}
vaapi_wayland += {'use': vaapi['use'] and egl_wayland['use'] and vaapi_wayland['deps'].found()}

vaapi_drm = {
    'name': 'vaapi-drm',
    'deps': dependency('libva-drm', version: '>= 1.1.0', required: get_option('vaapi-drm')),
}
vaapi_drm += {'use': vaapi['use'] and egl_drm['use'] and vaapi_drm['deps'].found()}

vaapi_x_egl = {
    'name': 'vaapi-x-egl',
    'use': vaapi_x11['use'] and egl_x11['use'],
}

vaapi_egl = {
    'name': 'vaapi-egl',
    'use': vaapi_x_egl['use'] or vaapi_wayland['use'] or vaapi_drm['use'],
}
if vaapi_egl['use']
    dependencies += [vaapi_wayland['deps'], vaapi_drm['deps']]
    sources += 'video/out/hwdec/hwdec_vaapi_gl.c'
endif

vaapi_vulkan = {
    'name': 'vaapi-vulkan',
    'use': vaapi['use'] and libplacebo.found() and vulkan.found(),
}
if vaapi_vulkan['use']
    sources += 'video/out/hwdec/hwdec_vaapi_vk.c'
endif

if vaapi_egl['use'] or vaapi_vulkan['use']
    sources += 'video/out/hwdec/hwdec_vaapi.c'
endif

ffnvcodec = dependency('ffnvcodec', version: '>= 8.2.15.7', required: get_option('cuda-hwaccel'))
if ffnvcodec.found()
    dependencies += ffnvcodec
    sources += 'video/cuda.c'
endif

if ffnvcodec.found() and not get_option('cuda-interop').disabled()
    sources += 'video/out/hwdec/hwdec_cuda.c'
endif

if ffnvcodec.found() and not get_option('cuda-interop').disabled() and gl['use']
    sources += 'video/out/hwdec/hwdec_cuda_gl.c'
endif

if ffnvcodec.found() and vulkan.found() and libplacebo.found() and not get_option('cuda-interop').disabled()
    sources += 'video/out/hwdec/hwdec_cuda_vk.c'
endif


# extra options
if get_option('tests').enabled()
    sources += ['test/champ.c',
                'test/gl_video.c',
                'test/img_format.c',
                'test/json.c',
                'test/linked_list.c',
                'test/paths.c',
                'test/scale_sws.c',
                'test/scale_test.c',
                'test/tests.c']
endif

if get_option('tests').enabled() and zimg.found()
    sources += ['test/repack.c',
                'test/scale_zimg.c']
endif

manpage = 'DOCS/man/mpv.rst'

rst2man = find_program('rst2man', required: get_option('manpage-build'))
if rst2man.found()
    out_dir = join_paths(meson.build_root(), 'DOCS', 'man')
    run_command('mkdir', '-p', out_dir)
    run_command('rst2man', '--strip-elements-with-class=contents', manpage, join_paths(out_dir, 'mpv.1'))
    mandir = get_option('mandir')
    install_data(join_paths(out_dir, 'mpv.1'), install_dir: join_paths(mandir, 'man1'), rename: 'mpv.1.gz')
endif

rst2html = find_program('rst2html', required: get_option('html-build'))
if rst2html.found()
    out_dir = join_paths(meson.build_root(), 'DOCS', 'man')
    run_command('mkdir', '-p', out_dir)
    run_command('rst2html', manpage, join_paths(out_dir, 'mpv.html'))
    datadir = get_option('datadir')
    install_data(join_paths(out_dir, 'mpv.html'), install_dir: join_paths(datadir, 'doc', 'mpv'))
endif

rst2pdf = find_program('rst2pdf', required: get_option('pdf-build'))
if rst2pdf.found()
    out_dir = join_paths(meson.build_root(), 'DOCS', 'man')
    run_command('mkdir', '-p', out_dir)
    run_command('rst2pdf', '-c', '-b', '1', '--repeat-table-rows', manpage, '-o', join_paths(out_dir, 'mpv.pdf'))
    datadir = get_option('datadir')
    install_data(join_paths(out_dir, 'mpv.html'), install_dir: join_paths(datadir, 'doc', 'mpv'))
endif


# Compatibility with the waf build
# Much of this could be removed in the future
conf_data = configuration_data()
conf_data.set_quoted('DEFAULT_DVD_DEVICE', dvd_device)
conf_data.set_quoted('DEFAULT_CDROM_DEVICE', cd_device)
conf_data.set10('HAVE_GPL', get_option('gpl').enabled())
conf_data.set10('HAVE_LGPL', not get_option('gpl').enabled())
conf_data.set10('HAVE_CPLAYER', not get_option('cplayer').disabled())
conf_data.set10('HAVE_LIBMPV', get_option('libmpv').enabled())
conf_data.set10('HAVE_BUILD_DATE', not get_option('build-date').disabled())
conf_data.set10('HAVE_OPTIMIZE', get_option('optimization'))
conf_data.set10('HAVE_DEBUG_BUILD', get_option('debug'))
conf_data.set10('HAVE_TESTS', get_option('tests').enabled())
conf_data.set10('HAVE_TA_LEAK_REPORT', not get_option('ta-leak-report').disabled())
conf_data.set10('HAVE_HTML_BUILD', rst2html.found())
conf_data.set10('HAVE_PDF_BUILD', rst2pdf.found())
conf_data.set10('HAVE_LIBDL', libdl['use'])
conf_data.set10('HAVE_CPLUGINS', cplugins['use'])
conf_data.set10('HAVE_ASM', get_option('asm').enabled())
conf_data.set10('HAVE_VECTOR', vector['use'])
conf_data.set10('HAVE_CLANG_DATABASE', get_option('clang-database').enabled())
conf_data.set10('HAVE_SWIFT_STATIC', 0)
conf_data.set10('HAVE_NOEXECSTACK', noexecstack)
conf_data.set10('HAVE_LIBM', libm.found())
conf_data.set10('HAVE_MINGW', mingw)
conf_data.set10('HAVE_POSIX', posix)
conf_data.set10('HAVE_ANDROID', android['use'])
conf_data.set10('HAVE_TVOS', tvos['use'])
conf_data.set10('HAVE_EGL_ANDROID', egl_android['use'])
conf_data.set10('HAVE_POSIX_OR_MINGW', posix or mingw)
conf_data.set10('HAVE_SWIFT', 0)
conf_data.set10('HAVE_UWP', uwp.found())
conf_data.set10('HAVE_WIN32_DESKTOP', win32_desktop)
conf_data.set10('HAVE_WIN32_INTERNAL_PTHREADS', win32_pthreads)
conf_data.set10('HAVE_PTHREADS', pthreads.found())
conf_data.set10('HAVE_PTHREADS_DEBUG', get_option('pthread-debug').enabled())
conf_data.set10('HAVE_STDATOMIC', stdatomic['use'])
conf_data.set10('HAVE_LIBRT', librt.found())
conf_data.set10('HAVE_ICONV', iconv.found())
conf_data.set10('HAVE_DOS_PATHS', win32)
conf_data.set10('HAVE_GLOB_POSIX', glob_posix['use'])
conf_data.set10('HAVE_GLOB_WIN32', glob_win32)
conf_data.set10('HAVE_GLOB', glob)
conf_data.set10('HAVE_VT_H', vt_h['use'])
conf_data.set10('HAVE_CONSIO_H', consio_h['use'])
conf_data.set10('HAVE_GBM_H', gbm['header'])
conf_data.set10('HAVE_GLIBC_THREAD_NAME', glibc_thread_name['use'])
conf_data.set10('HAVE_OSX_THREAD_NAME', osx_thread_name['use'])
conf_data.set10('HAVE_BSD_THREAD_NAME', bsd_thread_name['use'])
conf_data.set10('HAVE_BSD_FSTATFS', bsd_fstatfs['use'])
conf_data.set10('HAVE_LINUX_FSTATFS', linux_fstatfs['use'])
conf_data.set10('HAVE_LUA', lua.found())
conf_data.set10('HAVE_JAVASCRIPT', javascript.found())
conf_data.set10('HAVE_LIBASS', libass.found())
conf_data.set10('HAVE_ZLIB', zlib.found() or zlib['use'])
conf_data.set10('HAVE_LIBBLURAY', libbluray.found())
conf_data.set10('HAVE_DVDNAV', dvdnav.found() and dvdread.found())
conf_data.set10('HAVE_CDDA', cdda.found())
conf_data.set10('HAVE_UCHARDET', uchardet.found())
conf_data.set10('HAVE_RUBBERBAND', rubberband.found())
conf_data.set10('HAVE_ZIMG', zimg.found())
conf_data.set10('HAVE_LCMS2', lcms2.found())
conf_data.set10('HAVE_VAPOURSYNTH', vapoursynth.found() and vapoursynth_script.found())
conf_data.set10('HAVE_LIBARCHIVE', libarchive.found())
conf_data.set10('HAVE_DVBIN', dvbin)
conf_data.set10('HAVE_SDL2', sdl2.found())
conf_data.set10('HAVE_SDL2_GAMEPAD', sdl2.found())
conf_data.set10('HAVE_FFPMEG', 1)
conf_data.set10('HAVE_LIBAVDEVICE', libavdevice.found())
conf_data.set10('HAVE_FFMPEG_STRICT_ABI', get_option('ffmpeg-strict-abi').enabled())
conf_data.set10('HAVE_SDL2_AUDIO', sdl2.found())
conf_data.set10('HAVE_OSS_AUDIO', oss['use'])
conf_data.set10('HAVE_PULSE', pulse.found())
conf_data.set10('HAVE_JACK', jack.found())
conf_data.set10('HAVE_OPENAL', openal.found())
conf_data.set10('HAVE_OPENSLES', opensles['use'])
conf_data.set10('HAVE_ALSA', alsa.found())
conf_data.set10('HAVE_COREAUDIO', coreaudio['use'])
conf_data.set10('HAVE_AUDIOUNIT', audiounit['use'])
conf_data.set10('HAVE_WASAPI', wasapi['use'])
conf_data.set10('HAVE_SDL2_VIDEO', sdl2.found())
conf_data.set10('HAVE_COCOA', 0)
conf_data.set10('HAVE_DRM', drm['use'])
conf_data.set10('HAVE_GBM', gbm['use'])
conf_data.set10('HAVE_WAYLAND_PROTOCOLS', wayland['deps'][2].found())
conf_data.set10('HAVE_WAYLAND', wayland['use'])
conf_data.set10('HAVE_MEMFD_CREATE', memfd_create['use'])
conf_data.set10('HAVE_X11', x11['use'])
conf_data.set10('HAVE_XV', xv.found())
conf_data.set10('HAVE_GL_COCOA', 0)
conf_data.set10('HAVE_GL_X11', gl_x11['use'])
conf_data.set10('HAVE_RPI', 0)
conf_data.set10('HAVE_EGL', egl['use'])
conf_data.set10('HAVE_EGL_X11', egl_x11['use'])
conf_data.set10('HAVE_EGL_DRM', egl_drm['use'])
conf_data.set10('HAVE_GL_WAYLAND', egl_wayland['use'])
conf_data.set10('HAVE_GL_WIN32', gl_win32['use'])
conf_data.set10('HAVE_GL_DXINTEROP', gl_dxinterop['use'])
conf_data.set10('HAVE_EGL_ANGLE', egl_angle['use'])
conf_data.set10('HAVE_EGL_ANGLE_LIB', egl_angle_lib['use'])
conf_data.set10('HAVE_EGL_ANGLE_WIN32', egl_angle_win32['use'])
conf_data.set10('HAVE_VDPAU', vdpau.found() and x11['use'])
conf_data.set10('HAVE_VDPAU_GL_X11', vdpau.found() and gl_x11['use'])
conf_data.set10('HAVE_VAAPI', vaapi['use'])
conf_data.set10('HAVE_VAAPI_X11', vaapi_x11['use'])
conf_data.set10('HAVE_VAAPI_WAYLAND', vaapi_wayland['use'])
conf_data.set10('HAVE_VAAPI_DRM', vaapi_drm['use'])
conf_data.set10('HAVE_VAAPI_X_EGL', vaapi_x_egl['use'])
conf_data.set10('HAVE_VAAPI_EGL', vaapi_egl['use'])
conf_data.set10('HAVE_CACA', caca.found())
conf_data.set10('HAVE_JPEG', jpeg.found())
conf_data.set10('HAVE_DIRECT3D', 0)
conf_data.set10('HAVE_SHADERC_SHARED', shaderc_shared['use'])
conf_data.set10('HAVE_SHADERC_STATIC', shaderc_static['use'])
conf_data.set10('HAVE_SHADERC', shaderc['use'])
conf_data.set10('HAVE_SPIRV_CROSS_SHARED', spirv_cross_shared['use'])
conf_data.set10('HAVE_SPIRV_CROSS_STATIC', spirv_cross_static['use'])
conf_data.set10('HAVE_SPIRV_CROSS', spirv_cross['use'])
conf_data.set10('HAVE_D3D11', 0)
conf_data.set10('HAVE_IOS_GL', 0)
conf_data.set10('HAVE_PLAIN_GL', not get_option('plain-gl').disabled())
conf_data.set10('HAVE_GL', gl['use'])
conf_data.set10('HAVE_LIBPLACEBO', libplacebo.found())
conf_data.set('PL_API_VER', libplacebo_apiver)
conf_data.set10('HAVE_VULKAN', libplacebo.found() and vulkan.found())
conf_data.set10('HAVE_VAAPI_VULKAN', vaapi_vulkan['use'])
conf_data.set10('HAVE_EGL_HELPERS', egl_helpers['use'])
conf_data.set10('HAVE_SIXEL', sixel.found())
conf_data.set10('HAVE_VIDEOTOOBOX_HWACCEL', 0)
conf_data.set10('HAVE_VIDEOTOOLBOX_GL', 0)
conf_data.set10('HAVE_D3D_HWACCEL', 0)
conf_data.set10('HAVE_D3D9_HWACCEL', 0)
conf_data.set10('HAVE_GL_DXINTEROP_D3D9', 0)
conf_data.set10('HAVE_FFNVCODEC', ffnvcodec.found())
conf_data.set10('HAVE_CUDA_HWACCEL', ffnvcodec.found() and not get_option('cuda-interop').disabled())
conf_data.set10('HAVE_CUDA_INTEROP', ffnvcodec.found() and gl['use'] and not get_option('cuda-interop').disabled())
conf_data.set10('HAVE_RPI_MMAL', 0)
conf_data.set10('HAVE_WIN32_EXECUTABLE', 0)
conf_data.set10('HAVE_MACOS_TOUCHBAR', 0)
conf_data.set10('HAVE_MACOS_10_11_FEATURES', 0)
conf_data.set10('HAVE_MACOS_10_12_2_FEATURES', 0)
conf_data.set10('HAVE_MACOS_10_14_FEATURES', 0)
conf_data.set10('HAVE_MACOS_MEDIA_PLAYER', 0)
conf_data.set10('HAVE_MACOS_COCOA_CB', 0)
conf_data.set_quoted('CONFIGURATION', 'TODO')
conf_data.set_quoted('MPV_CONFDIR', join_paths(get_option('sysconfdir'), 'mpv'))
conf_data.set_quoted('FULLCONFIG', 'TODO')
configure_file(output : 'config.h',
               configuration : conf_data)

    # TODO: swift and cocoa sources
    #'osdep/macos/log_helper.swift'
    #'osdep/macos/libmpv_helper.swift'
    #'osdep/main-fn-cocoa.c'
    #'osdep/macos/mpv_helper.swift'
    #'osdep/macos/swift_extensions.swift'
    #'osdep/macos/swift_compat.swift'
    #'osdep/macos/remote_command_center.swift' 'macos-media-player'
    #'video/out/mac/common.swift'
    #'video/out/mac/view.swift'
    #'video/out/mac/window.swift'
    #'video/out/mac/title_bar.swift'
    #'video/out/cocoa_cb_common.swift', 'macos-cocoa-cb'
    #'video/out/mac/gl_layer.swift', 'macos-cocoa-cb'


    # TODO: dummy stuff

    #'osdep/terminal-dummy.c'
    #'input/ipc-dummy.c'
    #'osdep/subprocess-dummy.c'


    #'video/out/vulkan/context_android.c',  'vulkan && android',
    #'video/out/vulkan/context_win.c',      'vulkan && win32-desktop',


    #'video/d3d.c',                         'd3d-hwaccel',
    #'video/filter/vf_d3d11vpp.c',          'd3d-hwaccel',


    #'video/out/cocoa/events_view.m',       'cocoa',
    #'video/out/cocoa/video_view.m',        'cocoa',
    #'video/out/cocoa/window.m',            'cocoa',
    #'video/out/cocoa_common.m',            'cocoa',
    #'video/out/d3d11/context.c',           'd3d11',
    #'video/out/d3d11/hwdec_d3d11va.c',     'd3d11 && d3d-hwaccel',
    #'video/out/d3d11/hwdec_dxva2dxgi.c',   'd3d11 && d3d9-hwaccel',
    #'video/out/d3d11/ra_d3d11.c',          'd3d11',
    #'video/out/gpu/d3d11_helpers.c',       'd3d11 || egl-angle-win32',
    #'video/out/opengl/context_cocoa.c',    'gl-cocoa',
    #'video/out/opengl/context_rpi.c',      'rpi',
    #'video/out/opengl/hwdec_d3d11egl.c',   'd3d-hwaccel && egl-angle',
    #'video/out/opengl/hwdec_dxva2egl.c',   'd3d9-hwaccel && egl-angle',
    #'video/out/opengl/hwdec_dxva2gldx.c',  'gl-dxinterop-d3d9',
    #'video/out/opengl/hwdec_ios.m',        'ios-gl',
    #'video/out/opengl/hwdec_osx.c',        'videotoolbox-gl',
    #'video/out/opengl/hwdec_rpi.c',        'rpi-mmal',
    #'video/out/vo_direct3d.c',             'direct3d',
    #'video/out/vo_rpi.c',                  'rpi-mmal',


    #'osdep/macosx_application.m',          'cocoa',
    #'osdep/macosx_events.m',               'cocoa',
    #'osdep/macosx_menubar.m',              'cocoa',
    #'osdep/macosx_touchbar.m',             'macos-touchbar',
    #'osdep/mpv.rc',                        'win32-executable',
    #'osdep/path-macosx.m',                 'cocoa',

if not get_option('cplayer').disabled()
    datadir = get_option('datadir')
    confdir = get_option('sysconfdir')

    conf_files = ['etc/mpv.conf', 'etc/input.conf',
                  'etc/mplayer-input.conf', 'etc/restore-old-bindings.conf']
    install_data(conf_files, install_dir: join_paths(datadir, 'doc', 'mpv'))

    zsh = find_program('zsh', required: false)
    if zsh.found()
        zsh_install_dir = join_paths(datadir, 'zsh', 'site-functions')
        install_data('etc/_mpv.zsh', install_dir: zsh_install_dir, rename: '_mpv')
    endif

    bash = find_program('bash', required: false)
    if bash.found()
        bash_install_dir = join_paths(datadir, 'bash-completion', 'completions')
        install_data('etc/mpv.bash-completion', install_dir: bash_install_dir, rename: 'mpv')
    endif

    install_data('etc/mpv.desktop', install_dir: join_paths(datadir, 'applications'))
    install_data('etc/encoding-profiles.conf', install_dir: join_paths(confdir, 'mpv'))

    foreach size: ['16x16', '32x32', '64x64', '128x128']
        icon_dir = join_paths('datadir', 'icons', 'hicolor', size, 'apps')
        install_data('etc/mpv-icon-8bit-' + size + '.png', install_dir: icon_dir, rename: 'mpv.png')
    endforeach

    hicolor_dir = join_paths(datadir, 'icons', 'hicolor')
    install_data('etc/mpv-gradient.svg', install_dir: join_paths(hicolor_dir, 'scalable', 'apps'), rename: 'mpv.svg')
    install_data('etc/mpv-symbolic.svg', install_dir: join_paths(hicolor_dir, 'symbolc', 'apps'))
endif

if get_option('libmpv').enabled()
    client_h_path = meson.source_root() + '/libmpv/client.h'
    major = '''grep "#define MPV_CLIENT_API_VERSION" ''' + client_h_path + ''' | sed -e "s/.*(\(.*\),.*/\1/"'''
    minor = '''grep "#define MPV_CLIENT_API_VERSION" ''' + client_h_path + ''' | sed -e "s/.* \(.*\)).*/\1/"'''
    major_ver = run_command('sh', '-c', major).stdout().strip()
    minor_ver = run_command('sh', '-c', minor).stdout().strip()
    version = major_ver + '.' + minor_ver + '.0'

    library('mpv', sources, dependencies: dependencies, version: version, install: true)

    includedir = get_option('includedir')
    libdir = get_option('libdir')

    headers = ['libmpv/client.h', 'libmpv/opengl_cb.h', 'libmpv/render.h',
               'libmpv/render_gl.h', 'libmpv/stream_cb.h']
    install_data(headers, install_dir: join_paths(includedir, 'mpv'))
    install_data('libmpv/mpv.pc.in', install_dir: join_paths(libdir, 'pkgconfig'), rename: 'mpv.pc')
endif

executable('mpv', sources, dependencies: dependencies, install: true)
