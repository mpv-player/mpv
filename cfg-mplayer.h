/*
 * config for cfgparser
 */

#include "cfg-common.h"

extern char *playlist_file;

#ifdef HAVE_FBDEV
extern char *fb_dev_name;
extern char *fb_mode_cfgfile;
extern char *fb_mode_name;
extern char *monitor_hfreq_str;
extern char *monitor_vfreq_str;
extern char *monitor_dotclock_str;
#endif
#ifdef HAVE_PNG
extern int z_compression;
#endif
#ifdef HAVE_SDL
//extern char *sdl_driver;
extern int sdl_noxv;
extern int sdl_forcexv;
//extern char *sdl_adriver;
#endif
#ifdef USE_FAKE_MONO
extern int fakemono; // defined in dec_audio.c
#endif

#ifdef HAVE_LIRC
extern char *lirc_configfile;
#endif

#ifndef USE_LIBVO2
extern int vo_doublebuffering;
extern int vo_fsmode;
extern int vo_dbpp;
#endif

#ifdef USE_SUB
extern int sub_unicode;
extern int sub_utf8;
#ifdef USE_ICONV
extern char *sub_cp;
#endif
#endif

#ifdef USE_OSD
extern int osd_level;
#endif

extern char *ao_outputfilename;
extern int ao_pcm_waveheader;

#ifdef HAVE_X11
extern char *mDisplayName;
#endif

#ifdef HAVE_AA
extern int vo_aa_parseoption(struct config * conf, char *opt, char * param);
#endif

#ifdef HAVE_NEW_GUI
extern char * skinName;
#endif

#ifdef HAVE_ODIVX_POSTPROCESS
extern int use_old_pp;
#endif


/* from libvo/aspect.c */
extern float monitor_aspect;

/*
 * CONF_TYPE_FUNC_FULL :
 * allows own implemtations for passing the params
 * 
 * the function receives parameter name and argument (if it does not start with - )
 * useful with a conf.name like 'aa*' to parse several parameters to a function
 * return 0 =ok, but we didn't need the param (could be the filename)
 * return 1 =ok, we accepted the param
 * negative values: see cfgparser.h, ERR_XXX
 *
 * by Folke
 */

struct config conf[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, 0, 0, 0}, /* this must be the first!!! */

//---------------------- libao/libvo/mplayer options ------------------------
	{"o", "Option -o has been renamed to -vo (video-out), use -vo !\n",
            CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{"vo", &video_driver, CONF_TYPE_STRING, 0, 0, 0},
	{"ao", &audio_driver, CONF_TYPE_STRING, 0, 0, 0},
//	{"dsp", &dsp, CONF_TYPE_STRING, CONF_NOCFG, 0, 0},
	{"dsp", "Use -ao oss:dsp_path!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
        {"mixer", &mixer_device, CONF_TYPE_STRING, 0, 0, 0},
        {"master", &mixer_usemaster, CONF_TYPE_FLAG, 0, 0, 1},
#ifdef HAVE_X11
	{"display", &mDisplayName, CONF_TYPE_STRING, 0, 0, 0},
#endif
	{"osdlevel", &osd_level, CONF_TYPE_INT, CONF_RANGE, 0, 2 },

#ifdef HAVE_FBDEV
	{"fb", &fb_dev_name, CONF_TYPE_STRING, 0, 0, 0},
	{"fbmode", &fb_mode_name, CONF_TYPE_STRING, 0, 0, 0},
	{"fbmodeconfig", &fb_mode_cfgfile, CONF_TYPE_STRING, 0, 0, 0},
	{"monitor_hfreq", &monitor_hfreq_str, CONF_TYPE_STRING, 0, 0, 0},
	{"monitor_vfreq", &monitor_vfreq_str, CONF_TYPE_STRING, 0, 0, 0},
	{"monitor_dotclock", &monitor_dotclock_str, CONF_TYPE_STRING, 0, 0, 0},
#endif
//	{"encode", &encode_name, CONF_TYPE_STRING, 0, 0, 0},
#ifdef USE_SUB
	{"sub", &sub_name, CONF_TYPE_STRING, 0, 0, 0},
#ifdef USE_ICONV
	{"subcp", &sub_cp, CONF_TYPE_STRING, 0, 0, 0},
#endif	
	{"subdelay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0},
	{"subfps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0},
        {"noautosub", &sub_auto, CONF_TYPE_FLAG, 0, 1, 0},
	{"unicode", &sub_unicode, CONF_TYPE_FLAG, 0, 0, 1},
	{"nounicode", &sub_unicode, CONF_TYPE_FLAG, 0, 1, 0},
	{"utf8", &sub_utf8, CONF_TYPE_FLAG, 0, 0, 1},
	{"noutf8", &sub_utf8, CONF_TYPE_FLAG, 0, 1, 0},
#endif
#ifdef USE_OSD
	{"font", &font_name, CONF_TYPE_STRING, 0, 0, 0},
	{"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0},
#endif
//	{"bg", &play_in_bg, CONF_TYPE_FLAG, 0, 0, 1},
//	{"nobg", &play_in_bg, CONF_TYPE_FLAG, 0, 1, 0},
	{"sb", &seek_to_byte, CONF_TYPE_INT, CONF_MIN, 0, 0},
	{"ss", &seek_to_sec, CONF_TYPE_STRING, CONF_MIN, 0, 0},
	{"sstep", &step_sec, CONF_TYPE_INT, CONF_MIN, 0, 0},
	{"loop", &loop_times, CONF_TYPE_INT, CONF_RANGE, -1, 10000},
	{"sound", &has_audio, CONF_TYPE_FLAG, 0, 0, 1},
	{"nosound", &has_audio, CONF_TYPE_FLAG, 0, 1, 0},
	{"abs", &ao_data.buffersize, CONF_TYPE_INT, CONF_MIN, 0, 0},
	{"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0},

	{"alsa", "Option -alsa has been removed, new audio code doesn't need it! Remove it from your config file!\n",
            CONF_TYPE_PRINT, 0, 0, 0},
	{"noalsa", "Option -noalsa has been removed, new audio code doesn't need it! Remove it from your config file!\n",
            CONF_TYPE_PRINT, 0, 0, 0},

	{"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1},
	{"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2},
	{"noframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 1, 0},

	{"autoq", &auto_quality, CONF_TYPE_INT, CONF_RANGE, 0, 100},

	{"benchmark", &benchmark, CONF_TYPE_FLAG, 0, 0, 1},
	
	{"dumpfile", &stream_dump_name, CONF_TYPE_STRING, 0, 0, 0},
	{"dumpaudio", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 1},
	{"dumpvideo", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 2},
	{"dumpsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 3},
	{"dumpmpsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 4},

	{"aofile", &ao_outputfilename, CONF_TYPE_STRING, 0, 0, 0},
	{"waveheader", &ao_pcm_waveheader, CONF_TYPE_FLAG, 0, 0, 1},
	{"nowaveheader", &ao_pcm_waveheader, CONF_TYPE_FLAG, 0, 1, 0},

	{"dshow", &allow_dshow, CONF_TYPE_FLAG, 0, 0, 1}, // Is this still needed? atmos ::
	{"nodshow", &allow_dshow, CONF_TYPE_FLAG, 0, 1, 0},

#ifdef HAVE_PNG
	{"z", &z_compression, CONF_TYPE_INT, CONF_RANGE, 0, 9},
#endif	
#ifdef HAVE_SDL
	{"sdl", "Use -vo sdl:driver instead of -vo sdl -sdl driver\n",
	    CONF_TYPE_PRINT, 0, 0, 0},
	{"noxv", &sdl_noxv, CONF_TYPE_FLAG, 0, 0, 1},
	{"forcexv", &sdl_forcexv, CONF_TYPE_FLAG, 0, 0, 1},
	{"sdla", "Use -ao sdl:driver instead of -ao sdl -sdla driver\n",
	    CONF_TYPE_PRINT, 0, 0, 0},
#endif	
	{"x", &screen_size_x, CONF_TYPE_INT, CONF_RANGE, 0, 4096},
	{"y", &screen_size_y, CONF_TYPE_INT, CONF_RANGE, 0, 4096},
	{"xy", &screen_size_xy, CONF_TYPE_INT, CONF_RANGE, 0, 4096},
	{"screenw", &vo_screenwidth, CONF_TYPE_INT, CONF_RANGE, 0, 4096},
	{"screenh", &vo_screenheight, CONF_TYPE_INT, CONF_RANGE, 0, 4096},
	{"aspect", &movie_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0},
	{"noaspect", &movie_aspect, CONF_TYPE_FLAG, 0, 0, 0},
	{"monitoraspect", &monitor_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0},
        {"vm", &vidmode, CONF_TYPE_FLAG, 0, 0, 1},
        {"novm", &vidmode, CONF_TYPE_FLAG, 0, 1, 0},
	{"fs", &fullscreen, CONF_TYPE_FLAG, 0, 0, 1},
	{"nofs", &fullscreen, CONF_TYPE_FLAG, 0, 1, 0},
        {"zoom", &softzoom, CONF_TYPE_FLAG, 0, 0, 1},
        {"nozoom", &softzoom, CONF_TYPE_FLAG, 0, 1, 0},
        {"flip", &flip, CONF_TYPE_FLAG, 0, -1, 1},
        {"noflip", &flip, CONF_TYPE_FLAG, 0, -1, 0},
       
#ifndef USE_LIBVO2
        {"bpp", &vo_dbpp, CONF_TYPE_INT, CONF_RANGE, 0, 32},
	{"fsmode", &vo_fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 15},
	{"double", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 0, 1},
	{"nodouble", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 1, 0},
#endif

#ifdef HAVE_AA
	{"aa*",	vo_aa_parseoption,  CONF_TYPE_FUNC_FULL, 0, 0, 0 },
#endif

#ifdef HAVE_LIRC
	{"lircconf", &lirc_configfile, CONF_TYPE_STRING, 0, 0, 0}, 
#endif

	{"gui", &use_gui, CONF_TYPE_FLAG, 0, 0, 1},
	{"nogui", &use_gui, CONF_TYPE_FLAG, 0, 1, 0},
      
#ifdef HAVE_NEW_GUI
	{"skin", &skinName, CONF_TYPE_STRING, 0, 0, 0},
#endif

	{"playlist", &playlist_file, CONF_TYPE_STRING, 0, 0, 0},
	{"dapsync", &dapsync, CONF_TYPE_FLAG, 0, 0, 1},
	{"nodapsync", &dapsync, CONF_TYPE_FLAG, 0, 1, 0},
	{"softsleep", &softsleep, CONF_TYPE_FLAG, 0, 0, 1},


#define MAIN_CONF
#include "cfg-common.h"
#undef MAIN_CONF
        
	{"quiet", &quiet, CONF_TYPE_FLAG, 0, 0, 1},
	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE, 0, 100},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, 0, 0, 0},
	{"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};
