/*
 * config for cfgparser
 */

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
#endif

#ifdef USE_OSD
extern int osd_level;
#endif

extern char *ao_outputfilename;
extern int ao_pcm_waveheader;

#ifdef HAVE_X11
extern char *mDisplayName;
#endif

struct config conf[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, 0, 0, 0}, /* this must be the first!!! */
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
#ifdef HAVE_LIBCSS
        {"dvdauth", &dvd_auth_device, CONF_TYPE_STRING, 0, 0, 0},
        {"dvdkey", &dvdimportkey, CONF_TYPE_STRING, 0, 0, 0},
	{"dvd", "Option -dvd will be \"full disk\" mode, old meaning has been renamed to -dvdauth.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
#else
        {"dvd", "DVD support was not compiled in. See file DOCS/DVD.\n",
            CONF_TYPE_PRINT, CONF_NOCFG, 0 , 0},
        {"dvdkey", "DVD support was not compiled in. See file DOCS/DVD.\n",
            CONF_TYPE_PRINT, CONF_NOCFG, 0 , 0},
        {"dvdauth", "DVD support was not compiled in. See file DOCS/DVD.\n",
            CONF_TYPE_PRINT, CONF_NOCFG, 0 , 0},
#endif
			    
#ifdef HAVE_FBDEV
	{"fb", &fb_dev_name, CONF_TYPE_STRING, 0, 0, 0},
	{"fbmode", &fb_mode_name, CONF_TYPE_STRING, 0, 0, 0},
	{"fbmodeconfig", &fb_mode_cfgfile, CONF_TYPE_STRING, 0, 0, 0},
	{"monitor_hfreq", &monitor_hfreq_str, CONF_TYPE_STRING, 0, 0, 0},
	{"monitor_vfreq", &monitor_vfreq_str, CONF_TYPE_STRING, 0, 0, 0},
	{"monitor_dotclock", &monitor_dotclock_str, CONF_TYPE_STRING, 0, 0, 0},
#endif
	{"encode", &encode_name, CONF_TYPE_STRING, 0, 0, 0},
#ifdef USE_SUB
	{"sub", &sub_name, CONF_TYPE_STRING, 0, 0, 0},
	{"subdelay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0},
	{"subfps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0},
        {"noautosub", &sub_auto, CONF_TYPE_FLAG, 0, 1, 0},
	{"unicode", &sub_unicode, CONF_TYPE_FLAG, 0, 0, 1},
	{"nounicode", &sub_unicode, CONF_TYPE_FLAG, 0, 1, 0},
#endif
#ifdef USE_OSD
	{"font", &font_name, CONF_TYPE_STRING, 0, 0, 0},
	{"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0},
#endif
	{"bg", &play_in_bg, CONF_TYPE_FLAG, 0, 0, 1},
	{"nobg", &play_in_bg, CONF_TYPE_FLAG, 0, 1, 0},
	{"sb", &seek_to_byte, CONF_TYPE_INT, CONF_MIN, 0, 0},
	{"ss", &seek_to_sec, CONF_TYPE_STRING, CONF_MIN, 0, 0},
	{"sound", &has_audio, CONF_TYPE_FLAG, 0, 0, 1},
	{"nosound", &has_audio, CONF_TYPE_FLAG, 0, 1, 0},
	{"abs", &ao_buffersize, CONF_TYPE_INT, CONF_MIN, 0, 0},
	{"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0},
	{"bps", &pts_from_bps, CONF_TYPE_FLAG, 0, 0, 1},
	{"nobps", &pts_from_bps, CONF_TYPE_FLAG, 0, 1, 0},

//	{"alsa", &alsa, CONF_TYPE_FLAG, 0, 0, 1},
//	{"noalsa", &alsa, CONF_TYPE_FLAG, 0, 1, 0},
	{"alsa", "Option -alsa has been removed, new audio code doesn't need it!\n",
            CONF_TYPE_PRINT, 0, 0, 0},
	{"noalsa", "Option -noalsa has been removed, new audio code doesn't need it!\n",
            CONF_TYPE_PRINT, 0, 0, 0},

	{"ni", &force_ni, CONF_TYPE_FLAG, 0, 0, 1},
	{"noni", &force_ni, CONF_TYPE_FLAG, 0, 1, 0},

	{"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1},
	{"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2},
	{"noframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 1, 0},

	{"frames", &play_n_frames, CONF_TYPE_INT, CONF_MIN, 0, 0},
	{"benchmark", &benchmark, CONF_TYPE_FLAG, 0, 0, 1},
	
	{"aid", &audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 255},
	{"vid", &video_id, CONF_TYPE_INT, CONF_RANGE, 0, 255},
	{"sid", &dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31},
#ifdef USE_FAKE_MONO
	{"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2},
#endif

	{"dumpfile", &stream_dump_name, CONF_TYPE_STRING, 0, 0, 0},
	{"dumpaudio", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 1},
	{"dumpvideo", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 2},
	{"dumpsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 3},
	
	{"aofile", &ao_outputfilename, CONF_TYPE_STRING, 0, 0, 0},
	{"waveheader", &ao_pcm_waveheader, CONF_TYPE_FLAG, 0, 0, 1},
	{"nowaveheader", &ao_pcm_waveheader, CONF_TYPE_FLAG, 0, 1, 0},

//	{"auds", &avi_header.audio_codec, CONF_TYPE_STRING, 0, 0, 0},
//	{"vids", &avi_header.video_codec, CONF_TYPE_STRING, 0, 0, 0},
	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10},
	{"fps", &force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0},
	{"srate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000},
	{"afm", &audio_family, CONF_TYPE_INT, CONF_RANGE, 0, 8}, // keep ranges in sync
	{"vfm", &video_family, CONF_TYPE_INT, CONF_RANGE, 0, 7}, // with codec-cfg.c
	{"ac", &audio_codec, CONF_TYPE_STRING, 0, 0, 0},
	{"vc", &video_codec, CONF_TYPE_STRING, 0, 0, 0},
	{"dshow", &allow_dshow, CONF_TYPE_FLAG, 0, 0, 1}, // Is this still needed? atmos ::
	{"nodshow", &allow_dshow, CONF_TYPE_FLAG, 0, 1, 0},
	{"vcd", &vcd_track, CONF_TYPE_INT, CONF_RANGE, 1, 99},
	{"divxq", "Option -divxq has been renamed to -pp (postprocessing), use -pp !\n",
            CONF_TYPE_PRINT, 0, 0, 0},
	{"pp", &divx_quality, CONF_TYPE_INT, CONF_RANGE, 0, 63},
	{"autoq", &auto_quality, CONF_TYPE_INT, CONF_RANGE, 0, 100},
	{"br", &encode_bitrate, CONF_TYPE_INT, CONF_RANGE, 10000, 10000000},
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

#ifdef HAVE_LIRC
	{"lircconf", &lirc_configfile, CONF_TYPE_STRING, 0, 0, 0}, 
#endif
	
	{"noidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 0},
	{"idx", &index_mode, CONF_TYPE_FLAG, 0, -1, 1},
	{"forceidx", &index_mode, CONF_TYPE_FLAG, 0, -1, 2},
        
	{"quiet", &quiet, CONF_TYPE_FLAG, 0, 0, 1},
	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE, 0, 100},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, 0, 0, 0},
	{"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};
