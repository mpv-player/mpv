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
struct config vfwopts_conf[]={
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
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
	{"help", "\nAvailable codecs:\n   copy\n   frameno\n   rawrgb\n"
#ifdef HAVE_DIVX4ENCORE
	"   divx4\n"
#endif
#ifdef USE_LIBAVCODEC
	"   lavc\n"
#endif
#ifdef USE_WIN32DLL
	"   vfw\n"
#endif
#ifdef HAVE_LIBDV095
	"   libdv\n"
#endif
	"\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

struct config oac_conf[]={
	{"copy", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_COPY, NULL},
	{"pcm", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_PCM, NULL},
#ifdef HAVE_MP3LAME
	{"mp3lame", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_VBRMP3, NULL},
	{"help", "\nAvailable codecs:\n   copy\n   pcm\n   mp3lame\n\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#else
	{"mp3lame", "MPlayer was compiled without libmp3lame support!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"help", "\nAvailable codecs:\n   copy\n   pcm\n\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

static config_t mencoder_opts[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL}, /* this must be the first!!! */

	{"endpos", parse_end_at, CONF_TYPE_FUNC_PARAM, 0, 0, 0, NULL},
	
	{"ofps", &force_ofps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL},
	{"o", &out_filename, CONF_TYPE_STRING, 0, 0, 0, NULL},

	{"skiplimit", &skip_limit, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"noskiplimit", &skip_limit, CONF_TYPE_FLAG, 0, 0, -1, NULL},
	{"noskip", &skip_limit, CONF_TYPE_FLAG, 0, 0, 0, NULL},

	{"x", "This option is obsolete, use -vop scale=w:h for scaling\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},

	{"xsize", "This option is obsolete, use -vop crop=w:h:x0:y0 for cropping\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},

	{"mp3file", &mp3_filename, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ac3file", &ac3_filename, CONF_TYPE_STRING, 0, 0, 0, NULL},

//	{"oac", &out_audio_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},
//	{"ovc", &out_video_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"oac", oac_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
	{"ovc", ovc_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},

	{"ffourcc", &force_fourcc, CONF_TYPE_STRING, 0, 4, 4, NULL},

	{"pass", &pass, CONF_TYPE_INT, CONF_RANGE,0,2, NULL},
	{"passlogfile", &passtmpfile, CONF_TYPE_STRING, 0, 0, 0, NULL},
	
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
