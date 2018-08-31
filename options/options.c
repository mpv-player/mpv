/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_CFG_MPLAYER_H
#define MPLAYER_CFG_MPLAYER_H

/*
 * config for cfgparser
 */

#include <stddef.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>

#include "config.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "options.h"
#include "m_config.h"
#include "m_option.h"
#include "common/common.h"
#include "stream/stream.h"
#include "video/csputils.h"
#include "video/hwdec.h"
#include "video/image_writer.h"
#include "sub/osd.h"
#include "player/core.h"
#include "player/command.h"
#include "stream/stream.h"

#if HAVE_DRM
#include "video/out/drm_common.h"
#endif

static void print_version(struct mp_log *log)
{
    mp_print_version(log, true);
}

extern const struct m_sub_options tv_params_conf;
extern const struct m_sub_options stream_cdda_conf;
extern const struct m_sub_options stream_dvb_conf;
extern const struct m_sub_options stream_lavf_conf;
extern const struct m_sub_options sws_conf;
extern const struct m_sub_options drm_conf;
extern const struct m_sub_options demux_rawaudio_conf;
extern const struct m_sub_options demux_rawvideo_conf;
extern const struct m_sub_options demux_lavf_conf;
extern const struct m_sub_options demux_mkv_conf;
extern const struct m_sub_options vd_lavc_conf;
extern const struct m_sub_options ad_lavc_conf;
extern const struct m_sub_options input_config;
extern const struct m_sub_options encode_config;
extern const struct m_sub_options gl_video_conf;
extern const struct m_sub_options ao_alsa_conf;

extern const struct m_sub_options demux_conf;

extern const struct m_obj_list vf_obj_list;
extern const struct m_obj_list af_obj_list;
extern const struct m_obj_list vo_obj_list;

extern const struct m_sub_options ao_conf;

extern const struct m_sub_options opengl_conf;
extern const struct m_sub_options vulkan_conf;
extern const struct m_sub_options spirv_conf;
extern const struct m_sub_options d3d11_conf;
extern const struct m_sub_options d3d11va_conf;
extern const struct m_sub_options angle_conf;
extern const struct m_sub_options cocoa_conf;
extern const struct m_sub_options macos_conf;
extern const struct m_sub_options android_conf;
extern const struct m_sub_options vaapi_conf;

static const struct m_sub_options screenshot_conf = {
    .opts = image_writer_opts,
    .size = sizeof(struct image_writer_opts),
    .defaults = &image_writer_opts_defaults,
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct mp_vo_opts

static const m_option_t mp_vo_opt_list[] = {
    OPT_SETTINGSLIST("vo", video_driver_list, 0, &vo_obj_list, ),
    OPT_SUBSTRUCT("sws", sws_opts, sws_conf, 0),
    OPT_FLAG("taskbar-progress", taskbar_progress, 0),
    OPT_FLAG("snap-window", snap_window, 0),
    OPT_FLAG("ontop", ontop, 0),
    OPT_CHOICE_OR_INT("ontop-level", ontop_level, 0, 0, INT_MAX,
                      ({"window", -1}, {"system", -2})),
    OPT_FLAG("border", border, 0),
    OPT_FLAG("fit-border", fit_border, 0),
    OPT_FLAG("on-all-workspaces", all_workspaces, 0),
    OPT_GEOMETRY("geometry", geometry, 0),
    OPT_SIZE_BOX("autofit", autofit, 0),
    OPT_SIZE_BOX("autofit-larger", autofit_larger, 0),
    OPT_SIZE_BOX("autofit-smaller", autofit_smaller, 0),
    OPT_DOUBLE("window-scale", window_scale, CONF_RANGE, .min = 0.001, .max = 100),
    OPT_FLAG("force-window-position", force_window_position, 0),
    OPT_STRING("x11-name", winname, 0),
    OPT_FLOATRANGE("monitoraspect", force_monitor_aspect, 0, 0.0, 9.0),
    OPT_FLOATRANGE("monitorpixelaspect", monitor_pixel_aspect, 0, 1.0/32.0, 32.0),
    OPT_FLAG("fullscreen", fullscreen, 0),
    OPT_ALIAS("fs", "fullscreen"),
    OPT_FLAG("native-keyrepeat", native_keyrepeat, 0),
    OPT_FLOATRANGE("panscan", panscan, 0, 0.0, 1.0),
    OPT_FLOATRANGE("video-zoom", zoom, 0, -20.0, 20.0),
    OPT_FLOATRANGE("video-pan-x", pan_x, 0, -3.0, 3.0),
    OPT_FLOATRANGE("video-pan-y", pan_y, 0, -3.0, 3.0),
    OPT_FLOATRANGE("video-align-x", align_x, 0, -1.0, 1.0),
    OPT_FLOATRANGE("video-align-y", align_y, 0, -1.0, 1.0),
    OPT_CHOICE("video-unscaled", unscaled, 0,
               ({"no", 0}, {"yes", 1}, {"downscale-big", 2})),
    OPT_INT64("wid", WinID, 0),
    OPT_CHOICE_OR_INT("screen", screen_id, 0, 0, 32,
                      ({"default", -1})),
    OPT_CHOICE_OR_INT("fs-screen", fsscreen_id, 0, 0, 32,
                      ({"all", -2}, {"current", -1})),
    OPT_FLAG("keepaspect", keepaspect, 0),
    OPT_FLAG("keepaspect-window", keepaspect_window, 0),
    OPT_FLAG("hidpi-window-scale", hidpi_window_scale, 0),
    OPT_FLAG("native-fs", native_fs, 0),
    OPT_DOUBLE("display-fps", override_display_fps, M_OPT_MIN, .min = 0),
    OPT_DOUBLERANGE("video-timing-offset", timing_offset, 0, 0.0, 1.0),
#if HAVE_X11
    OPT_CHOICE("x11-netwm", x11_netwm, 0,
               ({"auto", 0}, {"no", -1}, {"yes", 1})),
    OPT_CHOICE("x11-bypass-compositor", x11_bypass_compositor, 0,
               ({"no", 0}, {"yes", 1}, {"fs-only", 2}, {"never", 3})),
#endif
#if HAVE_WIN32_DESKTOP
    OPT_STRING("vo-mmcss-profile", mmcss_profile, 0),
#endif
#if HAVE_DRM
    OPT_SUBSTRUCT("", drm_opts, drm_conf, 0),
#endif
    {0}
};

const struct m_sub_options vo_sub_opts = {
    .opts = mp_vo_opt_list,
    .size = sizeof(struct mp_vo_opts),
    .defaults = &(const struct mp_vo_opts){
        .video_driver_list = NULL,
        .monitor_pixel_aspect = 1.0,
        .screen_id = -1,
        .fsscreen_id = -1,
        .panscan = 0.0f,
        .keepaspect = 1,
        .keepaspect_window = 1,
        .hidpi_window_scale = 1,
        .native_fs = 1,
        .taskbar_progress = 1,
        .snap_window = 0,
        .border = 1,
        .fit_border = 1,
        .WinID = -1,
        .window_scale = 1.0,
        .x11_bypass_compositor = 2,
        .mmcss_profile = "Playback",
        .ontop_level = -1,
        .timing_offset = 0.050,
    },
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct mp_subtitle_opts

const struct m_sub_options mp_subtitle_sub_opts = {
    .opts = (const struct m_option[]){
        OPT_FLOAT("sub-delay", sub_delay, 0),
        OPT_FLOAT("sub-fps", sub_fps, 0),
        OPT_FLOAT("sub-speed", sub_speed, 0),
        OPT_FLAG("sub-visibility", sub_visibility, 0),
        OPT_FLAG("sub-forced-only", forced_subs_only, 0),
        OPT_FLAG("stretch-dvd-subs", stretch_dvd_subs, 0),
        OPT_FLAG("stretch-image-subs-to-screen", stretch_image_subs, 0),
        OPT_FLAG("image-subs-video-resolution", image_subs_video_res, 0),
        OPT_FLAG("sub-fix-timing", sub_fix_timing, 0),
        OPT_INTRANGE("sub-pos", sub_pos, 0, 0, 100),
        OPT_FLOATRANGE("sub-gauss", sub_gauss, 0, 0.0, 3.0),
        OPT_FLAG("sub-gray", sub_gray, 0),
        OPT_FLAG("sub-ass", ass_enabled, 0),
        OPT_FLAG("sub-filter-sdh", sub_filter_SDH, 0),
        OPT_FLAG("sub-filter-sdh-harder", sub_filter_SDH_harder, 0),
        OPT_FLOATRANGE("sub-scale", sub_scale, 0, 0, 100),
        OPT_FLOATRANGE("sub-ass-line-spacing", ass_line_spacing, 0, -1000, 1000),
        OPT_FLAG("sub-use-margins", sub_use_margins, 0),
        OPT_FLAG("sub-ass-force-margins", ass_use_margins, 0),
        OPT_FLAG("sub-ass-vsfilter-aspect-compat", ass_vsfilter_aspect_compat, 0),
        OPT_CHOICE("sub-ass-vsfilter-color-compat", ass_vsfilter_color_compat, 0,
                ({"no", 0}, {"basic", 1}, {"full", 2}, {"force-601", 3})),
        OPT_FLAG("sub-ass-vsfilter-blur-compat", ass_vsfilter_blur_compat, 0),
        OPT_FLAG("embeddedfonts", use_embedded_fonts, 0),
        OPT_STRINGLIST("sub-ass-force-style", ass_force_style_list, 0),
        OPT_STRING("sub-ass-styles", ass_styles_file, M_OPT_FILE),
        OPT_CHOICE("sub-ass-hinting", ass_hinting, 0,
                ({"none", 0}, {"light", 1}, {"normal", 2}, {"native", 3})),
        OPT_CHOICE("sub-ass-shaper", ass_shaper, 0,
                ({"simple", 0}, {"complex", 1})),
        OPT_FLAG("sub-ass-justify", ass_justify, 0),
        OPT_CHOICE("sub-ass-override", ass_style_override, 0,
                ({"no", 0}, {"yes", 1}, {"force", 3}, {"scale", 4}, {"strip", 5})),
        OPT_FLAG("sub-scale-by-window", sub_scale_by_window, 0),
        OPT_FLAG("sub-scale-with-window", sub_scale_with_window, 0),
        OPT_FLAG("sub-ass-scale-with-window", ass_scale_with_window, 0),
        OPT_SUBSTRUCT("sub", sub_style, sub_style_conf, 0),
        OPT_FLAG("sub-clear-on-seek", sub_clear_on_seek, 0),
        OPT_INTRANGE("teletext-page", teletext_page, 0, 1, 999),
        {0}
    },
    .size = sizeof(OPT_BASE_STRUCT),
    .defaults = &(OPT_BASE_STRUCT){
        .sub_visibility = 1,
        .sub_pos = 100,
        .sub_speed = 1.0,
        .ass_enabled = 1,
        .sub_scale_by_window = 1,
        .ass_use_margins = 0,
        .sub_use_margins = 1,
        .ass_scale_with_window = 0,
        .sub_scale_with_window = 1,
        .teletext_page = 100,
        .sub_scale = 1,
        .ass_vsfilter_aspect_compat = 1,
        .ass_vsfilter_color_compat = 1,
        .ass_vsfilter_blur_compat = 1,
        .ass_style_override = 1,
        .ass_shaper = 1,
        .use_embedded_fonts = 1,
    },
    .change_flags = UPDATE_OSD,
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct mp_osd_render_opts

const struct m_sub_options mp_osd_render_sub_opts = {
    .opts = (const struct m_option[]){
        OPT_FLOATRANGE("osd-bar-align-x", osd_bar_align_x, 0, -1.0, +1.0),
        OPT_FLOATRANGE("osd-bar-align-y", osd_bar_align_y, 0, -1.0, +1.0),
        OPT_FLOATRANGE("osd-bar-w", osd_bar_w, 0, 1, 100),
        OPT_FLOATRANGE("osd-bar-h", osd_bar_h, 0, 0.1, 50),
        OPT_SUBSTRUCT("osd", osd_style, osd_style_conf, 0),
        OPT_FLOATRANGE("osd-scale", osd_scale, 0, 0, 100),
        OPT_FLAG("osd-scale-by-window", osd_scale_by_window, 0),
        OPT_FLAG("force-rgba-osd-rendering", force_rgba_osd, 0),
        {0}
    },
    .size = sizeof(OPT_BASE_STRUCT),
    .defaults = &(OPT_BASE_STRUCT){
        .osd_bar_align_y = 0.5,
        .osd_bar_w = 75.0,
        .osd_bar_h = 3.125,
        .osd_scale = 1,
        .osd_scale_by_window = 1,
    },
    .change_flags = UPDATE_OSD,
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct dvd_opts

const struct m_sub_options dvd_conf = {
    .opts = (const struct m_option[]){
        OPT_STRING("dvd-device", device, M_OPT_FILE),
        OPT_INT("dvd-speed", speed, 0),
        OPT_INTRANGE("dvd-angle", angle, 0, 1, 99),
        {0}
    },
    .size = sizeof(struct dvd_opts),
    .defaults = &(const struct dvd_opts){
        .angle = 1,
    },
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct filter_opts

const struct m_sub_options filter_conf = {
    .opts = (const struct m_option[]){
        OPT_FLAG("deinterlace", deinterlace, 0),
        {0}
    },
    .size = sizeof(OPT_BASE_STRUCT),
    .change_flags = UPDATE_IMGPAR,
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct MPOpts

const m_option_t mp_opts[] = {
    // handled in command line pre-parser (parse_commandline.c)
    {"v", &m_option_type_dummy_flag, M_OPT_FIXED | CONF_NOCFG | M_OPT_NOPROP,
     .offset = -1},
    {"playlist", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_MIN | M_OPT_FIXED | M_OPT_FILE,
     .min = 1, .offset = -1},
    {"{", &m_option_type_dummy_flag, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP,
     .offset = -1},
    {"}", &m_option_type_dummy_flag, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP,
     .offset = -1},

    // handled in m_config.c
    { "include", CONF_TYPE_STRING, M_OPT_FILE, .offset = -1},
    { "profile", CONF_TYPE_STRING_LIST, 0, .offset = -1},
    { "show-profile", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP,
      .offset = -1},
    { "list-options", &m_option_type_dummy_flag, CONF_NOCFG | M_OPT_FIXED |
      M_OPT_NOPROP, .offset = -1},
    OPT_FLAG("list-properties", property_print_help,
             CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP),
    { "help", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP |
              M_OPT_OPTIONAL_PARAM, .offset = -1},
    { "h", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP |
           M_OPT_OPTIONAL_PARAM, .offset = -1},

    OPT_PRINT("list-protocols", stream_print_proto_list),
    OPT_PRINT("version", print_version),
    OPT_PRINT("V", print_version),

    OPT_CHOICE("player-operation-mode", operation_mode,
               M_OPT_FIXED | M_OPT_PRE_PARSE | M_OPT_NOPROP,
               ({"cplayer", 0}, {"pseudo-gui", 1})),

    OPT_FLAG("shuffle", shuffle, 0),

// ------------------------- common options --------------------
    OPT_FLAG("quiet", quiet, 0),
    OPT_FLAG("really-quiet", msg_really_quiet, CONF_PRE_PARSE | UPDATE_TERM),
    OPT_FLAG("terminal", use_terminal, CONF_PRE_PARSE | UPDATE_TERM),
    OPT_GENERAL(char**, "msg-level", msg_levels, CONF_PRE_PARSE | UPDATE_TERM,
                .type = &m_option_type_msglevels),
    OPT_STRING("dump-stats", dump_stats, UPDATE_TERM | CONF_PRE_PARSE),
    OPT_FLAG("msg-color", msg_color, CONF_PRE_PARSE | UPDATE_TERM),
    OPT_STRING("log-file", log_file, CONF_PRE_PARSE | M_OPT_FILE | UPDATE_TERM),
    OPT_FLAG("msg-module", msg_module, UPDATE_TERM),
    OPT_FLAG("msg-time", msg_time, UPDATE_TERM),
#if HAVE_WIN32_DESKTOP
    OPT_CHOICE("priority", w32_priority, UPDATE_PRIORITY,
               ({"no",          0},
                {"realtime",    REALTIME_PRIORITY_CLASS},
                {"high",        HIGH_PRIORITY_CLASS},
                {"abovenormal", ABOVE_NORMAL_PRIORITY_CLASS},
                {"normal",      NORMAL_PRIORITY_CLASS},
                {"belownormal", BELOW_NORMAL_PRIORITY_CLASS},
                {"idle",        IDLE_PRIORITY_CLASS})),
#endif
    OPT_FLAG("config", load_config, M_OPT_FIXED | CONF_PRE_PARSE),
    OPT_STRING("config-dir", force_configdir,
               M_OPT_FIXED | CONF_NOCFG | CONF_PRE_PARSE | M_OPT_FILE),
    OPT_STRINGLIST("reset-on-next-file", reset_options, 0),

#if HAVE_LUA || HAVE_JAVASCRIPT
    OPT_PATHLIST("scripts", script_files, M_OPT_FIXED),
    OPT_CLI_ALIAS("script", "scripts-append"),
    OPT_KEYVALUELIST("script-opts", script_opts, 0),
    OPT_FLAG("load-scripts", auto_load_scripts, 0),
#endif
#if HAVE_LUA
    OPT_FLAG("osc", lua_load_osc, UPDATE_BUILTIN_SCRIPTS),
    OPT_FLAG("ytdl", lua_load_ytdl, UPDATE_BUILTIN_SCRIPTS),
    OPT_STRING("ytdl-format", lua_ytdl_format, 0),
    OPT_KEYVALUELIST("ytdl-raw-options", lua_ytdl_raw_options, 0),
    OPT_FLAG("load-stats-overlay", lua_load_stats, UPDATE_BUILTIN_SCRIPTS),
#endif

// ------------------------- stream options --------------------

#if HAVE_DVDREAD || HAVE_DVDNAV
    OPT_SUBSTRUCT("", dvd_opts, dvd_conf, 0),
#endif /* HAVE_DVDREAD */
    OPT_INTPAIR("chapter", chapterrange, 0, .deprecation_message = "instead of "
        "--chapter=A-B use --start=#A --end=#B+1"),
    OPT_CHOICE_OR_INT("edition", edition_id, 0, 0, 8190,
                      ({"auto", -1})),
#if HAVE_LIBBLURAY
    OPT_STRING("bluray-device", bluray_device, M_OPT_FILE),
#endif /* HAVE_LIBBLURAY */

// ------------------------- demuxer options --------------------

    OPT_CHOICE_OR_INT("frames", play_frames, 0, 0, INT_MAX, ({"all", -1})),

    OPT_REL_TIME("start", play_start, 0),
    OPT_REL_TIME("end", play_end, 0),
    OPT_REL_TIME("length", play_length, 0),

    OPT_FLAG("rebase-start-time", rebase_start_time, 0),

    OPT_TIME("ab-loop-a", ab_loop[0], 0, .min = MP_NOPTS_VALUE),
    OPT_TIME("ab-loop-b", ab_loop[1], 0, .min = MP_NOPTS_VALUE),

    OPT_CHOICE_OR_INT("playlist-start", playlist_pos, 0, 0, INT_MAX,
                      ({"auto", -1}, {"no", -1})),

    OPT_FLAG("pause", pause, 0),
    OPT_CHOICE("keep-open", keep_open, 0,
               ({"no", 0},
                {"yes", 1},
                {"always", 2})),
    OPT_FLAG("keep-open-pause", keep_open_pause, 0),
    OPT_DOUBLE("image-display-duration", image_display_duration,
               M_OPT_RANGE, 0, INFINITY),

    OPT_CHOICE("index", index_mode, 0, ({"default", 1}, {"recreate", 0})),

    // select audio/video/subtitle stream
    OPT_TRACKCHOICE("aid", stream_id[0][STREAM_AUDIO]),
    OPT_TRACKCHOICE("vid", stream_id[0][STREAM_VIDEO]),
    OPT_TRACKCHOICE("sid", stream_id[0][STREAM_SUB]),
    OPT_TRACKCHOICE("secondary-sid", stream_id[1][STREAM_SUB]),
    OPT_ALIAS("sub", "sid"),
    OPT_ALIAS("video", "vid"),
    OPT_ALIAS("audio", "aid"),
    OPT_STRINGLIST("alang", stream_lang[STREAM_AUDIO], 0),
    OPT_STRINGLIST("slang", stream_lang[STREAM_SUB], 0),
    OPT_STRINGLIST("vlang", stream_lang[STREAM_VIDEO], 0),
    OPT_FLAG("track-auto-selection", stream_auto_sel, 0),

    OPT_STRING("lavfi-complex", lavfi_complex, UPDATE_LAVFI_COMPLEX),

    OPT_CHOICE("audio-display", audio_display, 0,
               ({"no", 0}, {"attachment", 1})),

    OPT_CHOICE_OR_INT("hls-bitrate", hls_bitrate, 0, 0, INT_MAX,
                      ({"no", -1}, {"min", 0}, {"max", INT_MAX})),

    OPT_STRINGLIST("display-tags", display_tags, 0),

#if HAVE_CDDA
    OPT_SUBSTRUCT("cdda", stream_cdda_opts, stream_cdda_conf, 0),
    OPT_STRING("cdrom-device", cdrom_device, M_OPT_FILE),
#endif

    // demuxer.c - select audio/sub file/demuxer
    OPT_PATHLIST("audio-files", audio_files, 0),
    OPT_CLI_ALIAS("audio-file", "audio-files-append"),
    OPT_STRING("demuxer", demuxer_name, 0),
    OPT_STRING("audio-demuxer", audio_demuxer_name, 0),
    OPT_STRING("sub-demuxer", sub_demuxer_name, 0),
    OPT_FLAG("demuxer-thread", demuxer_thread, 0),
    OPT_DOUBLE("demuxer-termination-timeout", demux_termination_timeout, 0),
    OPT_FLAG("prefetch-playlist", prefetch_open, 0),
    OPT_FLAG("cache-pause", cache_pause, 0),
    OPT_FLAG("cache-pause-initial", cache_pause_initial, 0),
    OPT_FLOAT("cache-pause-wait", cache_pause_wait, M_OPT_MIN, .min = 0),

    OPT_DOUBLE("mf-fps", mf_fps, 0),
    OPT_STRING("mf-type", mf_type, 0),
#if HAVE_TV
    OPT_SUBSTRUCT("tv", tv_params, tv_params_conf, 0),
#endif /* HAVE_TV */
#if HAVE_DVBIN
    OPT_SUBSTRUCT("dvbin", stream_dvb_opts, stream_dvb_conf, 0),
#endif
    OPT_SUBSTRUCT("", stream_lavf_opts, stream_lavf_conf, 0),

// ------------------------- a-v sync options --------------------

    // set A-V sync correction speed (0=disables it):
    OPT_FLOATRANGE("mc", default_max_pts_correction, 0, 0, 100),

    // force video/audio rate:
    OPT_DOUBLE("fps", force_fps, CONF_MIN, .min = 0),
    OPT_INTRANGE("audio-samplerate", force_srate, UPDATE_AUDIO, 0, 16*48000),
    OPT_CHANNELS("audio-channels", audio_output_channels, UPDATE_AUDIO),
    OPT_AUDIOFORMAT("audio-format", audio_output_format, UPDATE_AUDIO),
    OPT_DOUBLE("speed", playback_speed, M_OPT_RANGE, .min = 0.01, .max = 100.0),

    OPT_FLAG("audio-pitch-correction", pitch_correction, 0),

    // set a-v distance
    OPT_FLOAT("audio-delay", audio_delay, 0),

// ------------------------- codec/vfilter options --------------------

    OPT_SETTINGSLIST("af-defaults", af_defs, 0, &af_obj_list,
                     .deprecation_message = "use --af + enable/disable flags"),
    OPT_SETTINGSLIST("af", af_settings, 0, &af_obj_list, ),
    OPT_SETTINGSLIST("vf-defaults", vf_defs, 0, &vf_obj_list,
                     .deprecation_message = "use --vf + enable/disable flags"),
    OPT_SETTINGSLIST("vf", vf_settings, 0, &vf_obj_list, ),

    OPT_SUBSTRUCT("", filter_opts, filter_conf, 0),

    OPT_STRING("ad", audio_decoders, 0),
    OPT_STRING("vd", video_decoders, 0),

    OPT_STRING("audio-spdif", audio_spdif, 0),

    // -1 means auto aspect (prefer container size until aspect change)
    //  0 means square pixels
    OPT_ASPECT("video-aspect", movie_aspect, UPDATE_IMGPAR, -1.0, 10.0),
    OPT_CHOICE("video-aspect-method", aspect_method, UPDATE_IMGPAR,
               ({"bitstream", 1}, {"container", 2})),

    OPT_SUBSTRUCT("", vd_lavc_params, vd_lavc_conf, 0),
    OPT_SUBSTRUCT("ad-lavc", ad_lavc_params, ad_lavc_conf, 0),

    OPT_SUBSTRUCT("", demux_lavf, demux_lavf_conf, 0),
    OPT_SUBSTRUCT("demuxer-rawaudio", demux_rawaudio, demux_rawaudio_conf, 0),
    OPT_SUBSTRUCT("demuxer-rawvideo", demux_rawvideo, demux_rawvideo_conf, 0),
    OPT_SUBSTRUCT("demuxer-mkv", demux_mkv, demux_mkv_conf, 0),

// ------------------------- subtitles options --------------------

    OPT_PATHLIST("sub-files", sub_name, 0),
    OPT_CLI_ALIAS("sub-file", "sub-files-append"),
    OPT_PATHLIST("sub-file-paths", sub_paths, 0),
    OPT_PATHLIST("audio-file-paths", audiofile_paths, 0),
    OPT_PATHLIST("external-files", external_files, 0),
    OPT_CLI_ALIAS("external-file", "external-files-append"),
    OPT_FLAG("autoload-files", autoload_files, 0),
    OPT_CHOICE("sub-auto", sub_auto, 0,
               ({"no", -1}, {"exact", 0}, {"fuzzy", 1}, {"all", 2})),
    OPT_CHOICE("audio-file-auto", audiofile_auto, 0,
               ({"no", -1}, {"exact", 0}, {"fuzzy", 1}, {"all", 2})),

    OPT_SUBSTRUCT("", subs_rend, mp_subtitle_sub_opts, 0),
    OPT_SUBSTRUCT("", osd_rend, mp_osd_render_sub_opts, 0),

    OPT_FLAG("osd-bar", osd_bar_visible, UPDATE_OSD),

//---------------------- libao/libvo options ------------------------
    OPT_SUBSTRUCT("", ao_opts, ao_conf, 0),
    OPT_FLAG("audio-exclusive", audio_exclusive, UPDATE_AUDIO),
    OPT_FLAG("audio-fallback-to-null", ao_null_fallback, 0),
    OPT_FLAG("audio-stream-silence", audio_stream_silence, 0),
    OPT_FLOATRANGE("audio-wait-open", audio_wait_open, 0, 0, 60),
    OPT_CHOICE("force-window", force_vo, 0,
               ({"no", 0}, {"yes", 1}, {"immediate", 2})),

    OPT_FLOATRANGE("volume-max", softvol_max, 0, 100, 1000),
    // values <0 for volume and mute are legacy and ignored
    OPT_FLOATRANGE("volume", softvol_volume, UPDATE_VOL, -1, 1000),
    OPT_CHOICE("mute", softvol_mute, UPDATE_VOL,
               ({"no", 0},
                {"auto", 0},
                {"yes", 1})),
    OPT_CHOICE("replaygain", rgain_mode, UPDATE_VOL,
               ({"no", 0},
                {"track", 1},
                {"album", 2})),
    OPT_FLOATRANGE("replaygain-preamp", rgain_preamp, UPDATE_VOL, -15, 15),
    OPT_FLAG("replaygain-clip", rgain_clip, UPDATE_VOL),
    OPT_FLOATRANGE("replaygain-fallback", rgain_fallback, UPDATE_VOL, -200, 60),
    OPT_CHOICE("gapless-audio", gapless_audio, 0,
               ({"no", 0},
                {"yes", 1},
                {"weak", -1})),

    OPT_STRING("title", wintitle, 0),
    OPT_STRING("force-media-title", media_title, 0),
    OPT_CHOICE_OR_INT("video-rotate", video_rotate, UPDATE_IMGPAR, 0, 359,
                      ({"no", -1})),

    OPT_CHOICE_OR_INT("cursor-autohide", cursor_autohide_delay, 0,
                      0, 30000, ({"no", -1}, {"always", -2})),
    OPT_FLAG("cursor-autohide-fs-only", cursor_autohide_fs, 0),
    OPT_FLAG("stop-screensaver", stop_screensaver, UPDATE_SCREENSAVER),

    OPT_SUBSTRUCT("", video_equalizer, mp_csp_equalizer_conf, 0),

    OPT_FLAG("use-filedir-conf", use_filedir_conf, 0),
    OPT_CHOICE("osd-level", osd_level, 0,
               ({"0", 0}, {"1", 1}, {"2", 2}, {"3", 3})),
    OPT_CHOICE("osd-on-seek", osd_on_seek, 0,
               ({"no", 0},
                {"bar", 1},
                {"msg", 2},
                {"msg-bar", 3})),
    OPT_INTRANGE("osd-duration", osd_duration, 0, 0, 3600000),
    OPT_FLAG("osd-fractions", osd_fractions, 0),

    OPT_DOUBLE("sstep", step_sec, CONF_MIN, 0),

    OPT_CHOICE("framedrop", frame_dropping, 0,
               ({"no", 0},
                {"vo", 1},
                {"decoder", 2},
                {"decoder+vo", 3})),
    OPT_FLAG("video-latency-hacks", video_latency_hacks, 0),

    OPT_FLAG("untimed", untimed, 0),

    OPT_STRING("stream-dump", stream_dump, M_OPT_FILE),

    OPT_FLAG("stop-playback-on-init-failure", stop_playback_on_init_failure, 0),

    OPT_CHOICE_OR_INT("loop-playlist", loop_times, 0, 1, 10000,
                      ({"no", 1},
                       {"inf", -1}, {"yes", -1},
                       {"force", -2})),
    OPT_CHOICE_OR_INT("loop-file", loop_file, 0, 0, 10000,
                      ({"no", 0},
                       {"yes", -1},
                       {"inf", -1})),
    OPT_ALIAS("loop", "loop-file"),

    OPT_FLAG("resume-playback", position_resume, 0),
    OPT_FLAG("save-position-on-quit", position_save_on_quit, 0),
    OPT_FLAG("write-filename-in-watch-later-config", write_filename_in_watch_later_config, 0),
    OPT_FLAG("ignore-path-in-watch-later-config", ignore_path_in_watch_later_config, 0),
    OPT_STRING("watch-later-directory", watch_later_directory, M_OPT_FILE),

    OPT_FLAG("ordered-chapters", ordered_chapters, 0),
    OPT_STRING("ordered-chapters-files", ordered_chapters_files, M_OPT_FILE),
    OPT_INTRANGE("chapter-merge-threshold", chapter_merge_threshold, 0, 0, 10000),

    OPT_DOUBLE("chapter-seek-threshold", chapter_seek_threshold, 0),

    OPT_STRING("chapters-file", chapter_file, M_OPT_FILE),

    OPT_FLAG("load-unsafe-playlists", load_unsafe_playlists, 0),
    OPT_FLAG("merge-files", merge_files, 0),

    // a-v sync stuff:
    OPT_FLAG("correct-pts", correct_pts, 0),
    OPT_FLAG("initial-audio-sync", initial_audio_sync, 0),
    OPT_CHOICE("video-sync", video_sync, 0,
               ({"audio", VS_DEFAULT},
                {"display-resample", VS_DISP_RESAMPLE},
                {"display-resample-vdrop", VS_DISP_RESAMPLE_VDROP},
                {"display-resample-desync", VS_DISP_RESAMPLE_NONE},
                {"display-adrop", VS_DISP_ADROP},
                {"display-vdrop", VS_DISP_VDROP},
                {"display-desync", VS_DISP_NONE},
                {"desync", VS_NONE})),
    OPT_DOUBLE("video-sync-max-video-change", sync_max_video_change,
               M_OPT_MIN, .min = 0),
    OPT_DOUBLE("video-sync-max-audio-change", sync_max_audio_change,
               M_OPT_MIN | M_OPT_MAX, .min = 0, .max = 1),
    OPT_DOUBLE("video-sync-adrop-size", sync_audio_drop_size,
               M_OPT_MIN | M_OPT_MAX, .min = 0, .max = 1),
    OPT_CHOICE("hr-seek", hr_seek, 0,
               ({"no", -1}, {"absolute", 0}, {"yes", 1}, {"always", 1})),
    OPT_FLOAT("hr-seek-demuxer-offset", hr_seek_demuxer_offset, 0),
    OPT_FLAG("hr-seek-framedrop", hr_seek_framedrop, 0),
    OPT_CHOICE_OR_INT("autosync", autosync, 0, 0, 10000,
                      ({"no", -1})),

    OPT_CHOICE("term-osd", term_osd, 0,
               ({"force", 1},
                {"auto", 2},
                {"no", 0})),

    OPT_FLAG("term-osd-bar", term_osd_bar, 0),
    OPT_STRING("term-osd-bar-chars", term_osd_bar_chars, 0),

    OPT_STRING("term-playing-msg", playing_msg, 0),
    OPT_STRING("osd-playing-msg", osd_playing_msg, 0),
    OPT_STRING("term-status-msg", status_msg, 0),
    OPT_STRING("osd-status-msg", osd_status_msg, 0),
    OPT_STRING("osd-msg1", osd_msg[0], 0),
    OPT_STRING("osd-msg2", osd_msg[1], 0),
    OPT_STRING("osd-msg3", osd_msg[2], 0),

    OPT_FLAG("video-osd", video_osd, 0),

    OPT_CHOICE("idle", player_idle_mode, 0,
               ({"no",   0},
                {"once", 1},
                {"yes",  2})),

    OPT_FLAG("input-terminal", consolecontrols, UPDATE_TERM),

    OPT_STRING("input-file", input_file, M_OPT_FILE | UPDATE_INPUT),
    OPT_STRING("input-ipc-server", ipc_path, M_OPT_FILE | UPDATE_INPUT),

    OPT_SUBSTRUCT("screenshot", screenshot_image_opts, screenshot_conf, 0),
    OPT_STRING("screenshot-template", screenshot_template, 0),
    OPT_STRING("screenshot-directory", screenshot_directory, M_OPT_FILE),

    OPT_STRING("record-file", record_file, M_OPT_FILE),

    OPT_SUBSTRUCT("", resample_opts, resample_conf, 0),

    OPT_SUBSTRUCT("", input_opts, input_config, 0),

    OPT_SUBSTRUCT("", vo, vo_sub_opts, 0),
    OPT_SUBSTRUCT("", demux_opts, demux_conf, 0),

    OPT_SUBSTRUCT("", gl_video_opts, gl_video_conf, 0),
    OPT_SUBSTRUCT("", spirv_opts, spirv_conf, 0),

#if HAVE_GL
    OPT_SUBSTRUCT("", opengl_opts, opengl_conf, 0),
#endif

#if HAVE_VULKAN
    OPT_SUBSTRUCT("", vulkan_opts, vulkan_conf, 0),
#endif

#if HAVE_D3D11
    OPT_SUBSTRUCT("", d3d11_opts, d3d11_conf, 0),
#if HAVE_D3D_HWACCEL
    OPT_SUBSTRUCT("", d3d11va_opts, d3d11va_conf, 0),
#endif
#endif

#if HAVE_EGL_ANGLE_WIN32
    OPT_SUBSTRUCT("", angle_opts, angle_conf, 0),
#endif

#if HAVE_GL_COCOA
    OPT_SUBSTRUCT("", cocoa_opts, cocoa_conf, 0),
#endif

#if HAVE_MACOS_COCOA_CB
    OPT_SUBSTRUCT("", macos_opts, macos_conf, 0),
#endif

#if HAVE_EGL_ANDROID
    OPT_SUBSTRUCT("", android_opts, android_conf, 0),
#endif

#if HAVE_GL_WIN32
    OPT_CHOICE("opengl-dwmflush", wingl_dwm_flush, 0,
               ({"no", -1}, {"auto", 0}, {"windowed", 1}, {"yes", 2})),
#endif

#if HAVE_CUDA_HWACCEL
    OPT_CHOICE_OR_INT("cuda-decode-device", cuda_device, 0,
                      0, INT_MAX, ({"auto", -1})),
#endif

#if HAVE_VAAPI
    OPT_SUBSTRUCT("vaapi", vaapi_opts, vaapi_conf, 0),
#endif

    OPT_SUBSTRUCT("", encode_opts, encode_config, 0),

    OPT_REMOVED("a52drc", "use --ad-lavc-ac3drc=level"),
    OPT_REMOVED("afm", "use --ad=..."),
    OPT_REPLACED("aspect", "video-aspect"),
    OPT_REMOVED("ass-bottom-margin", "use --vf=sub=bottom:top"),
    OPT_REPLACED("ass", "sub-ass"),
    OPT_REPLACED("audiofile", "audio-file"),
    OPT_REMOVED("benchmark", "use --untimed (no stats)"),
    OPT_REMOVED("capture", NULL),
    OPT_REMOVED("stream-capture", NULL),
    OPT_REMOVED("channels", "use --audio-channels (changed semantics)"),
    OPT_REPLACED("cursor-autohide-delay", "cursor-autohide"),
    OPT_REPLACED("delay", "audio-delay"),
    OPT_REMOVED("dumpstream", "use --stream-dump=<filename>"),
    OPT_REPLACED("dvdangle", "dvd-angle"),
    OPT_REPLACED("endpos", "length"),
    OPT_REPLACED("font", "osd-font"),
    OPT_REPLACED("forcedsubsonly", "sub-forced-only"),
    OPT_REPLACED("format", "audio-format"),
    OPT_REMOVED("hardframedrop", NULL),
    OPT_REMOVED("identify", "use TOOLS/mpv_identify.sh"),
    OPT_REMOVED("lavdopts", "use --vd-lavc-..."),
    OPT_REMOVED("lavfdopts", "use --demuxer-lavf-..."),
    OPT_REPLACED("lua", "script"),
    OPT_REPLACED("lua-opts", "script-opts"),
    OPT_REMOVED("mixer-channel", "use AO suboptions (alsa, oss)"),
    OPT_REMOVED("mixer", "use AO suboptions (alsa, oss)"),
    OPT_REPLACED("mouse-movements", "input-cursor"),
    OPT_REPLACED("msgcolor", "msg-color"),
    OPT_REMOVED("msglevel", "use --msg-level (changed semantics)"),
    OPT_REPLACED("msgmodule", "msg-module"),
    OPT_REPLACED("name", "x11-name"),
    OPT_REPLACED("noar", "no-input-appleremote"),
    OPT_REPLACED("noautosub", "no-sub-auto"),
    OPT_REPLACED("noconsolecontrols", "no-input-terminal"),
    OPT_REPLACED("nosound", "no-audio"),
    OPT_REPLACED("osdlevel", "osd-level"),
    OPT_REMOVED("panscanrange", "use --video-zoom, --video-pan-x/y"),
    OPT_REPLACED("playing-msg", "term-playing-msg"),
    OPT_REMOVED("pp", NULL),
    OPT_REMOVED("pphelp", NULL),
    OPT_REMOVED("rawaudio", "use --demuxer-rawaudio-..."),
    OPT_REMOVED("rawvideo", "use --demuxer-rawvideo-..."),
    OPT_REPLACED("spugauss", "sub-gauss"),
    OPT_REPLACED("srate", "audio-samplerate"),
    OPT_REPLACED("ss", "start"),
    OPT_REPLACED("stop-xscreensaver", "stop-screensaver"),
    OPT_REPLACED("sub-fuzziness", "sub-auto"),
    OPT_REPLACED("subcp", "sub-codepage"),
    OPT_REPLACED("subdelay", "sub-delay"),
    OPT_REPLACED("subfile", "sub-file"),
    OPT_REPLACED("subfont-text-scale", "sub-scale"),
    OPT_REPLACED("subfont", "sub-text-font"),
    OPT_REPLACED("subfps", "sub-fps"),
    OPT_REPLACED("subpos", "sub-pos"),
    OPT_REPLACED("tvscan", "tv-scan"),
    OPT_REMOVED("use-filename-title", "use --title='${filename}'"),
    OPT_REMOVED("vc", "use --vd=..., --hwdec=..."),
    OPT_REMOVED("vobsub", "use --sub-file (pass the .idx file)"),
    OPT_REMOVED("xineramascreen", "use --screen (different values)"),
    OPT_REMOVED("xy", "use --autofit"),
    OPT_REMOVED("zoom", "Inverse available as ``--video-unscaled"),
    OPT_REPLACED("media-keys", "input-media-keys"),
    OPT_REPLACED("right-alt-gr", "input-right-alt-gr"),
    OPT_REPLACED("autosub", "sub-auto"),
    OPT_REPLACED("autosub-match", "sub-auto"),
    OPT_REPLACED("status-msg", "term-status-msg"),
    OPT_REPLACED("idx", "index"),
    OPT_REPLACED("forceidx", "index"),
    OPT_REMOVED("cache-pause-below", "for 'no', use --no-cache-pause"),
    OPT_REMOVED("no-cache-pause-below", "use --no-cache-pause"),
    OPT_REMOVED("volstep", "edit input.conf directly instead"),
    OPT_REMOVED("fixed-vo", "--fixed-vo=yes is now the default"),
    OPT_REPLACED("mkv-subtitle-preroll", "demuxer-mkv-subtitle-preroll"),
    OPT_REPLACED("ass-use-margins", "sub-use-margins"),
    OPT_REPLACED("media-title", "force-media-title"),
    OPT_REPLACED("input-unix-socket", "input-ipc-server"),
    OPT_REPLACED("softvol-max", "volume-max"),
    OPT_REMOVED("bluray-angle", "this didn't do anything for a few releases"),
    OPT_REPLACED("playlist-pos", "playlist-start"),
    OPT_REPLACED("sub-text-font", "sub-font"),
    OPT_REPLACED("sub-text-font-size", "sub-font-size"),
    OPT_REPLACED("sub-text-color", "sub-color"),
    OPT_REPLACED("sub-text-border-color", "sub-border-color"),
    OPT_REPLACED("sub-text-shadow-color", "sub-shadow-color"),
    OPT_REPLACED("sub-text-back-color", "sub-back-color"),
    OPT_REPLACED("sub-text-border-size", "sub-border-size"),
    OPT_REPLACED("sub-text-shadow-offset", "sub-shadow-offset"),
    OPT_REPLACED("sub-text-spacing", "sub-spacing"),
    OPT_REPLACED("sub-text-margin-x", "sub-margin-x"),
    OPT_REPLACED("sub-text-margin-y", "sub-margin-y"),
    OPT_REPLACED("sub-text-align-x", "sub-align-x"),
    OPT_REPLACED("sub-text-align-y", "sub-align-y"),
    OPT_REPLACED("sub-text-blur", "sub-blur"),
    OPT_REPLACED("sub-text-bold", "sub-bold"),
    OPT_REPLACED("sub-text-italic", "sub-italic"),
    OPT_REPLACED("ass-line-spacing", "sub-ass-line-spacing"),
    OPT_REPLACED("ass-force-margins", "sub-ass-force-margins"),
    OPT_REPLACED("ass-vsfilter-aspect-compat", "sub-ass-vsfilter-aspect-compat"),
    OPT_REPLACED("ass-vsfilter-color-compat", "sub-ass-vsfilter-color-compat"),
    OPT_REPLACED("ass-vsfilter-blur-compat", "sub-ass-vsfilter-blur-compat"),
    OPT_REPLACED("ass-force-style", "sub-ass-force-style"),
    OPT_REPLACED("ass-styles", "sub-ass-styles"),
    OPT_REPLACED("ass-hinting", "sub-ass-hinting"),
    OPT_REPLACED("ass-shaper", "sub-ass-shaper"),
    OPT_REPLACED("ass-style-override", "sub-ass-style-override"),
    OPT_REPLACED("ass-scale-with-window", "sub-ass-scale-with-window"),
    OPT_REPLACED("sub-ass-style-override", "sub-ass-override"),
    OPT_REMOVED("fs-black-out-screens", NULL),
    OPT_REPLACED("sub-paths", "sub-file-paths"),
    OPT_REMOVED("heartbeat-cmd", "use Lua scripting instead"),
    OPT_REMOVED("no-ometadata", "use --no-ocopy-metadata"),

    {0}
};

const struct MPOpts mp_default_opts = {
    .use_terminal = 1,
    .msg_color = 1,
    .audio_decoders = NULL,
    .video_decoders = NULL,
    .softvol_max = 130,
    .softvol_volume = 100,
    .softvol_mute = 0,
    .gapless_audio = -1,
    .wintitle = "${?media-title:${media-title}}${!media-title:No file} - mpv",
    .stop_screensaver = 1,
    .cursor_autohide_delay = 1000,
    .video_osd = 1,
    .osd_level = 1,
    .osd_on_seek = 1,
    .osd_duration = 1000,
#if HAVE_LUA
    .lua_load_osc = 1,
    .lua_load_ytdl = 1,
    .lua_ytdl_format = NULL,
    .lua_ytdl_raw_options = NULL,
    .lua_load_stats = 1,
#endif
    .auto_load_scripts = 1,
    .loop_times = 1,
    .ordered_chapters = 1,
    .chapter_merge_threshold = 100,
    .chapter_seek_threshold = 5.0,
    .hr_seek_framedrop = 1,
    .sync_max_video_change = 1,
    .sync_max_audio_change = 0.125,
    .sync_audio_drop_size = 0.020,
    .load_config = 1,
    .position_resume = 1,
    .autoload_files = 1,
    .demuxer_thread = 1,
    .demux_termination_timeout = 0.1,
    .hls_bitrate = INT_MAX,
    .cache_pause = 1,
    .cache_pause_wait = 1.0,
    .chapterrange = {-1, -1},
    .ab_loop = {MP_NOPTS_VALUE, MP_NOPTS_VALUE},
    .edition_id = -1,
    .default_max_pts_correction = -1,
    .correct_pts = 1,
    .initial_audio_sync = 1,
    .frame_dropping = 1,
    .term_osd = 2,
    .term_osd_bar_chars = "[-+-]",
    .consolecontrols = 1,
    .playlist_pos = -1,
    .play_frames = -1,
    .rebase_start_time = 1,
    .keep_open = 0,
    .keep_open_pause = 1,
    .image_display_duration = 1.0,
    .stream_id = { { [STREAM_AUDIO] = -1,
                     [STREAM_VIDEO] = -1,
                     [STREAM_SUB] = -1, },
                   { [STREAM_AUDIO] = -2,
                     [STREAM_VIDEO] = -2,
                     [STREAM_SUB] = -2, }, },
    .stream_auto_sel = 1,
    .audio_display = 1,
    .audio_output_format = 0,  // AF_FORMAT_UNKNOWN
    .playback_speed = 1.,
    .pitch_correction = 1,
    .movie_aspect = -1.,
    .aspect_method = 2,
    .sub_auto = 0,
    .audiofile_auto = -1,
    .osd_bar_visible = 1,
    .screenshot_template = "mpv-shot%n",

    .audio_output_channels = {
        .set = 1,
        .auto_safe = 1,
    },

    .index_mode = 1,

    .mf_fps = 1.0,

    .display_tags = (char **)(const char*[]){
        "Artist", "Album", "Album_Artist", "Comment", "Composer",
        "Date", "Description", "Genre", "Performer", "Rating",
        "Series", "Title", "Track", "icy-title", "service_name",
        NULL
    },

    .cuda_device = -1,
};

#endif /* MPLAYER_CFG_MPLAYER_H */
