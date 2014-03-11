def _add_rst_manual_dependencies(ctx):
    manpage_sources_basenames = """
        options.rst ao.rst vo.rst af.rst vf.rst encode.rst
        input.rst osc.rst lua.rst changes.rst""".split()

    manpage_sources = ['DOCS/man/en/'+x for x in manpage_sources_basenames]

    for manpage_source in manpage_sources:
        ctx.add_manual_dependency(
            ctx.path.find_node('DOCS/man/en/mpv.rst'),
            ctx.path.find_node(manpage_source))

def _build_man(ctx):
    ctx(
        name         = 'rst2man',
        target       = 'DOCS/man/en/mpv.1',
        source       = 'DOCS/man/en/mpv.rst',
        rule         = '${RST2MAN} ${SRC} ${TGT}',
        install_path = ctx.env.MANDIR + '/man1')

    _add_rst_manual_dependencies(ctx)

def _build_pdf(ctx):
    ctx(
        name         = 'rst2pdf',
        target       = 'DOCS/man/en/mpv.pdf',
        source       = 'DOCS/man/en/mpv.rst',
        rule         = '${RST2PDF} -c --repeat-table-rows ${SRC} -o ${TGT}',
        install_path = ctx.env.DOCDIR)

    _add_rst_manual_dependencies(ctx)

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
        source = "video/out/gl_video_shaders.glsl",
        target = "video/out/gl_video_shaders.h")

    ctx.file2string(
        source = "sub/osd_font.otf",
        target = "sub/osd_font.h")

    ctx.file2string(
        source = "player/lua/defaults.lua",
        target = "player/lua/defaults.inc")

    ctx.file2string(
        source = "player/lua/assdraw.lua",
        target = "player/lua/assdraw.inc")

    ctx.file2string(
        source = "player/lua/osc.lua",
        target = "player/lua/osc.inc")

    ctx.matroska_header(
        source = "demux/ebml.c demux/demux_mkv.c",
        target = "ebml_types.h")

    ctx.matroska_definitions(
        source = "demux/ebml.c",
        target = "ebml_defs.c")

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
        ( "audio/reorder_ch.c" ),
        ( "audio/decode/ad_lavc.c" ),
        ( "audio/decode/ad_mpg123.c",            "mpg123" ),
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
        ( "audio/filter/af_export.c",            "sys-mman-h" ),
        ( "audio/filter/af_extrastereo.c" ),
        ( "audio/filter/af_format.c" ),
        ( "audio/filter/af_hrtf.c" ),
        ( "audio/filter/af_karaoke.c" ),
        ( "audio/filter/af_ladspa.c",            "ladspa" ),
        ( "audio/filter/af_lavcac3enc.c" ),
        ( "audio/filter/af_lavfi.c",             "af-lavfi" ),
        ( "audio/filter/af_lavrresample.c" ),
        ( "audio/filter/af_pan.c" ),
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
        ( "audio/out/ao_coreaudio_properties.c", "coreaudio" ),
        ( "audio/out/ao_coreaudio_utils.c",      "coreaudio" ),
        ( "audio/out/ao_dsound.c",               "dsound" ),
        ( "audio/out/ao_jack.c",                 "jack" ),
        ( "audio/out/ao_lavc.c",                 "encoding" ),
        ( "audio/out/ao_null.c" ),
        ( "audio/out/ao_openal.c",               "openal" ),
        ( "audio/out/ao_oss.c",                  "oss-audio" ),
        ( "audio/out/ao_pcm.c" ),
        ( "audio/out/ao_portaudio.c",            "portaudio" ),
        ( "audio/out/ao_pulse.c",                "pulse" ),
        ( "audio/out/ao_rsound.c",               "rsound" ),
        ( "audio/out/ao_sdl.c",                  "sdl1" ),
        ( "audio/out/ao_sdl.c",                  "sdl2" ),
        ( "audio/out/ao_sndio.c",                "sndio" ),
        ( "audio/out/ao_wasapi.c",               "wasapi" ),
        ( "audio/out/pull.c" ),
        ( "audio/out/push.c" ),

        ## Bstr
        ( "bstr/bstr.c" ),

        ## Core
        ( "common/asxparser.c" ),
        ( "common/av_common.c" ),
        ( "common/av_log.c" ),
        ( "common/av_opts.c" ),
        ( "common/codecs.c" ),
        ( "common/cpudetect.c" ),
        ( "common/encode_lavc.c",                "encoding" ),
        ( "common/common.c" ),
        ( "common/msg.c" ),
        ( "common/playlist.c" ),
        ( "common/playlist_parser.c" ),
        ( "common/version.c" ),

        ## Demuxers
        ( "demux/codec_tags.c" ),
        ( "demux/demux.c" ),
        ( "demux/demux_cue.c" ),
        ( "demux/demux_edl.c" ),
        ( "demux/demux_lavf.c" ),
        ( "demux/demux_libass.c",                "libass"),
        ( "demux/demux_mf.c" ),
        ( "demux/demux_mkv.c" ),
        ( "demux/demux_playlist.c" ),
        ( "demux/demux_raw.c" ),
        ( "demux/demux_subreader.c" ),
        ( "demux/ebml.c" ),
        ( "demux/mf.c" ),

        ## Input
        ( "input/cmd_list.c" ),
        ( "input/cmd_parse.c" ),
        ( "input/event.c" ),
        ( "input/input.c" ),
        ( "input/keycodes.c" ),
        ( "input/joystick.c",                    "joystick" ),
        ( "input/lirc.c",                        "lirc" ),

        ## Misc
        ( "misc/ring.c" ),
        ( "misc/charset_conv.c" ),

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
        ( "player/dvdnav.c" ),
        ( "player/loadfile.c" ),
        ( "player/main.c" ),
        ( "player/misc.c" ),
        ( "player/lua.c",                        "lua" ),
        ( "player/osd.c" ),
        ( "player/playloop.c" ),
        ( "player/screenshot.c" ),
        ( "player/sub.c" ),
        ( "player/timeline/tl_cue.c" ),
        ( "player/timeline/tl_mpv_edl.c" ),
        ( "player/timeline/tl_matroska.c" ),
        ( "player/video.c" ),

        ## Streams
        ( "stream/ai_alsa1x.c",                  "alsa" ),
        ( "stream/ai_oss.c",                     "oss-audio" ),
        ( "stream/ai_sndio.c",                   "sndio" ),
        ( "stream/audio_in.c",                   "audio-input" ),
        ( "stream/cache.c" ),
        ( "stream/cdinfo.c",                     "cdda"),
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
        ( "stream/stream_radio.c",               "radio" ),
        ( "stream/stream_rar.c" ),
        ( "stream/stream_smb.c",                 "libsmbclient" ),
        ( "stream/stream_tv.c",                  "tv" ),
        ( "stream/stream_vcd.c",                 "vcd" ),
        ( "stream/tv.c",                         "tv" ),
        ( "stream/tvi_dummy.c",                  "tv" ),
        ( "stream/tvi_v4l2.c",                   "tv-v4l2"),
        ( "stream/resolve/resolve_quvi.c",       "libquvi4" ),
        ( "stream/resolve/resolve_quvi9.c",      "libquvi9" ),

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
        ( "sub/sd_spu.c" ),
        ( "sub/sd_srt.c" ),
        ( "sub/spudec.c" ),

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
        ( "video/decode/dec_video.c"),
        ( "video/decode/lavc_dr1.c",             "!avutil-refcounting" ),
        ( "video/decode/vaapi.c",                "vaapi-hwaccel" ),
        ( "video/decode/vd_lavc.c" ),
        ( "video/decode/vda.c",                  "vda-hwaccel" ),
        ( "video/decode/vdpau.c",                "vdpau-hwaccel" ),
        ( "video/decode/vdpau_old.c",            "vdpau-decoder" ),
        ( "video/filter/pullup.c" ),
        ( "video/filter/vf.c" ),
        ( "video/filter/vf_crop.c" ),
        ( "video/filter/vf_delogo.c" ),
        ( "video/filter/vf_divtc.c" ),
        ( "video/filter/vf_dlopen.c",            "dlopen" ),
        ( "video/filter/vf_dsize.c" ),
        ( "video/filter/vf_eq.c" ),
        ( "video/filter/vf_expand.c" ),
        ( "video/filter/vf_flip.c" ),
        ( "video/filter/vf_format.c" ),
        ( "video/filter/vf_gradfun.c" ),
        ( "video/filter/vf_hqdn3d.c" ),
        ( "video/filter/vf_ilpack.c" ),
        ( "video/filter/vf_lavfi.c",             "vf-lavfi"),
        ( "video/filter/vf_mirror.c" ),
        ( "video/filter/vf_noformat.c" ),
        ( "video/filter/vf_noise.c" ),
        ( "video/filter/vf_phase.c" ),
        ( "video/filter/vf_pp.c",                "libpostproc" ),
        ( "video/filter/vf_pullup.c" ),
        ( "video/filter/vf_rotate.c" ),
        ( "video/filter/vf_scale.c" ),
        ( "video/filter/vf_screenshot.c" ),
        ( "video/filter/vf_softpulldown.c" ),
        ( "video/filter/vf_stereo3d.c" ),
        ( "video/filter/vf_sub.c" ),
        ( "video/filter/vf_swapuv.c" ),
        ( "video/filter/vf_unsharp.c" ),
        ( "video/filter/vf_vavpp.c",             "vaapi-vpp"),
        ( "video/filter/vf_yadif.c" ),
        ( "video/out/aspect.c" ),
        ( "video/out/bitmap_packer.c" ),
        ( "video/out/cocoa/additions.m",         "cocoa" ),
        ( "video/out/cocoa/view.m",              "cocoa" ),
        ( "video/out/cocoa/window.m",            "cocoa" ),
        ( "video/out/cocoa_common.m",            "cocoa" ),
        ( "video/out/dither.c" ),
        ( "video/out/filter_kernels.c" ),
        ( "video/out/gl_cocoa.c",                "gl-cocoa" ),
        ( "video/out/gl_common.c",               "gl" ),
        ( "video/out/gl_hwdec_vaglx.c",          "vaapi-glx" ),
        ( "video/out/gl_hwdec_vda.c",            "vda-gl" ),
        ( "video/out/gl_hwdec_vdpau.c",          "vdpau-gl-x11" ),
        ( "video/out/gl_lcms.c",                 "gl" ),
        ( "video/out/gl_osd.c",                  "gl" ),
        ( "video/out/gl_video.c",                "gl" ),
        ( "video/out/gl_w32.c",                  "gl-win32" ),
        ( "video/out/gl_wayland.c",              "gl-wayland" ),
        ( "video/out/gl_x11.c",                  "gl-x11" ),
        ( "video/out/pnm_loader.c",              "gl" ),
        ( "video/out/vo.c" ),
        ( "video/out/vo_caca.c",                 "caca" ),
        ( "video/out/vo_corevideo.c",            "corevideo"),
        ( "video/out/vo_direct3d.c",             "direct3d" ),
        ( "video/out/vo_image.c" ),
        ( "video/out/vo_lavc.c",                 "encoding" ),
        ( "video/out/vo_null.c" ),
        ( "video/out/vo_opengl.c",               "gl" ),
        ( "video/out/vo_opengl_old.c",           "gl" ),
        ( "video/out/vo_sdl.c",                  "sdl2" ),
        ( "video/out/vo_vaapi.c",                "vaapi" ),
        ( "video/out/vo_vdpau.c",                "vdpau" ),
        ( "video/out/vo_wayland.c",              "wayland" ),
        ( "video/out/vo_x11.c" ,                 "x11" ),
        ( "video/out/vo_xv.c",                   "xv" ),
        ( "video/out/w32_common.c",              "gdi" ),
        ( "video/out/wayland_common.c",          "wayland" ),
        ( "video/out/x11_common.c",              "x11" ),

        ## osdep
        ( getch2_c ),
        ( "osdep/io.c" ),
        ( "osdep/numcores.c"),
        ( "osdep/timer.c" ),
        ( timer_c ),
        ( "osdep/threads.c" ),

        ( "osdep/ar/HIDRemote.m",                "cocoa" ),
        ( "osdep/macosx_application.m",          "cocoa" ),
        ( "osdep/macosx_events.m",               "cocoa" ),
        ( "osdep/path-macosx.m",                 "cocoa" ),

        ( "osdep/path-win.c",                    "os-win32" ),
        ( "osdep/path-win.c",                    "os-cygwin" ),
        ( "osdep/glob-win.c",                    "glob-win32-replacement" ),
        ( "osdep/priority.c",                    "priority" ),
        ( "osdep/w32_keyboard.c",                "os-win32" ),
        ( "osdep/w32_keyboard.c",                "os-cygwin" ),
        ( "osdep/mpv.rc",                        "win32-executable" ),

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

    cprog_kwargs = {}
    if ctx.dependency_satisfied('macosx-bundle'):
        import os
        basepath = 'TOOLS/osxbundle/mpv.app/Contents'
        cprog_kwargs['mac_app']   = True
        cprog_kwargs['mac_plist'] = os.path.join(basepath, 'Info.plist')

        resources_glob  = os.path.join(basepath, 'Resources', '*')
        resources_nodes = ctx.srcnode.ant_glob(resources_glob)
        resources       = [node.srcpath() for node in resources_nodes]
        cprog_kwargs['mac_resources'] = resources

        for resource in resources:
            res_basename = os.path.basename(resource)
            install_name = '/mpv.app/Contents/Resources/' + res_basename
            ctx.install_as(ctx.env.BINDIR + install_name, resource)

    ctx(
        target       = "mpv",
        source       = ctx.filtered_sources(sources) + ["player/main_fn.c"],
        use          = ctx.dependencies_use(),
        includes     = [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
                       ctx.dependencies_includes(),
        features     = "c cprogram",
        install_path = ctx.env.BINDIR,
        **cprog_kwargs
    )

    if ctx.dependency_satisfied('shared'):
        ctx.load("syms")
        ctx(
            target       = "mpv",
            source       = ctx.filtered_sources(sources),
            use          = ctx.dependencies_use(),
            includes     = [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
                            ctx.dependencies_includes(),
            features     = "c cshlib syms",
            export_symbols_regex = 'mpv_.*',
            install_path = ctx.env.LIBDIR,
            vnum         = "0.0.0",
        )

        ctx(
            target       = 'libmpv/mpv.pc',
            source       = 'libmpv/mpv.pc.in',
            features     = 'subst',
            PREFIX       = ctx.env.PREFIX,
            LIBDIR       = ctx.env.LIBDIR,
            INCDIR       = ctx.env.INCDIR,
            vnum         = "0.0.0",
        )

        headers = ["client.h"]
        for f in headers:
            ctx.install_as(ctx.env.INCDIR + '/mpv/' + f, 'libmpv/' + f)

        ctx.install_as(ctx.env.LIBDIR + '/pkgconfig/mpv.pc', 'libmpv/mpv.pc')

    if ctx.dependency_satisfied('client-api-examples'):
        # This assumes all examples are single-file (as examples should be)
        for f in ["simple"]:
            ctx(
                target       = f,
                source       = "DOCS/client_api_examples/" + f + ".c",
                includes     = [ctx.bldnode.abspath(), ctx.srcnode.abspath()],
                use          = "mpv",
                features     = "c cprogram",
            )

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

    if ctx.dependency_satisfied('macosx-bundle'):
        from waflib import Utils
        ctx.install_files(ctx.env.BINDIR, 'mpv', chmod=Utils.O755)

    if ctx.dependency_satisfied("vf-dlopen-filters"):
        dlfilters = "showqscale telecine tile rectangle framestep \
                     ildetect".split()
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

    ctx.install_files(
        ctx.env.DATADIR + '/applications',
        ['etc/mpv.desktop'] )

    ctx.install_files(ctx.env.CONFDIR, ['etc/encoding-profiles.conf'] )

    for size in '16x16 32x32 64x64'.split():
        ctx.install_as(
            ctx.env.DATADIR + '/icons/hicolor/' + size + '/apps/mpv.png',
            'etc/mpv-icon-8bit-' + size + '.png')
