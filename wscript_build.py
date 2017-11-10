import re
import os

def _add_rst_manual_dependencies(ctx):
    manpage_sources_basenames = """
        options.rst ao.rst vo.rst af.rst vf.rst encode.rst
        input.rst osc.rst stats.rst lua.rst ipc.rst changes.rst""".split()

    manpage_sources = ['DOCS/man/'+x for x in manpage_sources_basenames]

    for manpage_source in manpage_sources:
        ctx.add_manual_dependency(
            ctx.path.find_node('DOCS/man/mpv.rst'),
            ctx.path.find_node(manpage_source))

def _build_html(ctx):
    ctx(
        name         = 'rst2html',
        target       = 'DOCS/man/mpv.html',
        source       = 'DOCS/man/mpv.rst',
        rule         = '${RST2HTML} ${SRC} ${TGT}',
        install_path = ctx.env.HTMLDIR)

    _add_rst_manual_dependencies(ctx)

def _build_man(ctx):
    ctx(
        name         = 'rst2man',
        target       = 'DOCS/man/mpv.1',
        source       = 'DOCS/man/mpv.rst',
        rule         = '${RST2MAN} --strip-elements-with-class=contents ${SRC} ${TGT}',
        install_path = ctx.env.MANDIR + '/man1')

    _add_rst_manual_dependencies(ctx)

def _build_pdf(ctx):
    ctx(
        name         = 'rst2pdf',
        target       = 'DOCS/man/mpv.pdf',
        source       = 'DOCS/man/mpv.rst',
        rule         = '${RST2PDF} -c -b 1 --repeat-table-rows ${SRC} -o ${TGT}',
        install_path = ctx.env.DOCDIR)

    _add_rst_manual_dependencies(ctx)

def _all_includes(ctx):
    return [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
            ctx.dependencies_includes()

def build(ctx):
    ctx.load('waf_customizations')
    ctx.load('generators.sources')

    ctx(
        features = "file2string",
        source = "TOOLS/osxbundle/mpv.app/Contents/Resources/icon.icns",
        target = "osdep/macosx_icon.inc",
    )

    ctx(
        features = "file2string",
        source = "etc/mpv-icon-8bit-16x16.png",
        target = "video/out/x11_icon_16.inc",
    )

    ctx(
        features = "file2string",
        source = "etc/mpv-icon-8bit-32x32.png",
        target = "video/out/x11_icon_32.inc",
    )

    ctx(
        features = "file2string",
        source = "etc/mpv-icon-8bit-64x64.png",
        target = "video/out/x11_icon_64.inc",
    )

    ctx(
        features = "file2string",
        source = "etc/mpv-icon-8bit-128x128.png",
        target = "video/out/x11_icon_128.inc",
    )

    ctx(
        features = "file2string",
        source = "etc/input.conf",
        target = "input/input_conf.h",
    )

    ctx(
        features = "file2string",
        source = "etc/builtin.conf",
        target = "player/builtin_conf.inc",
    )

    ctx(
        features = "file2string",
        source = "sub/osd_font.otf",
        target = "sub/osd_font.h",
    )

    lua_files = ["defaults.lua", "assdraw.lua", "options.lua", "osc.lua",
                 "ytdl_hook.lua", "stats.lua"]

    for fn in lua_files:
        fn = "player/lua/" + fn
        ctx(
            features = "file2string",
            source = fn,
            target = os.path.splitext(fn)[0] + ".inc",
        )

    ctx(
        features = "file2string",
        source = "player/javascript/defaults.js",
        target = "player/javascript/defaults.js.inc",
    )

    if ctx.dependency_satisfied('wayland'):
        ctx.wayland_protocol_code(proto_dir = ctx.env.WL_PROTO_DIR,
            protocol  = "unstable/xdg-shell/xdg-shell-unstable-v6",
            target    = "video/out/wayland/xdg-shell-v6.c")
        ctx.wayland_protocol_header(proto_dir = ctx.env.WL_PROTO_DIR,
            protocol  = "unstable/xdg-shell/xdg-shell-unstable-v6",
            target    = "video/out/wayland/xdg-shell-v6.h")
        ctx.wayland_protocol_code(proto_dir = ctx.env.WL_PROTO_DIR,
            protocol  = "unstable/idle-inhibit/idle-inhibit-unstable-v1",
            target    = "video/out/wayland/idle-inhibit-v1.c")
        ctx.wayland_protocol_header(proto_dir = ctx.env.WL_PROTO_DIR,
            protocol  = "unstable/idle-inhibit/idle-inhibit-unstable-v1",
            target    = "video/out/wayland/idle-inhibit-v1.h")
        ctx.wayland_protocol_code(proto_dir = "../video/out/wayland",
            protocol = "server-decoration",
            target   = "video/out/wayland/srv-decor.c")
        ctx.wayland_protocol_header(proto_dir = "../video/out/wayland",
            protocol = "server-decoration",
            target   = "video/out/wayland/srv-decor.h")

    ctx(features = "ebml_header", target = "ebml_types.h")
    ctx(features = "ebml_definitions", target = "ebml_defs.c")

    if ctx.dependency_satisfied('cplayer'):
        main_fn_c = ctx.pick_first_matching_dep([
            ( "osdep/main-fn-cocoa.c",               "cocoa" ),
            ( "osdep/main-fn-unix.c",                "posix" ),
            ( "osdep/main-fn-win.c",                 "win32-desktop" ),
        ])

    getch2_c = ctx.pick_first_matching_dep([
        ( "osdep/terminal-unix.c",               "posix" ),
        ( "osdep/terminal-win.c",                "win32-desktop" ),
        ( "osdep/terminal-dummy.c" ),
    ])

    timer_c = ctx.pick_first_matching_dep([
        ( "osdep/timer-win2.c",                  "os-win32" ),
        ( "osdep/timer-darwin.c",                "os-darwin" ),
        ( "osdep/timer-linux.c",                 "posix" ),
    ])

    ipc_c = ctx.pick_first_matching_dep([
        ( "input/ipc-unix.c",                    "posix" ),
        ( "input/ipc-win.c",                     "win32-desktop" ),
        ( "input/ipc-dummy.c" ),
    ])

    subprocess_c = ctx.pick_first_matching_dep([
        ( "osdep/subprocess-posix.c",            "posix-spawn" ),
        ( "osdep/subprocess-win.c",              "win32-desktop" ),
        ( "osdep/subprocess-dummy.c" ),
    ])

    sources = [
        ## Audio
        ( "audio/aconverter.c" ),
        ( "audio/audio.c",                       "libaf" ),
        ( "audio/audio_buffer.c" ),
        ( "audio/chmap.c" ),
        ( "audio/chmap_sel.c" ),
        ( "audio/fmt-conversion.c" ),
        ( "audio/format.c" ),
        ( "audio/aframe.c" ),
        ( "audio/decode/ad_lavc.c" ),
        ( "audio/decode/ad_spdif.c" ),
        ( "audio/decode/dec_audio.c" ),
        ( "audio/filter/af.c",                   "libaf" ),
        ( "audio/filter/af_channels.c",          "libaf" ),
        ( "audio/filter/af_equalizer.c",         "libaf" ),
        ( "audio/filter/af_format.c",            "libaf" ),
        ( "audio/filter/af_lavcac3enc.c",        "libaf" ),
        ( "audio/filter/af_lavfi.c",             "libaf" ),
        ( "audio/filter/af_lavrresample.c",      "libaf" ),
        ( "audio/filter/af_pan.c",               "libaf" ),
        ( "audio/filter/af_rubberband.c",        "rubberband" ),
        ( "audio/filter/af_scaletempo.c",        "libaf" ),
        ( "audio/filter/af_volume.c",            "libaf" ),
        ( "audio/filter/tools.c",                "libaf" ),
        ( "audio/out/ao.c" ),
        ( "audio/out/ao_alsa.c",                 "alsa" ),
        ( "audio/out/ao_audiounit.m",            "audiounit" ),
        ( "audio/out/ao_coreaudio_chmap.c",      "audiounit" ),
        ( "audio/out/ao_coreaudio_utils.c",      "audiounit" ),
        ( "audio/out/ao_coreaudio.c",            "coreaudio" ),
        ( "audio/out/ao_coreaudio_chmap.c",      "coreaudio" ),
        ( "audio/out/ao_coreaudio_exclusive.c",  "coreaudio" ),
        ( "audio/out/ao_coreaudio_properties.c", "coreaudio" ),
        ( "audio/out/ao_coreaudio_utils.c",      "coreaudio" ),
        ( "audio/out/ao_jack.c",                 "jack" ),
        ( "audio/out/ao_lavc.c",                 "encoding" ),
        ( "audio/out/ao_null.c" ),
        ( "audio/out/ao_openal.c",               "openal" ),
        ( "audio/out/ao_opensles.c",             "opensles" ),
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
        ( "common/recorder.c" ),
        ( "common/version.c" ),

        ## Demuxers
        ( "demux/codec_tags.c" ),
        ( "demux/cue.c" ),
        ( "demux/demux.c" ),
        ( "demux/demux_cue.c" ),
        ( "demux/demux_disc.c" ),
        ( "demux/demux_edl.c" ),
        ( "demux/demux_lavf.c" ),
        ( "demux/demux_libarchive.c",            "libarchive" ),
        ( "demux/demux_mf.c" ),
        ( "demux/demux_mkv.c" ),
        ( "demux/demux_mkv_timeline.c" ),
        ( "demux/demux_null.c" ),
        ( "demux/demux_playlist.c" ),
        ( "demux/demux_raw.c" ),
        ( "demux/demux_rar.c" ),
        ( "demux/demux_timeline.c" ),
        ( "demux/demux_tv.c",                    "tv" ),
        ( "demux/ebml.c" ),
        ( "demux/packet.c" ),
        ( "demux/timeline.c" ),

        ## Input
        ( "input/cmd_list.c" ),
        ( "input/cmd_parse.c" ),
        ( "input/event.c" ),
        ( "input/input.c" ),
        ( "input/ipc.c" ),
        ( ipc_c ),
        ( "input/keycodes.c" ),
        ( "input/pipe-win32.c",                  "win32-pipes" ),

        ## Misc
        ( "misc/bstr.c" ),
        ( "misc/charset_conv.c" ),
        ( "misc/dispatch.c" ),
        ( "misc/json.c" ),
        ( "misc/node.c" ),
        ( "misc/ring.c" ),
        ( "misc/rendezvous.c" ),
        ( "misc/thread_pool.c" ),

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
        ( "player/external_files.c" ),
        ( "player/loadfile.c" ),
        ( "player/main.c" ),
        ( "player/misc.c" ),
        ( "player/lavfi.c" ),
        ( "player/lua.c",                        "lua" ),
        ( "player/javascript.c",                 "javascript" ),
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
        ( "stream/stream_dvd.c",                 "dvdread-common" ),
        ( "stream/stream_dvd_common.c",          "dvdread-common" ),
        ( "stream/stream_dvdnav.c",              "dvdnav" ),
        ( "stream/stream_edl.c" ),
        ( "stream/stream_file.c" ),
        ( "stream/stream_cb.c" ),
        ( "stream/stream_lavf.c" ),
        ( "stream/stream_libarchive.c",          "libarchive" ),
        ( "stream/stream_memory.c" ),
        ( "stream/stream_mf.c" ),
        ( "stream/stream_null.c" ),
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
        ( "sub/img_convert.c" ),
        ( "sub/lavc_conv.c" ),
        ( "sub/osd.c" ),
        ( "sub/osd_dummy.c",                     "dummy-osd" ),
        ( "sub/osd_libass.c",                    "libass-osd" ),
        ( "sub/sd_ass.c",                        "libass" ),
        ( "sub/sd_lavc.c" ),
        ( "sub/filter_sdh.c" ),

        ## Video
        ( "video/csputils.c" ),
        ( "video/fmt-conversion.c" ),
        ( "video/image_loader.c" ),
        ( "video/image_writer.c" ),
        ( "video/img_format.c" ),
        ( "video/hwdec.c" ),
        ( "video/mp_image.c" ),
        ( "video/mp_image_pool.c" ),
        ( "video/sws_utils.c" ),
        ( "video/vaapi.c",                       "vaapi" ),
        ( "video/vdpau.c",                       "vdpau" ),
        ( "video/vdpau_mixer.c",                 "vdpau" ),
        ( "video/decode/d3d.c",                  "d3d-hwaccel" ),
        ( "video/decode/dec_video.c"),
        ( "video/decode/hw_dxva2.c",             "d3d9-hwaccel" ),
        ( "video/decode/hw_d3d11va.c",           "d3d-hwaccel" ),
        ( "video/decode/hw_mediacodec.c",        "android" ),
        ( "video/decode/vd_lavc.c" ),
        ( "video/filter/refqueue.c" ),
        ( "video/filter/vf.c" ),
        ( "video/filter/vf_buffer.c" ),
        ( "video/filter/vf_crop.c",              "gpl" ),
        ( "video/filter/vf_d3d11vpp.c",          "d3d-hwaccel" ),
        ( "video/filter/vf_dsize.c",             "gpl" ),
        ( "video/filter/vf_expand.c",            "gpl" ),
        ( "video/filter/vf_flip.c",              "gpl" ),
        ( "video/filter/vf_format.c",            "gpl" ),
        ( "video/filter/vf_gradfun.c",           "gpl" ),
        ( "video/filter/vf_lavfi.c" ),
        ( "video/filter/vf_mirror.c",            "gpl" ),
        ( "video/filter/vf_noformat.c",          "gpl" ),
        ( "video/filter/vf_pullup.c",            "gpl" ),
        ( "video/filter/vf_rotate.c",            "gpl" ),
        ( "video/filter/vf_scale.c",             "gpl" ),
        ( "video/filter/vf_stereo3d.c",          "gpl" ),
        ( "video/filter/vf_sub.c",               "gpl" ),
        ( "video/filter/vf_vapoursynth.c",       "vapoursynth-core" ),
        ( "video/filter/vf_vavpp.c",             "vaapi" ),
        ( "video/filter/vf_vdpaupp.c",           "vdpau" ),
        ( "video/filter/vf_yadif.c",             "gpl" ),
        ( "video/out/aspect.c" ),
        ( "video/out/bitmap_packer.c" ),
        ( "video/out/cocoa/video_view.m",        "cocoa" ),
        ( "video/out/cocoa/events_view.m",       "cocoa" ),
        ( "video/out/cocoa/window.m",            "cocoa" ),
        ( "video/out/cocoa_common.m",            "cocoa" ),
        ( "video/out/dither.c" ),
        ( "video/out/filter_kernels.c" ),
        ( "video/out/d3d11/context.c",           "d3d11" ),
        ( "video/out/d3d11/hwdec_d3d11va.c",     "d3d11 && d3d-hwaccel" ),
        ( "video/out/d3d11/ra_d3d11.c",          "d3d11" ),
        ( "video/out/opengl/angle_dynamic.c",    "egl-angle" ),
        ( "video/out/gpu/context.c" ),
        ( "video/out/gpu/d3d11_helpers.c",       "d3d11 || egl-angle-win32" ),
        ( "video/out/gpu/hwdec.c" ),
        ( "video/out/gpu/lcms.c" ),
        ( "video/out/gpu/osd.c" ),
        ( "video/out/gpu/ra.c" ),
        ( "video/out/gpu/spirv.c" ),
        ( "video/out/gpu/spirv_shaderc.c",       "shaderc" ),
        ( "video/out/gpu/shader_cache.c" ),
        ( "video/out/gpu/user_shaders.c" ),
        ( "video/out/gpu/utils.c" ),
        ( "video/out/gpu/video.c" ),
        ( "video/out/gpu/video_shaders.c" ),
        ( "video/out/opengl/common.c",           "gl" ),
        ( "video/out/opengl/formats.c",          "gl" ),
        ( "video/out/opengl/utils.c",            "gl" ),
        ( "video/out/opengl/ra_gl.c",            "gl" ),
        ( "video/out/opengl/context.c",          "gl" ),
        ( "video/out/opengl/context_angle.c",    "egl-angle-win32" ),
        ( "video/out/opengl/context_cocoa.c",    "gl-cocoa" ),
        ( "video/out/opengl/context_drm_egl.c",  "egl-drm" ),
        ( "video/out/opengl/context_dxinterop.c","gl-dxinterop" ),
        ( "video/out/opengl/context_mali_fbdev.c","mali-fbdev" ),
        ( "video/out/opengl/context_android.c",  "android" ),
        ( "video/out/opengl/context_rpi.c",      "rpi" ),
        ( "video/out/opengl/context_vdpau.c",    "vdpau-gl-x11" ),
        ( "video/out/opengl/context_wayland.c",  "gl-wayland" ),
        ( "video/out/opengl/context_win.c",      "gl-win32" ),
        ( "video/out/opengl/context_glx.c",      "gl-x11" ),
        ( "video/out/opengl/context_x11egl.c",   "egl-x11" ),
        ( "video/out/opengl/cuda_dynamic.c",     "cuda-hwaccel" ),
        ( "video/out/opengl/egl_helpers.c",      "egl-helpers" ),
        ( "video/out/opengl/hwdec_cuda.c",       "cuda-hwaccel" ),
        ( "video/out/opengl/hwdec_d3d11egl.c",   "d3d-hwaccel" ),
        ( "video/out/opengl/hwdec_d3d11eglrgb.c","d3d-hwaccel" ),
        ( "video/out/opengl/hwdec_dxva2gldx.c",  "gl-dxinterop-d3d9" ),
        ( "video/out/opengl/hwdec_dxva2egl.c",   "d3d9-hwaccel" ),
        ( "video/out/opengl/hwdec_osx.c",        "videotoolbox-gl" ),
        ( "video/out/opengl/hwdec_ios.m",        "ios-gl" ),
        ( "video/out/opengl/hwdec_drmprime_drm.c","drmprime && drm" ),
        ( "video/out/opengl/hwdec_rpi.c",        "rpi" ),
        ( "video/out/opengl/hwdec_vaegl.c",      "vaapi-egl" ),
        ( "video/out/opengl/hwdec_vaglx.c",      "vaapi-glx" ),
        ( "video/out/opengl/hwdec_vdpau.c",      "vdpau-gl-x11" ),
        ( "video/out/vo.c" ),
        ( "video/out/vo_mediacodec_embed.c",     "android" ),
        ( "video/out/vo_caca.c",                 "caca" ),
        ( "video/out/vo_drm.c",                  "drm" ),
        ( "video/out/vo_direct3d.c",             "direct3d" ),
        ( "video/out/vo_image.c" ),
        ( "video/out/vo_lavc.c",                 "encoding" ),
        ( "video/out/vo_rpi.c",                  "rpi" ),
        ( "video/out/vo_null.c" ),
        ( "video/out/vo_gpu.c" ),
        ( "video/out/vo_opengl_cb.c",            "gl" ),
        ( "video/out/vo_sdl.c",                  "sdl2" ),
        ( "video/out/vo_tct.c" ),
        ( "video/out/vo_vaapi.c",                "vaapi-x11 && gpl" ),
        ( "video/out/vo_vdpau.c",                "vdpau" ),
        ( "video/out/vo_x11.c" ,                 "x11" ),
        ( "video/out/vo_xv.c",                   "xv" ),
        ( "video/out/w32_common.c",              "win32-desktop" ),
        ( "video/out/win32/displayconfig.c",     "win32-desktop" ),
        ( "video/out/win32/droptarget.c",        "win32-desktop" ),
        ( "video/out/vulkan/utils.c",            "vulkan" ),
        ( "video/out/vulkan/malloc.c",           "vulkan" ),
        ( "video/out/vulkan/formats.c",          "vulkan" ),
        ( "video/out/vulkan/ra_vk.c",            "vulkan" ),
        ( "video/out/vulkan/context.c",          "vulkan" ),
        ( "video/out/vulkan/context_xlib.c",     "vulkan && x11" ),
        ( "video/out/vulkan/context_wayland.c",  "vulkan && wayland" ),
        ( "video/out/vulkan/context_win.c",      "vulkan && win32-desktop" ),
        ( "video/out/vulkan/spirv_nvidia.c",     "vulkan" ),
        ( "video/out/win32/exclusive_hack.c",    "gl-win32" ),
        ( "video/out/wayland_common.c",          "wayland" ),
        ( "video/out/wayland/xdg-shell-v6.c",    "wayland" ),
        ( "video/out/wayland/idle-inhibit-v1.c", "wayland" ),
        ( "video/out/wayland/srv-decor.c",       "wayland" ),
        ( "video/out/win_state.c"),
        ( "video/out/x11_common.c",              "x11" ),
        ( "video/out/drm_atomic.c",              "drm" ),
        ( "video/out/drm_common.c",              "drm" ),
        ( "video/out/drm_prime.c",               "drm && drmprime" ),

        ## osdep
        ( getch2_c ),
        ( "osdep/io.c" ),
        ( "osdep/timer.c" ),
        ( timer_c ),
        ( "osdep/threads.c" ),

        ( "osdep/ar/HIDRemote.m",                "apple-remote" ),
        ( "osdep/macosx_application.m",          "cocoa" ),
        ( "osdep/macosx_events.m",               "cocoa" ),
        ( "osdep/macosx_menubar.m",              "cocoa" ),
        ( "osdep/macosx_touchbar.m",             "macos-touchbar" ),
        ( "osdep/semaphore_osx.c" ),
        ( "osdep/subprocess.c" ),
        ( subprocess_c ),
        ( "osdep/path-macosx.m",                 "cocoa" ),
        ( "osdep/path-unix.c"),
        ( "osdep/path-win.c",                    "win32-desktop" ),
        ( "osdep/path-uwp.c",                    "uwp" ),
        ( "osdep/glob-win.c",                    "glob-win32" ),
        ( "osdep/w32_keyboard.c",                "os-win32" ),
        ( "osdep/w32_keyboard.c",                "os-cygwin" ),
        ( "osdep/windows_utils.c",               "os-win32" ),
        ( "osdep/windows_utils.c",               "os-cygwin" ),
        ( "osdep/mpv.rc",                        "win32-executable" ),
        ( "osdep/win32/pthread.c",               "win32-internal-pthreads"),
        ( "osdep/android/posix-spawn.c",         "android"),
        ( "osdep/android/strnlen.c",             "android"),

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

        version = ctx.bldnode.find_node('version.h')
        if version:
            ctx.add_manual_dependency(
                ctx.path.find_node('osdep/mpv.rc'),
                version)

    if ctx.dependency_satisfied('cplayer') or ctx.dependency_satisfied('test'):
        ctx(
            target       = "objects",
            source       = ctx.filtered_sources(sources),
            use          = ctx.dependencies_use(),
            includes     = _all_includes(ctx),
            features     = "c",
        )

    syms = False
    if ctx.dependency_satisfied('cplugins'):
        syms = True
        ctx.load("syms")

    if ctx.dependency_satisfied('cplayer'):
        ctx(
            target       = "mpv",
            source       = main_fn_c,
            use          = ctx.dependencies_use() + ['objects'],
            includes     = _all_includes(ctx),
            features     = "c cprogram" + (" syms" if syms else ""),
            export_symbols_def = "libmpv/mpv.def", # for syms=True
            install_path = ctx.env.BINDIR
        )
        for f in ['mpv.conf', 'input.conf', 'mplayer-input.conf', \
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
                target       = os.path.splitext(test.srcpath())[0],
                source       = test.srcpath(),
                use          = ctx.dependencies_use() + ['objects'],
                includes     = _all_includes(ctx),
                features     = "c cprogram",
                install_path = None,
            )

    build_shared = ctx.dependency_satisfied('libmpv-shared')
    build_static = ctx.dependency_satisfied('libmpv-static')
    if build_shared or build_static:
        if build_shared:
            waftoolsdir = os.path.join(os.path.dirname(__file__), "waftools")
            ctx.load("syms", tooldir=waftoolsdir)
        vre = '#define MPV_CLIENT_API_VERSION MPV_MAKE_VERSION\((.*), (.*)\)'
        libmpv_header = ctx.path.find_node("libmpv/client.h").read()
        major, minor = re.search(vre, libmpv_header).groups()
        libversion = major + '.' + minor + '.0'

        def _build_libmpv(shared):
            features = "c "
            if shared:
                features += "cshlib syms"
            else:
                features += "cstlib"

            libmpv_kwargs = {
                "target": "mpv",
                "source":   ctx.filtered_sources(sources),
                "use":      ctx.dependencies_use(),
                "includes": [ctx.bldnode.abspath(), ctx.srcnode.abspath()] + \
                             ctx.dependencies_includes(),
                "features": features,
                "export_symbols_def": "libmpv/mpv.def",
                "install_path": ctx.env.LIBDIR,
                "install_path_implib": ctx.env.LIBDIR,
            }

            if shared and ctx.dependency_satisfied('android'):
                # for Android we just add the linker flag without version
                # as we still need the SONAME for proper linkage.
                # (LINKFLAGS logic taken from waf's apply_vnum in ccroot.py)
                v=ctx.env.SONAME_ST%'libmpv.so'
                ctx.env.append_value('LINKFLAGS',v.split())
            else:
                # for all other configurations we want SONAME to be used
                libmpv_kwargs["vnum"] = libversion

            if shared and ctx.env.DEST_OS == 'win32':
                libmpv_kwargs["install_path"] = ctx.env.BINDIR

            ctx(**libmpv_kwargs)

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

        headers = ["client.h", "qthelper.hpp", "opengl_cb.h", "stream_cb.h"]
        for f in headers:
            ctx.install_as(ctx.env.INCDIR + '/mpv/' + f, 'libmpv/' + f)

        ctx.install_as(ctx.env.LIBDIR + '/pkgconfig/mpv.pc', 'libmpv/mpv.pc')

    if ctx.dependency_satisfied('html-build'):
        _build_html(ctx)

    if ctx.dependency_satisfied('manpage-build'):
        _build_man(ctx)

    if ctx.dependency_satisfied('pdf-build'):
        _build_pdf(ctx)

    if ctx.dependency_satisfied('cplayer'):

        if ctx.dependency_satisfied('zsh-comp'):
            ctx.zshcomp(target = "etc/_mpv", source = "TOOLS/zsh.pl")
            ctx.install_files(
                ctx.env.ZSHDIR,
                ['etc/_mpv'])

        ctx.install_files(
            ctx.env.DATADIR + '/applications',
            ['etc/mpv.desktop'] )

        if ctx.dependency_satisfied('encoding'):
            ctx.install_files(ctx.env.CONFDIR, ['etc/encoding-profiles.conf'] )

        for size in '16x16 32x32 64x64'.split():
            ctx.install_as(
                ctx.env.DATADIR + '/icons/hicolor/' + size + '/apps/mpv.png',
                'etc/mpv-icon-8bit-' + size + '.png')

        ctx.install_as(
                ctx.env.DATADIR + '/icons/hicolor/scalable/apps/mpv.svg',
                'etc/mpv-gradient.svg')

        ctx.install_files(
            ctx.env.DATADIR + '/icons/hicolor/symbolic/apps',
            ['etc/mpv-symbolic.svg'])
