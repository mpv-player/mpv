/*
 * config for cfgparser
 */

#include "cfg-common.h"

#ifdef USE_FAKE_MONO
extern int fakemono; // defined in dec_audio.c
#endif
#ifdef HAVE_ODIVX_POSTPROCESS
extern int use_old_pp;
#endif

extern int sws_flags;
extern int readPPOpt(void *, char *arg);
extern int readNPPOpt(void *conf, char *arg);
extern void revertPPOpt(void *conf, char* opt);

#ifdef HAVE_DIVX4ENCORE
extern struct config divx4opts_conf[];
#endif

#ifdef HAVE_MP3LAME
struct config lameopts_conf[]={
	{"q", &lame_param_quality, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
	{"aq", &lame_param_algqual, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
	{"vbr", &lame_param_vbr, CONF_TYPE_INT, CONF_RANGE, 0, vbr_max_indicator, NULL},
	{"cbr", &lame_param_vbr, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"abr", &lame_param_vbr, CONF_TYPE_FLAG, 0, 0, vbr_abr, NULL},
	{"mode", &lame_param_mode, CONF_TYPE_INT, CONF_RANGE, 0, MAX_INDICATOR, NULL},
	{"padding", &lame_param_padding, CONF_TYPE_INT, CONF_RANGE, 0, PAD_MAX_INDICATOR, NULL},
	{"br", &lame_param_br, CONF_TYPE_INT, CONF_RANGE, 0, 1024, NULL},
	{"ratio", &lame_param_ratio, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"vol", &lame_param_scale, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10, NULL},
	{"help", "TODO: lameopts help!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

#ifdef USE_LIBAVCODEC
extern struct config lavcopts_conf[];
#endif

#ifdef USE_WIN32DLL
extern struct config vfwopts_conf[];
#endif

struct config ovc_conf[]={
	{"copy", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_COPY, NULL},
	{"frameno", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_FRAMENO, NULL},
	{"divx4", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_DIVX4, NULL},
//	{"raw", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_RAW, NULL},
	{"lavc", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_LIBAVCODEC, NULL},
//	{"null", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_NULL, NULL},
	{"rawrgb", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_RAWRGB, NULL},
	{"vfw", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_VFW, NULL},
	{"libdv", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_LIBDV, NULL},
	{"help", "\nAvailable codecs:\n"
	"   copy     - frame copy, without re-encoding. doesn't work with filters!\n"
	"   frameno  - special audio-only file for 3-pass encoding, see DOCS!\n"
	"   rawrgb   - uncompressed RGB 24bpp video\n"
#ifdef HAVE_DIVX4ENCORE
	"   divx4    - using divx4linux/divx5linux or xvid (depends on configuration)\n"
#endif
#ifdef USE_LIBAVCODEC
	"   lavc     - using libavcodec codecs - best quality!\n"
#endif
#ifdef USE_WIN32DLL
	"   vfw      - using VfW DLLs, currently only AVID is supported\n"
#endif
#ifdef HAVE_LIBDV095
	"   libdv    - DV encoding using libdv v0.9.5\n"
#endif
	"\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

struct config oac_conf[]={
	{"copy", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_COPY, NULL},
	{"pcm", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_PCM, NULL},
#ifdef HAVE_MP3LAME
	{"mp3lame", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_VBRMP3, NULL},
#else
	{"mp3lame", "MPlayer was compiled without libmp3lame support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{"help", "\nAvailable codecs:\n"
	"   copy     - frame copy, without re-encoding (usefull for AC3)\n"
	"   pcm      - uncompressed PCM audio\n"
#ifdef HAVE_MP3LAME
	"   mp3lame  - cbr/abr/vbr MP3 using libmp3lame\n"
#endif
	"\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

static config_t mencoder_opts[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL}, /* this must be the first!!! */

	{"endpos", parse_end_at, CONF_TYPE_FUNC_PARAM, 0, 0, 0, NULL},

	// set output framerate - recommended for variable fps (.asf etc) files
	// and for 29.97fps progressive mpeg2 streams
	{"ofps", &force_ofps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},
	{"o", &out_filename, CONF_TYPE_STRING, 0, 0, 0, NULL},

	// limit number of skippable frames after a non-skipped one
	{"skiplimit", &skip_limit, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"noskiplimit", &skip_limit, CONF_TYPE_FLAG, 0, 0, -1, NULL},
	{"noskip", &skip_limit, CONF_TYPE_FLAG, 0, 0, 0, NULL},

	{"audio-density", &audio_density, CONF_TYPE_INT, CONF_RANGE, 1, 50, NULL},
	{"audio-preload", &audio_preload, CONF_TYPE_FLOAT, CONF_RANGE, 0, 2, NULL},
	{"audio-delay",   &audio_delay, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},

	{"x", "This option is obsolete, use -vop scale=w:h for scaling\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"xsize", "This option is obsolete, use -vop crop=w:h:x0:y0 for cropping\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},

	// outut audio/video codec selection
	{"oac", oac_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
	{"ovc", ovc_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

	// override FOURCC in output file
	{"ffourcc", &force_fourcc, CONF_TYPE_STRING, 0, 4, 4, NULL},

	{"pass", &pass, CONF_TYPE_INT, CONF_RANGE,0,2, NULL},
	{"passlogfile", &passtmpfile, CONF_TYPE_STRING, 0, 0, 0, NULL},
	
	{"vobsubout", &vobsub_out, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vobsuboutindex", &vobsub_out_index, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
	{"vobsuboutid", &vobsub_out_id, CONF_TYPE_STRING, 0, 0, 0, NULL},

#ifdef HAVE_DIVX4ENCORE
	{"divx4opts", divx4opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
#ifdef HAVE_MP3LAME
	{"lameopts", lameopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
#ifdef USE_LIBAVCODEC
	{"lavcopts", lavcopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif
#ifdef USE_WIN32DLL
	{"vfwopts", vfwopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#endif

#define MAIN_CONF
#include "cfg-common.h"
#undef MAIN_CONF

//	{"quiet", &quiet, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE|CONF_GLOBAL, 0, 100, NULL},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL, 0, 0, NULL},
//	{"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
//	{"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
//	{"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
