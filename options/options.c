/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_CFG_MPLAYER_H
#define MPLAYER_CFG_MPLAYER_H

/*
 * config for cfgparser
 */

#include <stddef.h>
#include <sys/types.h>
#include <limits.h>

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
#include "sub/osd.h"
#include "audio/mixer.h"
#include "audio/filter/af.h"
#include "audio/decode/dec_audio.h"
#include "player/core.h"
#include "player/command.h"
#include "stream/stream.h"

extern const char mp_help_text[];

static void print_version(struct mp_log *log)
{
    mp_print_version(log, true);
}

static void print_help(struct mp_log *log)
{
    mp_info(log, "%s", mp_help_text);
}

extern const struct m_sub_options tv_params_conf;
extern const struct m_sub_options stream_pvr_conf;
extern const struct m_sub_options stream_cdda_conf;
extern const struct m_sub_options stream_dvb_conf;
extern const struct m_sub_options stream_lavf_conf;
extern const struct m_sub_options sws_conf;
extern const struct m_sub_options demux_rawaudio_conf;
extern const struct m_sub_options demux_rawvideo_conf;
extern const struct m_sub_options demux_lavf_conf;
extern const struct m_sub_options demux_mkv_conf;
extern const struct m_sub_options vd_lavc_conf;
extern const struct m_sub_options ad_lavc_conf;
extern const struct m_sub_options input_config;
extern const struct m_sub_options encode_config;
extern const struct m_sub_options image_writer_conf;

extern const struct m_obj_list vf_obj_list;
extern const struct m_obj_list af_obj_list;
extern const struct m_obj_list vo_obj_list;
extern const struct m_obj_list ao_obj_list;

#define OPT_BASE_STRUCT struct MPOpts

const m_option_t mp_opts[] = {
    // handled in command line pre-parser (parse_commandline.c)
    {"v", CONF_TYPE_STORE, CONF_GLOBAL | CONF_NOCFG, .offset = -1},
    {"playlist", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_MIN | M_OPT_FIXED | M_OPT_FILE,
     .min = 1, .offset = -1},
    {"{", CONF_TYPE_STORE, CONF_NOCFG | M_OPT_FIXED, .offset = -1},
    {"}", CONF_TYPE_STORE, CONF_NOCFG | M_OPT_FIXED, .offset = -1},

    // handled in m_config.c
    { "include", CONF_TYPE_STRING, M_OPT_FIXED | M_OPT_FILE, .offset = -1},
    { "profile", CONF_TYPE_STRING_LIST, M_OPT_FIXED, .offset = -1},
    { "show-profile", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED, .offset = -1},
    { "list-options", CONF_TYPE_STORE, CONF_NOCFG | M_OPT_FIXED, .offset = -1},

    // handled in main.c (looks at the raw argv[])
    { "leak-report", CONF_TYPE_STORE, CONF_GLOBAL | CONF_NOCFG | M_OPT_FIXED,
      .offset = -1 },

    OPT_FLAG("shuffle", shuffle, 0),

// ------------------------- common options --------------------
    OPT_FLAG("quiet", quiet, CONF_GLOBAL),
    OPT_FLAG_STORE("really-quiet", verbose, CONF_GLOBAL | CONF_PRE_PARSE, -10),
    OPT_FLAG("terminal", use_terminal, CONF_GLOBAL | CONF_PRE_PARSE),
    OPT_GENERAL(char**, "msg-level", msg_levels, CONF_GLOBAL|CONF_PRE_PARSE,
                .type = &m_option_type_msglevels),
    OPT_STRING("dump-stats", dump_stats, CONF_GLOBAL | CONF_PRE_PARSE),
    OPT_FLAG("msg-color", msg_color, CONF_GLOBAL | CONF_PRE_PARSE),
    OPT_STRING("log-file", log_file, CONF_GLOBAL | CONF_PRE_PARSE | M_OPT_FILE),
    OPT_FLAG("msg-module", msg_module, CONF_GLOBAL),
    OPT_FLAG("msg-time", msg_time, CONF_GLOBAL),
#ifdef _WIN32
    OPT_CHOICE("priority", w32_priority, 0,
               ({"no",          0},
                {"realtime",    REALTIME_PRIORITY_CLASS},
                {"high",        HIGH_PRIORITY_CLASS},
                {"abovenormal", ABOVE_NORMAL_PRIORITY_CLASS},
                {"normal",      NORMAL_PRIORITY_CLASS},
                {"belownormal", BELOW_NORMAL_PRIORITY_CLASS},
                {"idle",        IDLE_PRIORITY_CLASS})),
#endif
    OPT_FLAG("config", load_config, CONF_GLOBAL | CONF_NOCFG | CONF_PRE_PARSE),
    OPT_STRING("config-dir", force_configdir,
               CONF_GLOBAL | CONF_NOCFG | CONF_PRE_PARSE),
    OPT_STRINGLIST("reset-on-next-file", reset_options, M_OPT_GLOBAL),

#if HAVE_LUA
    OPT_STRINGLIST("script", script_files, CONF_GLOBAL | M_OPT_FILE),
    OPT_KEYVALUELIST("script-opts", script_opts, M_OPT_GLOBAL),
    OPT_FLAG("osc", lua_load_osc, CONF_GLOBAL),
    OPT_FLAG("ytdl", lua_load_ytdl, CONF_GLOBAL),
    OPT_STRING("ytdl-format", lua_ytdl_format, CONF_GLOBAL),
    OPT_KEYVALUELIST("ytdl-raw-options", lua_ytdl_raw_options, CONF_GLOBAL),
    OPT_FLAG("load-scripts", auto_load_scripts, CONF_GLOBAL),
#endif

// ------------------------- stream options --------------------

    OPT_CHOICE_OR_INT("cache", stream_cache.size, 0, 32, 0x7fffffff,
                      ({"no", 0},
                       {"auto", -1},
                       {"yes", -2})),
    OPT_CHOICE_OR_INT("cache-default", stream_cache.def_size, 0, 32, 0x7fffffff,
                      ({"no", 0})),
    OPT_INTRANGE("cache-initial", stream_cache.initial, 0, 0, 0x7fffffff),
    OPT_INTRANGE("cache-seek-min", stream_cache.seek_min, 0, 0, 0x7fffffff),
    OPT_STRING("cache-file", stream_cache.file, M_OPT_FILE),
    OPT_INTRANGE("cache-file-size", stream_cache.file_max, 0, 0, 0x7fffffff),

#if HAVE_DVDREAD || HAVE_DVDNAV
    OPT_STRING("dvd-device", dvd_device, M_OPT_FILE),
    OPT_INT("dvd-speed", dvd_speed, 0),
    OPT_INTRANGE("dvd-angle", dvd_angle, 0, 1, 99),
#endif /* HAVE_DVDREAD */
    OPT_INTPAIR("chapter", chapterrange, 0),
    OPT_CHOICE_OR_INT("edition", edition_id, 0, 0, 8190,
                      ({"auto", -1})),
#if HAVE_LIBBLURAY
    OPT_STRING("bluray-device", bluray_device, M_OPT_FILE),
    OPT_INTRANGE("bluray-angle", bluray_angle, 0, 0, 999),
#endif /* HAVE_LIBBLURAY */

    OPT_STRINGLIST("http-header-fields", network_http_header_fields, 0),
    OPT_STRING("user-agent", network_useragent, 0),
    OPT_STRING("referrer", network_referrer, 0),
    OPT_FLAG("cookies", network_cookies_enabled, 0),
    OPT_STRING("cookies-file", network_cookies_file, M_OPT_FILE),
    OPT_CHOICE("rtsp-transport", network_rtsp_transport, 0,
               ({"lavf", 0},
                {"udp", 1},
                {"tcp", 2},
                {"http", 3})),
    OPT_FLAG("tls-verify", network_tls_verify, 0),
    OPT_STRING("tls-ca-file", network_tls_ca_file, M_OPT_FILE),
    OPT_DOUBLE("network-timeout", network_timeout, M_OPT_MIN, .min = 0),

// ------------------------- demuxer options --------------------

    OPT_CHOICE_OR_INT("frames", play_frames, M_OPT_FIXED, 0, INT_MAX,
                      ({"all", -1})),

    OPT_REL_TIME("start", play_start, 0),
    OPT_REL_TIME("end", play_end, 0),
    OPT_REL_TIME("length", play_length, 0),

    OPT_TIME("ab-loop-a", ab_loop[0], 0, .min = MP_NOPTS_VALUE),
    OPT_TIME("ab-loop-b", ab_loop[1], 0, .min = MP_NOPTS_VALUE),

    OPT_FLAG("pause", pause, M_OPT_FIXED),
    OPT_CHOICE("keep-open", keep_open, 0,
               ({"no", 0},
                {"yes", 1},
                {"always", 2})),

    OPT_CHOICE("index", index_mode, 0, ({"default", 1}, {"recreate", 0})),

    // select audio/video/subtitle stream
    OPT_TRACKCHOICE("aid", audio_id),
    OPT_TRACKCHOICE("vid", video_id),
    OPT_TRACKCHOICE("sid", sub_id),
    OPT_TRACKCHOICE("secondary-sid", sub2_id),
    OPT_TRACKCHOICE("ff-aid", audio_id_ff),
    OPT_TRACKCHOICE("ff-vid", video_id_ff),
    OPT_TRACKCHOICE("ff-sid", sub_id_ff),
    OPT_FLAG_STORE("no-sub", sub_id, 0, -2),
    OPT_FLAG_STORE("no-video", video_id, 0, -2),
    OPT_FLAG_STORE("no-audio", audio_id, 0, -2),
    OPT_STRINGLIST("alang", audio_lang, 0),
    OPT_STRINGLIST("slang", sub_lang, 0),

    OPT_CHOICE("audio-display", audio_display, 0,
               ({"no", 0}, {"attachment", 1})),

    OPT_CHOICE("hls-bitrate", hls_bitrate, 0,
               ({"no", 0}, {"min", 1}, {"max", 2})),

    OPT_STRINGLIST("display-tags*", display_tags, 0),

#if HAVE_CDDA
    OPT_SUBSTRUCT("cdda", stream_cdda_opts, stream_cdda_conf, 0),
    OPT_STRING("cdrom-device", cdrom_device, M_OPT_FILE),
#endif

    // demuxer.c - select audio/sub file/demuxer
    OPT_STRING_APPEND_LIST("audio-file", audio_files, M_OPT_FILE),
    OPT_STRING("demuxer", demuxer_name, 0),
    OPT_STRING("audio-demuxer", audio_demuxer_name, 0),
    OPT_STRING("sub-demuxer", sub_demuxer_name, 0),
    OPT_FLAG("demuxer-thread", demuxer_thread, 0),
    OPT_DOUBLE("demuxer-readahead-secs", demuxer_min_secs, M_OPT_MIN, .min = 0),
    OPT_INTRANGE("demuxer-readahead-packets", demuxer_min_packs, 0, 0, MAX_PACKS),
    OPT_INTRANGE("demuxer-readahead-bytes", demuxer_min_bytes, 0, 0, MAX_PACK_BYTES),

    OPT_DOUBLE("cache-secs", demuxer_min_secs_cache, M_OPT_MIN, .min = 0),
    OPT_FLAG("cache-pause", cache_pausing, 0),

    OPT_DOUBLE("mf-fps", mf_fps, 0),
    OPT_STRING("mf-type", mf_type, 0),
#if HAVE_TV
    OPT_SUBSTRUCT("tv", tv_params, tv_params_conf, 0),
#endif /* HAVE_TV */
#if HAVE_PVR
    OPT_SUBSTRUCT("pvr", stream_pvr_opts, stream_pvr_conf, 0),
#endif /* HAVE_PVR */
#if HAVE_DVBIN
    OPT_SUBSTRUCT("dvbin", stream_dvb_opts, stream_dvb_conf, 0),
#endif
    OPT_SUBSTRUCT("", stream_lavf_opts, stream_lavf_conf, 0),

// ------------------------- a-v sync options --------------------

    // set A-V sync correction speed (0=disables it):
    OPT_FLOATRANGE("mc", default_max_pts_correction, 0, 0, 100),

    // force video/audio rate:
    OPT_DOUBLE("fps", force_fps, CONF_MIN, .min = 0),
    OPT_INTRANGE("audio-samplerate", force_srate, 0, 1000, 8*48000),
    OPT_CHMAP("audio-channels", audio_output_channels, CONF_MIN, .min = 0),
    OPT_AUDIOFORMAT("audio-format", audio_output_format, 0),
    OPT_DOUBLE("speed", playback_speed, M_OPT_RANGE | M_OPT_FIXED,
               .min = 0.01, .max = 100.0),

    OPT_FLAG("audio-pitch-correction", pitch_correction, 0),

    // set a-v distance
    OPT_FLOATRANGE("audio-delay", audio_delay, 0, -100.0, 100.0),

// ------------------------- codec/vfilter options --------------------

    OPT_SETTINGSLIST("af-defaults", af_defs, 0, &af_obj_list),
    OPT_SETTINGSLIST("af*", af_settings, 0, &af_obj_list),
    OPT_SETTINGSLIST("vf-defaults", vf_defs, 0, &vf_obj_list),
    OPT_SETTINGSLIST("vf*", vf_settings, 0, &vf_obj_list),

    OPT_CHOICE("deinterlace", deinterlace, 0,
               ({"auto", -1},
                {"no", 0},
                {"yes", 1})),

    OPT_STRING("ad", audio_decoders, 0),
    OPT_STRING("vd", video_decoders, 0),

    OPT_FLAG("ad-spdif-dtshd", dtshd, 0),

    OPT_CHOICE("hwdec", hwdec_api, 0,
               ({"no", 0},
                {"auto", -1},
                {"vdpau", 1},
                {"vda", 2},
                {"vaapi", 4},
                {"vaapi-copy", 5},
                {"dxva2-copy", 6},
                {"rpi", 7})),
    OPT_STRING("hwdec-codecs", hwdec_codecs, 0),

    OPT_SUBSTRUCT("sws", vo.sws_opts, sws_conf, 0),

    // -1 means auto aspect (prefer container size until aspect change)
    //  0 means square pixels
    OPT_FLOATRANGE("video-aspect", movie_aspect, 0, -1.0, 10.0),
    OPT_FLOAT_STORE("no-video-aspect", movie_aspect, 0, 0.0),

    OPT_CHOICE("field-dominance", field_dominance, 0,
               ({"auto", -1}, {"top", 0}, {"bottom", 1})),

    OPT_SUBSTRUCT("vd-lavc", vd_lavc_params, vd_lavc_conf, 0),
    OPT_SUBSTRUCT("ad-lavc", ad_lavc_params, ad_lavc_conf, 0),

    OPT_SUBSTRUCT("demuxer-lavf", demux_lavf, demux_lavf_conf, 0),
    OPT_SUBSTRUCT("demuxer-rawaudio", demux_rawaudio, demux_rawaudio_conf, 0),
    OPT_SUBSTRUCT("demuxer-rawvideo", demux_rawvideo, demux_rawvideo_conf, 0),
    OPT_SUBSTRUCT("demuxer-mkv", demux_mkv, demux_mkv_conf, 0),

// ------------------------- subtitles options --------------------

    OPT_STRING_APPEND_LIST("sub-file", sub_name, M_OPT_FILE),
    OPT_PATHLIST("sub-paths", sub_paths, 0),
    OPT_STRING("sub-codepage", sub_cp, 0),
    OPT_FLOAT("sub-delay", sub_delay, 0),
    OPT_FLOAT("sub-fps", sub_fps, 0),
    OPT_FLOAT("sub-speed", sub_speed, 0),
    OPT_FLAG("sub-visibility", sub_visibility, 0),
    OPT_FLAG("sub-forced-only", forced_subs_only, 0),
    OPT_FLAG("stretch-dvd-subs", stretch_dvd_subs, 0),
    OPT_FLAG("sub-fix-timing", sub_fix_timing, 0),
    OPT_CHOICE("sub-auto", sub_auto, 0,
               ({"no", -1}, {"exact", 0}, {"fuzzy", 1}, {"all", 2})),
    OPT_CHOICE("audio-file-auto", audiofile_auto, 0,
               ({"no", -1}, {"exact", 0}, {"fuzzy", 1}, {"all", 2})),
    OPT_INTRANGE("sub-pos", sub_pos, 0, 0, 100),
    OPT_FLOATRANGE("sub-gauss", sub_gauss, 0, 0.0, 3.0),
    OPT_FLAG("sub-gray", sub_gray, 0),
    OPT_FLAG("sub-ass", ass_enabled, 0),
    OPT_FLOATRANGE("sub-scale", sub_scale, 0, 0, 100),
    OPT_FLOATRANGE("ass-line-spacing", ass_line_spacing, 0, -1000, 1000),
    OPT_FLAG("sub-use-margins", sub_use_margins, 0),
    OPT_FLAG("ass-force-margins", ass_use_margins, 0),
    OPT_FLAG("ass-vsfilter-aspect-compat", ass_vsfilter_aspect_compat, 0),
    OPT_CHOICE("ass-vsfilter-color-compat", ass_vsfilter_color_compat, 0,
               ({"no", 0}, {"basic", 1}, {"full", 2}, {"force-601", 3})),
    OPT_FLAG("ass-vsfilter-blur-compat", ass_vsfilter_blur_compat, 0),
    OPT_FLAG("embeddedfonts", use_embedded_fonts, 0),
    OPT_STRINGLIST("ass-force-style", ass_force_style_list, 0),
    OPT_STRING("ass-styles", ass_styles_file, M_OPT_FILE),
    OPT_CHOICE("ass-hinting", ass_hinting, 0,
               ({"none", 0}, {"light", 1}, {"normal", 2}, {"native", 3})),
    OPT_CHOICE("ass-shaper", ass_shaper, 0,
               ({"simple", 0}, {"complex", 1})),
    OPT_CHOICE("ass-style-override", ass_style_override, 0,
               ({"no", 0}, {"yes", 1}, {"force", 3}, {"signfs", 4})),
    OPT_FLAG("sub-scale-by-window", sub_scale_by_window, 0),
    OPT_FLAG("sub-scale-with-window", sub_scale_with_window, 0),
    OPT_FLAG("ass-scale-with-window", ass_scale_with_window, 0),
    OPT_FLAG("osd-bar", osd_bar_visible, 0),
    OPT_FLOATRANGE("osd-bar-align-x", osd_bar_align_x, 0, -1.0, +1.0),
    OPT_FLOATRANGE("osd-bar-align-y", osd_bar_align_y, 0, -1.0, +1.0),
    OPT_FLOATRANGE("osd-bar-w", osd_bar_w, 0, 1, 100),
    OPT_FLOATRANGE("osd-bar-h", osd_bar_h, 0, 0.1, 50),
    OPT_SUBSTRUCT("osd", osd_style, osd_style_conf, 0),
    OPT_FLAG("use-text-osd", use_text_osd, CONF_GLOBAL),
    OPT_SUBSTRUCT("sub-text", sub_text_style, sub_style_conf, 0),
    OPT_FLAG("sub-clear-on-seek", sub_clear_on_seek, 0),

//---------------------- libao/libvo options ------------------------
    OPT_SETTINGSLIST("vo", vo.video_driver_list, 0, &vo_obj_list),
    OPT_SETTINGSLIST("vo-defaults", vo.vo_defs, 0, &vo_obj_list),
    OPT_SETTINGSLIST("ao", audio_driver_list, 0, &ao_obj_list),
    OPT_SETTINGSLIST("ao-defaults", ao_defs, 0, &ao_obj_list),
    OPT_STRING("audio-device", audio_device, 0),
    OPT_STRING("audio-client-name", audio_client_name, 0),
    OPT_FLAG("force-window", force_vo, 0),
    OPT_FLAG("ontop", vo.ontop, M_OPT_FIXED),
    OPT_FLAG("border", vo.border, M_OPT_FIXED),
    OPT_FLAG("on-all-workspaces", vo.all_workspaces, M_OPT_FIXED),

    OPT_FLAG("window-dragging", allow_win_drag, CONF_GLOBAL),

    OPT_CHOICE("softvol", softvol, 0,
               ({"no", SOFTVOL_NO},
                {"yes", SOFTVOL_YES},
                {"auto", SOFTVOL_AUTO})),
    OPT_FLOATRANGE("softvol-max", softvol_max, 0, 10, 10000),
    OPT_FLOATRANGE("volume", mixer_init_volume, 0, -1, 100),
    OPT_CHOICE("mute", mixer_init_mute, 0,
               ({"auto", -1},
                {"no", 0},
                {"yes", 1})),
    OPT_STRING("volume-restore-data", mixer_restore_volume_data, 0),
    OPT_CHOICE("gapless-audio", gapless_audio, 0,
               ({"no", 0},
                {"yes", 1},
                {"weak", -1})),
    OPT_DOUBLE("audio-buffer", audio_buffer, M_OPT_MIN | M_OPT_MAX,
               .min = 0, .max = 10),

    OPT_GEOMETRY("geometry", vo.geometry, 0),
    OPT_SIZE_BOX("autofit", vo.autofit, 0),
    OPT_SIZE_BOX("autofit-larger", vo.autofit_larger, 0),
    OPT_SIZE_BOX("autofit-smaller", vo.autofit_smaller, 0),
    OPT_FLOATRANGE("window-scale", vo.window_scale, 0, 0.001, 100),
    OPT_FLAG("force-window-position", vo.force_window_position, 0),
    // vo name (X classname) and window title strings
    OPT_STRING("x11-name", vo.winname, 0),
    OPT_STRING("title", wintitle, 0),
    OPT_STRING("media-title", media_title, 0),
    // set aspect ratio of monitor - useful for 16:9 TV-out
    OPT_FLOATRANGE("monitoraspect", vo.force_monitor_aspect, 0, 0.0, 9.0),
    OPT_FLOATRANGE("monitorpixelaspect", vo.monitor_pixel_aspect, 0, 0.2, 9.0),
    // start in fullscreen mode:
    OPT_FLAG("fullscreen", vo.fullscreen, M_OPT_FIXED),
    OPT_FLAG("fs", vo.fullscreen, M_OPT_FIXED),
    OPT_FLAG("native-keyrepeat", vo.native_keyrepeat, M_OPT_FIXED),
    OPT_FLOATRANGE("panscan", vo.panscan, 0, 0.0, 1.0),
    OPT_FLOATRANGE("video-zoom", vo.zoom, 0, -20.0, 20.0),
    OPT_FLOATRANGE("video-pan-x", vo.pan_x, 0, -3.0, 3.0),
    OPT_FLOATRANGE("video-pan-y", vo.pan_y, 0, -3.0, 3.0),
    OPT_FLOATRANGE("video-align-x", vo.align_x, 0, -1.0, 1.0),
    OPT_FLOATRANGE("video-align-y", vo.align_y, 0, -1.0, 1.0),
    OPT_FLAG("video-unscaled", vo.unscaled, 0),
    OPT_FLAG("force-rgba-osd-rendering", force_rgba_osd, 0),
    OPT_CHOICE_OR_INT("video-rotate", video_rotate, 0, 0, 360,
                      ({"no", -1})),
    OPT_CHOICE_C("video-stereo-mode", video_stereo_mode, 0, mp_stereo3d_names),

    OPT_CHOICE_OR_INT("cursor-autohide", cursor_autohide_delay, 0,
                      0, 30000, ({"no", -1}, {"always", -2})),
    OPT_FLAG("cursor-autohide-fs-only", cursor_autohide_fs, 0),
    OPT_FLAG("stop-screensaver", stop_screensaver, 0),

    OPT_INT64("wid", vo.WinID, 0),
#if HAVE_X11
    OPT_CHOICE("x11-netwm", vo.x11_netwm, 0,
               ({"auto", 0}, {"no", -1}, {"yes", 1})),
#endif
    OPT_STRING("heartbeat-cmd", heartbeat_cmd, 0),
    OPT_FLOAT("heartbeat-interval", heartbeat_interval, CONF_MIN, 0),

    OPT_CHOICE_OR_INT("screen", vo.screen_id, 0, 0, 32,
                      ({"default", -1})),

    OPT_CHOICE_OR_INT("fs-screen", vo.fsscreen_id, 0, 0, 32,
                      ({"all", -2}, {"current", -1})),

    OPT_FLAG("fs-black-out-screens", vo.fs_black_out_screens, 0),
    OPT_INTRANGE("brightness", gamma_brightness, 0, -100, 100),
    OPT_INTRANGE("saturation", gamma_saturation, 0, -100, 100),
    OPT_INTRANGE("contrast", gamma_contrast, 0, -100, 100),
    OPT_INTRANGE("hue", gamma_hue, 0, -100, 100),
    OPT_INTRANGE("gamma", gamma_gamma, 0, -100, 100),
    OPT_FLAG("keepaspect", vo.keepaspect, 0),
    OPT_FLAG("keepaspect-window", vo.keepaspect_window, 0),

    OPT_FLAG("use-filedir-conf", use_filedir_conf, 0),
    OPT_CHOICE("osd-level", osd_level, 0,
               ({"0", 0}, {"1", 1}, {"2", 2}, {"3", 3})),
    OPT_INTRANGE("osd-duration", osd_duration, 0, 0, 3600000),
    OPT_FLAG("osd-fractions", osd_fractions, 0),
    OPT_FLOATRANGE("osd-scale", osd_scale, 0, 0, 100),
    OPT_FLAG("osd-scale-by-window", osd_scale_by_window, 0),

    OPT_DOUBLE("sstep", step_sec, CONF_MIN, 0),

    OPT_CHOICE("framedrop", frame_dropping, 0,
               ({"no", 0},
                {"vo", 1},
                {"decoder", 2},
                {"decoder+vo", 3})),

    OPT_DOUBLE("display-fps", frame_drop_fps, M_OPT_MIN, .min = 0),

    OPT_FLAG("untimed", untimed, 0),

    OPT_STRING("stream-capture", stream_capture, M_OPT_FILE),
    OPT_STRING("stream-dump", stream_dump, M_OPT_FILE),

    OPT_FLAG("stop-playback-on-init-failure", stop_playback_on_init_failure, 0),

    OPT_CHOICE_OR_INT("loop", loop_times, 0, 1, 10000,
                      ({"no", 1},
                       {"inf", -1},
                       {"force", -2})),
    OPT_CHOICE_OR_INT("loop-file", loop_file, 0, 0, 10000,
                      ({"no", 0},
                       {"yes", -1},
                       {"inf", -1})),

    OPT_FLAG("resume-playback", position_resume, 0),
    OPT_FLAG("save-position-on-quit", position_save_on_quit, 0),
    OPT_FLAG("write-filename-in-watch-later-config", write_filename_in_watch_later_config, 0),
    OPT_FLAG("ignore-path-in-watch-later-config", ignore_path_in_watch_later_config, 0),

    OPT_FLAG("ordered-chapters", ordered_chapters, 0),
    OPT_STRING("ordered-chapters-files", ordered_chapters_files, M_OPT_FILE),
    OPT_INTRANGE("chapter-merge-threshold", chapter_merge_threshold, 0, 0, 10000),

    OPT_DOUBLE("chapter-seek-threshold", chapter_seek_threshold, 0),

    OPT_STRING("chapters-file", chapter_file, M_OPT_FILE),

    OPT_FLAG("load-unsafe-playlists", load_unsafe_playlists, 0),
    OPT_FLAG("merge-files", merge_files, 0),

    // a-v sync stuff:
    OPT_FLAG("correct-pts", correct_pts, 0),
    OPT_CHOICE("pts-association-mode", user_pts_assoc_mode, 0,
               ({"auto", 0}, {"decoder", 1}, {"sort", 2})),
    OPT_FLAG("initial-audio-sync", initial_audio_sync, 0),
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

    OPT_CHOICE("idle", player_idle_mode, 0,
               ({"no",   0},
                {"once", 1},
                {"yes",  2})),

    OPT_FLAG("input-terminal", consolecontrols, CONF_GLOBAL),

    OPT_STRING("input-file", input_file, M_OPT_FILE | M_OPT_GLOBAL),
    OPT_STRING("input-unix-socket", ipc_path, M_OPT_FILE),

    OPT_SUBSTRUCT("screenshot", screenshot_image_opts, image_writer_conf, 0),
    OPT_STRING("screenshot-template", screenshot_template, 0),

    OPT_SUBSTRUCT("input", input_opts, input_config, 0),

    OPT_PRINT("list-properties", property_print_help),
    OPT_PRINT("list-protocols", stream_print_proto_list),
    OPT_PRINT("help", print_help),
    OPT_PRINT("h", print_help),
    OPT_PRINT("version", print_version),
    OPT_PRINT("V", print_version),

#if HAVE_ENCODING
    OPT_SUBSTRUCT("", encode_opts, encode_config, 0),
#endif

    OPT_FLAG("slave-broken", slave_mode, CONF_GLOBAL),

    OPT_REMOVED("a52drc", "use --ad-lavc-ac3drc=level"),
    OPT_REMOVED("afm", "use --ad=..."),
    OPT_REPLACED("aspect", "video-aspect"),
    OPT_REMOVED("ass-bottom-margin", "use --vf=sub=bottom:top"),
    OPT_REPLACED("ass", "sub-ass"),
    OPT_REPLACED("audiofile", "audio-file"),
    OPT_REMOVED("benchmark", "use --untimed (no stats)"),
    OPT_REMOVED("capture", "use --stream-capture=<filename>"),
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
    OPT_REPLACED("sub", "sub-file"),
    OPT_REPLACED("subcp", "sub-codepage"),
    OPT_REPLACED("subdelay", "sub-delay"),
    OPT_REPLACED("subfile", "sub"),
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
    OPT_REPLACED("dtshd", "ad-spdif-dtshd"),
    OPT_REPLACED("ass-use-margins", "sub-use-margins"),

    {0}
};

const struct MPOpts mp_default_opts = {
    .use_terminal = 1,
    .msg_color = 1,
    .audio_driver_list = NULL,
    .audio_decoders = "lavc:libdcadec,-spdif:*", // never select spdif by default
    .video_decoders = NULL,
    .deinterlace = -1,
    .softvol = SOFTVOL_AUTO,
    .softvol_max = 200,
    .mixer_init_volume = -1,
    .mixer_init_mute = -1,
    .gapless_audio = -1,
    .audio_buffer = 0.2,
    .audio_device = "auto",
    .audio_client_name = "mpv",
    .vo = {
        .video_driver_list = NULL,
        .monitor_pixel_aspect = 1.0,
        .screen_id = -1,
        .fsscreen_id = -1,
        .panscan = 0.0f,
        .keepaspect = 1,
        .keepaspect_window = 1,
        .border = 1,
        .WinID = -1,
        .window_scale = 1.0,
    },
    .allow_win_drag = 1,
    .wintitle = "mpv - ${?media-title:${media-title}}${!media-title:No file.}",
    .heartbeat_interval = 30.0,
    .stop_screensaver = 1,
    .cursor_autohide_delay = 1000,
    .gamma_gamma = 1000,
    .gamma_brightness = 1000,
    .gamma_contrast = 1000,
    .gamma_saturation = 1000,
    .gamma_hue = 1000,
    .osd_level = 1,
    .osd_duration = 1000,
    .osd_bar_align_y = 0.5,
    .osd_bar_w = 75.0,
    .osd_bar_h = 3.125,
    .osd_scale = 1,
    .osd_scale_by_window = 1,
    .sub_scale_by_window = 1,
    .ass_use_margins = 0,
    .sub_use_margins = 1,
    .ass_scale_with_window = 0,
    .sub_scale_with_window = 1,
    .use_text_osd = 1,
#if HAVE_LUA
    .lua_load_osc = 1,
    .lua_load_ytdl = 1,
    .lua_ytdl_format = "best",
    .lua_ytdl_raw_options = NULL,
#endif
    .auto_load_scripts = 1,
    .loop_times = 1,
    .ordered_chapters = 1,
    .chapter_merge_threshold = 100,
    .chapter_seek_threshold = 5.0,
    .hr_seek_framedrop = 1,
    .load_config = 1,
    .position_resume = 1,
    .stream_cache = {
        .size = -1,
        .def_size = 150000,
        .initial = 0,
        .seek_min = 500,
        .file_max = 1024 * 1024,
    },
    .demuxer_thread = 1,
    .demuxer_min_packs = 0,
    .demuxer_min_bytes = 0,
    .demuxer_min_secs = 1.0,
    .network_rtsp_transport = 2,
    .network_timeout = 0.0,
    .hls_bitrate = 2,
    .demuxer_min_secs_cache = 10.0,
    .cache_pausing = 1,
    .chapterrange = {-1, -1},
    .ab_loop = {MP_NOPTS_VALUE, MP_NOPTS_VALUE},
    .edition_id = -1,
    .default_max_pts_correction = -1,
    .correct_pts = 1,
    .user_pts_assoc_mode = 1,
    .initial_audio_sync = 1,
    .frame_dropping = 1,
    .term_osd = 2,
    .term_osd_bar_chars = "[-+-]",
    .consolecontrols = 1,
    .play_frames = -1,
    .keep_open = 0,
    .audio_id = -1,
    .video_id = -1,
    .sub_id = -1,
    .audio_id_ff = -1,
    .video_id_ff = -1,
    .sub_id_ff = -1,
    .sub2_id = -2,
    .audio_display = 1,
    .sub_visibility = 1,
    .sub_pos = 100,
    .sub_speed = 1.0,
    .audio_output_channels = {0}, // auto
    .audio_output_format = 0,  // AF_FORMAT_UNKNOWN
    .playback_speed = 1.,
    .pitch_correction = 1,
    .movie_aspect = -1.,
    .field_dominance = -1,
    .sub_auto = 0,
    .audiofile_auto = 0,
    .osd_bar_visible = 1,
#if HAVE_LIBASS
    .ass_enabled = 1,
#endif
    .sub_scale = 1,
    .ass_vsfilter_aspect_compat = 1,
    .ass_vsfilter_color_compat = 1,
    .ass_vsfilter_blur_compat = 1,
    .ass_style_override = 1,
    .ass_shaper = 1,
    .use_embedded_fonts = 1,
    .sub_fix_timing = 1,
    .sub_cp = "auto",
    .screenshot_template = "shot%n",

    .hwdec_codecs = "h264,vc1,wmv3",

    .index_mode = 1,

    .dvd_angle = 1,

    .mf_fps = 1.0,

#if HAVE_RPI
    .hwdec_api = -1,
#endif

    .display_tags = (char **)(const char*[]){
        "Artist", "Album", "Album_Artist", "Comment", "Composer", "Genre",
        "Performer", "Title", "Track", "icy-title",
        NULL
    },
};

#endif /* MPLAYER_CFG_MPLAYER_H */
