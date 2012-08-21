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

#include "options.h"
#include "config.h"
#include "m_config.h"
#include "m_option.h"
#include "stream/tv.h"
#include "stream/stream_radio.h"
#include "libvo/csputils.h"

extern char *fb_mode_cfgfile;
extern char *fb_mode_name;

extern char *lirc_configfile;

/* only used at startup (setting these values from configfile) */
extern char *vo_geometry;
extern int stop_xscreensaver;

extern int mp_msg_color;
extern int mp_msg_module;

/* defined in codec-cfg.c */
extern char *codecs_file;

/* defined in dec_video.c */
extern int field_dominance;

/* from dec_audio, currently used for ac3surround decoder only */
extern int fakemono;

/* defined in network.c */
extern char *network_username;
extern char *network_password;
extern int   network_bandwidth;
extern char *network_useragent;
extern char *network_referrer;
extern int   network_cookies_enabled;
extern char *cookies_file;

extern int network_prefer_ipv4;
extern int network_ipv4_only_proxy;
extern int reuse_socket;

extern int dvd_speed; /* stream/stream_dvd.c */

/* defined in libmpdemux: */
extern const m_option_t demux_rawaudio_opts[];
extern const m_option_t demux_rawvideo_opts[];
extern const m_option_t cdda_opts[];

extern int sws_flags;
extern const char pp_help[];

#ifdef CONFIG_RADIO
const m_option_t radioopts_conf[]={
    {"device", &stream_radio_defaults.device, CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {"driver", &stream_radio_defaults.driver, CONF_TYPE_STRING, 0, 0 ,0, NULL},
#ifdef RADIO_BSDBT848_HDR
    {"freq_min", &stream_radio_defaults.freq_min, CONF_TYPE_FLOAT, 0, 0 ,0, NULL},
    {"freq_max", &stream_radio_defaults.freq_max, CONF_TYPE_FLOAT, 0, 0 ,0, NULL},
#endif
    {"channels", &stream_radio_defaults.channels, CONF_TYPE_STRING_LIST, 0, 0 ,0, NULL},
    {"volume", &stream_radio_defaults.volume, CONF_TYPE_INT, CONF_RANGE, 0 ,100, NULL},
    {"adevice", &stream_radio_defaults.adevice, CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {"arate", &stream_radio_defaults.arate, CONF_TYPE_INT, CONF_MIN, 0 ,0, NULL},
    {"achannels", &stream_radio_defaults.achannels, CONF_TYPE_INT, CONF_MIN, 0 ,0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif /* CONFIG_RADIO */

#ifdef CONFIG_TV
const m_option_t tvopts_conf[]={
    {"immediatemode", &stream_tv_defaults.immediate, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
    {"no-audio", &stream_tv_defaults.noaudio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"audiorate", &stream_tv_defaults.audiorate, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"driver", &stream_tv_defaults.driver, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"device", &stream_tv_defaults.device, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"freq", &stream_tv_defaults.freq, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"channel", &stream_tv_defaults.channel, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"chanlist", &stream_tv_defaults.chanlist, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"norm", &stream_tv_defaults.norm, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"automute", &stream_tv_defaults.automute, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
#if defined(CONFIG_TV_V4L2) || defined(CONFIG_TV_DSHOW)
    {"normid", &stream_tv_defaults.normid, CONF_TYPE_INT, 0, 0, 0, NULL},
#endif
    {"width", &stream_tv_defaults.width, CONF_TYPE_INT, 0, 0, 4096, NULL},
    {"height", &stream_tv_defaults.height, CONF_TYPE_INT, 0, 0, 4096, NULL},
    {"input", &stream_tv_defaults.input, CONF_TYPE_INT, 0, 0, 20, NULL},
    {"outfmt", &stream_tv_defaults.outfmt, CONF_TYPE_IMGFMT, 0, 0, 0, NULL},
    {"fps", &stream_tv_defaults.fps, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
    {"channels", &stream_tv_defaults.channels, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"brightness", &stream_tv_defaults.brightness, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"contrast", &stream_tv_defaults.contrast, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"hue", &stream_tv_defaults.hue, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"saturation", &stream_tv_defaults.saturation, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"gain", &stream_tv_defaults.gain, CONF_TYPE_INT, CONF_RANGE, -1, 100, NULL},
#if defined(CONFIG_TV_V4L2) || defined(CONFIG_TV_DSHOW)
    {"buffersize", &stream_tv_defaults.buffer_size, CONF_TYPE_INT, CONF_RANGE, 16, 1024, NULL},
    {"amode", &stream_tv_defaults.amode, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
    {"volume", &stream_tv_defaults.volume, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
#endif
#if defined(CONFIG_TV_V4L2)
    {"bass", &stream_tv_defaults.bass, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
    {"treble", &stream_tv_defaults.treble, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
    {"balance", &stream_tv_defaults.balance, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
    {"forcechan", &stream_tv_defaults.forcechan, CONF_TYPE_INT, CONF_RANGE, 1, 2, NULL},
    {"forceaudio", &stream_tv_defaults.force_audio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"buffersize", &stream_tv_defaults.buffer_size, CONF_TYPE_INT, CONF_RANGE, 16, 1024, NULL},
    {"mjpeg", &stream_tv_defaults.mjpeg, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"decimation", &stream_tv_defaults.decimation, CONF_TYPE_INT, CONF_RANGE, 1, 4, NULL},
    {"quality", &stream_tv_defaults.quality, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
#ifdef CONFIG_ALSA
    {"alsa", &stream_tv_defaults.alsa, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif /* CONFIG_ALSA */
#endif /* defined(CONFIG_TV_V4L2) */
    {"adevice", &stream_tv_defaults.adevice, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"audioid", &stream_tv_defaults.audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
#ifdef CONFIG_TV_DSHOW
    {"hidden_video_renderer", &stream_tv_defaults.hidden_video_renderer, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"hidden_vp_renderer", &stream_tv_defaults.hidden_vp_renderer, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"system_clock", &stream_tv_defaults.system_clock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"normalize_audio_chunks", &stream_tv_defaults.normalize_audio_chunks, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif /* CONFIG_TV */

extern int pvr_param_aspect_ratio;
extern int pvr_param_sample_rate;
extern int pvr_param_audio_layer;
extern int pvr_param_audio_bitrate;
extern char *pvr_param_audio_mode;
extern int pvr_param_bitrate;
extern char *pvr_param_bitrate_mode;
extern int pvr_param_bitrate_peak;
extern char *pvr_param_stream_type;

#ifdef CONFIG_PVR
const m_option_t pvropts_conf[]={
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
#endif /* CONFIG_PVR */

extern const m_option_t dvbin_opts_conf[];
extern const m_option_t lavfdopts_conf[];

extern int sws_chr_vshift;
extern int sws_chr_hshift;
extern float sws_chr_gblur;
extern float sws_lum_gblur;
extern float sws_chr_sharpen;
extern float sws_lum_sharpen;

const m_option_t scaler_filter_conf[]={
    {"lgb", &sws_lum_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
    {"cgb", &sws_chr_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
    {"cvs", &sws_chr_vshift, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"chs", &sws_chr_hshift, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"ls", &sws_lum_sharpen, CONF_TYPE_FLOAT, 0, -100.0, 100.0, NULL},
    {"cs", &sws_chr_sharpen, CONF_TYPE_FLOAT, 0, -100.0, 100.0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern char *dvd_device, *cdrom_device;

extern int    mf_w;
extern int    mf_h;
extern double mf_fps;
extern char * mf_type;
extern m_obj_list_t vf_obj_list;

const m_option_t mfopts_conf[]={
    {"w", &mf_w, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"h", &mf_h, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"fps", &mf_fps, CONF_TYPE_DOUBLE, 0, 0, 0, NULL},
    {"type", &mf_type, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#include "libaf/af.h"
extern af_cfg_t af_cfg; // Audio filter configuration, defined in libmpcodecs/dec_audio.c
const m_option_t audio_filter_conf[]={
    {"list", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"force", &af_cfg.force, CONF_TYPE_INT, CONF_RANGE, 0, 7, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern int mp_msg_levels[MSGT_MAX];
extern int mp_msg_level_all;

const m_option_t msgl_config[]={
    { "all", &mp_msg_level_all, CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL},

    { "global", &mp_msg_levels[MSGT_GLOBAL], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cplayer", &mp_msg_levels[MSGT_CPLAYER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "vo", &mp_msg_levels[MSGT_VO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "ao", &mp_msg_levels[MSGT_AO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "demuxer", &mp_msg_levels[MSGT_DEMUXER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "ds", &mp_msg_levels[MSGT_DS], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "demux", &mp_msg_levels[MSGT_DEMUX], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "header", &mp_msg_levels[MSGT_HEADER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "avsync", &mp_msg_levels[MSGT_AVSYNC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "autoq", &mp_msg_levels[MSGT_AUTOQ], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cfgparser", &mp_msg_levels[MSGT_CFGPARSER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "decaudio", &mp_msg_levels[MSGT_DECAUDIO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "decvideo", &mp_msg_levels[MSGT_DECVIDEO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "seek", &mp_msg_levels[MSGT_SEEK], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "win32", &mp_msg_levels[MSGT_WIN32], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "open", &mp_msg_levels[MSGT_OPEN], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "dvd", &mp_msg_levels[MSGT_DVD], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "parsees", &mp_msg_levels[MSGT_PARSEES], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "lirc", &mp_msg_levels[MSGT_LIRC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "stream", &mp_msg_levels[MSGT_STREAM], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cache", &mp_msg_levels[MSGT_CACHE], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "xacodec", &mp_msg_levels[MSGT_XACODEC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "tv", &mp_msg_levels[MSGT_TV], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "radio", &mp_msg_levels[MSGT_RADIO], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "osdep", &mp_msg_levels[MSGT_OSDEP], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "spudec", &mp_msg_levels[MSGT_SPUDEC], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "playtree", &mp_msg_levels[MSGT_PLAYTREE], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "input", &mp_msg_levels[MSGT_INPUT], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "vfilter", &mp_msg_levels[MSGT_VFILTER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "osd", &mp_msg_levels[MSGT_OSD], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "network", &mp_msg_levels[MSGT_NETWORK], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "cpudetect", &mp_msg_levels[MSGT_CPUDETECT], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "codeccfg", &mp_msg_levels[MSGT_CODECCFG], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "sws", &mp_msg_levels[MSGT_SWS], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "vobsub", &mp_msg_levels[MSGT_VOBSUB], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "subreader", &mp_msg_levels[MSGT_SUBREADER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "afilter", &mp_msg_levels[MSGT_AFILTER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "netst", &mp_msg_levels[MSGT_NETST], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "muxer", &mp_msg_levels[MSGT_MUXER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "identify", &mp_msg_levels[MSGT_IDENTIFY], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "ass", &mp_msg_levels[MSGT_ASS], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "statusline", &mp_msg_levels[MSGT_STATUSLINE], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    { "fixme", &mp_msg_levels[MSGT_FIXME], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
    {"help", "Available msg modules:\n"
    "   global     - common player errors/information\n"
    "   cplayer    - console player (mplayer.c)\n"
    "   vo         - libvo\n"
    "   ao         - libao\n"
    "   demuxer    - demuxer.c (general stuff)\n"
    "   ds         - demux stream (add/read packet etc)\n"
    "   demux      - fileformat-specific stuff (demux_*.c)\n"
    "   header     - fileformat-specific header (*header.c)\n"
    "   avsync     - mplayer.c timer stuff\n"
    "   autoq      - mplayer.c auto-quality stuff\n"
    "   cfgparser  - cfgparser.c\n"
    "   decaudio   - av decoder\n"
    "   decvideo\n"
    "   seek       - seeking code\n"
    "   win32      - win32 dll stuff\n"
    "   open       - open.c (stream opening)\n"
    "   dvd        - open.c (DVD init/read/seek)\n"
    "   parsees    - parse_es.c (mpeg stream parser)\n"
    "   lirc       - lirc_mp.c and input lirc driver\n"
    "   stream     - stream.c\n"
    "   cache      - cache2.c\n"
    "   xacodec    - XAnim codecs\n"
    "   tv         - TV input subsystem\n"
    "   osdep      - OS-dependent parts\n"
    "   spudec     - spudec.c\n"
    "   playtree   - Playtree handling (playtree.c, playtreeparser.c)\n"
    "   input\n"
    "   vfilter\n"
    "   osd\n"
    "   network\n"
    "   cpudetect\n"
    "   codeccfg\n"
    "   sws\n"
    "   vobsub\n"
    "   subreader\n"
    "   afilter    - Audio filter messages\n"
    "   netst      - Netstream\n"
    "   muxer      - muxer layer\n"
    "   identify   - identify output\n"
    "   ass        - libass messages\n"
    "   statusline - playback/encoding status line\n"
    "   fixme      - messages not yet fixed to map to module\n"
    "\n", CONF_TYPE_PRINT, CONF_GLOBAL | CONF_NOCFG, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}

};

extern const m_option_t lavc_decode_opts_conf[];
extern const m_option_t xvid_dec_opts[];

const m_option_t common_opts[] = {
// ------------------------- common options --------------------
    OPT_MAKE_FLAGS("quiet", quiet, CONF_GLOBAL),
    {"really-quiet", &verbose, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_PRE_PARSE, 0, -10, NULL},
    // -v is handled in command line preparser
    {"v", NULL, CONF_TYPE_FLAG, CONF_GLOBAL | CONF_NOCFG, 0, 0, NULL},
    {"msglevel", (void *) msgl_config, CONF_TYPE_SUBCONFIG, CONF_GLOBAL, 0, 0, NULL},
    {"msgcolor", &mp_msg_color, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"msgmodule", &mp_msg_module, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
#ifdef CONFIG_PRIORITY
    {"priority", &proc_priority, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
    OPT_CHOICE("noconfig", noconfig, CONF_GLOBAL | CONF_NOCFG | CONF_PRE_PARSE,
               ({"off", 0}, {"user", 1}, {"system", 2}, {"all", 3})),

// ------------------------- stream options --------------------

#ifdef CONFIG_STREAM_CACHE
    {"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 32, 0x7fffffff, NULL},
    {"no-cache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    OPT_FLOATRANGE("cache-min", stream_cache_min_percent, 0, 0, 99),
    OPT_FLOATRANGE("cache-seek-min", stream_cache_seek_min_percent, 0, 0, 99),
#endif /* CONFIG_STREAM_CACHE */
    {"cdrom-device", &cdrom_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef CONFIG_DVDREAD
    {"dvd-device", &dvd_device,  CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"dvd-speed", &dvd_speed, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"dvdangle", &dvd_angle, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
#endif /* CONFIG_DVDREAD */
    OPT_INTPAIR("chapter", chapterrange, 0),
    OPT_INTRANGE("edition", edition_id, 0, -1, 8190),
#ifdef CONFIG_LIBBLURAY
    {"bluray-device",  &bluray_device,  CONF_TYPE_STRING, 0,          0,  0, NULL},
    {"bluray-angle",   &bluray_angle,   CONF_TYPE_INT,    CONF_RANGE, 0, 999, NULL},
    {"bluray-chapter", &bluray_chapter, CONF_TYPE_INT,    CONF_RANGE, 0, 999, NULL},
#endif /* CONFIG_LIBBLURAY */

#ifdef CONFIG_NETWORKING
    {"user", &network_username, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"passwd", &network_password, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"bandwidth", &network_bandwidth, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
    {"http-header-fields", &network_http_header_fields, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"user-agent", &network_useragent, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"referrer", &network_referrer, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cookies", &network_cookies_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"cookies-file", &cookies_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"prefer-ipv4", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"ipv4-only-proxy", &network_ipv4_only_proxy, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"reuse-socket", &reuse_socket, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#ifdef HAVE_AF_INET6
    {"prefer-ipv6", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#endif /* HAVE_AF_INET6 */
#endif /* CONFIG_NETWORKING */

// ------------------------- demuxer options --------------------

    // number of frames to play/convert
    {"frames", &play_n_frames_mf, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

    // seek to byte/seconds position
    {"sb", &seek_to_byte, CONF_TYPE_POSITION, CONF_MIN, 0, 0, NULL},
    OPT_TIME("ss", seek_to_sec, 0),

    // start paused
    OPT_FLAG_ON("pause", start_paused, 0),

    // stop at given position
    {"endpos", &end_at, CONF_TYPE_TIME_SIZE, 0, 0, 0, NULL},

    // AVI specific: force non-interleaved mode
    {"ni", &force_ni, CONF_TYPE_FLAG, 0, 0, 1, NULL},

    // AVI and Ogg only: (re)build index at startup
    {"idx", &index_mode, CONF_TYPE_FLAG, 0, -1, 1, NULL},
    {"forceidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 2, NULL},
    {"saveidx", &index_file_save, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"loadidx", &index_file_load, CONF_TYPE_STRING, 0, 0, 0, NULL},

    // select audio/video/subtitle stream
    OPT_INTRANGE("aid", audio_id, 0, -2, 8190),
    OPT_INTRANGE("vid", video_id, 0, -2, 8190),
    OPT_INTRANGE("sid", sub_id, 0, -2, 8190),
    OPT_FLAG_CONSTANTS("no-sub", sub_id, 0, -1, -2),
    OPT_FLAG_CONSTANTS("no-video", video_id, 0, -1, -2),
    OPT_FLAG_CONSTANTS("audio", audio_id, 0, -2, -1),
    OPT_FLAG_CONSTANTS("no-audio", audio_id, 0, -1, -2),
    OPT_STRINGLIST("alang", audio_lang, 0),
    OPT_STRINGLIST("slang", sub_lang, 0),

    OPT_MAKE_FLAGS("hr-mp3-seek", hr_mp3_seek, 0),

    OPT_STRING("quvi-format", quvi_format, 0),

    { "rawaudio", (void *)&demux_rawaudio_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    { "rawvideo", (void *)&demux_rawvideo_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

#ifdef CONFIG_CDDA
    { "cdda", (void *)&cdda_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

    // demuxer.c - select audio/sub file/demuxer
    OPT_STRING("audiofile", audio_stream, 0),
    OPT_INTRANGE("audiofile-cache", audio_stream_cache, 0, 50, 65536),
    OPT_STRING("subfile", sub_stream, 0),
    OPT_STRING("demuxer", demuxer_name, 0),
    OPT_STRING("audio-demuxer", audio_demuxer_name, 0),
    OPT_STRING("sub-demuxer", sub_demuxer_name, 0),
    OPT_MAKE_FLAGS("extbased", extension_parsing, 0),

    {"mf", (void *) mfopts_conf, CONF_TYPE_SUBCONFIG, 0,0,0, NULL},
#ifdef CONFIG_RADIO
    {"radio", radioopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif /* CONFIG_RADIO */
#ifdef CONFIG_TV
    {"tv", (void *) tvopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif /* CONFIG_TV */
#ifdef CONFIG_PVR
    {"pvr", (void *) pvropts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif /* CONFIG_PVR */
#ifdef CONFIG_DVBIN
    {"dvbin", (void *) dvbin_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

// ------------------------- a-v sync options --------------------

    // AVI specific: A-V sync mode (bps vs. interleaving)
    {"bps", &pts_from_bps, CONF_TYPE_FLAG, 0, 0, 1, NULL},

    // set A-V sync correction speed (0=disables it):
    {"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},

    // force video/audio rate:
    {"fps", &force_fps, CONF_TYPE_DOUBLE, CONF_MIN, 0, 0, NULL},
    {"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},
    OPT_INTRANGE("channels", audio_output_channels, 0, 1, 8),
    OPT_AUDIOFORMAT("format", audio_output_format, 0),
    OPT_FLOATRANGE("speed", playback_speed, 0, 0.01, 100.0),

    // set a-v distance
    {"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -100.0, 100.0, NULL},

    // ignore header-specified delay (dwStart)
    {"ignore-start", &ignore_start, CONF_TYPE_FLAG, 0, 0, 1, NULL},

    OPT_FLOATRANGE("a52drc", drc_level, 0, 0, 2),

// ------------------------- codec/vfilter options --------------------

    // MP3-only: select stereo/left/right
    {"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},

    {"af*", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"af-adv", (void *) audio_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

    OPT_SETTINGSLIST("vf*", vf_settings, 0, &vf_obj_list),
    // select audio/video codec (by name) or codec family (by number):
    {"afm", &audio_fm_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"vfm", &video_fm_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"ac", &audio_codec_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
    {"vc", &video_codec_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},

    // postprocessing:
    {"pp", &divx_quality, CONF_TYPE_INT, 0, 0, 0, NULL},
#ifdef CONFIG_LIBPOSTPROC
    {"pphelp", &pp_help, CONF_TYPE_PRINT, CONF_GLOBAL | CONF_NOCFG, 0, 0, NULL},
#endif

    // scaling:
    {"sws", &sws_flags, CONF_TYPE_INT, 0, 0, 2, NULL},
    {"ssf", (void *) scaler_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    OPT_MAKE_FLAGS("zoom", softzoom, 0),
    OPT_FLOATRANGE("aspect", movie_aspect, 0, 0.1, 10.0),
    OPT_FLAG_CONSTANTS("no-aspect", movie_aspect, 0, 0, 0),
    OPT_FLOATRANGE("xy", screen_size_xy, 0, 0.001, 4096),

    OPT_FLAG_CONSTANTS("flip", flip, 0, -1, 1),

    // draw by slices or whole frame (useful with libmpeg2/libavcodec)
    OPT_MAKE_FLAGS("slices", vd_use_slices, 0),
    {"field-dominance", &field_dominance, CONF_TYPE_INT, CONF_RANGE, -1, 1, NULL},

    {"lavdopts", (void *) lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    {"lavfdopts", (void *) lavfdopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#ifdef CONFIG_XVID4
    {"xvidopts", (void *)xvid_dec_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
    {"codecs-file", &codecs_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
// ------------------------- subtitles options --------------------

    OPT_STRINGLIST("sub", sub_name, 0),
    OPT_PATHLIST("sub-paths", sub_paths, 0),
    {"subcp", &sub_cp, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"subdelay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
    {"subfps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
    OPT_MAKE_FLAGS("autosub", sub_auto, 0),
    {"unicode", &sub_unicode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"utf8", &sub_utf8, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"forcedsubsonly", &forced_subs_only, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    // specify IFO file for VOBSUB subtitle
    {"ifo", &spudec_ifo, CONF_TYPE_STRING, 0, 0, 0, NULL},
    // enable Closed Captioning display
    {"overlapsub", &suboverlap_enabled, CONF_TYPE_FLAG, 0, 0, 2, NULL},
    {"sub-bg-color", &sub_bg_color, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
    {"sub-bg-alpha", &sub_bg_alpha, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
    {"sub-no-text-pp", &sub_no_text_pp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"sub-fuzziness", &sub_match_fuzziness, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"font", &font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"subfont", &sub_font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, NULL},
    {"subpos", &sub_pos, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
    {"subalign", &sub_alignment, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"subwidth", &sub_width_p, CONF_TYPE_INT, CONF_RANGE, 10, 100, NULL},
    {"spualign", &spu_alignment, CONF_TYPE_INT, CONF_RANGE, -1, 2, NULL},
    {"spuaa", &spu_aamode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
    {"spugauss", &spu_gaussvar, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 3.0, NULL},
    {"subfont-encoding", &subtitle_font_encoding, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"subfont-text-scale", &text_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
    {"subfont-osd-scale", &osd_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
    {"subfont-blur", &subtitle_font_radius, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
    {"subfont-outline", &subtitle_font_thickness, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
    {"subfont-autoscale", &subtitle_autoscale, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
    OPT_MAKE_FLAGS("ass", ass_enabled, 0),
    OPT_FLOATRANGE("ass-font-scale", ass_font_scale, 0, 0, 100),
    OPT_FLOATRANGE("ass-line-spacing", ass_line_spacing, 0, -1000, 1000),
    OPT_INTRANGE("ass-top-margin", ass_top_margin, 0, 0, 2000),
    OPT_INTRANGE("ass-bottom-margin", ass_bottom_margin, 0, 0, 2000),
    OPT_MAKE_FLAGS("ass-use-margins", ass_use_margins, 0),
    OPT_MAKE_FLAGS("ass-vsfilter-aspect-compat", ass_vsfilter_aspect_compat, 0),
    OPT_MAKE_FLAGS("embeddedfonts", use_embedded_fonts, 0),
    OPT_STRINGLIST("ass-force-style", ass_force_style_list, 0),
    OPT_STRING("ass-color", ass_color, 0),
    OPT_STRING("ass-border-color", ass_border_color, 0),
    OPT_STRING("ass-styles", ass_styles_file, 0),
    OPT_INTRANGE("ass-hinting", ass_hinting, 0, 0, 7),
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#ifdef CONFIG_TV
const m_option_t tvscan_conf[]={
    {"autostart", &stream_tv_defaults.scan, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"threshold", &stream_tv_defaults.scan_threshold, CONF_TYPE_INT, CONF_RANGE, 1, 100, NULL},
    {"period", &stream_tv_defaults.scan_period, CONF_TYPE_FLOAT, CONF_RANGE, 0.1, 2.0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

extern const struct m_sub_options image_writer_conf;

const m_option_t screenshot_conf[] = {
    OPT_SUBSTRUCT(screenshot_image_opts, image_writer_conf, M_OPT_MERGE),
    OPT_STRING("template", screenshot_template, 0),
    {0},
};

const m_option_t mplayer_opts[]={
    /* name, pointer, type, flags, min, max */

//---------------------- libao/libvo options ------------------------
    OPT_STRINGLIST("vo", video_driver_list, 0),
    OPT_STRINGLIST("ao", audio_driver_list, 0),
    OPT_MAKE_FLAGS("fixed-vo", fixed_vo, CONF_GLOBAL),
    OPT_MAKE_FLAGS("ontop", vo_ontop, 0),
    {"rootwin", &vo_rootwin, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"border", &vo_border, CONF_TYPE_FLAG, 0, 0, 1, NULL},

    OPT_STRING("mixer", mixer_device, 0),
    OPT_STRING("mixer-channel", mixer_channel, 0),
    OPT_MAKE_FLAGS("softvol", softvol, 0),
    OPT_FLOATRANGE("softvol-max", softvol_max, 0, 10, 10000),
    {"volstep", &volstep, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
    {"volume", &start_volume, CONF_TYPE_FLOAT, CONF_RANGE, -1, 10000, NULL},
    OPT_MAKE_FLAGS("gapless-audio", gapless_audio, 0),
    // override audio buffer size (used only by -ao oss/win32, obsolete)
    OPT_INT("abs", ao_buffersize, 0),

    {"edlout", &edl_output_filename,  CONF_TYPE_STRING, 0, 0, 0, NULL},

#ifdef CONFIG_X11
    {"display", &mDisplayName, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

#if defined(CONFIG_FBDEV) || defined(CONFIG_VESA)
    {"monitor-hfreq", &monitor_hfreq_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"monitor-vfreq", &monitor_vfreq_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"monitor-dotclock", &monitor_dotclock_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

#ifdef CONFIG_FBDEV
    {"fbmode", &fb_mode_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"fbmodeconfig", &fb_mode_cfgfile, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

    // force window width/height or resolution (with -vm)
    OPT_INTRANGE("x", screen_size_x, 0, 0, 4096),
    OPT_INTRANGE("y", screen_size_y, 0, 0, 4096),
    // set screen dimensions (when not detectable or virtual!=visible)
    OPT_INTRANGE("screenw", vo_screenwidth, CONF_GLOBAL, 0, 4096),
    OPT_INTRANGE("screenh", vo_screenheight, CONF_GLOBAL, 0, 4096),
    // Geometry string
    {"geometry", &vo_geometry, CONF_TYPE_STRING, 0, 0, 0, NULL},
    OPT_MAKE_FLAGS("force-window-position", force_window_position, 0),
    // vo name (X classname) and window title strings
    OPT_STRING("name", vo_winname, 0),
    OPT_STRING("title", vo_wintitle, 0),
    // set aspect ratio of monitor - useful for 16:9 TV-out
    OPT_FLOATRANGE("monitoraspect", force_monitor_aspect, 0, 0.0, 9.0),
    OPT_FLOATRANGE("monitorpixelaspect", monitor_pixel_aspect, 0, 0.2, 9.0),
    // video mode switching: (x11,xv,dga)
    OPT_MAKE_FLAGS("vm", vidmode, 0),
    // start in fullscreen mode:
    OPT_MAKE_FLAGS("fs", fullscreen, CONF_GLOBAL),
    // set fullscreen switch method (workaround for buggy WMs)
    {"fsmode-dontuse", &vo_fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
    // set bpp (x11+vm, dga, fbdev, vesa, svga?)
    OPT_INTRANGE("bpp", vo_dbpp, 0, 0, 32),
    {"colorkey", &vo_colorkey, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"no-colorkey", &vo_colorkey, CONF_TYPE_FLAG, 0, 0, 0x1000000, NULL},
    {"double", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    // wait for v-sync (vesa)
    {"vsync", &vo_vsync, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"panscan", &vo_panscan, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    OPT_FLOATRANGE("panscanrange", vo_panscanrange, 0, -19.0, 99.0),
    OPT_CHOICE("colormatrix", requested_colorspace, 0,
               ({"auto", MP_CSP_AUTO},
                {"BT.601", MP_CSP_BT_601},
                {"BT.709", MP_CSP_BT_709},
                {"SMPTE-240M", MP_CSP_SMPTE_240M})),
    OPT_CHOICE("colormatrix-input-range", requested_input_range, 0,
               ({"auto", MP_CSP_LEVELS_AUTO},
                {"limited", MP_CSP_LEVELS_TV},
                {"full", MP_CSP_LEVELS_PC})),
    OPT_CHOICE("colormatrix-output-range", requested_output_range, 0,
               ({"auto", MP_CSP_LEVELS_AUTO},
                {"limited", MP_CSP_LEVELS_TV},
                {"full", MP_CSP_LEVELS_PC})),

    {"grabpointer", &vo_grabpointer, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    OPT_INTRANGE("cursor-autohide-delay", cursor_autohide_delay, 0, -2, 30000),

    {"adapter", &vo_adapter_num, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
    {"refreshrate",&vo_refresh_rate,CONF_TYPE_INT,CONF_RANGE, 0,100, NULL},
    {"wid", &WinID, CONF_TYPE_INT64, 0, 0, 0, NULL},
#ifdef CONFIG_X11
    {"stop-xscreensaver", &stop_xscreensaver, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"fstype", &vo_fstype_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
#endif
    {"heartbeat-cmd", &heartbeat_cmd, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"mouseinput", &vo_nomouse_input, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    {"xineramascreen", &xinerama_screen, CONF_TYPE_INT, CONF_RANGE, -2, 32, NULL},

    OPT_INTRANGE("brightness", vo_gamma_brightness, 0, -100, 100),
    OPT_INTRANGE("saturation", vo_gamma_saturation, 0, -100, 100),
    OPT_INTRANGE("contrast", vo_gamma_contrast, 0, -100, 100),
    OPT_INTRANGE("hue", vo_gamma_hue, 0, -100, 100),
    OPT_INTRANGE("gamma", vo_gamma_gamma, 0, -100, 100),
    {"keepaspect", &vo_keepaspect, CONF_TYPE_FLAG, 0, 0, 1, NULL},

//---------------------- mplayer-only options ------------------------

    {"use-filedir-conf", &use_filedir_conf, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    OPT_INTRANGE("osdlevel", osd_level, 0, 0, 3),
    OPT_INTRANGE("osd-duration", osd_duration, 0, 0, 3600000),
    OPT_MAKE_FLAGS("osd-fractions", osd_fractions, 0),

    OPT_STRING("vobsub", vobsub_name, 0),
    {"vobsubid", &vobsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},

    {"sstep", &step_sec, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

    {"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2, NULL},

    OPT_FLAG_ON("untimed", untimed, 0),

#ifdef CONFIG_LIRC
    {"lircconf", &lirc_configfile, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
#endif

    {"leak-report", "", CONF_TYPE_PRINT, 0, 0, 0, (void*)1},

    OPT_FLAG_CONSTANTS("no-loop", loop_times, 0, 0, -1),
    OPT_INTRANGE("loop", loop_times, 0, -1, 10000),

    {"playlist", NULL, CONF_TYPE_STRING, CONF_NOCFG | M_OPT_MIN, 1, 0, NULL},
    {"shuffle", NULL, CONF_TYPE_FLAG, CONF_NOCFG, 0, 0, NULL},
    {"{", NULL, CONF_TYPE_FLAG, CONF_NOCFG, 0, 0, NULL},
    {"}", NULL, CONF_TYPE_FLAG, CONF_NOCFG, 0, 0, NULL},

    OPT_MAKE_FLAGS("ordered-chapters", ordered_chapters, 0),
    OPT_INTRANGE("chapter-merge-threshold", chapter_merge_threshold, 0, 0, 10000),

    // a-v sync stuff:
    OPT_MAKE_FLAGS("correct-pts", user_correct_pts, 0),
    OPT_CHOICE("pts-association-mode", user_pts_assoc_mode, 0,
               ({"auto", 0}, {"decoder", 1}, {"sort", 2})),
    OPT_MAKE_FLAGS("initial-audio-sync", initial_audio_sync, 0),
    OPT_CHOICE("hr-seek", hr_seek, 0,
               ({"off", -1}, {"absolute", 0}, {"always", 1}, {"on", 1})),
    OPT_FLOATRANGE("hr-seek-demuxer-offset", hr_seek_demuxer_offset, 0, -9, 99),
    OPT_FLAG_CONSTANTS("no-autosync", autosync, 0, 0, -1),
    OPT_INTRANGE("autosync", autosync, 0, 0, 10000),

    OPT_FLAG_ON("softsleep", softsleep, 0),

    OPT_CHOICE("term-osd", term_osd, M_OPT_IMPLICIT_DEFAULT,
               ({"force", 1},
                {"auto", 2},
                {"off", 0})),

    OPT_STRING("term-osd-esc", term_osd_esc, 0, OPTDEF_STR("\x1b[A\r\x1b[K")),
    OPT_STRING("playing-msg", playing_msg, 0),

    {"slave-broken", &slave_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL},
    OPT_MAKE_FLAGS("idle", player_idle_mode, CONF_GLOBAL),
    OPT_INTRANGE("key-fifo-size", input.key_fifo_size, CONF_GLOBAL, 2, 65000),
    OPT_MAKE_FLAGS("consolecontrols", consolecontrols, CONF_GLOBAL),
    {"mouse-movements", &enable_mouse_movements, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    OPT_INTRANGE("doubleclick-time", doubleclick_time, 0, 0, 1000),
#ifdef CONFIG_TV
    {"tvscan", (void *) tvscan_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif /* CONFIG_TV */

    {"screenshot", (void *) screenshot_conf, CONF_TYPE_SUBCONFIG,
     M_OPT_PREFIXED, 0, 0, NULL},

    OPT_FLAG_ON("list-properties", list_properties, CONF_GLOBAL),
    {"identify", &mp_msg_levels[MSGT_IDENTIFY], CONF_TYPE_FLAG, CONF_GLOBAL, 0, MSGL_V, NULL},
    {"help", (void *) help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
    {"h", (void *) help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},

    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#endif /* MPLAYER_CFG_MPLAYER_H */
