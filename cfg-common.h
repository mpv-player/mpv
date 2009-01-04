#ifndef MPLAYER_CFG_COMMON_H
#define MPLAYER_CFG_COMMON_H

#include <sys/types.h>
#include "config.h"
#include "m_config.h"
#include "m_option.h"

extern char *mp_msg_charset;
extern int mp_msg_color;
extern int mp_msg_module;

// codec/filter opts: (defined at libmpcodecs/vd.c)
extern float screen_size_xy;
extern float movie_aspect;
extern int softzoom;
extern int flip;

/* defined in codec-cfg.c */
extern char * codecs_file;

/* defined in dec_video.c */
extern int field_dominance;

/* from dec_audio, currently used for ac3surround decoder only */
extern int audio_output_channels;
extern int fakemono;

/* defined in network.c */
extern char *network_username;
extern char *network_password;
extern int   network_bandwidth;
extern char *network_useragent;
extern int   network_cookies_enabled;
extern char *cookies_file;

extern int network_prefer_ipv4;
extern int network_ipv4_only_proxy;
extern int reuse_socket;

extern int dvd_speed; /* stream/stream_dvd.c */

extern float a52_drc_level;

/* defined in libmpdemux: */
extern int hr_mp3_seek;
extern const m_option_t demux_rawaudio_opts[];
extern const m_option_t demux_rawvideo_opts[];
extern const m_option_t cdda_opts[];

extern char* sub_stream;
extern int demuxer_type, audio_demuxer_type, sub_demuxer_type;
extern int ts_prog;
extern int ts_keep_broken;
extern off_t ts_probe;
extern int audio_substream_id;
extern off_t ps_probe;

#include "stream/tv.h"
#include "stream/stream_radio.h"



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
	{"on", "-tv on has been removed, use tv:// instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"immediatemode", &stream_tv_defaults.immediate, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
	{"noaudio", &stream_tv_defaults.noaudio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
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
#if defined(CONFIG_TV_V4L) || defined(CONFIG_TV_V4L2) || defined(CONFIG_TV_DSHOW)
	{"buffersize", &stream_tv_defaults.buffer_size, CONF_TYPE_INT, CONF_RANGE, 16, 1024, NULL},
	{"amode", &stream_tv_defaults.amode, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
	{"volume", &stream_tv_defaults.volume, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
#endif
#if defined(CONFIG_TV_V4L) || defined(CONFIG_TV_V4L2)
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
#endif /* defined(CONFIG_TV_V4L) || defined(CONFIG_TV_V4L2) */
	{"adevice", &stream_tv_defaults.adevice, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef CONFIG_TV_TELETEXT
	{"tdevice", &stream_tv_defaults.tdevice, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"tpage", &stream_tv_defaults.tpage, CONF_TYPE_INT, CONF_RANGE, 100, 899, NULL},
	{"tformat", &stream_tv_defaults.tformat, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
	{"tlang", &stream_tv_defaults.tlang, CONF_TYPE_INT, CONF_RANGE, -1, 0x7f, NULL},
#endif /* CONFIG_TV_TELETEXT */
	{"audioid", &stream_tv_defaults.audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
#ifdef CONFIG_TV_DSHOW
	{"hidden_video_renderer", &stream_tv_defaults.hidden_video_renderer, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nohidden_video_renderer", &stream_tv_defaults.hidden_video_renderer, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"hidden_vp_renderer", &stream_tv_defaults.hidden_vp_renderer, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nohidden_vp_renderer", &stream_tv_defaults.hidden_vp_renderer, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"system_clock", &stream_tv_defaults.system_clock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nosystem_clock", &stream_tv_defaults.system_clock, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"normalize_audio_chunks", &stream_tv_defaults.normalize_audio_chunks, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nonormalize_audio_chunks", &stream_tv_defaults.normalize_audio_chunks, CONF_TYPE_FLAG, 0, 0, 0, NULL},
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

extern const m_config_t dvbin_opts_conf[];
extern const m_option_t lavfdopts_conf[];

extern int rtspStreamOverTCP;
extern int rtsp_transport_tcp;
extern int rtsp_transport_sctp;
extern int rtsp_port;
extern char *rtsp_destination;


extern int audio_stream_cache;

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

/* VIVO demuxer options: */
extern int vivo_param_version;
extern char *vivo_param_acodec;
extern int vivo_param_abitrate;
extern int vivo_param_samplerate;
extern int vivo_param_bytesperblock;
extern int vivo_param_width;
extern int vivo_param_height;
extern int vivo_param_vformat;
extern char *dvd_device, *cdrom_device;

const m_option_t vivoopts_conf[]={
	{"version", &vivo_param_version, CONF_TYPE_INT, 0, 0, 0, NULL},
	/* audio options */
	{"acodec", &vivo_param_acodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"abitrate", &vivo_param_abitrate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"samplerate", &vivo_param_samplerate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"bytesperblock", &vivo_param_bytesperblock, CONF_TYPE_INT, 0, 0, 0, NULL},
	/* video options */
	{"width", &vivo_param_width, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"height", &vivo_param_height, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"vformat", &vivo_param_vformat, CONF_TYPE_INT, 0, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

extern int    mf_w;
extern int    mf_h;
extern double mf_fps;
extern char * mf_type;
extern m_obj_settings_t* vf_settings;
extern m_obj_list_t vf_obj_list;

const m_option_t mfopts_conf[]={
        {"on", "-mf on has been removed, use mf:// instead.\n", CONF_TYPE_PRINT, 0, 0, 1, NULL},
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
	{ "gplayer", &mp_msg_levels[MSGT_GPLAYER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
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
	{ "mencoder", &mp_msg_levels[MSGT_MENCODER], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
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
	{ "osd-menu", &mp_msg_levels[MSGT_OSD_MENU], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
	{ "identify", &mp_msg_levels[MSGT_IDENTIFY], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
	{ "ass", &mp_msg_levels[MSGT_ASS], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
	{ "statusline", &mp_msg_levels[MSGT_STATUSLINE], CONF_TYPE_INT, CONF_RANGE, -1, 9, NULL },
        {"help", "Available msg modules:\n"
        "   global     - common player errors/information\n"
        "   cplayer    - console player (mplayer.c)\n"
        "   gplayer    - gui player\n"
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
        "   mencoder\n"
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
        "   osd-menu   - OSD menu messages\n"
        "   afilter    - Audio filter messages\n"
        "   netst      - Netstream\n"
        "   muxer      - muxer layer\n"
        "   identify   - identify output\n"
        "   ass        - libass messages\n"
        "   statusline - playback/encoding status line\n"
        "\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
      	{NULL, NULL, 0, 0, 0, 0, NULL}

};

#if defined(__MINGW32__) || defined(__CYGWIN__)
struct {
  char* name;
  int prio;
} priority_presets_defs[] = {
  { "realtime", REALTIME_PRIORITY_CLASS},
  { "high", HIGH_PRIORITY_CLASS},
#ifdef ABOVE_NORMAL_PRIORITY_CLASS
  { "abovenormal", ABOVE_NORMAL_PRIORITY_CLASS},
#endif
  { "normal", NORMAL_PRIORITY_CLASS},
#ifdef BELOW_NORMAL_PRIORITY_CLASS
  { "belownormal", BELOW_NORMAL_PRIORITY_CLASS},
#endif
  { "idle", IDLE_PRIORITY_CLASS},
  { NULL, NORMAL_PRIORITY_CLASS} /* default */
};
#endif /* defined(__MINGW32__) || defined(__CYGWIN__) */

extern const m_option_t noconfig_opts[];

extern const m_option_t lavc_decode_opts_conf[];
extern const m_option_t xvid_dec_opts[];

int dvd_parse_chapter_range(const m_option_t*, const char*);

#endif /* MPLAYER_CFG_COMMON_H */
