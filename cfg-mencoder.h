/*
 * config for cfgparser
 */

#ifdef USE_FAKE_MONO
extern int fakemono; // defined in dec_audio.c
#endif
#ifdef HAVE_ODIVX_POSTPROCESS
extern int use_old_pp;
#endif

struct config divx4opts_conf[]={
	{"br", &divx4_param.bitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000},
	{"rc_period", &divx4_param.rc_period, CONF_TYPE_INT, 0,0,0},
	{"rc_reaction_period", &divx4_param.rc_reaction_period, CONF_TYPE_INT, 0,0,0},
	{"rc_reaction_ratio", &divx4_param.rc_reaction_ratio, CONF_TYPE_INT, 0,0,0},
	{"min_quant", &divx4_param.min_quantizer, CONF_TYPE_INT, CONF_RANGE,0,32},
	{"max_quant", &divx4_param.max_quantizer, CONF_TYPE_INT, CONF_RANGE,0,32},
	{"key", &divx4_param.max_key_interval, CONF_TYPE_INT, CONF_MIN,0,0},
	{"deinterlace", &divx4_param.deinterlace, CONF_TYPE_FLAG, 0,0,1},
	{"q", &divx4_param.quality, CONF_TYPE_INT, CONF_RANGE, 1, 5},
	{"crispness", &divx4_crispness, CONF_TYPE_INT, CONF_RANGE,0,100},
	{"help", "TODO: divx4opts help!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};

struct config lameopts_conf[]={
	{"q", &lame_param_quality, CONF_TYPE_INT, CONF_RANGE, 0, 9},
	{"vbr", &lame_param_vbr, CONF_TYPE_INT, CONF_RANGE, 0, vbr_max_indicator},
	{"cbr", &lame_param_vbr, CONF_TYPE_FLAG, 0, 0, 0},
	{"mode", &lame_param_mode, CONF_TYPE_INT, CONF_RANGE, 0, MAX_INDICATOR},
	{"padding", &lame_param_padding, CONF_TYPE_INT, CONF_RANGE, 0, PAD_MAX_INDICATOR},
	{"br", &lame_param_br, CONF_TYPE_INT, CONF_RANGE, 0, 1024},
	{"ratio", &lame_param_ratio, CONF_TYPE_INT, CONF_RANGE, 0, 100},
	{"help", "TODO: lameopts help!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};

struct config ovc_conf[]={
	{"copy", &out_video_codec, CONF_TYPE_FLAG, 0, 0, 0},
	{"divx4", &out_video_codec, CONF_TYPE_FLAG, 0, 0, VCODEC_DIVX4},
	{"help", "\nAvailable codecs:\n   copy\n   divx4\n\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};

struct config oac_conf[]={
	{"copy", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, 0},
	{"pcm", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_PCM},
	{"mp3lame", &out_audio_codec, CONF_TYPE_FLAG, 0, 0, ACODEC_VBRMP3},
	{"help", "\nAvailable codecs:\n   copy\n   pcm\n   mp3lame\n\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};

struct config conf[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, 0, 0, 0}, /* this must be the first!!! */

	{"ofps", &force_ofps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0},
	{"o", &out_filename, CONF_TYPE_STRING, 0, 0, 0},

	{"mp3file", &mp3_filename, CONF_TYPE_STRING, 0, 0, 0},
	{"ac3file", &ac3_filename, CONF_TYPE_STRING, 0, 0, 0},

//	{"oac", &out_audio_codec, CONF_TYPE_STRING, 0, 0, 0},
//	{"ovc", &out_video_codec, CONF_TYPE_STRING, 0, 0, 0},
	{"oac", oac_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0},
	{"ovc", ovc_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0},

	{"pass", &pass, CONF_TYPE_INT, CONF_RANGE,0,2},
	
	{"divx4opts", divx4opts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0},
	{"lameopts", lameopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0},

#include "cfg-common.h"

//	{"quiet", &quiet, CONF_TYPE_FLAG, 0, 0, 1},
	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE, 0, 100},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, 0, 0, 0},
//	{"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
//	{"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
//	{"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};
