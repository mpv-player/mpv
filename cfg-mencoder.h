/*
 * config for cfgparser
 */

#ifdef USE_FAKE_MONO
extern int fakemono; // defined in dec_audio.c
#endif
#ifdef HAVE_ODIVX_POSTPROCESS
extern int use_old_pp;
#endif

struct config conf[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, 0, 0, 0}, /* this must be the first!!! */


#include "cfg-common.h"

//	{"quiet", &quiet, CONF_TYPE_FLAG, 0, 0, 1},
	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE, 0, 100},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, 0, 0, 0},
//	{"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
//	{"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
//	{"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};
