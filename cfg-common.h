#ifdef MAIN_CONF /* this will be included in conf[] */

// ------------------------- common options --------------------
	{"quiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
	{"noquiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
	{"really-quiet", &verbose, CONF_TYPE_FLAG, CONF_GLOBAL, 0, -10, NULL},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOSAVE, 0, 0, NULL},
	{"msglevel", msgl_config, CONF_TYPE_SUBCONFIG, CONF_GLOBAL, 0, 0, NULL},
#ifdef USE_ICONV
	{"msgcharset", &mp_msg_charset, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
#endif
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL},
#ifdef WIN32
	{"priority", &proc_priority, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

// ------------------------- stream options --------------------

#ifdef USE_STREAM_CACHE
	{"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 32, 1048576, NULL},
	{"nocache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"cache-min", &stream_cache_min_percent, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
	{"cache-seek-min", &stream_cache_seek_min_percent, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
#else
	{"cache", "MPlayer was compiled without cache2 support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"vcd", "-vcd N has been removed, use vcd://N instead.\n", CONF_TYPE_PRINT, CONF_NOCFG ,0,0, NULL},
	{"cuefile", "-cuefile has been removed, use cue://filename:N where N is the track number.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"cdrom-device", &cdrom_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
#if defined(USE_DVDREAD) || defined(USE_DVDNAV)
	{"dvd-device", &dvd_device,  CONF_TYPE_STRING, 0, 0, 0, NULL}, 
	{"dvd-speed", &dvd_speed, CONF_TYPE_INT, 0, 0, 0, NULL},
#else
	{"dvd-device", "MPlayer was compiled without libdvdread support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"dvd-speed", "MPlayer was compiled without libdvdread support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#ifdef USE_DVDREAD
	{"dvd", "-dvd N has been removed, use dvd://N instead.\n" , CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"dvdangle", &dvd_angle, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"chapter", dvd_parse_chapter_range, CONF_TYPE_FUNC_PARAM, 0, 0, 0, NULL},
#else
	{"dvd", "MPlayer was compiled without libdvdread support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"alang", &audio_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},
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
	{"reuse-socket", &reuse_socket, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
	{"noreuse-socket", &reuse_socket, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
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

#ifdef STREAMING_LIVE555
        {"sdp", "-sdp has been removed, use sdp://file instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	// -rtsp-stream-over-tcp option, specifying TCP streaming of RTP/RTCP
        {"rtsp-stream-over-tcp", &rtspStreamOverTCP, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#else
	{"rtsp-stream-over-tcp", "-rtsp-stream-over-tcp requires the \"LIVE555 Streaming Media\" libraries.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#ifdef MPLAYER_NETWORK
        {"rtsp-port", &rtsp_port, CONF_TYPE_INT, CONF_RANGE, -1, 65535, NULL},	
        {"rtsp-destination", &rtsp_destination, CONF_TYPE_STRING, CONF_MIN, 0, 0, NULL},
#else
        {"rtsp-port", "MPlayer was compiled without network support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"rtsp-destination", "MPlayer was compiled without network support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
  
// ------------------------- demuxer options --------------------

	// number of frames to play/convert
	{"frames", &play_n_frames_mf, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	// seek to byte/seconds position
	{"sb", &seek_to_byte, CONF_TYPE_POSITION, CONF_MIN, 0, 0, NULL},
	{"ss", &seek_to_sec, CONF_TYPE_TIME, 0, 0, 0, NULL},

	// stop at given position
	{"endpos", &end_at, CONF_TYPE_TIME_SIZE, 0, 0, 0, NULL},

	{"edl", &edl_filename,  CONF_TYPE_STRING, 0, 0, 0, NULL},

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
	{ "demuxer", &demuxer_name, CONF_TYPE_STRING, 0, 0, 0, NULL },
	{ "audio-demuxer", &audio_demuxer_name, CONF_TYPE_STRING, 0, 0, 0, NULL },
	{ "sub-demuxer", &sub_demuxer_name, CONF_TYPE_STRING, 0, 0, 0, NULL },
	{ "extbased", &extension_parsing, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{ "noextbased", &extension_parsing, CONF_TYPE_FLAG, 0, 1, 0, NULL },

        {"mf", mfopts_conf, CONF_TYPE_SUBCONFIG, 0,0,0, NULL},
#ifdef USE_RADIO
	{"radio", radioopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"radio", "MPlayer was compiled without Radio interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif
#ifdef USE_TV
	{"tv", tvopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"tv", "MPlayer was compiled without TV interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif
#ifdef HAVE_PVR
	{"pvr", pvropts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"pvr", "MPlayer was compiled without V4L2/PVR interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
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
	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
	
	// force video/audio rate:
	{"fps", &force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},
	{"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},
	{"channels", &audio_output_channels, CONF_TYPE_INT, CONF_RANGE, 1, 6, NULL},
	{"format", &audio_output_format, CONF_TYPE_AFMT, 0, 0, 0, NULL},
	{"speed", &playback_speed, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 100.0, NULL},

	// set a-v distance
	{"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -100.0, 100.0, NULL},

	// ignore header-specified delay (dwStart)
	{"ignore-start", &ignore_start, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noignore-start", &ignore_start, CONF_TYPE_FLAG, 0, 1, 0, NULL},

#ifdef USE_LIBA52
        {"a52drc", &a52_drc_level, CONF_TYPE_FLOAT, CONF_RANGE, 0, 1, NULL},
#endif

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

	{"vop", "-vop has been removed, use -vf instead.\n", CONF_TYPE_PRINT, CONF_NOCFG ,0,0, NULL},
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
#ifdef USE_LIBAVCODEC
	{"pp", &divx_quality, CONF_TYPE_INT, 0, 0, 0, NULL},
#endif
#if defined(USE_LIBPOSTPROC) || defined(USE_LIBPOSTPROC_SO)
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
	{"psprobe", &ps_probe, CONF_TYPE_POSITION, 0, 0, TS_MAX_PROBE_SIZE, NULL},
	{"tskeepbroken", &ts_keep_broken, CONF_TYPE_FLAG, 0, 0, 1, NULL},

	// draw by slices or whole frame (useful with libmpeg2/libavcodec)
	{"slices", &vd_use_slices, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noslices", &vd_use_slices, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"field-dominance", &field_dominance, CONF_TYPE_INT, CONF_RANGE, -1, 1, NULL},

#ifdef USE_LIBAVCODEC
	{"lavdopts", lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
#if defined(USE_LIBAVFORMAT) ||  defined(USE_LIBAVFORMAT_SO)
        {"lavfdopts",  lavfdopts_conf, CONF_TYPE_SUBCONFIG, CONF_GLOBAL, 0, 0, NULL},
#endif
#if defined(HAVE_XVID3) || defined(HAVE_XVID4)
	{"xvidopts", xvid_dec_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
	{"codecs-file", &codecs_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
// ------------------------- subtitles options --------------------

	{"sub", &sub_name, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
#ifdef USE_FRIBIDI
	{"fribidi-charset", &fribidi_charset, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"flip-hebrew", &flip_hebrew, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noflip-hebrew", &flip_hebrew, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"flip-hebrew-commas", &fribidi_flip_commas, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"noflip-hebrew-commas", &fribidi_flip_commas, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#else 
	{"fribidi-charset", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"flip-hebrew", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"noflip-hebrew", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"flip-hebrew-commas", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"noflip-hebrew-commas", "MPlayer was compiled without FriBiDi support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
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
#ifdef USE_ASS
	{"ass", &ass_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noass", &ass_enabled, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"ass-font-scale", &ass_font_scale, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
	{"ass-line-spacing", &ass_line_spacing, CONF_TYPE_FLOAT, CONF_RANGE, -1000, 1000, NULL},
	{"ass-top-margin", &ass_top_margin, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
	{"ass-bottom-margin", &ass_bottom_margin, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
	{"ass-use-margins", &ass_use_margins, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noass-use-margins", &ass_use_margins, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"embeddedfonts", &extract_embedded_fonts, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noembeddedfonts", &extract_embedded_fonts, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"ass-force-style", &ass_force_style_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
	{"ass-color", &ass_color, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ass-border-color", &ass_border_color, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ass-styles", &ass_styles_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
#ifdef HAVE_FONTCONFIG
	{"fontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nofontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#else
	{"fontconfig", "MPlayer was compiled without fontconfig support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"nofontconfig", "MPlayer was compiled without fontconfig support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif

#else

#include "config.h"

extern int quiet;
extern int verbose;
extern char *mp_msg_charset;

// codec/filter opts: (defined at libmpcodecs/vd.c)
extern float screen_size_xy;
extern float movie_aspect;
extern int softzoom;
extern int flip;
extern int vd_use_slices;
extern int divx_quality;

/* defined in codec-cfg.c */
extern char * codecs_file;

/* defined in dec_video.c */
extern int field_dominance;

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
extern int reuse_socket;

#endif

#if defined(USE_DVDREAD) || defined(USE_DVDNAV)
extern int dvd_speed; /* stream/stream_dvd.c */
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
extern off_t ps_probe;

#include "stream/tv.h"
#include "stream/stream_radio.h"

extern char* edl_filename;
extern char* edl_output_filename;


#ifdef USE_RADIO
m_option_t radioopts_conf[]={
    {"device", &radio_param_device, CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {"driver", &radio_param_driver, CONF_TYPE_STRING, 0, 0 ,0, NULL},
#ifdef RADIO_BSDBT848_HDR
    {"freq_min", &radio_param_freq_min, CONF_TYPE_FLOAT, 0, 0 ,0, NULL},
    {"freq_max", &radio_param_freq_max, CONF_TYPE_FLOAT, 0, 0 ,0, NULL},
#endif
    {"channels", &radio_param_channels, CONF_TYPE_STRING_LIST, 0, 0 ,0, NULL},
    {"volume", &radio_param_volume, CONF_TYPE_INT, CONF_RANGE, 0 ,100, NULL},
    {"adevice", &radio_param_adevice, CONF_TYPE_STRING, 0, 0 ,0, NULL},
    {"arate", &radio_param_arate, CONF_TYPE_INT, CONF_MIN, 0 ,0, NULL},
    {"achannels", &radio_param_achannels, CONF_TYPE_INT, CONF_MIN, 0 ,0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

#ifdef USE_TV
m_option_t tvopts_conf[]={
	{"on", "-tv on has been removed, use tv:// instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"immediatemode", &tv_param_immediate, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
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

#ifdef HAVE_PVR
extern int pvr_param_aspect_ratio;
extern int pvr_param_sample_rate;
extern int pvr_param_audio_layer;
extern int pvr_param_audio_bitrate;
extern char *pvr_param_audio_mode;
extern int pvr_param_bitrate;
extern char *pvr_param_bitrate_mode;
extern int pvr_param_bitrate_peak;
extern char *pvr_param_stream_type;

m_option_t pvropts_conf[]={
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
#endif

#ifdef HAS_DVBIN_SUPPORT
#include "stream/dvbin.h"
extern m_config_t dvbin_opts_conf[];
#endif

#if defined(USE_LIBAVFORMAT) ||  defined(USE_LIBAVFORMAT_SO)
extern m_option_t lavfdopts_conf[];
#endif

#ifdef  USE_FRIBIDI
extern char *fribidi_charset;
extern int flip_hebrew;
#endif

#ifdef STREAMING_LIVE555
extern int rtspStreamOverTCP;
#endif
extern int rtsp_port;
extern char *rtsp_destination;


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
        {"on", "-mf on has been removed, use mf:// instead.\n", CONF_TYPE_PRINT, 0, 0, 1, NULL},
        {"w", &mf_w, CONF_TYPE_INT, 0, 0, 0, NULL},
        {"h", &mf_h, CONF_TYPE_INT, 0, 0, 0, NULL},
        {"fps", &mf_fps, CONF_TYPE_FLOAT, 0, 0, 0, NULL},
        {"type", &mf_type, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {NULL, NULL, 0, 0, 0, 0, NULL}
};

#include "libaf/af.h"
extern af_cfg_t af_cfg; // Audio filter configuration, defined in libmpcodecs/dec_audio.c
m_option_t audio_filter_conf[]={       
	{"list", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
        {"force", &af_cfg.force, CONF_TYPE_INT, CONF_RANGE, 0, 7, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

m_option_t msgl_config[]={
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
        "\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
      	{NULL, NULL, 0, 0, 0, 0, NULL}

};

#ifdef WIN32

extern char * proc_priority;

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
#endif /* WIN32 */

#ifdef USE_LIBAVCODEC
extern m_option_t lavc_decode_opts_conf[];
#endif

#if defined(HAVE_XVID3) || defined(HAVE_XVID4)
extern m_option_t xvid_dec_opts[];
#endif

int dvd_parse_chapter_range(m_option_t*, const char*);

#endif
