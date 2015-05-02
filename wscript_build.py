import re
import os

def _add_rst_manual_dependencies(ctx):
    manpage_sources_basenames = """
        options.rst ao.rst vo.rst af.rst vf.rst encode.rst
        input.rst osc.rst lua.rst ipc.rst changes.rst""".split()

    manpage_sources = ['DOCS/man/'+x for x in manpage_sources_basenames]

    for manpage_source in manpage_sources:
        ctx.add_manual_dependency(
            ctx.path.find_node('DOCS/man/mpv.rst'),
            ctx.path.find_node(manpage_source))

def _build_man(ctx):
    ctx(
        name         = 'rst2man',
        target       = 'DOCS/man/mpv.1',
        source       = 'DOCS/man/mpv.rst',
        rule         = '${RST2MAN} ${SRC} ${TGT}',
        install_path = ctx.env.MANDIR + '/man1')

    _add_rst_manual_dependencies(ctx)

def _build_pdf(ctx):
    ctx(
        name         = 'rst2pdf',
        target       = 'DOCS/man/mpv.pdf',
        source       = 'DOCS/man/mpv.rst',
        rule         = '${RST2PDF} -c --repeat-table-rows ${SRC} -o ${TGT}',
        install_path = ctx.env.DOCDIR)

    _add_rst_manual_dependencies(ctx)

def _all_includes(ctx):
    return [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
            ctx.dependencies_includes()

def build(ctx):
    ctx.load('waf_customizations')
    ctx.load('generators.sources')

    ctx.file2string(
        source = "TOOLS/osxbundle/mpv.app/Contents/Resources/icon.icns",
        target = "osdep/macosx_icon.inc")

    ctx.file2string(
        source = "video/out/x11_icon.bin",
        target = "video/out/x11_icon.inc")

    ctx.file2string(
        source = "etc/input.conf",
        target = "input/input_conf.h")

    ctx.file2string(
        source = "sub/osd_font.otf",
        target = "sub/osd_font.h")

    lua_files = ["defaults.lua", "assdraw.lua", "options.lua", "osc.lua",
                 "ytdl_hook.lua"]
    for fn in lua_files:
        fn = "player/lua/" + fn
        ctx.file2string(source = fn, target = os.path.splitext(fn)[0] + ".inc")

    ctx.matroska_header(
        source = "demux/ebml.c demux/demux_mkv.c",
        target = "ebml_types.h")

    ctx.matroska_definitions(
        source = "demux/ebml.c",
        target = "ebml_defs.c")

    if ctx.env.DEST_OS == 'win32':
        main_fn_c = 'osdep/main-fn-win.c'
    elif ctx.dependency_satisfied('cocoa'):
        main_fn_c = 'osdep/main-fn-cocoa.c'
    else:
        main_fn_c = 'osdep/main-fn-unix.c'

    getch2_c = {
        'win32':  'osdep/terminal-win.c',
    }.get(ctx.env.DEST_OS, "osdep/terminal-unix.c")

    timer_c = {
        'win32':  'osdep/timer-win2.c',
        'darwin': 'osdep/timer-darwin.c',
    }.get(ctx.env.DEST_OS, "osdep/timer-linux.c")

    sources = [
        ## Audio
        ( "audio/audio.c" ),
        ( "audio/audio_buffer.c" ),
        ( "audio/chmap.c" ),
        ( "audio/chmap_sel.c" ),
        ( "audio/fmt-conversion.c" ),
        ( "audio/format.c" ),
        ( "audio/mixer.c" ),
        ( "audio/decode/ad_lavc.c" ),
        ( "audio/decode/ad_spdif.c" ),
        ( "audio/decode/dec_audio.c" ),
        ( "audio/filter/af.c" ),
        ( "audio/filter/af_bs2b.c",              "libbs2b" ),
        ( "audio/filter/af_center.c" ),
        ( "audio/filter/af_channels.c" ),
        ( "audio/filter/af_convert24.c" ),
        ( "audio/filter/af_convertsignendian.c" ),
        ( "audio/filter/af_delay.c" ),
        ( "audio/filter/af_drc.c" ),
        ( "audio/filter/af_dummy.c" ),
        ( "audio/filter/af_equalizer.c" ),
        ( "audio/filter/af_export.c" ),
        ( "audio/filter/af_extrastereo.c" ),
        ( "audio/filter/af_format.c" ),
        ( "audio/filter/af_hrtf.c" ),
        ( "audio/filter/af_karaoke.c" ),
        ( "audio/filter/af_ladspa.c",            "ladspa" ),
        ( "audio/filter/af_lavcac3enc.c" ),
        ( "audio/filter/af_lavfi.c",             "libavfilter" ),
        ( "audio/filter/af_lavrresample.c" ),
        ( "audio/filter/af_pan.c" ),
        ( "audio/filter/af_rubberband.c",        "rubberband" ),
        ( "audio/filter/af_scaletempo.c" ),
        ( "audio/filter/af_sinesuppress.c" ),
        ( "audio/filter/af_sub.c" ),
        ( "audio/filter/af_surround.c" ),
        ( "audio/filter/af_sweep.c" ),
        ( "audio/filter/af_volume.c" ),
        ( "audio/filter/filter.c" ),
        ( "audio/filter/tools.c" ),
        ( "audio/filter/window.c" ),
        ( "audio/out/ao.c" ),
        ( "audio/out/ao_alsa.c",                 "alsa" ),
        ( "audio/out/ao_coreaudio.c",            "coreaudio" ),
        ( "audio/out/ao_coreaudio_exclusive.c",  "coreaudio" ),
        ( "audio/out/ao_coreaudio_properties.c", "coreaudio" ),
        ( "audio/out/ao_coreaudio_utils.c",      "coreaudio" ),
        ( "audio/out/ao_dsound.c",               "dsound" ),
        ( "audio/out/ao_jack.c",                 "jack" ),
        ( "audio/out/ao_lavc.c",                 "encoding" ),
        ( "audio/out/ao_null.c" ),
        ( "audio/out/ao_openal.c",               "openal" ),
        ( "audio/out/ao_oss.c",                  "oss-audio" ),
        ( "audio/out/ao_pcm.c" ),
        ( "audio/out/ao_pulse.c",                "pulse" ),
        ( "audio/out/ao_rsound.c",               "rsound" ),
        ( "audio/out/ao_sdl.c",                  "sdl1" ),
        ( "audio/out/ao_sdl.c",                  "sdl2" ),
        ( "audio/out/ao_sndio.c",                "sndio" ),
        ( "audio/out/ao_wasapi.c",               "wasapi" ),
        ( "audio/out/ao_wasapi_utils.c",         "wasapi" ),
        ( "audio/out/ao_wasapi_changenotify.c",  "wasapi" ),
        ( "audio/out/pull.c" ),
        ( "audio/out/push.c" ),

        ## Core
        ( "common/av_common.c" ),
        ( "common/av_log.c" ),
        ( "common/codecs.c" ),
        ( "common/encode_lavc.c",                "encoding" ),
        ( "common/common.c" ),
        ( "common/tags.c" ),
        ( "common/msg.c" ),
        ( "common/playlist.c" ),
        ( "common/version.c" ),

        ## Demuxers
        ( "demux/codec_tags.c" ),
        ( "demux/demux.c" ),
        ( "demux/demux_cue.c" ),
        ( "demux/demux_disc.c" ),
        ( "demux/demux_edl.c" ),
        ( "demux/demux_lavf.c" ),
        ( "demux/demux_libass.c",                "libass"),
        ( "demux/demux_mf.c" ),
        ( "demux/demux_mkv.c" ),
        ( "demux/demux_mkv_timeline.c" ),
        ( "demux/demux_playlist.c" ),
        ( "demux/demux_raw.c" ),
        ( "demux/demux_rar.c" ),
        ( "demux/demux_subreader.c" ),
        ( "demux/demux_tv.c",                    "tv" ),
        ( "demux/ebml.c" ),
        ( "demux/packet.c" ),
        ( "demux/timeline.c" ),

        ## Input
        ( "input/cmd_list.c" ),
        ( "input/cmd_parse.c" ),
        ( "input/event.c" ),
        ( "input/input.c" ),
        ( "input/ipc.c",                         "!mingw" ),
        ( "input/keycodes.c" ),
        ( "input/pipe-win32.c",                  "waio" ),

        ## Misc
        ( "misc/bstr.c" ),
        ( "misc/charset_conv.c" ),
        ( "misc/dispatch.c" ),
        ( "misc/json.c" ),
        ( "misc/ring.c" ),
        ( "misc/rendezvous.c" ),

        ## Options
        ( "options/m_config.c" ),
        ( "options/m_option.c" ),
        ( "options/m_property.c" ),
        ( "options/options.c" ),
        ( "options/parse_commandline.c" ),
        ( "options/parse_configfile.c" ),
        ( "options/path.c" ),

        ## Player
        ( "player/audio.c" ),
        ( "player/client.c" ),
        ( "player/command.c" ),
        ( "player/configfiles.c" ),
        ( "player/discnav.c" ),
        ( "player/loadfile.c" ),
        ( "player/main.c" ),
        ( "player/misc.c" ),
        ( "player/lua.c",                        "lua" ),
        ( "player/osd.c" ),
        ( "player/playloop.c" ),
        ( "player/screenshot.c" ),
        ( "player/scripting.c" ),
        ( "player/sub.c" ),
        ( "player/video.c" ),

        ## Streams
        ( "stream/ai_alsa1x.c",                  "alsa" ),
        ( "stream/ai_oss.c",                     "oss-audio" ),
        ( "stream/ai_sndio.c",                   "sndio" ),
        ( "stream/audio_in.c",                   "audio-input" ),
        ( "stream/cache.c" ),
        ( "stream/cache_file.c" ),
        ( "stream/cookies.c" ),
        ( "stream/dvb_tune.c",                   "dvbin" ),
        ( "stream/frequencies.c",                "tv" ),
        ( "stream/rar.c" ),
        ( "stream/stream.c" ),
        ( "stream/stream_avdevice.c" ),
        ( "stream/stream_bluray.c",              "libbluray" ),
        ( "stream/stream_cdda.c",                "cdda" ),
        ( "stream/stream_dvb.c",                 "dvbin" ),
        ( "stream/stream_dvd.c",                 "dvdread" ),
        ( "stream/stream_dvd_common.c",          "dvdread" ),
        ( "stream/stream_dvdnav.c",              "dvdnav" ),
        ( "stream/stream_edl.c" ),
        ( "stream/stream_file.c" ),
        ( "stream/stream_lavf.c" ),
        ( "stream/stream_memory.c" ),
        ( "stream/stream_mf.c" ),
        ( "stream/stream_null.c" ),
        ( "stream/stream_pvr.c",                 "pvr" ),
        ( "stream/stream_rar.c" ),
        ( "stream/stream_smb.c",                 "libsmbclient" ),
        ( "stream/stream_tv.c",                  "tv" ),
        ( "stream/tv.c",                         "tv" ),
        ( "stream/tvi_dummy.c",                  "tv" ),
        ( "stream/tvi_v4l2.c",                   "tv-v4l2"),

        ## Subtitles
        ( "sub/ass_mp.c",                        "libass"),
        ( "sub/dec_sub.c" ),
        ( "sub/draw_bmp.c" ),
        ( "sub/find_subfiles.c" ),
        ( "sub/img_convert.c" ),
        ( "sub/osd.c" ),
        ( "sub/osd_dummy.c",                     "dummy-osd" ),
        ( "sub/osd_libass.c",                    "libass-osd" ),
        ( "sub/sd_ass.c",                        "libass" ),
        ( "sub/sd_lavc.c" ),
        ( "sub/sd_lavc_conv.c" ),
        ( "sub/sd_lavf_srt.c" ),
        ( "sub/sd_microdvd.c" ),
        ( "sub/sd_movtext.c" ),
        ( "sub/sd_srt.c" ),

        ## Video
        ( "video/csputils.c" ),
        ( "video/fmt-conversion.c" ),
        ( "video/image_writer.c" ),
        ( "video/img_format.c" ),
        ( "video/mp_image.c" ),
        ( "video/mp_image_pool.c" ),
        ( "video/sws_utils.c" ),
        ( "video/vaapi.c",                       "vaapi" ),
        ( "video/vdpau.c",                       "vdpau" ),
        ( "video/vdpau_mixer.c",                 "vdpau" ),
        ( "video/decode/dec_video.c"),
        ( "video/decode/dxva2.c",                "dxva2-hwaccel" ),
        ( "video/decode/rpi.c",                  "rpi" ),
        ( "video/decode/vaapi.c",                "vaapi-hwaccel" ),
        ( "video/decode/vd_lavc.c" ),
        ( "video/decode/vda.c",                  "vda-hwaccel" ),
        ( "video/decode/vdpau.c",                "vdpau-hwaccel" ),
        ( "video/filter/vf.c" ),
        ( "video/filter/vf_buffer.c" ),
        ( "video/filter/vf_crop.c" ),
        ( "video/filter/vf_delogo.c",            "libavfilter"),
        ( "video/filter/vf_dlopen.c",            "dlopen" ),
        ( "video/filter/vf_dsize.c" ),
        ( "video/filter/vf_eq.c" ),
        ( "video/filter/vf_expand.c" ),
        ( "video/filter/vf_flip.c" ),
        ( "video/filter/vf_format.c" ),
        ( "video/filter/vf_gradfun.c",           "libavfilter"),
        ( "video/filter/vf_hqdn3d.c",            "libavfilter"),
        ( "video/filter/vf_lavfi.c",             "libavfilter"),
        ( "video/filter/vf_mirror.c",            "libavfilter"),
        ( "video/filter/vf_noformat.c" ),
        ( "video/filter/vf_noise.c",             "libavfilter"),
        ( "video/filter/vf_pullup.c",            "libavfilter"),
        ( "video/filter/vf_rotate.c",            "libavfilter"),
        ( "video/filter/vf_scale.c" ),
        ( "video/filter/vf_stereo3d.c" ),
        ( "video/filter/vf_sub.c" ),
        ( "video/filter/vf_unsharp.c",           "libavfilter"),
        ( "video/filter/vf_vapoursynth.c",       "vapoursynth-core" ),
        ( "video/filter/vf_vavpp.c",             "vaapi-vpp"),
        ( "video/filter/vf_vdpaupp.c",           "vdpau" ),
        ( "video/filter/vf_yadif.c",             "libavfilter"),
        ( "video/out/aspect.c" ),
        ( "video/out/bitmap_packer.c" ),
        ( "video/out/cocoa/video_view.m",        "cocoa" ),
        ( "video/out/cocoa/events_view.m",       "cocoa" ),
        ( "video/out/cocoa/window.m",            "cocoa" ),
        ( "video/out/cocoa_common.m",            "cocoa" ),
        ( "video/out/dither.c" ),
        ( "video/out/filter_kernels.c" ),
        ( "video/out/gl_cocoa.c",                "gl-cocoa" ),
        ( "video/out/gl_common.c",               "gl" ),
        ( "video/out/gl_rpi.c",                  "rpi-gles" ),
        ( "video/out/gl_hwdec.c",                "gl" ),
        ( "video/out/gl_hwdec_vaglx.c",          "vaapi-glx" ),
        ( "video/out/gl_hwdec_vda.c",            "vda-gl" ),
        ( "video/out/gl_hwdec_vdpau.c",          "vdpau-gl-x11" ),
        ( "video/out/gl_lcms.c",                 "gl" ),
        ( "video/out/gl_osd.c",                  "gl" ),
        ( "video/out/gl_utils.c",                "gl" ),
        ( "video/out/gl_video.c",                "gl" ),
        ( "video/out/gl_w32.c",                  "gl-win32" ),
        ( "video/out/gl_wayland.c",              "gl-wayland" ),
        ( "video/out/gl_x11.c",                  "gl-x11" ),
        ( "video/out/gl_x11egl.c",               "egl-x11" ),
        ( "video/out/vo.c" ),
        ( "video/out/vo_caca.c",                 "caca" ),
        ( "video/out/vo_drm.c",                  "drm" ),
        ( "video/out/vo_direct3d.c",             "direct3d" ),
        ( "video/out/vo_image.c" ),
        ( "video/out/vo_lavc.c",                 "encoding" ),
        ( "video/out/vo_rpi.c",                  "rpi" ),
        ( "video/out/vo_null.c" ),
        ( "video/out/vo_opengl.c",               "gl" ),
        ( "video/out/vo_opengl_cb.c",            "gl" ),
        ( "video/out/vo_sdl.c",                  "sdl2" ),
        ( "video/out/vo_vaapi.c",                "vaapi" ),
        ( "video/out/vo_vdpau.c",                "vdpau" ),
        ( "video/out/vo_wayland.c",              "wayland" ),
        ( "video/out/vo_x11.c" ,                 "x11" ),
        ( "video/out/vo_xv.c",                   "xv" ),
        ( "video/out/w32_common.c",              "win32" ),
        ( "video/out/wayland_common.c",          "wayland" ),
        ( "video/out/wayland/buffer.c",          "wayland" ),
        ( "video/out/wayland/memfile.c",         "wayland" ),
        ( "video/out/win_state.c"),
        ( "video/out/x11_common.c",              "x11" ),
        ( "video/out/drm_common.c",              "drm" ),

        ## osdep
        ( getch2_c ),
        ( "osdep/io.c" ),
        ( "osdep/timer.c" ),
        ( timer_c ),
        ( "osdep/threads.c" ),

        ( "osdep/ar/HIDRemote.m",                "apple-remote" ),
        ( "osdep/macosx_application.m",          "cocoa" ),
        ( "osdep/macosx_events.m",               "cocoa" ),
        ( "osdep/semaphore_osx.c" ),
        ( "osdep/subprocess.c" ),
        ( "osdep/subprocess-posix.c",            "posix-spawn" ),
        ( "osdep/subprocess-win.c",              "os-win32" ),
        ( "osdep/path-macosx.m",                 "cocoa" ),
        ( "osdep/path-unix.c"),
        ( "osdep/path-win.c",                    "os-win32" ),
        ( "osdep/path-win.c",                    "os-cygwin" ),
        ( "osdep/glob-win.c",                    "glob-win32-replacement" ),
        ( "osdep/w32_keyboard.c",                "os-win32" ),
        ( "osdep/w32_keyboard.c",                "os-cygwin" ),
        ( "osdep/mpv.rc",                        "win32-executable" ),
        ( "osdep/win32/pthread.c",               "win32-internal-pthreads"),

        ## tree_allocator
        "ta/ta.c", "ta/ta_talloc.c", "ta/ta_utils.c"
    ]

    if ctx.dependency_satisfied('win32-executable'):
        from waflib import TaskGen

        TaskGen.declare_chain(
            name    = 'windres',
            rule    = '${WINDRES} ${WINDRES_FLAGS} ${SRC} ${TGT}',
            ext_in  = '.rc',
            ext_out = '-rc.o',
            color   = 'PINK')

        ctx.env.WINDRES_FLAGS = [
            '--include-dir={0}'.format(ctx.bldnode.abspath()),
            '--include-dir={0}'.format(ctx.srcnode.abspath())
        ]

        for node in 'osdep/mpv.exe.manifest etc/mpv-icon.ico'.split():
            ctx.add_manual_dependency(
                ctx.path.find_node('osdep/mpv.rc'),
                ctx.path.find_node(node))

    if ctx.dependency_satisfied('cplayer') or ctx.dependency_satisfied('test'):
        ctx(
            target       = "objects",
            source       = ctx.filtered_sources(sources),
            use          = ctx.dependencies_use(),
            includes     = _all_includes(ctx),
            features     = "c",
        )

    if ctx.dependency_satisfied('cplayer'):
        ctx(
            target       = "mpv",
            source       = main_fn_c,
            use          = ctx.dependencies_use() + ['objects'],
            includes     = _all_includes(ctx),
            features     = "c cprogram",
            install_path = ctx.env.BINDIR
        )
        for f in ['example.conf', 'input.conf', 'mplayer-input.conf', \
                  'restore-old-bindings.conf']:
            ctx.install_as(os.path.join(ctx.env.DOCDIR, f),
                           os.path.join('etc/', f))

        if ctx.env.DEST_OS == 'win32':
            wrapctx = ctx(
                target       = "mpv",
                source       = ['osdep/win32-console-wrapper.c'],
                features     = "c cprogram",
                install_path = ctx.env.BINDIR
            )

            wrapctx.env.cprogram_PATTERN = "%s.com"
            wrapflags = ['-municode', '-mconsole']
            wrapctx.env.CFLAGS = wrapflags
            wrapctx.env.LAST_LINKFLAGS = wrapflags

    if ctx.dependency_satisfied('test'):
        for test in ctx.path.ant_glob("test/*.c"):
            ctx(
                target   = os.path.splitext(test.srcpath())[0],
                source   = test.srcpath(),
                use      = ctx.dependencies_use() + ['objects'],
                includes = _all_includes(ctx),
                features = "c cprogram",
            )

    build_shared = ctx.dependency_satisfied('libmpv-shared')
    build_static = ctx.dependency_satisfied('libmpv-static')
    if build_shared or build_static:
        if build_shared:
            waftoolsdir = os.path.join(os.path.dirname(__file__), "waftools")
            ctx.load("syms", tooldir=waftoolsdir)
        vre = '^#define MPV_CLIENT_API_VERSION MPV_MAKE_VERSION\((.*), (.*)\)$'
        libmpv_header = ctx.path.find_node("libmpv/client.h").read()
        major, minor = re.search(vre, libmpv_header, re.M).groups()
        libversion = major + '.' + minor + '.0'

        def _build_libmpv(shared):
            features = "c "
            if shared:
                features += "cshlib syms"
            else:
                features += "cstlib"
            ctx(
                target       = "mpv",
                source       = ctx.filtered_sources(sources),
                use          = ctx.dependencies_use(),
                includes     = [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
                                ctx.dependencies_includes(),
                features     = features,
                export_symbols_def = "libmpv/mpv.def",
                install_path = ctx.env.LIBDIR,
                vnum         = libversion,
            )
        if build_shared:
            _build_libmpv(True)
        if build_static:
            _build_libmpv(False)

        def get_deps():
            res = ""
            for k in ctx.env.keys():
                if k.startswith("LIB_") and k != "LIB_ST":
                    res += " ".join(["-l" + x for x in ctx.env[k]]) + " "
            return res

        ctx(
            target       = 'libmpv/mpv.pc',
            source       = 'libmpv/mpv.pc.in',
            features     = 'subst',
            PREFIX       = ctx.env.PREFIX,
            LIBDIR       = ctx.env.LIBDIR,
            INCDIR       = ctx.env.INCDIR,
            VERSION      = libversion,
            PRIV_LIBS    = get_deps(),
        )

        headers = ["client.h", "qthelper.hpp", "opengl_cb.h"]
        for f in headers:
            ctx.install_as(ctx.env.INCDIR + '/mpv/' + f, 'libmpv/' + f)

        ctx.install_as(ctx.env.LIBDIR + '/pkgconfig/mpv.pc', 'libmpv/mpv.pc')

    if ctx.dependency_satisfied("vf-dlopen-filters"):
        dlfilters = "telecine tile rectangle framestep ildetect".split()
        for dlfilter in dlfilters:
            ctx(
                target       = dlfilter,
                source       = ['TOOLS/vf_dlopen/'+dlfilter+'.c',
                                'TOOLS/vf_dlopen/filterutils.c'],
                includes     = [ctx.srcnode.abspath() + '/video/filter'],
                features     = 'c cshlib',
                install_path = ctx.env.LIBDIR + '/mpv' )

    if ctx.dependency_satisfied('manpage-build'):
        _build_man(ctx)

    if ctx.dependency_satisfied('pdf-build'):
        _build_pdf(ctx)

    if ctx.dependency_satisfied('cplayer'):

        if ctx.dependency_satisfied('zsh-comp'):
            ctx.zshcomp(target = "etc/_mpv")
            ctx.install_files(
                ctx.env.ZSHDIR,
                ['etc/_mpv'])

        ctx.install_files(
            ctx.env.DATADIR + '/applications',
            ['etc/mpv.desktop'] )

        ctx.install_files(ctx.env.CONFDIR, ['etc/encoding-profiles.conf'] )

        for size in '16x16 32x32 64x64'.split():
            ctx.install_as(
                ctx.env.DATADIR + '/icons/hicolor/' + size + '/apps/mpv.png',
                'etc/mpv-icon-8bit-' + size + '.png')
