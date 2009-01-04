#ifndef MPLAYER_CFG_COMMON_OPTS_H
#define MPLAYER_CFG_COMMON_OPTS_H

#include "config.h"

// ------------------------- common options --------------------
	{"quiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
	{"noquiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
	{"really-quiet", &verbose, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_PRE_PARSE, 0, -10, NULL},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOSAVE, 0, 0, NULL},
	{"msglevel", msgl_config, CONF_TYPE_SUBCONFIG, CONF_GLOBAL, 0, 0, NULL},
	{"msgcolor", &mp_msg_color, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
	{"nomsgcolor", &mp_msg_color, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
	{"msgmodule", &mp_msg_module, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
	{"nomsgmodule", &mp_msg_module, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
#ifdef CONFIG_ICONV
	{"msgcharset", &mp_msg_charset, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
#endif
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL},
#if defined(__MINGW32__) || defined(__CYGWIN__)
	{"priority", &proc_priority, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
	{"noconfig", noconfig_opts, CONF_TYPE_SUBCONFIG, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 0, NULL},

// ------------------------- stream options --------------------

#ifdef CONFIG_STREAM_CACHE
	{"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 32, 1048576, NULL},
	{"nocache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"cache-min", &stream_cache_min_percent, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
	{"cache-seek-min", &stream_cache_seek_min_percent, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
#else
	{"cache", "MPlayer was compiled without cache2 support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif /* CONFIG_STREAM_CACHE */
	{"vcd", "-vcd N has been removed, use vcd://N instead.\n", CONF_TYPE_PRINT, CONF_NOCFG ,0,0, NULL},
	{"cuefile", "-cuefile has been removed, use cue://filename:N where N is the track number.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"cdrom-device", &cdrom_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef CONFIG_DVDREAD
	{"dvd-device", &dvd_device,  CONF_TYPE_STRING, 0, 0, 0, NULL}, 
	{"dvd-speed", &dvd_speed, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"dvd", "-dvd N has been removed, use dvd://N instead.\n" , CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"dvdangle", &dvd_angle, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"chapter", dvd_parse_chapter_range, CONF_TYPE_FUNC_PARAM, 0, 0, 0, NULL},
#else
	{"dvd-device", "MPlayer was compiled without libdvdread support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"dvd-speed", "MPlayer was compiled without libdvdread support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"dvd", "MPlayer was compiled without libdvdread support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif /* CONFIG_DVDREAD */
	{"alang", &audio_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"slang", &dvdsub_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},

        {"dvdauth", "libcss is obsolete. Try libdvdread instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"dvdkey", "libcss is obsolete. Try libdvdread instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"csslib", "libcss is obsolete. Try libdvdread instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},

#ifdef CONFIG_NETWORK
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
#endif /* HAVE_AF_INET6 */

#else
	{"user", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"passwd", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"bandwidth", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"user-agent", "MPlayer was compiled without streaming (network) support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif /* CONFIG_NETWORK */

#ifdef CONFIG_LIVE555
        {"sdp", "-sdp has been removed, use sdp://file instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	// -rtsp-stream-over-tcp option, specifying TCP streaming of RTP/RTCP
        {"rtsp-stream-over-tcp", &rtspStreamOverTCP, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#elif defined (CONFIG_LIBNEMESI)
        {"rtsp-stream-over-tcp", &rtsp_transport_tcp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"rtsp-stream-over-sctp", &rtsp_transport_sctp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#else
	{"rtsp-stream-over-tcp", "-rtsp-stream-over-tcp requires the \"LIVE555 Streaming Media\" libraries.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif /* CONFIG_LIVE555 */
#ifdef CONFIG_NETWORK
        {"rtsp-port", &rtsp_port, CONF_TYPE_INT, CONF_RANGE, -1, 65535, NULL},	
        {"rtsp-destination", &rtsp_destination, CONF_TYPE_STRING, CONF_MIN, 0, 0, NULL},
#else
        {"rtsp-port", "MPlayer was compiled without network support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"rtsp-destination", "MPlayer was compiled without network support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif /* CONFIG_NETWORK */
  
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
	{"ausid", &audio_substream_id, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"vid", &video_id, CONF_TYPE_INT, CONF_RANGE, 0, 8190, NULL},
	{"sid", &dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 8190, NULL},
	{"novideo", &video_id, CONF_TYPE_FLAG, 0, -1, -2, NULL},

	{ "hr-mp3-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{ "nohr-mp3-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{ "rawaudio", &demux_rawaudio_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
	{ "rawvideo", &demux_rawvideo_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

#ifdef CONFIG_CDDA
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
#ifdef CONFIG_RADIO
	{"radio", radioopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"radio", "MPlayer was compiled without Radio interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif /* CONFIG_RADIO */
#ifdef CONFIG_TV
	{"tv", tvopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"tv", "MPlayer was compiled without TV interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif /* CONFIG_TV */
#ifdef CONFIG_PVR
	{"pvr", pvropts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"pvr", "MPlayer was compiled without V4L2/PVR interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif /* CONFIG_PVR */
	{"vivo", vivoopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#ifdef CONFIG_DVBIN
	{"dvbin", dvbin_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

// ------------------------- a-v sync options --------------------

	// AVI specific: A-V sync mode (bps vs. interleaving)
	{"bps", &pts_from_bps, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nobps", &pts_from_bps, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	// set A-V sync correction speed (0=disables it):
	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
	
	// force video/audio rate:
	{"fps", &force_fps, CONF_TYPE_DOUBLE, CONF_MIN, 0, 0, NULL},
	{"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},
	{"channels", &audio_output_channels, CONF_TYPE_INT, CONF_RANGE, 1, 6, NULL},
	{"format", &audio_output_format, CONF_TYPE_AFMT, 0, 0, 0, NULL},
	{"speed", &playback_speed, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 100.0, NULL},

	// set a-v distance
	{"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -100.0, 100.0, NULL},

	// ignore header-specified delay (dwStart)
	{"ignore-start", &ignore_start, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noignore-start", &ignore_start, CONF_TYPE_FLAG, 0, 1, 0, NULL},

#ifdef CONFIG_LIBA52
        {"a52drc", &a52_drc_level, CONF_TYPE_FLOAT, CONF_RANGE, 0, 1, NULL},
#endif

// ------------------------- codec/vfilter options --------------------

	// MP3-only: select stereo/left/right
#ifdef CONFIG_FAKE_MONO
	{"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#endif

	// disable audio
	{"sound", &audio_id, CONF_TYPE_FLAG, 0, -2, -1, NULL},
	{"nosound", &audio_id, CONF_TYPE_FLAG, 0, -1, -2, NULL},

	{"af*", &af_cfg.list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
	{"af-adv", audio_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

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
#ifdef CONFIG_LIBAVCODEC
	{"pp", &divx_quality, CONF_TYPE_INT, 0, 0, 0, NULL},
#endif
#ifdef CONFIG_LIBPOSTPROC
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

#ifdef CONFIG_LIBAVCODEC
	{"lavdopts", lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
#ifdef CONFIG_LIBAVFORMAT
        {"lavfdopts",  lavfdopts_conf, CONF_TYPE_SUBCONFIG, CONF_GLOBAL, 0, 0, NULL},
#endif
#ifdef CONFIG_XVID4
	{"xvidopts", xvid_dec_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
	{"codecs-file", &codecs_file, CONF_TYPE_STRING, 0, 0, 0, NULL},
// ------------------------- subtitles options --------------------

	{"sub", &sub_name, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
#ifdef CONFIG_FRIBIDI
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
#endif /* CONFIG_FRIBIDI */
#ifdef CONFIG_ICONV
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
	{"subfont", &sub_font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, NULL},
 	{"subpos", &sub_pos, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"subalign", &sub_alignment, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
 	{"subwidth", &sub_width_p, CONF_TYPE_INT, CONF_RANGE, 10, 100, NULL},
	{"spualign", &spu_alignment, CONF_TYPE_INT, CONF_RANGE, -1, 2, NULL},
	{"spuaa", &spu_aamode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
	{"spugauss", &spu_gaussvar, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 3.0, NULL},
#ifdef CONFIG_FREETYPE
	{"subfont-encoding", &subtitle_font_encoding, CONF_TYPE_STRING, 0, 0, 0, NULL},
 	{"subfont-text-scale", &text_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
 	{"subfont-osd-scale", &osd_font_scale_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0, 100, NULL},
 	{"subfont-blur", &subtitle_font_radius, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
 	{"subfont-outline", &subtitle_font_thickness, CONF_TYPE_FLOAT, CONF_RANGE, 0, 8, NULL},
 	{"subfont-autoscale", &subtitle_autoscale, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
#endif
#ifdef CONFIG_ASS
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
	{"ass-hinting", &ass_hinting, CONF_TYPE_INT, CONF_RANGE, 0, 7, NULL},
#endif
#ifdef CONFIG_FONTCONFIG
	{"fontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, -1, 1, NULL},
	{"nofontconfig", &font_fontconfig, CONF_TYPE_FLAG, 0, 1, -1, NULL},
#else
	{"fontconfig", "MPlayer was compiled without fontconfig support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"nofontconfig", "MPlayer was compiled without fontconfig support.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif /* CONFIG_FONTCONFIG */

#endif /* MPLAYER_CFG_COMMON_OPTS_H */
