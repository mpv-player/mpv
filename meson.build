project('mpv', 'c', license: ['GPL2+', 'LGPL2.1+'],
    default_options: 'buildtype=debugoptimized')

add_project_arguments(['-D_ISOC99_SOURCE', '-D_GNU_SOURCE',
                       '-D_FILE_OFFSET_BITS=64', '-Wall'],
                       language: 'c')
cc = meson.get_compiler('c')

if cc.has_link_argument('-std=c11')
    add_project_arguments('-std=c11', language: 'c')
elif
    add_project_arguments('-std=c99', language: 'c')
endif

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
              'fno-math-errno'] 

foreach flag: test_flags
    if cc.has_link_argument(flag)
        add_project_arguments(flag, language: 'c')
    endif
endforeach

if cc.get_id() == 'gcc'
    add_project_arguments(['-Wall', '-Wundef', '-Wmissing-prototypes', '-Wshadow',
                           '-Wno-switch', '-Wparentheses', '-Wpointer-arith',
                           '-Wno-pointer-sign',
                           # GCC bug 66425
                           '-Wno-unused-result'], language: 'c')
elif cc.get_id() == 'clang'
    add_project_arguments(['-Wno-logical-op-parentheses', '-fcolor-diagnostics',
                           '-Wno-tautological-compare',
                           '-Wno-tautological-constant-out-of-range-compare'],
                           language: 'c')
endif

if host_machine.system() == 'windows'
    add_project_arguments(['-D_WIN32_WINNT=0x0602', '-DUNICODE', '-DCOBJMACROS',
                           '-DINITGUID', '-U__STRICT_ANSI__', '-D__USE_MINGW_ANSI_STDIO=1',
                           '-Wl,--major-os-version=6,--minor-os-version=0',
                           '--major-subsystem-version=6,--minor-subsystem-version=0',
                           '-mwindows'], language: 'c')
endif

if host_machine.system() == 'cygwin'
    add_project_arguments(['-D_WIN32_WINNT=0x0602', '-DUNICODE', '-DCOBJMACROS',
                           '-DINITGUID', '-U__STRICT_ANSI__', '-Wl,--major-os-version=6',
                           '--minor-os-version=0', '--major-subsystem-version=6', 
                           '--minor-subsystem-version=0', '-mwin32'], language: 'c')
endif

noexecstack = false
if cc.has_link_argument('-Wl,-z,noexecstack')
    add_project_arguments('-Wl,-z,noexecstack', language: 'c')
    noexecstack = true
endif

if cc.has_link_argument('-Wl,--nxcompat,--no-seh,--dynamicbase')
    add_project_arguments('-Wl,--nxcompat,--no-seh,--dynamicbase', language: 'c')
    noexecstack = true
endif

# essential dependencies
ffmpeg = {
    'name': 'ffmpeg',
    'deps': [dependency('libavutil', version: '>= 56.12.100'),
             dependency('libavcodec', version: '>= 58.12.100'),
             dependency('libavformat', version: '>= 58.9.100'),
             dependency('libswscale', version: '>= 5.0.101'),
             dependency('libavfilter', version: '>= 7.14.100'),
             dependency('libswresample', version: '>= 3.0.100')],
    'use': true,
}
libass = dependency('libass', version: '>= 0.12.1')
pthreads = dependency('threads')
zlib = cc.find_library('z')

dependencies = [ffmpeg['deps'],
               libass,
               pthreads,
               zlib]

stdatomic = {
    'name': 'stdatomic',
    'deps': cc.find_library('atomic'),
    'statement': '''#include <stdatomic.h>
                    int main(int argc, char **argv) {
                        atomic_int_least64_t test = ATOMIC_VAR_INIT(123);
                        atomic_fetch_add(&test, 1);
                        return 0;
                    }''',
}
stdatomic_run = cc.run(stdatomic['statement'], dependencies: stdatomic['deps'], name: 'stdatomic check')
stdatomic += {'use': stdatomic_run.returncode() == 0 and stdatomic['deps'].found()}
if stdatomic['use']
    dependencies += stdatomic['deps']
endif

iconv = find_program('iconv', required: get_option('iconv'))
if not iconv.found() and not get_option('iconv').disabled()
    error('''Unable to find iconv which should be part of a standard
          compilation environment. Aborting. If you really mean to compile
          without iconv support use -Diconv=disabled.''')
endif

license = get_option('license')

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
            'ytdl_hook.lua', 'stats.lua', 'console.lua']
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

sources = files(
    ## Audio
    'audio/aframe.c',
    'audio/audio_buffer.c',
    'audio/chmap.c',
    'audio/chmap_sel.c',
    'audio/decode/ad_lavc.c',
    'audio/decode/ad_spdif.c',
    'audio/filter/af_drop.c',
    'audio/filter/af_format.c',
    'audio/filter/af_lavcac3enc.c',
    'audio/filter/af_scaletempo.c',
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
    'misc/ring.c',
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

    'osdep/path-unix.c',
    'osdep/semaphore_osx.c',
    'osdep/subprocess.c',

    ## tree_allocator
    'ta/ta.c',
    'ta/ta_talloc.c',
    'ta/ta_utils.c'
)

# osdep
if host_machine.system() == 'windows'
    mingw = true
    posix = false
else
    mingw = false
    posix = true
endif

if posix
    sources += ['input/ipc-unix.c',
                'osdep/main-fn-unix.c',
                'osdep/polldev.c',
                'osdep/subprocess-posix.c',
                'osdep/terminal-unix.c',
                'sub/filter_regex.c']
endif

if host_machine.system() == 'linux'
    sources += ['osdep/timer-linux.c']
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

# main dependencies
if not get_option('build-date')
    add_project_arguments('-DNO_BUILD_TIMESTAMPS', language: 'c')
endif

libm = cc.find_library('m', required: false)
if libm.found()
    dependencies += libm
endif

librt = cc.find_library('rt', required: false)
if librt.found()
    dependencies += librt
endif

libdl = {
    'name': 'libdl',
    'deps': cc.find_library('libdl', required: get_option('libdl')),
    'statement': '''#include <dlfcn.h>
                    int main(int argc, char **argv) {
                        dlopen("", 0);
                        return 0;
                    }''',
}
libdl_run = cc.run(libdl['statement'], dependencies: libdl['deps'], name: 'libdl check')
libdl += {'use': libdl_run.returncode() == 0 and libdl['deps'].found()}
if libdl['use']
    dependencies += libdl['deps']
endif

if libdl['use'] and host_machine.system() != 'windows' and get_option('cplugins')
    if cc.has_link_argument('-rdynamic')
        add_project_arguments('-rdynamic', language: 'c')
    endif
endif

glob_posix = {
    'name': 'glob-posix',
    'statement': '''#include <glob.h>
                    int main(int argc, char **argv) {
                        glob("filename", 0, 0, 0);
                        return 0;
                    }''',
}
glob_posix_run = cc.run(glob_posix['statement'], name: 'glob-posix check')
glob_posix += {'use': glob_posix_run.returncode() == 0}

glob = false
if glob_posix['use']
    glob = true
endif

vt_h = {
    'name': 'vt.h',
    'statement': '''#include <sys/vt.h>
                    #include <sys/ioctl.h>
                    int main(int argc, char **argv) {
                        int m;
                        ioctl(0, VT_GETMODE, &m);
                        return 0;
                    }''',
}
vt_h_run = cc.run(vt_h['statement'], name: 'vt.h check')
vt_h += {'use': vt_h_run.returncode() == 0}

consio_h = {'use': false}
if not vt_h['use']
    consio_h += {'name': 'consio.h',
               'statement': '''#include <sys/consio.h>
                               #include <sys/ioctl.h>
                               int main(int argc, char **argv) {
                                   int m;
                                   ioctl(0, VT_GETMODE, &m);
                                   return 0;
                               }''',
    }
    consio_h_run = cc.run(consio_h['statement'], name: 'consio.h check')
    consio_h += {'use': consio_h_run.returncode() == 0}
endif

glibc_thread_name = {
    'name': 'glibc-thread-name',
    'statement': '''#include <pthread.h>
                    int main(int argc, char **argv) {
                        pthread_setname_np(pthread_self(), "ducks");
                        return 0;
                    }''',
}
glibc_thread_name_run = cc.run(glibc_thread_name['statement'], dependencies: pthreads, name: 'glibc-thread-name check')
glibc_thread_name += {'use': glibc_thread_name_run.returncode() == 0}

linux_fstatfs = {
    'name': 'linux-fstatfs',
    'statement': '''#include <sys/vfs.h>
                    int main(int argc, char **argv) {
                        struct statfs fs;
                        fstatfs(0, &fs);
                        fs.f_namelen;
                        return 0;
                    }''',
}
linux_fstatfs_run = cc.run(linux_fstatfs['statement'], name: 'linux-fstatfs check')
linux_fstatfs += {'use': linux_fstatfs_run.returncode() == 0}

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
    lua = dependency(version[0], version: version[1], required: get_option('lua'))
    if lua.found()
        break
    endif
endforeach
if lua.found()
    dependencies += lua
    sources += 'player/lua.c'
endif

javascript = dependency('mujs', version: '>= 1.0.0', required: get_option('javascript'))
if javascript.found()
    dependencies += javascript
    sources += 'player/javascript.c'
endif

libbluray = dependency('libbluray', version: '>= 0.3.0', required: get_option('libbluray'))
if libbluray.found()
    dependencies += libbluray
    sources += 'stream/stream_bluray.c'
endif

dvdnav = dependency('dvdnav', version: '>= 4.2.0', required: get_option('dvdnav'))
dvdread = dependency('dvdread', version: '>= 4.1.0', required: get_option('dvdnav'))
if dvdnav.found() and dvdread.found()
    dependencies += [dvdnav, dvdread]
    sources += 'stream/stream_dvdnav.c'
endif

cdda = dependency('libcdio_paranoia', required: get_option('cdda'))
if cdda.found()
    dependencies += cdda
    sources += 'stream/stream_cdda.c'
endif

uchardet = dependency('uchardet', required: get_option('uchardet'))
if uchardet.found()
    dependencies += uchardet
endif

rubberband = dependency('rubberband', version: '>= 1.8.0', required: get_option('rubberband'))
if rubberband.found()
    dependencies += rubberband
    sources += 'audio/filter/af_rubberband.c'
endif

zimg = dependency('zimg', version: '>= 2.9', required: get_option('zimg'))
if zimg.found()
    dependencies += zimg
    sources += ['video/filter/vf_fingerprint.c',
                'video/zimg.c']
endif

lcms2 = dependency('lcms2', version: '>= 2.6', required: get_option('lcms2'))
if lcms2.found()
    dependencies += lcms2
endif

vapoursynth = dependency('vapoursynth', version: '>= 24', required: get_option('vapoursynth'))
vapoursynth_script = dependency('vapoursynth-script', version: '>= 23', required: get_option('vapoursynth'))
if vapoursynth.found() and vapoursynth_script.found()
    dependencies += vapoursynth
    sources += 'video/filter/vf_vapoursynth.c'
endif

libarchive = dependency('libarchive', version: '>= 3.4.0', required: get_option('libarchive'))
if libarchive.found()
    dependencies += libarchive
    sources += ['demux/demux_libarchive.c',
                'stream/stream_libarchive.c']
endif

dvbin = license == 'gpl' and get_option('dvbin')
if dvbin
    sources += ['stream/dvb_tune.c',
                'stream/stream_dvb.c']
endif

sdl2 = dependency('sdl2', required: get_option('sdl2'))
if sdl2.found()
    dependencies += sdl2
endif

if sdl2.found() and get_option('sdl2-gamepad')
    sources += 'input/sdl_gamepad.c'
endif

libavdevice = dependency('libavdevice', version: '>= 57.0.0', required: get_option('libavdevice'))
if libavdevice.found()
    dependencies += libavdevice
endif


# audio output dependencies
if sdl2.found() and get_option('sdl2-audio')
    sources += 'audio/out/ao_sdl.c'
endif

pulse = dependency('libpulse', version: '>= 1.0', required: get_option('pulse'))
if pulse.found()
    dependencies += pulse
    sources += 'audio/out/ao_pulse.c'
endif

jack = dependency('jack', required: get_option('jack'))
if jack.found() and license == 'gpl'
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
    'statement': '''#include <SLES/OpenSLES.h
                    int main(int argc, char **argv) {
                        slCreateEngine;
                        return 0;
                    }''',
}
opensles_run = cc.run(opensles['statement'], dependencies: opensles['deps'], name: 'opensles check')
opensles += {'use': opensles_run.returncode() == 0}
 if opensles['use']
     dependencies += opensles['deps']
     sources += 'audio/out/ao_opensles.c'
 endif
                    
alsa = dependency('alsa', version: '>= 1.0.18', required: get_option('alsa'))
if alsa.found()
    dependencies += alsa
    sources += 'audio/out/ao_alsa.c'
endif

# if wasapi
#     sources += ['audio/out/ao_wasapi.c',
#                 'audio/out/ao_wasapi_changenotify.c',
#                 'audio/out/ao_wasapi_utils.c']
# endif

# video output dependencies
caca = dependency('caca', version: '>= 0.99.beta18', required: get_option('caca'))
if caca.found()
    dependencies += caca
    sources += 'video/out/vo_caca.c'
endif

jpeg = cc.find_library('jpeg', required: get_option('jpeg'))
if jpeg.found()
    dependencies += jpeg
endif

shaderc_shared = {
    'name': 'shaderc-shared',
    'deps': dependency('shaderc', required: get_option('shaderc')),
    'header': cc.check_header('shaderc/shaderc.h') and not get_option('static-build'),
}
shaderc_shared += {'use': shaderc_shared['deps'].found() and shaderc_shared['header']}
if shaderc_shared['use']
    dependencies += shaderc_shared['deps']
endif

shaderc_static = {
    #TODO
    'name': 'shaderc-static',
    'use': false,
}

shaderc = {
    'name': 'shaderc',
    'use': shaderc_shared['use'] or shaderc_static['use']
}
if shaderc['use']
    sources += 'video/out/gpu/spirv_shaderc.c'
endif

spirv_cross_shared = {
    'name': 'spirv-cross-shared',
    'deps': dependency('spirv-cross-c-shared', required: get_option('spirv-cross')),
}
spirv_cross_shared += {'use': spirv_cross_shared['deps'].found() and not get_option('static-build')}

spirv_cross_static = {
    'name': 'spirv-cross-static',
    'deps': dependency('spirv-cross', required: get_option('spirv-cross')),
}
spirv_cross_static += {'use': spirv_cross_static['deps'].found() and not spirv_cross_shared['use']}

spirv_cross = {
    'name': 'spirv-cross',
    'use': spirv_cross_shared['use'] or spirv_cross_static['use']
}

if sdl2.found() and get_option('sdl2-video')
    sources += 'video/out/vo_sdl.c'
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

wayland = {
    'name': 'wayland',
    'deps': [dependency('wayland-client', version: '>= 1.15.0', required: get_option('wayland')),
             dependency('wayland-cursor', version: '>= 1.15.0', required: get_option('wayland')),
             dependency('wayland-protocols', required: get_option('wayland')),
             dependency('xkbcommon', version: '>= 0.3.0', required: get_option('wayland'))],
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

if wayland_deps and wayland['scanner'].found()
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
    'statement': '''#define _GNU_SOURCE
                    #include <sys/mman.h>
                    int main(int argc, char **argv) {
                        memfd_create("mpv", MFD_CLOEXEC | MFD_ALLOW_SEALING);
                        return 0;
                    }''',
    'use': false
}
if wayland['use']
    memfd_create_run = cc.run(memfd_create['statement'], name: 'memfd_create check')
    memfd_create += {'use': memfd_create_run.returncode() == 0}
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

if get_option('gl')
    sources += ['video/out/opengl/common.c',
                'video/out/opengl/context.c',
                'video/out/opengl/formats.c',
                'video/out/opengl/libmpv_gl.c',
                'video/out/opengl/ra_gl.c',
                'video/out/opengl/utils.c']
endif

gl_x11 = {
    'name': 'gl_x11',
    'deps': cc.find_library('GL', required: get_option('gl-x11')),
    'use': false,
}
if x11['use']
    if gl_x11['deps'].found() and get_option('gl')
       gl_x11 += {'use': true}
    endif
endif
if gl_x11['use']
    dependencies += gl_x11['deps']
    sources += 'video/out/opengl/context_glx.c'
endif
         
egl = {
    'name': 'egl',
    'deps': dependency('egl', version: '> 1.4.0', required: get_option('egl')),
    'use' : false
}
if egl['deps'].found() and get_option('gl')
    dependencies += egl['deps']
    egl += {'use': true}
endif

egl_helpers = {
    'name': 'egl_helpers',
    'use': egl['use'] # or rpi or egl_angle_win32 or egl_android
}
if egl_helpers['use']
    sources += 'video/out/opengl/egl_helpers.c'
endif

if egl['use'] and egl_helpers['use']
    sources += 'video/filter/vf_gpu.c'
endif

egl_drm = {
    'name': 'egl-drm',
    'use': drm['use'] and gbm['use'] and egl['use'] and get_option('egl-drm'),
}
if egl_drm['use']
    sources += 'video/out/opengl/context_drm_egl.c'
endif

gl_wayland = {
    'name': 'gl-wayland',
    'deps': dependency('wayland-egl', version: '>= 9.0.0', required: get_option('gl-wayland')),
}
gl_wayland += {'use': wayland['use'] and egl['use'] and gl_wayland['deps'].found()}
if gl_wayland['use']
    dependencies += gl_wayland['deps']
    sources += 'video/out/opengl/context_wayland.c'
endif

egl_x11 = {
    'name': 'egl-x11',
    'use': x11['use'] and egl['use'] and get_option('egl-x11'),
}
if egl_x11['use']
    sources += 'video/out/opengl/context_x11egl.c'
endif

if egl_x11['use'] or gl_x11['use']
    sources += 'video/out/opengl/oml_sync.c'
endif

gl = {
    'name': 'gl',
    'use': gl_x11['use'] or egl_x11['use'] or egl_drm['use'] or gl_wayland['use'] or get_option('plain-gl')
}

libplacebo = dependency('libplacebo', version: '>= 1.18.0', required: get_option('libplacebo'))
if libplacebo.found()
    dependencies += libplacebo
    sources += ['video/out/placebo/ra_pl.c',
                'video/out/placebo/utils.c']
endif

vulkan = dependency('vulkan', required: get_option('vulkan'))
if vulkan.found() and libplacebo.found()
    dependencies += vulkan
    sources += ['video/out/vulkan/context.c',
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
if vaapi_x11['use'] and license == 'gpl'
    dependencies += vaapi_x11['deps']
    sources += 'video/out/vo_vaapi.c'
endif

vaapi_wayland = {
    'name': 'vaapi-wayland',
    'deps': dependency('libva-wayland', version: '>= 1.1.0', required: get_option('vaapi-wayland')),
}
vaapi_wayland += {'use': vaapi['use'] and gl_wayland['use'] and vaapi_wayland['deps'].found()}

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

if ffnvcodec.found() and get_option('cuda-interop')
    sources += 'video/out/hwdec/hwdec_cuda.c'
endif

if ffnvcodec.found() and get_option('cuda-interop') and gl['use']
    sources += 'video/out/hwdec/hwdec_cuda_gl.c'
endif

if ffnvcodec.found() and vulkan.found() and libplacebo.found() and get_option('cuda-interop')
    sources += 'video/out/hwdec/hwdec_cuda_vk.c'
endif

if get_option('tests')
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

if get_option('tests') and zimg.found()
    sources += ['test/repack.c',
                'test/scale_zimg.c']
endif


# neccesary to not break the waf build
conf_data = configuration_data()
conf_data.set_quoted('DEFAULT_DVD_DEVICE', dvd_device)
conf_data.set_quoted('DEFAULT_CDROM_DEVICE', cd_device)
if license == 'gpl'
    conf_data.set10('HAVE_GPL', 1)
    conf_data.set10('HAVE_LGPL', 0)
else
    conf_data.set10('HAVE_GPL', 0)
    conf_data.set10('HAVE_LGPL', 1)
endif
conf_data.set10('HAVE_CPLAYER', get_option('cplayer'))
conf_data.set10('HAVE_LIBMPV_SHARED', get_option('libmpv-shared'))
conf_data.set10('HAVE_LIBMPV_STATIC', 0)
conf_data.set10('HAVE_STATIC_BUILD', get_option('static-build'))
conf_data.set10('HAVE_BUILD_DATE', get_option('build-date'))
conf_data.set10('HAVE_OPTIMIZE', get_option('optimization'))
conf_data.set10('HAVE_DEBUG_BUILD', get_option('debug'))
conf_data.set10('HAVE_TESTS', get_option('tests'))
conf_data.set10('HAVE_TA_LEAK_REPORT', get_option('ta-leak-report'))
conf_data.set10('HAVE_HTML_BUILD', 0)
conf_data.set10('HAVE_PDF_BUILD', 0)
conf_data.set10('HAVE_LIBDL', libdl['use'])
conf_data.set10('HAVE_CPLUGINS', get_option('cplugins'))
conf_data.set10('HAVE_ASM', get_option('asm'))
conf_data.set10('HAVE_CLANG_DATABASE', get_option('clang-database'))
conf_data.set10('HAVE_SWIFT_STATIC', 0)
conf_data.set10('HAVE_NOEXECSTACK', noexecstack)
conf_data.set10('HAVE_LIBM', libm.found())
conf_data.set10('HAVE_MINGW', 0)
conf_data.set10('HAVE_POSIX', posix)
conf_data.set10('HAVE_ANDROID', 0)
conf_data.set10('HAVE_TVOS', 0)
conf_data.set10('HAVE_EGL_ANDROID', 0)
conf_data.set10('HAVE_POSIX_OR_MINGW', posix or mingw)
conf_data.set10('HAVE_SWIFT', 0)
conf_data.set10('HAVE_UWP', 0)
conf_data.set10('HAVE_WIN32_DESKTOP', 0)
conf_data.set10('HAVE_WIN32_INTERNAL_PTHREADS', 0)
conf_data.set10('HAVE_PTHREADS', pthreads.found())
conf_data.set10('HAVE_PTHREADS_DEBUG', 0)
conf_data.set10('HAVE_STDATOMIC', stdatomic['use'])
conf_data.set10('HAVE_LIBRT', librt.found())
conf_data.set10('HAVE_ICONV', iconv.found())
conf_data.set10('HAVE_DOS_PATHS', 0)
conf_data.set10('HAVE_GLOB_POSIX', glob_posix['use'])
conf_data.set10('HAVE_GLOB_WIN32', 0)
conf_data.set10('HAVE_GLOB', glob)
conf_data.set10('HAVE_VT_H', vt_h['use'])
conf_data.set10('HAVE_CONSIO_H', consio_h['use'])
conf_data.set10('HAVE_GBM_H', gbm['header'])
conf_data.set10('HAVE_GLIBC_THREAD_NAME', glibc_thread_name['use'])
conf_data.set10('HAVE_OSX_THREAD_NAME', 0)
conf_data.set10('HAVE_BSD_THREAD_NAME', 0)
conf_data.set10('HAVE_BSD_FSTATFS', 0)
conf_data.set10('HAVE_LINUX_FSTATFS', linux_fstatfs['use'])
conf_data.set10('HAVE_LUA', lua.found())
conf_data.set10('HAVE_JAVASCRIPT', javascript.found())
conf_data.set10('HAVE_LIBASS', libass.found())
conf_data.set10('HAVE_ZLIB', zlib.found())
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
conf_data.set10('HAVE_SDL2_GAMEPAD', sdl2.found() and get_option('sdl2-gamepad'))
conf_data.set10('HAVE_FFPMEG', 1)
conf_data.set10('HAVE_LIBAVDEVICE', libavdevice.found())
conf_data.set10('HAVE_FFMPEG_STRICT_ABI', get_option('ffmpeg-strict-abi'))
conf_data.set10('HAVE_SDL2_AUDIO', sdl2.found() and get_option('sdl2-audio'))
conf_data.set10('HAVE_PULSE', pulse.found())
conf_data.set10('HAVE_JACK', jack.found())
conf_data.set10('HAVE_OPENAL', openal.found())
conf_data.set10('HAVE_OPENSLES', opensles['use'])
conf_data.set10('HAVE_ALSA', alsa.found())
conf_data.set10('HAVE_COREAUDIO', 0)
conf_data.set10('HAVE_AUDIOUNIT', 0)
conf_data.set10('HAVE_WASAPI', 0)
conf_data.set10('HAVE_SDL2_VIDEO', sdl2.found() and get_option('sdl2-video'))
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
conf_data.set10('HAVE_GL_WAYLAND', egl['use'] and gl_wayland['use'])
conf_data.set10('HAVE_GL_WIN32', 0)
conf_data.set10('HAVE_GL_DXINTEROP', 0)
conf_data.set10('HAVE_EGL_ANGLE', 0)
conf_data.set10('HAVE_EGL_ANGLE_LIB', 0)
conf_data.set10('HAVE_EGL_ANGLE_WIN32', 0)
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
conf_data.set10('HAVE_PLAIN_GL', get_option('plain-gl'))
conf_data.set10('HAVE_GL', gl['use'])
conf_data.set10('HAVE_LIBPLACEBO', libplacebo.found())
conf_data.set10('HAVE_VULKAN', libplacebo.found() and vulkan.found())
conf_data.set10('HAVE_VAAPI_VULKAN', vaapi_vulkan['use'])
conf_data.set10('HAVE_EGL_HELPERS', egl_helpers['use'])
conf_data.set10('HAVE_VIDEOTOOBOX_HWACCEL', 0)
conf_data.set10('HAVE_VIDEOTOOLBOX_GL', 0)
conf_data.set10('HAVE_D3D_HWACCEL', 0)
conf_data.set10('HAVE_D3D9_HWACCEL', 0)
conf_data.set10('HAVE_GL_DXINTEROP_D3D9', 0)
conf_data.set10('HAVE_FFNVCODEC', ffnvcodec.found())
conf_data.set10('HAVE_CUDA_HWACCEL', ffnvcodec.found() and get_option('cuda-interop'))
conf_data.set10('HAVE_CUDA_INTEROP', ffnvcodec.found() and gl['use'] and get_option('cuda-interop'))
conf_data.set10('HAVE_RPI_MMAL', 0)
conf_data.set10('HAVE_WIN32_EXECUTABLE', 0)
conf_data.set10('HAVE_MACOS_TOUCHBAR', 0)
conf_data.set10('HAVE_MACOS_10_11_FEATURES', 0)
conf_data.set10('HAVE_MACOS_10_12_2_FEATURES', 0)
conf_data.set10('HAVE_MACOS_10_14_FEATURES', 0)
conf_data.set10('HAVE_MACOS_MEDIA_PLAYER', 0)
conf_data.set10('HAVE_MACOS_COCOA_CB', 0)
conf_data.set('CONFIGURATION', '"meson configure"')
conf_data.set_quoted('MPV_CONFDIR', '/' + get_option('sysconfdir') / 'mpv')
conf_data.set('FULLCONFIG', '"dummy"')
configure_file(output : 'config.h',
               configuration : conf_data)

    #'video/out/vulkan/context_android.c',  'vulkan && android',
    #'video/out/vulkan/context_win.c',      'vulkan && win32-desktop',


    #'video/out/opengl/context_dxinterop.c','gl-dxinterop',

    #'video/d3d.c',                         'd3d-hwaccel',
    #'video/filter/vf_d3d11vpp.c',          'd3d-hwaccel',

    #'video/out/android_common.c',          'android',
    #'video/out/cocoa/events_view.m',       'cocoa',
    #'video/out/cocoa/video_view.m',        'cocoa',
    #'video/out/cocoa/window.m',            'cocoa',
    #'video/out/cocoa_common.m',            'cocoa',
    #'video/out/d3d11/context.c',           'd3d11',
    #'video/out/d3d11/hwdec_d3d11va.c',     'd3d11 && d3d-hwaccel',
    #'video/out/d3d11/hwdec_dxva2dxgi.c',   'd3d11 && d3d9-hwaccel',
    #'video/out/d3d11/ra_d3d11.c',          'd3d11',
    #'video/out/gpu/d3d11_helpers.c',       'd3d11 || egl-angle-win32',
    #'video/out/opengl/context_win.c',      'gl-win32',
    #'video/out/opengl/angle_dynamic.c',    'egl-angle',
    #'video/out/opengl/context_android.c',  'egl-android',
    #'video/out/opengl/context_angle.c',    'egl-angle-win32',
    #'video/out/opengl/context_cocoa.c',    'gl-cocoa',
    #'video/out/opengl/context_rpi.c',      'rpi',
    #'video/out/opengl/hwdec_d3d11egl.c',   'd3d-hwaccel && egl-angle',
    #'video/out/opengl/hwdec_dxva2egl.c',   'd3d9-hwaccel && egl-angle',
    #'video/out/opengl/hwdec_dxva2gldx.c',  'gl-dxinterop-d3d9',
    #'video/out/opengl/hwdec_ios.m',        'ios-gl',
    #'video/out/opengl/hwdec_osx.c',        'videotoolbox-gl',
    #'video/out/opengl/hwdec_rpi.c',        'rpi-mmal',
    #'video/out/vo_direct3d.c',             'direct3d',
    #'video/out/vo_mediacodec_embed.c',     'android',
    #'video/out/vo_rpi.c',                  'rpi-mmal',
    #'video/out/w32_common.c',              'win32-desktop',
    #'video/out/win32/displayconfig.c',     'win32-desktop',
    #'video/out/win32/droptarget.c',        'win32-desktop',


    #'osdep/glob-win.c',                    'glob-win32',
    #'osdep/macosx_application.m',          'cocoa',
    #'osdep/macosx_events.m',               'cocoa',
    #'osdep/macosx_menubar.m',              'cocoa',
    #'osdep/macosx_touchbar.m',             'macos-touchbar',
    #'osdep/mpv.rc',                        'win32-executable',
    #'osdep/path-macosx.m',                 'cocoa',
    #'osdep/path-uwp.c',                    'uwp',
    #'osdep/path-win.c',                    'win32-desktop',
    #'osdep/w32_keyboard.c',                'os-cygwin',
    #'osdep/w32_keyboard.c',                'os-win32',
    #'osdep/win32/pthread.c',               'win32-internal-pthreads',
    #'osdep/windows_utils.c',               'os-cygwin',
    #'osdep/windows_utils.c',               'os-win32',

if get_option('cplayer')
    datadir = get_option('datadir')
    confdir = get_option('sysconfdir')

    conf_files = ['etc/mpv.conf', 'etc/input.conf',
                  'etc/mplayer-input.conf', 'etc/restore-old-bindings.conf']
    install_data(conf_files, install_dir: datadir + '/doc/mpv')

    zsh = find_program('zsh', required: false)
    if zsh.found()
        zsh_install_dir = datadir + '/zsh/site-functions'
        install_data('etc/_mpv.zsh', install_dir: zsh_install_dir, rename: '_mpv')
    endif

    bash = find_program('bash', required: false)
    if bash.found()
        bash_install_dir = datadir + '/bash-completion/completions'
        install_data('etc/mpv.bash-completion', install_dir: bash_install_dir, rename: 'mpv')
    endif

    install_data('etc/mpv.desktop', install_dir: datadir + '/applications')
    install_data('etc/encoding-profiles.conf', install_dir: confdir)

    foreach size: ['16x16', '32x32', '64x64', '128x128']
        icon_dir = datadir + '/icons/hicolor/' + size + '/apps'
        install_data('etc/mpv-icon-8bit-' + size + '.png', install_dir: icon_dir, rename: 'mpv.png')
    endforeach

    install_data('etc/mpv-gradient.svg', install_dir: datadir + '/icons/hicolor/scalable/apps', rename: 'mpv.svg')
    install_data('etc/mpv-symbolic.svg', install_dir: datadir + '/icons/hicolor/symbolc/apps')
endif

if get_option('libmpv-shared')
    client_h_path = meson.source_root() + '/libmpv/client.h'
    major = '''grep "#define MPV_CLIENT_API_VERSION" ''' + client_h_path + ''' | sed -e "s/.*(\(.*\),.*/\1/"'''
    minor = '''grep "#define MPV_CLIENT_API_VERSION" ''' + client_h_path + ''' | sed -e "s/.* \(.*\)).*/\1/"'''
    major_ver = run_command('sh', '-c', major).stdout().strip()
    minor_ver = run_command('sh', '-c', minor).stdout().strip()
    version = major_ver + '.' + minor_ver

    library('mpv', sources, dependencies: dependencies, version: version, install: true)

    includedir = get_option('includedir')
    libdir = get_option('libdir')

    headers = ['libmpv/client.h', 'libmpv/opengl_cb.h', 'libmpv/render.h',
               'libmpv/render_gl.h', 'libmpv/stream_cb.h']
    install_data(headers, install_dir: includedir)
    install_data('libmpv/mpv.pc.in', install_dir: libdir + '/pkgconfig', rename: 'mpv.pc')
endif

executable('mpv', sources, dependencies: dependencies, install: true)
