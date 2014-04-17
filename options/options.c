/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_CFG_MPLAYER_H
#define MPLAYER_CFG_MPLAYER_H

/*
 * config for cfgparser
 */

#include <stddef.h>
#include <sys/types.h>
#include <limits.h>

#include "options.h"
#include "config.h"
#include "m_config.h"
#include "m_option.h"
#include "common/common.h"
#include "stream/stream.h"
#include "stream/tv.h"
#include "video/csputils.h"
#include "sub/osd.h"
#include "audio/mixer.h"
#include "audio/filter/af.h"
#include "audio/decode/dec_audio.h"
#include "player/core.h"
#include "player/command.h"
#include "osdep/priority.h"

/* defined in demux: */
extern const m_option_t demux_rawaudio_opts[];
extern const m_option_t demux_rawvideo_opts[];
extern const m_option_t cdda_opts[];

extern int sws_flags;

extern const char mp_help_text[];

static void print_version(struct mp_log *log)
{
    mp_print_version(log, true);
}

static void print_help(struct mp_log *log)
{
    mp_info(log, "%s", mp_help_text);
}

#if HAVE_TV
static const m_option_t tvopts_conf[]={
    {"immediatemode", &stream_tv_defaults.immediate, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
    {"audio", &stream_tv_defaults.noaudio, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"audiorate", &stream_tv_defaults.audiorate, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"driver", &stream_tv_defaults.driver, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"device", &stream_tv_defaults.device, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"freq", &stream_tv_defaults.freq, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"channel", &stream_tv_defaults.channel, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"chanlist", &stream_tv_defaults.chanlist, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"norm", &stream_tv_defaults.norm, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"automute", &stream_tv_defaults.automute, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
#if HAVE_TV_V4L2
    {"normid", &stream_tv_defaults.normid, CONF_TYPE_INT, 0, 0, 0, NULL},
#endif
    {"width", &stream_tv_defaults.width, CONF_TYPE_INT, 0, 0, 4096, NULL},
    {"height", &stream_tv_defaults.height, CONF_TYPE_INT, 0, 0, 4096, NULL},
    {"input", &stream_tv_defaults.input, CONF_TYPE_INT, 0, 0, 20, NULL},
    {"outfmt", &stream_tv_defaults.outfmt, CONF_TYPE_FOURCC, 0, 0, 0, NULL},
    {"fps", &stream_tv_defaults.fps, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
    {"channels", &stream_tv_defaults.channels, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"brightness", &stream_tv_defaults.brightness, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"contrast", &stream_tv_defaults.contrast, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"hue", &stream_tv_defaults.hue, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"saturation", &stream_tv_defaults.saturation, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"gain", &stream_tv_defaults.gain, CONF_TYPE_INT, CONF_RANGE, -1, 100, NULL},
#if HAVE_TV_V4L2
    {"amode", &stream_tv_defaults.amode, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
    {"volume", &stream_tv_defaults.volume, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
    {"bass", &stream_tv_defaults.bass, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
    {"treble", &stream_tv_defaults.treble, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
    {"balance", &stream_tv_defaults.balance, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
    {"forcechan", &stream_tv_defaults.forcechan, CONF_TYPE_INT, CONF_RANGE, 1, 2, NULL},
    {"forceaudio", &stream_tv_defaults.force_audio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"buffersize", &stream_tv_defaults.buffer_size, CONF_TYPE_INT, CONF_RANGE, 16, 1024, NULL},
    {"mjpeg", &stream_tv_defaults.mjpeg, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"decimation", &stream_tv_defaults.decimation, CONF_TYPE_INT, CONF_RANGE, 1, 4, NULL},
    {"quality", &stream_tv_defaults.quality, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
#if HAVE_ALSA
    {"alsa", &stream_tv_defaults.alsa, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif /* HAVE_ALSA */
#endif /* HAVE_TV_V4L2 */
    {"adevice", &stream_tv_defaults.adevice, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"audioid", &stream_tv_defaults.audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif /* HAVE_TV */

extern int pvr_param_aspect_ratio;
extern int pvr_param_sample_rate;
extern int pvr_param_audio_layer;
extern int pvr_param_audio_bitrate;
extern char *pvr_param_audio_mode;
extern int pvr_param_bitrate;
extern char *pvr_param_bitrate_mode;
extern int pvr_param_bitrate_peak;
extern char *pvr_param_stream_type;

#if HAVE_PVR
static const m_option_t pvropts_conf[]={
    {"aspect", &pvr_param_aspect_ratio, CONF_TYPE_INT, 0, 1, 4, NULL},
    {"arate", &pvr_param_sample_rate, CONF_TYPE_INT, 0, 32000, 48000, NULL},
    {"alayer", &pvr_param_audio_layer, CONF_TYPE_INT, 0, 1, 2, NULL},
    {"abitrate", &pvr_param_audio_bitrate, CONF_TYPE_INT, 0, 32, 448, NULL},
    {"amode", &pvr_param_audio_mode, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"vbitrate", &pvr_param_bitrate, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"vmode", &pvr_param_bitrate_mode, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"vpeak", &pvr_param_bitrate_peak, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"fmt", &pvr_param_stream_type, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif /* HAVE_PVR */

extern const m_option_t dvbin_opts_conf[];
extern const m_option_t lavfdopts_conf[];

extern int sws_chr_vshift;
extern int sws_chr_hshift;
extern float sws_chr_gblur;
extern float sws_lum_gblur;
extern float sws_chr_sharpen;
extern float sws_lum_sharpen;

static const m_option_t scaler_filter_conf[]={
    {"lgb", &sws_lum_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
    {"cgb", &sws_chr_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
    {"cvs", &sws_chr_vshift, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"chs", &sws_chr_hshift, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"ls", &sws_lum_sharpen, CONF_TYPE_FLOAT, 0, -100.0, 100.0, NULL},
    {"cs", &sws_chr_sharpen, CONF_TYPE_FLOAT, 0, -100.0, 100.0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern double mf_fps;
extern char * mf_type;
extern const struct m_obj_list vf_obj_list;
extern const struct m_obj_list af_obj_list;
extern const struct m_obj_list vo_obj_list;
extern const struct m_obj_list ao_obj_list;

static const m_option_t mfopts_conf[]={
    {"fps", &mf_fps, CONF_TYPE_DOUBLE, 0, 0, 0, NULL},
    {"type", &mf_type, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#if HAVE_TV
static const m_option_t tvscan_conf[]={
    {"autostart", &stream_tv_defaults.scan, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"threshold", &stream_tv_defaults.scan_threshold, CONF_TYPE_INT, CONF_RANGE, 1, 100, NULL},
    {"period", &stream_tv_defaults.scan_period, CONF_TYPE_FLOAT, CONF_RANGE, 0.1, 2.0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

#define OPT_BASE_STRUCT struct MPOpts

extern const struct m_sub_options image_writer_conf;

static const m_option_t screenshot_conf[] = {
    OPT_SUBSTRUCT("", screenshot_image_opts, image_writer_conf, 0),
    OPT_STRING("template", screenshot_template, 0),
    {0},
};

extern const m_option_t lavc_decode_opts_conf[];
extern const m_option_t ad_lavc_decode_opts_conf[];

extern const m_option_t mp_input_opts[];

const m_option_t mp_opts[] = {
    // handled in command line pre-parser (parse_commandline.c)
    {"v", NULL, CONF_TYPE_STORE, CONF_GLOBAL | CONF_NOCFG, 0, 0, NULL},
    {"playlist", NULL, CONF_TYPE_STRING, CONF_NOCFG | M_OPT_MIN | M_OPT_FIXED,
     1, 0, NULL},
    {"{", NULL, CONF_TYPE_STORE, CONF_NOCFG | M_OPT_FIXED, 0, 0, NULL},
    {"}", NULL, CONF_TYPE_STORE, CONF_NOCFG | M_OPT_FIXED, 0, 0, NULL},

    // handled in m_config.c
    { "include", NULL, CONF_TYPE_STRING, M_OPT_FIXED },
    { "profile", NULL, CONF_TYPE_STRING_LIST, M_OPT_FIXED },
    { "show-profile", NULL, CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED },
    { "list-options", NULL, CONF_TYPE_STORE, CONF_NOCFG | M_OPT_FIXED },

    // handled in main.c (looks at the raw argv[])
    {"leak-report", NULL, CONF_TYPE_STORE, CONF_GLOBAL | CONF_NOCFG | M_OPT_FIXED },

    OPT_FLAG("shuffle", shuffle, CONF_GLOBAL | CONF_NOCFG),

// ------------------------- common options --------------------
    OPT_FLAG("quiet", quiet, CONF_GLOBAL),
    OPT_FLAG_STORE("really-quiet", verbose, CONF_GLOBAL | CONF_PRE_PARSE, -10),
    OPT_FLAG("terminal", use_terminal, CONF_GLOBAL | CONF_PRE_PARSE),
    OPT_GENERAL(char*, "msglevel", msglevels, CONF_GLOBAL|CONF_PRE_PARSE,
                .type = &m_option_type_msglevels),
    OPT_STRING("dump-stats", dump_stats, CONF_GLOBAL | CONF_PRE_PARSE),
    OPT_FLAG("msgcolor", msg_color, CONF_GLOBAL | CONF_PRE_PARSE),
    OPT_FLAG("msgmodule", msg_module, CONF_GLOBAL),
    OPT_FLAG("msgtime", msg_time, CONF_GLOBAL),
    OPT_FLAG("identify", msg_identify, CONF_GLOBAL),
#if HAVE_PRIORITY
    {"priority", &proc_priority, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
    OPT_FLAG("config", load_config, CONF_GLOBAL | CONF_NOCFG | CONF_PRE_PARSE),
    OPT_STRING("config-dir", force_configdir,
               CONF_GLOBAL | CONF_NOCFG | CONF_PRE_PARSE),
    OPT_STRINGLIST("reset-on-next-file", reset_options, M_OPT_GLOBAL),

#if HAVE_LUA
    OPT_STRINGLIST("lua", lua_files, CONF_GLOBAL),
    OPT_KEYVALUELIST("lua-opts", lua_opts, M_OPT_GLOBAL),
    OPT_FLAG("osc", lua_load_osc, CONF_GLOBAL),
    OPT_FLAG("load-scripts", auto_load_scripts, CONF_GLOBAL),
#endif

// ------------------------- stream options --------------------

    OPT_CHOICE_OR_INT("cache", stream_cache_size, 0, 32, 0x7fffffff,
                      ({"no", 0},
                       {"auto", -1}),
                      OPTDEF_INT(-1)),
    OPT_CHOICE_OR_INT("cache-default", stream_cache_def_size, 0, 32, 0x7fffffff,
                      ({"no", 0}),
                      OPTDEF_INT(320)),
    OPT_FLOATRANGE("cache-min", stream_cache_min_percent, 0, 0, 99),
    OPT_FLOATRANGE("cache-seek-min", stream_cache_seek_min_percent, 0,
                   0, 99),
    OPT_CHOICE_OR_INT("cache-pause", stream_cache_pause, 0,
                      0, 40, ({"no", -1})),

    {"cdrom-device", &cdrom_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
#if HAVE_DVDREAD || HAVE_DVDNAV
    {"dvd-device", &dvd_device,  CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"dvd-speed", &dvd_speed, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"dvdangle", &dvd_angle, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
#endif /* HAVE_DVDREAD */
    OPT_INTPAIR("chapter", chapterrange, 0),
    OPT_CHOICE_OR_INT("edition", edition_id, 0, 0, 8190,
                      ({"auto", -1})),
#if HAVE_LIBBLURAY
    {"bluray-device",  &bluray_device,  CONF_TYPE_STRING, 0,          0,  0, NULL},
    {"bluray-angle",   &bluray_angle,   CONF_TYPE_INT,    CONF_RANGE, 0, 999, NULL},
#endif /* HAVE_LIBBLURAY */

    OPT_STRINGLIST("http-header-fields", network_http_header_fields, 0),
    OPT_STRING("user-agent", network_useragent, 0),
    OPT_STRING("referrer", network_referrer, 0),
    OPT_FLAG("cookies", network_cookies_enabled, 0),
    OPT_STRING("cookies-file", network_cookies_file, 0),
    OPT_CHOICE("rtsp-transport", network_rtsp_transport, 0,
               ({"lavf", 0},
                {"udp", 1},
                {"tcp", 2},
                {"http", 3})),
    OPT_FLAG("tls-verify", network_tls_verify, 0),
    OPT_STRING("tls-ca-file", network_tls_ca_file, 0),

// ------------------------- demuxer options --------------------

    OPT_CHOICE_OR_INT("frames", play_frames, M_OPT_FIXED, 0, INT_MAX,
                      ({"all", -1})),

    // seek to byte/seconds position
    OPT_INT64("sb", seek_to_byte, 0),
    OPT_REL_TIME("start", play_start, 0),
    OPT_REL_TIME("end", play_end, 0),
    OPT_REL_TIME("length", play_length, 0),

    OPT_FLAG("pause", pause, M_OPT_FIXED),
    OPT_FLAG("keep-open", keep_open, 0),

    // AVI and Ogg only: (re)build index at startup
    OPT_FLAG_CONSTANTS("idx", index_mode, 0, -1, 1),
    OPT_FLAG_STORE("forceidx", index_mode, 0, 2),

    // select audio/video/subtitle stream
    OPT_TRACKCHOICE("aid", audio_id),
    OPT_TRACKCHOICE("vid", video_id),
    OPT_TRACKCHOICE("sid", sub_id),
    OPT_TRACKCHOICE("secondary-sid", sub2_id),
    OPT_FLAG_STORE("no-sub", sub_id, 0, -2),
    OPT_FLAG_STORE("no-video", video_id, 0, -2),
    OPT_FLAG_STORE("no-audio", audio_id, 0, -2),
    OPT_STRINGLIST("alang", audio_lang, 0),
    OPT_STRINGLIST("slang", sub_lang, 0),

    OPT_CHOICE("audio-display", audio_display, 0,
               ({"no", 0}, {"attachment", 1})),

    OPT_STRING("quvi-format", quvi_format, 0),
    OPT_FLAG("quvi-fetch-subtitles", quvi_fetch_subtitles, 0),

#if HAVE_CDDA
    { "cdda", (void *)&cdda_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

    // demuxer.c - select audio/sub file/demuxer
    OPT_STRING("audiofile", audio_stream, 0),
    OPT_INTRANGE("audiofile-cache", audio_stream_cache, 0, 50, 65536),
    OPT_STRING("demuxer", demuxer_name, 0),
    OPT_STRING("audio-demuxer", audio_demuxer_name, 0),
    OPT_STRING("sub-demuxer", sub_demuxer_name, 0),

    {"mf", (void *) mfopts_conf, CONF_TYPE_SUBCONFIG, 0,0,0, NULL},
#if HAVE_TV
    {"tv", (void *) tvopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif /* HAVE_TV */
#if HAVE_PVR
    {"pvr", (void *) pvropts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif /* HAVE_PVR */
#if HAVE_DVBIN
    {"dvbin", (void *) dvbin_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

// ------------------------- a-v sync options --------------------

    // set A-V sync correction speed (0=disables it):
    OPT_FLOATRANGE("mc", default_max_pts_correction, 0, 0, 100),

    // force video/audio rate:
    OPT_DOUBLE("fps", force_fps, CONF_MIN | M_OPT_FIXED),
    OPT_INTRANGE("srate", force_srate, 0, 1000, 8*48000),
    OPT_CHMAP("channels", audio_output_channels, CONF_MIN, .min = 0),
    OPT_AUDIOFORMAT("format", audio_output_format, 0),
    OPT_DOUBLE("speed", playback_speed, M_OPT_RANGE | M_OPT_FIXED,
               .min = 0.01, .max = 100.0),

    // set a-v distance
    OPT_FLOATRANGE("audio-delay", audio_delay, 0, -100.0, 100.0),

// ------------------------- codec/vfilter options --------------------

    OPT_SETTINGSLIST("af-defaults", af_defs, 0, &af_obj_list),
    OPT_SETTINGSLIST("af*", af_settings, M_OPT_FIXED, &af_obj_list),
    OPT_SETTINGSLIST("vf-defaults", vf_defs, 0, &vf_obj_list),
    OPT_SETTINGSLIST("vf*", vf_settings, M_OPT_FIXED, &vf_obj_list),

    OPT_CHOICE("deinterlace", deinterlace, M_OPT_OPTIONAL_PARAM | M_OPT_FIXED,
               ({"auto", -1},
                {"no", 0},
                {"yes", 1}, {"", 1})),

    OPT_STRING("ad", audio_decoders, 0),
    OPT_STRING("vd", video_decoders, 0),

    OPT_FLAG("ad-spdif-dtshd", dtshd, 0),
    OPT_FLAG("dtshd", dtshd, 0), // old alias

    OPT_CHOICE("hwdec", hwdec_api, 0,
               ({"no", 0},
                {"auto", -1},
                {"vdpau", 1},
                {"vda", 2},
                {"vaapi", 4},
                {"vaapi-copy", 5})),
    OPT_STRING("hwdec-codecs", hwdec_codecs, 0),

    // scaling:
    {"sws", &sws_flags, CONF_TYPE_INT, 0, 0, 2, NULL},
    {"ssf", (void *) scaler_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    // -1 means auto aspect (prefer container size until aspect change)
    //  0 means square pixels
    OPT_FLOATRANGE("aspect", movie_aspect, 0, -1.0, 10.0),
    OPT_FLOAT_STORE("no-aspect", movie_aspect, 0, 0.0),

    OPT_CHOICE("field-dominance", field_dominance, 0,
               ({"auto", -1}, {"top", 0}, {"bottom", 1})),

    {"vd-lavc", (void *) lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG},
    {"ad-lavc", (void *) ad_lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG},

    {"demuxer-lavf", (void *) lavfdopts_conf, CONF_TYPE_SUBCONFIG},
    {"demuxer-rawaudio", (void *)&demux_rawaudio_opts, CONF_TYPE_SUBCONFIG},
    {"demuxer-rawvideo", (void *)&demux_rawvideo_opts, CONF_TYPE_SUBCONFIG},

    OPT_FLAG("demuxer-mkv-subtitle-preroll", mkv_subtitle_preroll, 0),
    OPT_FLAG("mkv-subtitle-preroll", mkv_subtitle_preroll, 0), // old alias

// ------------------------- subtitles options --------------------

    OPT_STRINGLIST("sub", sub_name, 0),
    OPT_PATHLIST("sub-paths", sub_paths, 0),
    OPT_STRING("subcp", sub_cp, 0),
    OPT_FLOAT("sub-delay", sub_delay, 0),
    OPT_FLOAT("subfps", sub_fps, 0),
    OPT_FLOAT("sub-speed", sub_speed, 0),
    OPT_FLAG("autosub", sub_auto, 0),
    OPT_FLAG("sub-visibility", sub_visibility, 0),
    OPT_FLAG("sub-forced-only", forced_subs_only, 0),
    OPT_FLAG("stretch-dvd-subs", stretch_dvd_subs, 0),
    OPT_FLAG_CONSTANTS("sub-fix-timing", suboverlap_enabled, 0, 1, 0),
    OPT_CHOICE("autosub-match", sub_match_fuzziness, 0,
               ({"exact", 0}, {"fuzzy", 1}, {"all", 2})),
    OPT_INTRANGE("sub-pos", sub_pos, 0, 0, 100),
    OPT_FLOATRANGE("sub-gauss", sub_gauss, 0, 0.0, 3.0),
    OPT_FLAG("sub-gray", sub_gray, 0),
    OPT_FLAG("ass", ass_enabled, 0),
    OPT_FLOATRANGE("sub-scale", sub_scale, 0, 0, 100),
    OPT_FLOATRANGE("ass-line-spacing", ass_line_spacing, 0, -1000, 1000),
    OPT_FLAG("ass-use-margins", ass_use_margins, 0),
    OPT_FLAG("ass-vsfilter-aspect-compat", ass_vsfilter_aspect_compat, 0),
    OPT_CHOICE("ass-vsfilter-color-compat", ass_vsfilter_color_compat, 0,
               ({"no", 0}, {"basic", 1}, {"full", 2}, {"force-601", 3})),
    OPT_FLAG("ass-vsfilter-blur-compat", ass_vsfilter_blur_compat, 0),
    OPT_FLAG("embeddedfonts", use_embedded_fonts, 0),
    OPT_STRINGLIST("ass-force-style", ass_force_style_list, 0),
    OPT_STRING("ass-styles", ass_styles_file, 0),
    OPT_CHOICE("ass-hinting", ass_hinting, 0,
               ({"none", 0}, {"light", 1}, {"normal", 2}, {"native", 3})),
    OPT_CHOICE("ass-shaper", ass_shaper, 0,
               ({"simple", 0}, {"complex", 1})),
    OPT_CHOICE("ass-style-override", ass_style_override, 0,
               ({"no", 0}, {"yes", 1}, {"force", 2})),
    OPT_FLAG("osd-bar", osd_bar_visible, 0),
    OPT_FLOATRANGE("osd-bar-align-x", osd_bar_align_x, 0, -1.0, +1.0),
    OPT_FLOATRANGE("osd-bar-align-y", osd_bar_align_y, 0, -1.0, +1.0),
    OPT_FLOATRANGE("osd-bar-w", osd_bar_w, 0, 1, 100),
    OPT_FLOATRANGE("osd-bar-h", osd_bar_h, 0, 0.1, 50),
    OPT_SUBSTRUCT("osd", osd_style, osd_style_conf, 0),
    OPT_SUBSTRUCT("sub-text", sub_text_style, osd_style_conf, 0),

//---------------------- libao/libvo options ------------------------
    OPT_SETTINGSLIST("vo", vo.video_driver_list, 0, &vo_obj_list),
    OPT_SETTINGSLIST("vo-defaults", vo.vo_defs, 0, &vo_obj_list),
    OPT_SETTINGSLIST("ao", audio_driver_list, 0, &ao_obj_list),
    OPT_SETTINGSLIST("ao-defaults", ao_defs, 0, &ao_obj_list),
    OPT_FLAG("fixed-vo", fixed_vo, CONF_GLOBAL),
    OPT_FLAG("force-window", force_vo, CONF_GLOBAL),
    OPT_FLAG("ontop", vo.ontop, M_OPT_FIXED),
    OPT_FLAG("border", vo.border, M_OPT_FIXED),

    OPT_CHOICE("softvol", softvol, 0,
               ({"no", SOFTVOL_NO},
                {"yes", SOFTVOL_YES},
                {"auto", SOFTVOL_AUTO})),
    OPT_FLOATRANGE("softvol-max", softvol_max, 0, 10, 10000),
    OPT_INTRANGE("volstep", volstep, 0, 0, 100),
    OPT_FLOATRANGE("volume", mixer_init_volume, 0, -1, 100),
    OPT_CHOICE("mute", mixer_init_mute, M_OPT_OPTIONAL_PARAM,
               ({"auto", -1},
                {"no", 0},
                {"yes", 1}, {"", 1})),
    OPT_STRING("volume-restore-data", mixer_restore_volume_data, 0),
    OPT_FLAG("gapless-audio", gapless_audio, M_OPT_FIXED),

    OPT_GEOMETRY("geometry", vo.geometry, 0),
    OPT_SIZE_BOX("autofit", vo.autofit, 0),
    OPT_SIZE_BOX("autofit-larger", vo.autofit_larger, 0),
    OPT_FLAG("force-window-position", vo.force_window_position, 0),
    // vo name (X classname) and window title strings
    OPT_STRING("name", vo.winname, 0),
    OPT_STRING("title", wintitle, 0),
    // set aspect ratio of monitor - useful for 16:9 TV-out
    OPT_FLOATRANGE("monitoraspect", vo.force_monitor_aspect, 0, 0.0, 9.0),
    OPT_FLOATRANGE("monitorpixelaspect", vo.monitor_pixel_aspect, 0, 0.2, 9.0),
    // start in fullscreen mode:
    OPT_FLAG("fullscreen", vo.fullscreen, M_OPT_FIXED),
    OPT_FLAG("fs", vo.fullscreen, M_OPT_FIXED),
    // set fullscreen switch method (workaround for buggy WMs)
    OPT_INTRANGE("fsmode-dontuse", vo.fsmode, 0, 31, 4096),
    OPT_FLAG("native-keyrepeat", vo.native_keyrepeat, M_OPT_FIXED),
    OPT_FLOATRANGE("panscan", vo.panscan, 0, 0.0, 1.0),
    OPT_FLOATRANGE("video-zoom", vo.zoom, 0, -20.0, 20.0),
    OPT_FLOATRANGE("video-pan-x", vo.pan_x, 0, -3.0, 3.0),
    OPT_FLOATRANGE("video-pan-y", vo.pan_y, 0, -3.0, 3.0),
    OPT_FLOATRANGE("video-align-x", vo.align_x, 0, -1.0, 1.0),
    OPT_FLOATRANGE("video-align-y", vo.align_y, 0, -1.0, 1.0),
    OPT_FLAG("video-unscaled", vo.unscaled, 0),
    OPT_FLAG("force-rgba-osd-rendering", force_rgba_osd, 0),
    OPT_CHOICE("colormatrix", requested_colorspace, 0,
               ({"auto", MP_CSP_AUTO},
                {"BT.601", MP_CSP_BT_601},
                {"BT.709", MP_CSP_BT_709},
                {"SMPTE-240M", MP_CSP_SMPTE_240M},
                {"YCgCo", MP_CSP_YCGCO})),
    OPT_CHOICE("colormatrix-input-range", requested_input_range, 0,
               ({"auto", MP_CSP_LEVELS_AUTO},
                {"limited", MP_CSP_LEVELS_TV},
                {"full", MP_CSP_LEVELS_PC})),
    OPT_CHOICE("colormatrix-output-range", requested_output_range, 0,
               ({"auto", MP_CSP_LEVELS_AUTO},
                {"limited", MP_CSP_LEVELS_TV},
                {"full", MP_CSP_LEVELS_PC})),

    OPT_CHOICE_OR_INT("cursor-autohide", cursor_autohide_delay, 0,
                      0, 30000, ({"no", -1}, {"always", -2})),
    OPT_FLAG("cursor-autohide-fs-only", cursor_autohide_fs, 0),
    OPT_FLAG("stop-screensaver", stop_screensaver, 0),

    OPT_INT64("wid", vo.WinID, CONF_GLOBAL),
#if HAVE_X11
    OPT_STRINGLIST("fstype", vo.fstype_list, 0),
#endif
    OPT_STRING("heartbeat-cmd", heartbeat_cmd, 0),
    OPT_FLOAT("heartbeat-interval", heartbeat_interval, CONF_MIN, 0),

    OPT_CHOICE_OR_INT("screen", vo.screen_id, 0, 0, 32,
                      ({"default", -1})),

    OPT_CHOICE_OR_INT("fs-screen", vo.fsscreen_id, 0, 0, 32,
                      ({"all", -2}, {"current", -1})),

#if HAVE_COCOA
    OPT_FLAG("native-fs", vo.native_fs, 0),
#endif

    OPT_INTRANGE("brightness", gamma_brightness, 0, -100, 100),
    OPT_INTRANGE("saturation", gamma_saturation, 0, -100, 100),
    OPT_INTRANGE("contrast", gamma_contrast, 0, -100, 100),
    OPT_INTRANGE("hue", gamma_hue, 0, -100, 100),
    OPT_INTRANGE("gamma", gamma_gamma, 0, -100, 100),
    OPT_FLAG("keepaspect", vo.keepaspect, 0),

//---------------------- mplayer-only options ------------------------

    OPT_FLAG("use-filedir-conf", use_filedir_conf, M_OPT_GLOBAL),
    OPT_CHOICE("osd-level", osd_level, 0,
               ({"0", 0}, {"1", 1}, {"2", 2}, {"3", 3})),
    OPT_INTRANGE("osd-duration", osd_duration, 0, 0, 3600000),
    OPT_FLAG("osd-fractions", osd_fractions, 0),
    OPT_FLOATRANGE("osd-scale", osd_scale, 0, 0, 100),
    OPT_FLAG("osd-scale-by-window", osd_scale_by_window, 0),

    OPT_DOUBLE("sstep", step_sec, CONF_MIN, 0),

    OPT_CHOICE("framedrop", frame_dropping, 0,
               ({"no", 0},
                {"yes", 1},
                {"hard", 2})),

    OPT_FLAG("untimed", untimed, M_OPT_FIXED),

    OPT_STRING("stream-capture", stream_capture, M_OPT_FIXED),
    OPT_STRING("stream-dump", stream_dump, M_OPT_FIXED),

    OPT_CHOICE_OR_INT("loop", loop_times, M_OPT_GLOBAL, 2, 10000,
                      ({"no", -1}, {"1", -1},
                       {"inf", 0})),

    OPT_FLAG("resume-playback", position_resume, 0),
    OPT_FLAG("save-position-on-quit", position_save_on_quit, 0),

    OPT_FLAG("ordered-chapters", ordered_chapters, 0),
    OPT_STRING("ordered-chapters-files", ordered_chapters_files, 0),
    OPT_INTRANGE("chapter-merge-threshold", chapter_merge_threshold, 0, 0, 10000),

    OPT_DOUBLE("chapter-seek-threshold", chapter_seek_threshold, 0),

    OPT_FLAG("load-unsafe-playlists", load_unsafe_playlists, 0),
    OPT_FLAG("merge-files", merge_files, 0),

    // a-v sync stuff:
    OPT_FLAG("correct-pts", correct_pts, 0),
    OPT_CHOICE("pts-association-mode", user_pts_assoc_mode, 0,
               ({"auto", 0}, {"decoder", 1}, {"sort", 2})),
    OPT_FLAG("initial-audio-sync", initial_audio_sync, 0),
    OPT_CHOICE("hr-seek", hr_seek, 0,
               ({"no", -1}, {"absolute", 0}, {"always", 1}, {"yes", 1})),
    OPT_FLOATRANGE("hr-seek-demuxer-offset", hr_seek_demuxer_offset, 0, -9, 99),
    OPT_CHOICE_OR_INT("autosync", autosync, 0, 0, 10000,
                      ({"no", -1})),

    OPT_FLAG("softsleep", softsleep, 0),

    OPT_CHOICE("term-osd", term_osd, 0,
               ({"force", 1},
                {"auto", 2},
                {"no", 0})),

    OPT_FLAG("term-osd-bar", term_osd_bar, 0),
    OPT_STRING("term-osd-bar-chars", term_osd_bar_chars, 0),

    OPT_STRING("playing-msg", playing_msg, 0),
    OPT_STRING("status-msg", status_msg, 0),
    OPT_STRING("osd-status-msg", osd_status_msg, 0),

    OPT_FLAG("slave-broken", slave_mode, CONF_GLOBAL),
    OPT_FLAG("idle", player_idle_mode, M_OPT_GLOBAL),
    OPT_INTRANGE("key-fifo-size", input.key_fifo_size, CONF_GLOBAL, 2, 65000),
    OPT_FLAG("consolecontrols", consolecontrols, CONF_GLOBAL),
    OPT_FLAG("mouse-movements", vo.enable_mouse_movements, CONF_GLOBAL),
#if HAVE_TV
    {"tvscan", (void *) tvscan_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif /* HAVE_TV */

    {"screenshot", (void *) screenshot_conf, CONF_TYPE_SUBCONFIG},

    {"", (void *) mp_input_opts, CONF_TYPE_SUBCONFIG},

    OPT_PRINT("list-properties", property_print_help),
    OPT_PRINT("help", print_help),
    OPT_PRINT("h", print_help),
    OPT_PRINT("version", print_version),
    OPT_PRINT("V", print_version),

#if HAVE_ENCODING
    OPT_STRING("o", encode_output.file, CONF_GLOBAL),
    OPT_STRING("of", encode_output.format, CONF_GLOBAL),
    OPT_STRINGLIST("ofopts*", encode_output.fopts, CONF_GLOBAL),
    OPT_FLOATRANGE("ofps", encode_output.fps, CONF_GLOBAL, 0.0, 1000000.0),
    OPT_FLOATRANGE("omaxfps", encode_output.maxfps, CONF_GLOBAL, 0.0, 1000000.0),
    OPT_STRING("ovc", encode_output.vcodec, CONF_GLOBAL),
    OPT_STRINGLIST("ovcopts*", encode_output.vopts, CONF_GLOBAL),
    OPT_STRING("oac", encode_output.acodec, CONF_GLOBAL),
    OPT_STRINGLIST("oacopts*", encode_output.aopts, CONF_GLOBAL),
    OPT_FLAG("oharddup", encode_output.harddup, CONF_GLOBAL),
    OPT_FLOATRANGE("ovoffset", encode_output.voffset, CONF_GLOBAL, -1000000.0, 1000000.0),
    OPT_FLOATRANGE("oaoffset", encode_output.aoffset, CONF_GLOBAL, -1000000.0, 1000000.0),
    OPT_FLAG("ocopyts", encode_output.copyts, CONF_GLOBAL),
    OPT_FLAG("orawts", encode_output.rawts, CONF_GLOBAL),
    OPT_FLAG("oautofps", encode_output.autofps, CONF_GLOBAL),
    OPT_FLAG("oneverdrop", encode_output.neverdrop, CONF_GLOBAL),
    OPT_FLAG("ovfirst", encode_output.video_first, CONF_GLOBAL),
    OPT_FLAG("oafirst", encode_output.audio_first, CONF_GLOBAL),
    OPT_FLAG("ometadata", encode_output.metadata, CONF_GLOBAL),
#endif

    {NULL, NULL, 0, 0, 0, 0, NULL}
};

const struct MPOpts mp_default_opts = {
    .use_terminal = 1,
    .msg_color = 1,
    .audio_driver_list = NULL,
    .audio_decoders = "-spdif:*", // never select spdif by default
    .video_decoders = NULL,
    .deinterlace = -1,
    .fixed_vo = 1,
    .softvol = SOFTVOL_AUTO,
    .softvol_max = 200,
    .mixer_init_volume = -1,
    .mixer_init_mute = -1,
    .volstep = 3,
    .vo = {
        .video_driver_list = NULL,
        .monitor_pixel_aspect = 1.0,
        .screen_id = -1,
        .fsscreen_id = -1,
        .enable_mouse_movements = 1,
        .fsmode = 0,
        .panscan = 0.0f,
        .keepaspect = 1,
        .border = 1,
        .WinID = -1,
    },
    .wintitle = "mpv - ${media-title}",
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
    .lua_load_osc = 1,
    .auto_load_scripts = 1,
    .loop_times = -1,
    .ordered_chapters = 1,
    .chapter_merge_threshold = 100,
    .chapter_seek_threshold = 5.0,
    .load_config = 1,
    .position_resume = 1,
    .stream_cache_min_percent = 20.0,
    .stream_cache_seek_min_percent = 50.0,
    .stream_cache_pause = 10.0,
    .network_rtsp_transport = 2,
    .chapterrange = {-1, -1},
    .edition_id = -1,
    .default_max_pts_correction = -1,
    .correct_pts = 1,
    .user_pts_assoc_mode = 1,
    .initial_audio_sync = 1,
    .term_osd = 2,
    .term_osd_bar_chars = "[-+-]",
    .consolecontrols = 1,
    .play_frames = -1,
    .keep_open = 0,
    .audio_id = -1,
    .video_id = -1,
    .sub_id = -1,
    .sub2_id = -2,
    .audio_display = 1,
    .sub_visibility = 1,
    .sub_pos = 100,
    .sub_speed = 1.0,
    .quvi_fetch_subtitles = 0,
    .audio_output_channels = MP_CHMAP_INIT_STEREO,
    .audio_output_format = 0,  // AF_FORMAT_UNKNOWN
    .playback_speed = 1.,
    .movie_aspect = -1.,
    .field_dominance = -1,
    .sub_auto = 1,
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
    .suboverlap_enabled = 0,
#if HAVE_ENCA
    .sub_cp = "enca",
#else
    .sub_cp = "UTF-8:UTF-8-BROKEN",
#endif

    .hwdec_codecs = "h264,vc1,wmv3",

    .index_mode = -1,

    .ad_lavc_param = {
        .ac3drc = 1.,
        .downmix = 1,
        .threads = 1,
    },
    .lavfdopts = {
        .allow_mimetype = 1,
    },
    .lavc_param = {
        .show_all = 0,
        .check_hw_profile = 1,
    },
    .input = {
        .key_fifo_size = 7,
        .doubleclick_time = 300,
        .ar_delay = 200,
        .ar_rate = 40,
        .use_lirc = 1,
        .use_alt_gr = 1,
#if HAVE_COCOA
        .use_ar = 1,
        .use_media_keys = 1,
#endif
        .default_bindings = 1,
    },
    .encode_output = {
        .metadata = 1,
    },
};

#endif /* MPLAYER_CFG_MPLAYER_H */
