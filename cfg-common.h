#ifdef MAIN_CONF /* this will be included in conf[] */

// ------------------------- stream options --------------------

#ifdef USE_STREAM_CACHE
	{"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 4, 65536, NULL},
	{"nocache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#else
	{"cache", "MPlayer was compiled WITHOUT cache2 support\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#ifdef HAVE_VCD
	{"vcd", &vcd_track, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"cdrom-device", &cdrom_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
#else
	{"vcd", "VCD support is NOT available on this system!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#ifdef USE_DVDNAV
	{"dvdnav", &dvd_nav, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"skipopening", &dvd_nav_skip_opening, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif
#ifdef USE_DVDREAD
	{"dvd-device", &dvd_device,  CONF_TYPE_STRING, 0, 0, 0, NULL}, 
	{"dvd", &dvd_title, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"dvdangle", &dvd_angle, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"chapter", dvd_parse_chapter_range, CONF_TYPE_FUNC_PARAM, 0, 0, 0, NULL},
	{"alang", &audio_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"slang", &dvdsub_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},
#else
	{"dvd", "MPlayer was compiled WITHOUT libdvdread support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif

#ifdef HAVE_LIBCSS
        {"dvdauth", &dvd_auth_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {"dvdkey", &dvdimportkey, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"csslib", &css_so, CONF_TYPE_STRING, 0, 0, 0, NULL},
#else
        {"dvdauth", "MPlayer was compiled WITHOUT libcss support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"dvdkey", "MPlayer was compiled WITHOUT libcss support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"csslib", "MPlayer was compiled WITHOUT libcss support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif

// ------------------------- demuxer options --------------------

	// number of frames to play/convert
	{"frames", &play_n_frames, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	// seek to byte/seconds position
	{"sb", &seek_to_byte, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"ss", &seek_to_sec, CONF_TYPE_STRING, CONF_MIN, 0, 0, NULL},

	// AVI specific: force non-interleaved mode
	{"ni", &force_ni, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noni", &force_ni, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	// AVI and OGG only: (re)build index at startup
	{"noidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 0, NULL},
	{"idx", &index_mode, CONF_TYPE_FLAG, 0, -1, 1, NULL},
	{"forceidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 2, NULL},

	// select audio/videosubtitle stream
	{"aid", &audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"vid", &video_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"sid", &dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},

        {"mf", mfopts_conf, CONF_TYPE_SUBCONFIG, 0,0,0, NULL},
#ifdef USE_TV
	{"tv", tvopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"tv", "MPlayer was compiled without TV Interface support\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif
	{"vivo", vivoopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

// ------------------------- a-v sync options --------------------

	// AVI specific: A-V sync mode (bps vs. interleaving)
	{"bps", &pts_from_bps, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nobps", &pts_from_bps, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	// set A-V sync correction speed (0=disables it):
	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10, NULL},
	
	// force video/audio rate:
	{"fps", &force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},
	{"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},

// ------------------------- codec/vfilter options --------------------

	// MP3-only: select stereo/left/right
#ifdef USE_FAKE_MONO
	{"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#endif

	// disable audio
	{"sound", &has_audio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nosound", &has_audio, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	// select audio/video codec (by name) or codec family (by number):
	{"afm", &audio_family, CONF_TYPE_INT, CONF_MIN, 0, 16, NULL}, // keep ranges in sync
	{"vfm", &video_family, CONF_TYPE_INT, CONF_MIN, 0, 14, NULL}, // with codec-cfg.c
	{"ac", &audio_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vc", &video_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},

	// postprocessing:
	{"divxq", "Option -divxq has been renamed to -pp (postprocessing), use -pp !\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"pp", readPPOpt, CONF_TYPE_FUNC_PARAM, 0, 0, 0, (cfg_default_func_t)&revertPPOpt},
	{"npp", readNPPOpt, CONF_TYPE_FUNC_PARAM, 0, 0, 0, (cfg_default_func_t)&revertPPOpt},
#ifdef HAVE_ODIVX_POSTPROCESS
        {"oldpp", &use_old_pp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#else
        {"oldpp", "MPlayer was compiled without opendivx library\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif

	{"vop", &vo_plugin_args, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},

	// scaling:
	{"sws", &sws_flags, CONF_TYPE_INT, 0, 0, 2, NULL},
	{"ssf", scaler_filter_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
        {"zoom", &softzoom, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nozoom", &softzoom, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"aspect", &movie_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0, NULL},
	{"noaspect", &movie_aspect, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"xy", &screen_size_xy, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},

        {"flip", &flip, CONF_TYPE_FLAG, 0, -1, 1, NULL},
        {"noflip", &flip, CONF_TYPE_FLAG, 0, -1, 0, NULL},

#ifdef USE_LIBAVCODEC
	{"lavdopts", lavc_decode_opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
// ------------------------- subtitles options --------------------

#ifdef USE_SUB
	{"sub", &sub_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef USE_ICONV
	{"subcp", &sub_cp, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif	
	{"subdelay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
	{"subfps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
        {"noautosub", &sub_auto, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"unicode", &sub_unicode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nounicode", &sub_unicode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"utf8", &sub_utf8, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noutf8", &sub_utf8, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	// specify IFO file for VOBSUB subtitle
	{"ifo", &spudec_ifo, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
#ifdef USE_OSD
	{"font", &font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, NULL},
 	{"subpos", &sub_pos, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
#endif

#else

#include "config.h"

#include "libmpdemux/tv.h"

#ifdef USE_TV
struct config tvopts_conf[]={
	{"on", &tv_param_on, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#ifdef HAVE_TV_BSDBT848
	{"immediatemode", &tv_param_immediate, CONF_TYPE_FLAG, 0, 0, 0, NULL},
#endif
	{"noaudio", &tv_param_noaudio, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"audiorate", &tv_param_audiorate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"driver", &tv_param_driver, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"device", &tv_param_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"freq", &tv_param_freq, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"channel", &tv_param_channel, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"chanlist", &tv_param_chanlist, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"norm", &tv_param_norm, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"width", &tv_param_width, CONF_TYPE_INT, 0, 0, 4096, NULL},
	{"height", &tv_param_height, CONF_TYPE_INT, 0, 0, 4096, NULL},
	{"input", &tv_param_input, CONF_TYPE_INT, 0, 0, 20, NULL},
	{"outfmt", &tv_param_outfmt, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"fps", &tv_param_fps, CONF_TYPE_FLOAT, 0, 0, 100.0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

extern int sws_chr_vshift;
extern int sws_chr_hshift;
extern float sws_chr_gblur;
extern float sws_lum_gblur;
extern float sws_chr_sharpen;
extern float sws_lum_sharpen;

struct config scaler_filter_conf[]={
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

struct config vivoopts_conf[]={
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

extern int    mf_support;
extern int    mf_w;
extern int    mf_h;
extern float  mf_fps;
extern char * mf_type;

struct config mfopts_conf[]={
        {"on", &mf_support, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"w", &mf_w, CONF_TYPE_INT, 0, 0, 0, NULL},
        {"h", &mf_h, CONF_TYPE_INT, 0, 0, 0, NULL},
        {"fps", &mf_fps, CONF_TYPE_FLOAT, 0, 0, 0, NULL},
        {"type", &mf_type, CONF_TYPE_STRING, 0, 0, 0, NULL},
        {NULL, NULL, 0, 0, 0, 0, NULL}
};
						
extern char** vo_plugin_args;

#ifdef USE_LIBAVCODEC
extern struct config lavc_decode_opts_conf[];
#endif

#endif
