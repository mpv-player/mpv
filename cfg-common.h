#ifdef MAIN_CONF /* this will be included in conf[] */

// ------------------------- common options --------------------
	{"quiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
	{"noquiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE|CONF_GLOBAL, 0, 100, NULL},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOSAVE, 0, 0, NULL},
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL},

// ------------------------- stream options --------------------

#ifdef USE_STREAM_CACHE
	{"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 4, 65536, NULL},
	{"nocache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#else
	{"cache", "MPlayer was compiled without cache2 support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"vcd", "-vcd N is deprecated, use vcd://N instead.\n", CONF_TYPE_PRINT, CONF_NOCFG ,0,0, NULL},
	{"cuefile", "-cuefile is deprecated, use cue://filename:N where N is the track number.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"cdrom-device", &cdrom_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef USE_DVDNAV
	{"dvdnav", "-dvdnav is deprecated, use dvdnav:// instead.\n", CONF_TYPE_PRINT, 0, 0, 1, NULL},
	{"skipopening", &dvd_nav_skip_opening, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noskipopening", &dvd_nav_skip_opening, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#endif
#ifdef USE_DVDREAD
	{"dvd-device", &dvd_device,  CONF_TYPE_STRING, 0, 0, 0, NULL}, 
	{"dvd", "-dvd N is deprecated, use dvd://N instead.\n" , CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"dvdangle", &dvd_angle, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"chapter", dvd_parse_chapter_range, CONF_TYPE_FUNC_PARAM, 0, 0, 0, NULL},
	{"alang", &audio_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},
#else
	{"dvd", "MPlayer was compiled without libdvdread support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"slang", &dvdsub_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},

        {"dvdauth", "libcss is obsolete. Try libdvdread instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"dvdkey", "libcss is obsolete. Try libdvdread instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"csslib", "libcss is obsolete. Try libdvdread instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},

#ifdef MPLAYER_NETWORK
	{"user", &network_username, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"passwd", &network_password, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"bandwidth", &network_bandwidth, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"user-agent", &network_useragent, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"cookies", &network_cookies_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nocookies", &network_cookies_enabled, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"cookies-file", &cookies_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"prefer-ipv4", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 0, 1, NULL},	
	{"ipv4-only-proxy", &network_ipv4_only_proxy, CONF_TYPE_FLAG, 0, 0, 1, NULL},	
#ifdef HAVE_AF_INET6
	{"prefer-ipv6", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#else
	{"prefer-ipv6", "MPlayer was compiled without IPv6 support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif

#else
	{"user", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"passwd", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"bandwidth", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"user-agent", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif

	
// ------------------------- demuxer options --------------------

	// number of frames to play/convert
	{"frames", &play_n_frames_mf, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	// seek to byte/seconds position
	{"sb", &seek_to_byte, CONF_TYPE_POSITION, CONF_MIN, 0, 0, NULL},
	{"ss", &seek_to_sec, CONF_TYPE_STRING, CONF_MIN, 0, 0, NULL},

	// AVI specific: force non-interleaved mode
	{"ni", &force_ni, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noni", &force_ni, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	// AVI and Ogg only: (re)build index at startup
	{"noidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 0, NULL},
	{"idx", &index_mode, CONF_TYPE_FLAG, 0, -1, 1, NULL},
	{"forceidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 2, NULL},
	{"saveidx", &index_file_save, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"loadidx", &index_file_load, CONF_TYPE_STRING, 0, 0, 0, NULL},

	// select audio/video/subtitle stream
	{"aid", &audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 8190, NULL},
	{"vid", &video_id, CONF_TYPE_INT, CONF_RANGE, 0, 8190, NULL},
	{"sid", &dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 8190, NULL},
	{"novideo", &video_id, CONF_TYPE_FLAG, 0, -1, -2, NULL},

	{ "hr-mp3-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{ "nohr-mp3-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{ "rawaudio", &demux_rawaudio_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
	{ "rawvideo", &demux_rawvideo_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

#ifdef HAVE_CDDA
	{ "cdda", &cdda_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

	// demuxer.c - select audio/sub file/demuxer
	{ "audiofile", &audio_stream, CONF_TYPE_STRING, 0, 0, 0, NULL },
	{ "audiofile-cache", &audio_stream_cache, CONF_TYPE_INT, CONF_RANGE, 50, 65536, NULL},
	{ "subfile", &sub_stream, CONF_TYPE_STRING, 0, 0, 0, NULL },
	{ "demuxer", &demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, NULL },
	{ "audio-demuxer", &audio_demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, NULL },
	{ "sub-demuxer", &sub_demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, NULL },
	{ "extbased", &extension_parsing, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{ "noextbased", &extension_parsing, CONF_TYPE_FLAG, 0, 1, 0, NULL },

        {"mf", mfopts_conf, CONF_TYPE_SUBCONFIG, 0,0,0, NULL},
#ifdef USE_TV
	{"tv", tvopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"tv", "MPlayer was compiled without TV interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif
	{"vivo", vivoopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#ifdef HAS_DVBIN_SUPPORT
	{"dvbin", dvbin_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

// ------------------------- a-v sync options --------------------

	// AVI specific: A-V sync mode (bps vs. interleaving)
	{"bps", &pts_from_bps, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nobps", &pts_from_bps, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	// set A-V sync correction speed (0=disables it):
	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10, NULL},
	
	// force video/audio rate:
	{"fps", &force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},
	{"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},
	{"channels", &audio_output_channels, CONF_TYPE_INT, CONF_RANGE, 1, 6, NULL},
	{"format", &audio_output_format, CONF_TYPE_INT, CONF_RANGE, 0, 0x00002000, NULL},

        {"a52drc", &a52_drc_level, CONF_TYPE_FLOAT, CONF_RANGE, 0, 1, NULL},

// ------------------------- codec/vfilter options --------------------

	// MP3-only: select stereo/left/right
#ifdef USE_FAKE_MONO
	{"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#endif

	// disable audio
	{"sound", &audio_id, CONF_TYPE_FLAG, 0, -2, -1, NULL},
	{"nosound", &audio_id, CONF_TYPE_FLAG, 0, -1, -2, NULL},

	{"af-adv", audio_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
	{"af", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},

	{"vop*", &vo_plugin_args, CONF_TYPE_OBJ_SETTINGS_LIST, 0, 0, 0,&vf_obj_list },
	{"vf*", &vf_settings, CONF_TYPE_OBJ_SETTINGS_LIST, 0, 0, 0, &vf_obj_list},
	// select audio/video codec (by name) or codec family (by number):
//	{"afm", &audio_family, CONF_TYPE_INT, CONF_MIN, 0, 22, NULL}, // keep ranges in sync
//	{"vfm", &video_family, CONF_TYPE_INT, CONF_MIN, 0, 29, NULL}, // with codec-cfg.c
//	{"afm", &audio_fm, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"afm", &audio_fm_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
	{"vfm", &video_fm_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
//	{"ac", &audio_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ac", &audio_codec_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
	{"vc", &video_codec_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},

	// postprocessing:
	{"divxq", "-divxq has been renamed to -pp (postprocessing), use -pp.\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
#ifdef USE_LIBAVCODEC
	{"pp", &divx_quality, CONF_TYPE_INT, 0, 0, 0, NULL},
#endif
#ifdef HAVE_ODIVX_POSTPROCESS
        {"oldpp", &use_old_pp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#else
        {"oldpp", "MPlayer was compiled without the OpenDivX library.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"npp", "-npp has been removed, use -vf pp and read the fine manual.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#ifdef FF_POSTPROCESS
        {"pphelp", &pp_help, CONF_TYPE_PRINT_INDIRECT, CONF_NOCFG, 0, 0, NULL},
#endif

	// scaling:
	{"sws", &sws_flags, CONF_TYPE_INT, 0, 0, 2, NULL},
	{"ssf", scaler_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
        {"zoom", &softzoom, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nozoom", &softzoom, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"aspect", &movie_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0, NULL},
	{"noaspect", &movie_aspect, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"xy", &screen_size_xy, CONF_TYPE_FLOAT, CONF_RANGE, 0.001, 4096, NULL},

        {"flip", &flip, CONF_TYPE_FLAG, 0, -1, 1, NULL},
        {"noflip", &flip, CONF_TYPE_FLAG, 0, -1, 0, NULL},
	{"tsfastparse", "-tsfastparse is no longer a valid option.\n", CONF_TYPE_PRINT, CONF_NOCFG ,0,0, NULL
},
	{"tsprog", &ts_prog, CONF_TYPE_INT, CONF_RANGE, 0, 65534, NULL},
#define TS_MAX_PROBE_SIZE 2000000 /* don't forget to change this in libmpdemux/demux_ts.c too */
	{"tsprobe", &ts_probe, CONF_TYPE_POSITION, 0, 0, TS_MAX_PROBE_SIZE, NULL},
	{"tskeepbroken", &ts_keep_broken, CONF_TYPE_FLAG, 0, 0, 1, NULL},

	// draw by slices or whole frame (useful with libmpeg2/libavcodec)
	{"slices", &vd_use_slices, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noslices", &vd_use_slices, CONF_TYPE_FLAG, 0, 1, 0, NULL},

#ifdef USE_LIBAVCODEC
	{"lavdopts", lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
#if defined(HAVE_XVID3) || defined(HAVE_XVID4)
	{"xvidopts", xvid_dec_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
	{"codecs-file", &codecs_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
// ------------------------- subtitles options --------------------

#ifdef USE_SUB
	{"sub", &sub_name, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
#ifdef USE_FRIBIDI
	{"fribidi-charset", &fribidi_charset, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"flip-hebrew", &flip_hebrew, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noflip-hebrew", &flip_hebrew, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#else 
	{"fribidi-charset", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"flip-hebrew", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"noflip-hebrew", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#ifdef USE_ICONV
	{"subcp", &sub_cp, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif	
	{"subdelay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
	{"subfps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
	{"autosub", &sub_auto, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"noautosub", &sub_auto, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"unicode", &sub_unicode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nounicode", &sub_unicode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"utf8", &sub_utf8, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noutf8", &sub_utf8, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"forcedsubsonly", &forced_subs_only, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	// specify IFO file for VOBSUB subtitle
	{"ifo", &spudec_ifo, CONF_TYPE_STRING, 0, 0, 0, NULL},
	// enable Closed Captioning display
	{"subcc", &subcc_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nosubcc", &subcc_enabled, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"overlapsub", &suboverlap_enabled, CONF_TYPE_FLAG, 0, 0, 2, NULL},
	{"nooverlapsub", &suboverlap_enabled, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"sub-bg-color", &sub_bg_color, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"sub-bg-alpha", &sub_bg_alpha, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"sub-no-text-pp", &sub_no_text_pp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"sub-fuzziness", &sub_match_fuzziness, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#endif
#ifdef USE_OSD
	{"font", &font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, NULL},
 	{"subpos", &sub_pos, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"subalign", &sub_alignment, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
 	{"subwidth", &sub_width_p, CONF_TYPE_INT, CONF_RANGE, 10, 100, NULL},
	{"spualign", &spu_alignment, CONF_TYPE_INT, CONF_RANGE, -1, 2, NULL},
	{"spuaa", &spu_aamode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
	{"spugauss", &spu_gaussvar, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 3.0, NULL},
#ifdef HAVE_FREETYPE
	{"subfont-encoding", &subtitle_font_encoding, CONF_TYPE_STRING, 0, 0, 0, NULL},
 	{"subfont-text-scale", &text_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
 	{"subfont-osd-scale", &osd_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
 	{"subfont-blur", &subtitle_font_radius, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
 	{"subfont-outline", &subtitle_font_thickness, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
 	{"subfont-autoscale", &subtitle_autoscale, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
#endif
#ifdef HAVE_FONTCONFIG
	{"fontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nofontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#else
	{"fontconfig", "MPlayer was compiled without fontconfig support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"nofontconfig", "MPlayer was compiled without fontconfig support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#endif

#else

#include "config.h"

extern int quiet;
extern int verbose;

// codec/filter opts: (defined at libmpcodecs/vd.c)
extern float screen_size_xy;
extern float movie_aspect;
extern int softzoom;
extern int flip;
extern int vd_use_slices;
extern int divx_quality;

/* defined in codec-cfg.c */
extern char * codecs_file;

/* from dec_audio, currently used for ac3surround decoder only */
extern int audio_output_channels;

#ifdef MPLAYER_NETWORK
/* defined in network.c */
extern char *network_username;
extern char *network_password;
extern int   network_bandwidth;
extern char *network_useragent;
extern int   network_cookies_enabled;
extern char *cookies_file;

extern int network_prefer_ipv4;
extern int network_ipv4_only_proxy;

#endif

extern float a52_drc_level;

/* defined in libmpdemux: */
extern int hr_mp3_seek;
extern m_option_t demux_rawaudio_opts[];
extern m_option_t demux_rawvideo_opts[];
extern m_option_t cdda_opts[];

extern char* audio_stream;
extern char* sub_stream;
extern int demuxer_type, audio_demuxer_type, sub_demuxer_type;
extern int ts_prog;
extern int ts_keep_broken;
extern off_t ts_probe;

#include "libmpdemux/tv.h"

#ifdef USE_EDL
extern char* edl_filename;
extern char* edl_output_filename;
#endif

#ifdef USE_TV
m_option_t tvopts_conf[]={
	{"on", "-tv on is deprecated, use tv:// instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"immediatemode", &tv_param_immediate, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"noaudio", &tv_param_noaudio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"audiorate", &tv_param_audiorate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"driver", &tv_param_driver, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"device", &tv_param_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"freq", &tv_param_freq, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"channel", &tv_param_channel, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"chanlist", &tv_param_chanlist, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"norm", &tv_param_norm, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef HAVE_TV_V4L2
	{"normid", &tv_param_normid, CONF_TYPE_INT, 0, 0, 0, NULL},
#endif
	{"width", &tv_param_width, CONF_TYPE_INT, 0, 0, 4096, NULL},
	{"height", &tv_param_height, CONF_TYPE_INT, 0, 0, 4096, NULL},
	{"input", &tv_param_input, CONF_TYPE_INT, 0, 0, 20, NULL},
	{"outfmt", &tv_param_outfmt, CONF_TYPE_IMGFMT, 0, 0, 0, NULL},
	{"fps", &tv_param_fps, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{"channels", &tv_param_channels, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
	{"brightness", &tv_param_brightness, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
	{"contrast", &tv_param_contrast, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
	{"hue", &tv_param_hue, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
	{"saturation", &tv_param_saturation, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
#if defined(HAVE_TV_V4L) || defined(HAVE_TV_V4L2)
	{"amode", &tv_param_amode, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
	{"volume", &tv_param_volume, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
	{"bass", &tv_param_bass, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
	{"treble", &tv_param_treble, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
	{"balance", &tv_param_balance, CONF_TYPE_INT, CONF_RANGE, 0, 65535, NULL},
	{"forcechan", &tv_param_forcechan, CONF_TYPE_INT, CONF_RANGE, 1, 2, NULL},
	{"forceaudio", &tv_param_force_audio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"buffersize", &tv_param_buffer_size, CONF_TYPE_INT, CONF_RANGE, 16, 1024, NULL},
	{"mjpeg", &tv_param_mjpeg, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"decimation", &tv_param_decimation, CONF_TYPE_INT, CONF_RANGE, 1, 4, NULL},
	{"quality", &tv_param_quality, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
#if defined(HAVE_ALSA9) || defined(HAVE_ALSA1X)
	{"alsa", &tv_param_alsa, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif
	{"adevice", &tv_param_adevice, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
	{"audioid", &tv_param_audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

#ifdef HAS_DVBIN_SUPPORT
#include "libmpdemux/dvbin.h"
extern m_config_t dvbin_opts_conf[];
#endif

#ifdef  USE_FRIBIDI
extern char *fribidi_charset;
extern int flip_hebrew;
#endif


extern int audio_stream_cache;

extern int sws_chr_vshift;
extern int sws_chr_hshift;
extern float sws_chr_gblur;
extern float sws_lum_gblur;
extern float sws_chr_sharpen;
extern float sws_lum_sharpen;

m_option_t scaler_filter_conf[]={
	{"lgb", &sws_lum_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{"cgb", &sws_chr_gblur, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{"cvs", &sws_chr_vshift, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"chs", &sws_chr_hshift, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"ls", &sws_lum_sharpen, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{"cs", &sws_chr_sharpen, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
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

m_option_t vivoopts_conf[]={
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
extern float  mf_fps;
extern char * mf_type;
extern m_obj_settings_t* vf_settings;
extern m_obj_list_t vf_obj_list;

m_option_t mfopts_conf[]={
        {"on", "-mf on is deprecated, use mf:// instead.\n", CONF_TYPE_PRINT, 0, 0, 1, NULL},
        {"w", &mf_w, CONF_TYPE_INT, 0, 0, 0, NULL},
        {"h", &mf_h, CONF_TYPE_INT, 0, 0, 0, NULL},
        {"fps", &mf_fps, CONF_TYPE_FLOAT, 0, 0, 0, NULL},
        {"type", &mf_type, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {NULL, NULL, 0, 0, 0, 0, NULL}
};
						
extern m_obj_settings_t* vo_plugin_args;

#include "libaf/af.h"
extern af_cfg_t af_cfg; // Audio filter configuration, defined in libmpcodecs/dec_audio.c
m_option_t audio_filter_conf[]={       
	{"list", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
        {"force", &af_cfg.force, CONF_TYPE_INT, CONF_RANGE, 0, 7, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

#ifdef USE_LIBAVCODEC
extern m_option_t lavc_decode_opts_conf[];
#endif

#if defined(HAVE_XVID3) || defined(HAVE_XVID4)
extern m_option_t xvid_dec_opts[];
#endif

int dvd_parse_chapter_range(m_option_t*, const char*);

#endif
