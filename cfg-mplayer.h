/*
 * config for cfgparser
 */

#include "cfg-common.h"

extern int use_stdin;

#if defined(HAVE_FBDEV)||defined(HAVE_VESA)
extern char *monitor_hfreq_str;
extern char *monitor_vfreq_str;
extern char *monitor_dotclock_str;
#endif

#ifdef HAVE_FBDEV
extern char *fb_dev_name;
extern char *fb_mode_cfgfile;
extern char *fb_mode_name;
#else
#ifdef HAVE_DIRECTFB
extern char *fb_dev_name;
#endif
#endif
#ifdef HAVE_PNG
extern int z_compression;
#endif
#ifdef HAVE_JPEG
extern int jpeg_baseline;
extern int jpeg_progressive_mode;
extern int jpeg_optimize;
extern int jpeg_smooth;
extern int jpeg_quality;
extern char * jpeg_outdir;
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

extern int vo_doublebuffering;
extern int vo_vsync;
extern int vo_fsmode;
extern int vo_dbpp;
extern int vo_directrendering;
extern int vd_use_slices;
extern float vo_panscan;
/* only used by startup (setting these values from configfile) */
extern int vo_gamma_brightness;
extern int vo_gamma_saturation;
extern int vo_gamma_contrast;
extern int vo_gamma_hue;

#ifdef USE_OSD
extern int osd_level;
#endif

extern char *ao_outputfilename;
extern int ao_pcm_waveheader;

#ifdef HAVE_X11
extern char *mDisplayName;
extern int WinID;
extern int ice_layer;
extern int stop_xscreensaver;
#endif

#ifdef HAVE_AA
extern int vo_aa_parseoption(struct config * conf, char *opt, char * param);
extern void vo_aa_revertoption(config_t* opt,char* param);
#endif

#ifdef HAVE_ZR
extern int vo_zr_parseoption(struct config * conf, char *opt, char * param);
extern void vo_zr_revertoption(config_t* opt,char* pram);
#endif

#ifdef STREAMING_LIVE_DOT_COM
extern int isSDPFile;
#endif

#ifdef HAVE_NEW_GUI
extern char * skinName;
#endif

#ifdef HAVE_ODIVX_POSTPROCESS
extern int use_old_pp;
#endif

#ifdef HAVE_XINERAMA
extern int xinerama_screen;
#endif

#ifdef HAVE_RTC
extern int nortc;
#endif

/* from libvo/aspect.c */
extern float monitor_aspect;

/* from dec_audio, currently used for ac3surround decoder only */
extern int audio_output_channels;

/* Options related to audio out plugins */
struct config ao_plugin_conf[]={
	{"list", &ao_plugin_cfg.plugin_list, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"delay", &ao_plugin_cfg.pl_delay_len, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"format", &ao_plugin_cfg.pl_format_type, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"fout", &ao_plugin_cfg.pl_resample_fout, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"volume", &ao_plugin_cfg.pl_volume_volume, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"mul", &ao_plugin_cfg.pl_extrastereo_mul, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0, NULL},
	{"softclip", &ao_plugin_cfg.pl_volume_softclip, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

#ifdef HAVE_JPEG
struct config jpeg_conf[]={
	{"progressiv", &jpeg_progressive_mode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noprogressiv", &jpeg_progressive_mode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"baseline", &jpeg_baseline, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nobaseline", &jpeg_baseline, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"optimize", &jpeg_optimize, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"smooth", &jpeg_smooth, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"quality", &jpeg_quality, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"outdir", &jpeg_outdir, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

extern int sws_flags;
extern int readPPOpt(void *conf, char *arg);
extern int readNPPOpt(void *conf, char *arg);
extern void revertPPOpt(void *conf, char* opt);


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

static config_t mplayer_opts[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL}, /* this don't need anymore to be the first!!! */

//---------------------- libao/libvo options ------------------------
	{"o", "Option -o has been renamed to -vo (video-out), use -vo !\n",
            CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"vo", &video_driver, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ao", &audio_driver, CONF_TYPE_STRING, 0, 0, 0, NULL},

	{"aop", ao_plugin_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
	{"dsp", "Use -ao oss:dsp_path!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"mixer", &mixer_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"master", "Option -master has been removed, use -aop list=volume instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"channels", &audio_output_channels, CONF_TYPE_INT, CONF_RANGE, 2, 6, NULL},
	// override audio buffer size (used only by -ao oss, anyway obsolete...)
	{"abs", &ao_data.buffersize, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	// -ao pcm options:
	{"aofile", &ao_outputfilename, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"waveheader", &ao_pcm_waveheader, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nowaveheader", &ao_pcm_waveheader, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{"alsa", "Option -alsa has been removed, new audio code doesn't need it! Remove it from your config file!\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"noalsa", "Option -noalsa has been removed, new audio code doesn't need it! Remove it from your config file!\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},

#ifdef HAVE_X11
	{"display", &mDisplayName, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

	// -vo png only:
#ifdef HAVE_PNG
	{"z", &z_compression, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
#endif
	// -vo jpeg only:
#ifdef HAVE_JPEG
	{"jpeg", jpeg_conf, CONF_TYPE_SUBCONFIG, 0,0,0, NULL},
#endif
	// -vo sdl only:
#ifdef HAVE_SDL
	{"sdl", "Use -vo sdl:driver instead of -vo sdl -sdl driver\n",
	    CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"noxv", &sdl_noxv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"forcexv", &sdl_forcexv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	// -ao sdl only:
	{"sdla", "Use -ao sdl:driver instead of -ao sdl -sdla driver\n",
	    CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif

#ifdef HAVE_FBDEV
	{"fb", &fb_dev_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"fbmode", &fb_mode_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"fbmodeconfig", &fb_mode_cfgfile, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"monitor_hfreq", &monitor_hfreq_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"monitor_vfreq", &monitor_vfreq_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"monitor_dotclock", &monitor_dotclock_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
#else
#ifdef HAVE_DIRECTFB
	{"fb", &fb_dev_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
#endif

	// force window width/height or resolution (with -vm)
	{"x", &opt_screen_size_x, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	{"y", &opt_screen_size_y, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	// set screen dimensions (when not detectable or virtual!=visible)
	{"screenw", &vo_screenwidth, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	{"screenh", &vo_screenheight, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	// set aspect ratio of monitor - usefull for 16:9 TVout
	{"monitoraspect", &monitor_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0, NULL},
	// video mode switching: (x11,xv,dga)
        {"vm", &vidmode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"novm", &vidmode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	// start in fullscreen mode:
	{"fs", &fullscreen, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nofs", &fullscreen, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	// set fullscreen switch method (workaround for buggy WMs)
	{"fsmode", "option 'fsmode' is obsolete, avoid using it! if you really want it, try -fsmode-dontuse, but don't report bugs with this option used!", CONF_TYPE_PRINT, CONF_RANGE, 0, 31, NULL},
	{"fsmode-dontuse", &vo_fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
	// set bpp (x11+vm, dga, fbdev, vesa, svga?)
        {"bpp", &vo_dbpp, CONF_TYPE_INT, CONF_RANGE, 0, 32, NULL},
	// double buffering:  (mga/xmga, xv, vidix, vesa, fbdev)
	{"double", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nodouble", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	// wait for v-sync (vesa)
	{"vsync", &vo_vsync, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"novsync", &vo_vsync, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"panscan", &vo_panscan, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},

#ifdef HAVE_X11
	// x11,xv,xmga,xvidix
	{"wid", &WinID, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"rootwin", &WinID, CONF_TYPE_FLAG, 0, -1, 0, NULL},
	{"icelayer", &ice_layer, CONF_TYPE_INT, CONF_RANGE, 0, 15, NULL},
	{"stop_xscreensaver", &stop_xscreensaver, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nostop_xscreensaver", &stop_xscreensaver, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#endif

#ifdef HAVE_XINERAMA
	{"xineramascreen", &xinerama_screen, CONF_TYPE_INT, CONF_RANGE, 0, 32, NULL},
#endif

	{"brightness",&vo_gamma_brightness, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
	{"saturation",&vo_gamma_saturation, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
	{"contrast",&vo_gamma_contrast, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
	{"hue",&vo_gamma_hue, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},

	// direct rendering (decoding to video out buffer)
	{"dr", &vo_directrendering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nodr", &vo_directrendering, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"vaa_dr", "Use -dr, -vaa_dr was obsoleted\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"vaa_nodr", "Use -nodr, -vaa_nodr was obsoleted\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},

	// draw by slices or whole frame (usefull with libmpeg2/libavcodec)
	{"slices", &vd_use_slices, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noslices", &vd_use_slices, CONF_TYPE_FLAG, 0, 1, 0, NULL},

#ifdef HAVE_AA
	// -vo aa
	{"aa*",	vo_aa_parseoption,  CONF_TYPE_FUNC_FULL, 0, 0, 0 , &vo_aa_revertoption},
#endif

#ifdef HAVE_ZR
	// -vo zr
	{"zr*", vo_zr_parseoption, CONF_TYPE_FUNC_FULL, 0, 0, 0, &vo_zr_revertoption },
#endif

#ifdef STREAMING_LIVE_DOT_COM
	// -sdp option, specifying that the source is a SDP file
        {"sdp", &isSDPFile, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#else
	{"sdp", "MPlayer was compiled WITHOUT the \"LIVE.COM Streaming Media\" libraries!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
#endif

//---------------------- mplayer-only options ------------------------

	{"osdlevel", &osd_level, CONF_TYPE_INT, CONF_RANGE, 0, 2 , NULL},

	// these should be moved to -common, and suppot in mencoder too
	{"vobsub", &vobsub_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vobsubid", &vobsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},

	{"sstep", &step_sec, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	// set a-v distance, should be moved to -common and support in mencoder
	{"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -100.0, 100.0, NULL},

	{"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2, NULL},
	{"noframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{"autoq", &auto_quality, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},

	{"benchmark", &benchmark, CONF_TYPE_FLAG, 0, 0, 1, NULL},

	// dump some stream out instead of playing the file
	// this really should be in mencoder instead of mplayer... -> TODO
	{"dumpfile", &stream_dump_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"dumpaudio", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"dumpvideo", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 2, NULL},
	{"dumpsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 3, NULL},
	{"dumpmpsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 4, NULL},
	{"dumpstream", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 5, NULL},
	{"dumpsrtsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 6, NULL},

#ifdef HAVE_LIRC
	{"lircconf", &lirc_configfile, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL}, 
#endif

	{"gui", &use_gui, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_NOCMD, 0, 1, NULL},
//	{"nogui", &use_gui, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
      
#ifdef HAVE_NEW_GUI
	{"skin", &skinName, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
#endif

	{"noloop", &loop_times, CONF_TYPE_FLAG, 0, 0, -1, NULL},
	{"loop", &loop_times, CONF_TYPE_INT, CONF_RANGE, -1, 10000, NULL},
	{"playlist", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL},

	// a-v sync stuff:
	{"dapsync", &dapsync, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nodapsync", &dapsync, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"softsleep", &softsleep, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#ifdef HAVE_RTC
	{"nortc", &nortc, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif

	{"slave", &slave_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL},
	{"use-stdin", &use_stdin, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},

#define MAIN_CONF
#include "cfg-common.h"
#undef MAIN_CONF
        
	{"quiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
	{"noquiet", &quiet, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE|CONF_GLOBAL, 0, 100, NULL},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOSAVE, 0, 0, NULL},
	{"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
	{"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
	{"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
