#ifdef MAIN_CONF /* this will be included in conf[] */
// ------------------------- stream options --------------------

#ifdef USE_STREAM_CACHE
	{"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 4, 65536, NULL},
#else
	{"cache", "MPlayer was compiled WITHOUT cache2 support\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#ifdef HAVE_VCD
	{"vcd", &vcd_track, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
#else
	{"vcd", "VCD support is NOT available on this system!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
#ifdef USE_DVDREAD
	{"dvd", &dvd_title, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"dvdangle", &dvd_angle, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
	{"chapter", &dvd_chapter, CONF_TYPE_INT, CONF_RANGE, 1, 99, NULL},
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

	{"bps", &pts_from_bps, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nobps", &pts_from_bps, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{"ni", &force_ni, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noni", &force_ni, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{"noidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 0, NULL},
	{"idx", &index_mode, CONF_TYPE_FLAG, 0, -1, 1, NULL},
	{"forceidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 2, NULL},

	{"aid", &audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"vid", &video_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"sid", &dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},

// ------------------------- a-v sync options --------------------

	{"frames", &play_n_frames, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10, NULL},
	{"fps", &force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},
	{"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL},

// ------------------------- codec/pp options --------------------

#ifdef USE_FAKE_MONO
	{"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
#endif

	{"afm", &audio_family, CONF_TYPE_INT, CONF_MIN, 0, 16, NULL}, // keep ranges in sync
	{"vfm", &video_family, CONF_TYPE_INT, CONF_MIN, 0, 14, NULL}, // with codec-cfg.c
	{"ac", &audio_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vc", &video_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},

	{"divxq", "Option -divxq has been renamed to -pp (postprocessing), use -pp !\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"pp", &divx_quality, CONF_TYPE_INT, CONF_MIN, 0, 63, NULL},
	{"npp", readPPOpt, CONF_TYPE_FUNC_PARAM, 0, 0, 0, NULL},
#ifdef HAVE_ODIVX_POSTPROCESS
        {"oldpp", &use_old_pp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#else
        {"oldpp", "MPlayer was compiled without opendivx library\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"sws", &sws_flags, CONF_TYPE_INT, 0, 0, 2, NULL},

#ifdef USE_TV
	{"tv", tvopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
	{"tv", "MPlayer was compiled without TV Interface support\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif
	{"vivo", vivoopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

#else

#include "config.h"

#include "libmpdemux/tv.h"

#ifdef USE_TV
struct config tvopts_conf[]={
	{"on", &tv_param_on, CONF_TYPE_FLAG, 0, 0, 1, NULL},
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

/* VIVO demuxer options: */
extern int vivo_param_version;
extern char *vivo_param_acodec;
extern int vivo_param_abitrate;
extern int vivo_param_samplerate;
extern int vivo_param_bytesperblock;
extern int vivo_param_width;
extern int vivo_param_height;
extern int vivo_param_vformat;

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

#endif
