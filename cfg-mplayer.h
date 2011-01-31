/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_CFG_MPLAYER_H
#define MPLAYER_CFG_MPLAYER_H

/*
 * config for cfgparser
 */

#include <stddef.h>

#include "cfg-common.h"
#include "options.h"

extern char *fb_mode_cfgfile;
extern char *fb_mode_name;

extern char *lirc_configfile;

/* only used at startup (setting these values from configfile) */
extern char *vo_geometry;
extern int stop_xscreensaver;

extern int menu_startup;
extern int menu_keepdir;
extern char *menu_chroot;
extern char *menu_fribidi_charset;
extern int menu_flip_hebrew;
extern int menu_fribidi_flip_commas;

extern char *unrar_executable;

const m_option_t vd_conf[]={
    {"help", "Use MPlayer with an appropriate video file instead of live partners to avoid vd.\n", CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#ifdef CONFIG_TV
const m_option_t tvscan_conf[]={
    {"autostart", &stream_tv_defaults.scan, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"threshold", &stream_tv_defaults.scan_threshold, CONF_TYPE_INT, CONF_RANGE, 1, 100, NULL},
    {"period", &stream_tv_defaults.scan_period, CONF_TYPE_FLOAT, CONF_RANGE, 0.1, 2.0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

const m_option_t mplayer_opts[]={
    /* name, pointer, type, flags, min, max */

//---------------------- libao/libvo options ------------------------
    {"o", "Option -o has been renamed to -vo (video-out), use -vo.\n",
     CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    OPT_STRINGLIST("vo", video_driver_list, 0),
    OPT_STRINGLIST("ao", audio_driver_list, 0),
    OPT_MAKE_FLAGS("fixed-vo", fixed_vo, CONF_GLOBAL),
    OPT_MAKE_FLAGS("ontop", vo_ontop, 0),
    {"rootwin", &vo_rootwin, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"border", &vo_border, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noborder", &vo_border, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    {"aop", "-aop has been removed, use -af instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    {"dsp", "-dsp has been removed. Use -ao oss:dsp_path instead.\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    {"mixer", &mixer_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"mixer-channel", &mixer_channel, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"softvol", &soft_vol, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nosoftvol", &soft_vol, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"softvol-max", &soft_vol_max, CONF_TYPE_FLOAT, CONF_RANGE, 10, 10000, NULL},
    {"volstep", &volstep, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
    {"volume", &start_volume, CONF_TYPE_FLOAT, CONF_RANGE, -1, 10000, NULL},
    OPT_MAKE_FLAGS("gapless-audio", gapless_audio, 0),
    {"master", "Option -master has been removed, use -af volume instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    // override audio buffer size (used only by -ao oss/win32, obsolete)
    OPT_INT("abs", ao_buffersize, 0),

    // -ao pcm options:
    {"aofile", "-aofile has been removed. Use -ao pcm:file=<filename> instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"waveheader", "-waveheader has been removed. Use -ao pcm:waveheader instead.\n", CONF_TYPE_PRINT, 0, 0, 1, NULL},
    {"nowaveheader", "-nowaveheader has been removed. Use -ao pcm:nowaveheader instead.\n", CONF_TYPE_PRINT, 0, 1, 0, NULL},

    {"alsa", "-alsa has been removed. Remove it from your config file.\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"noalsa", "-noalsa has been removed. Remove it from your config file.\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"edlout", &edl_output_filename,  CONF_TYPE_STRING, 0, 0, 0, NULL},

#ifdef CONFIG_X11
    {"display", &mDisplayName, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

    // -vo png only:
#ifdef CONFIG_PNG
    {"z", "-z has been removed. Use -vo png:z=<0-9> instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif
    // -vo jpeg only:
#ifdef CONFIG_JPEG
    {"jpeg", "-jpeg has been removed. Use -vo jpeg:<options> instead.\n",
     CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif
    // -vo sdl only:
    {"sdl", "Use -vo sdl:driver=<driver> instead of -vo sdl -sdl driver.\n",
     CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"noxv", "-noxv has been removed. Use -vo sdl:nohwaccel instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"forcexv", "-forcexv has been removed. Use -vo sdl:forcexv instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    // -ao sdl only:
    {"sdla", "Use -ao sdl:driver instead of -ao sdl -sdla driver.\n",
     CONF_TYPE_PRINT, 0, 0, 0, NULL},

#if defined(CONFIG_FBDEV) || defined(CONFIG_VESA)
    {"monitor-hfreq", &monitor_hfreq_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"monitor-vfreq", &monitor_vfreq_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"monitor-dotclock", &monitor_dotclock_str, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

#ifdef CONFIG_FBDEV
    {"fbmode", &fb_mode_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"fbmodeconfig", &fb_mode_cfgfile, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

    // force window width/height or resolution (with -vm)
    OPT_INTRANGE("x", screen_size_x, 0, 0, 4096),
    OPT_INTRANGE("y", screen_size_y, 0, 0, 4096),
    // set screen dimensions (when not detectable or virtual!=visible)
    OPT_INTRANGE("screenw", vo_screenwidth, CONF_NOSAVE, 0, 4096),
    OPT_INTRANGE("screenh", vo_screenheight, CONF_NOSAVE, 0, 4096),
    // Geometry string
    {"geometry", &vo_geometry, CONF_TYPE_STRING, 0, 0, 0, NULL},
    OPT_MAKE_FLAGS("force-window-position", force_window_position, 0),
    // vo name (X classname) and window title strings
    OPT_STRING("name", vo_winname, 0),
    OPT_STRING("title", vo_wintitle, 0),
    // set aspect ratio of monitor - useful for 16:9 TV-out
    OPT_FLOATRANGE("monitoraspect", force_monitor_aspect, 0, 0.0, 9.0),
    OPT_FLOATRANGE("monitorpixelaspect", monitor_pixel_aspect, 0, 0.2, 9.0),
    // video mode switching: (x11,xv,dga)
    OPT_MAKE_FLAGS("vm", vidmode, 0),
    // start in fullscreen mode:
    OPT_MAKE_FLAGS("fs", fullscreen, CONF_NOSAVE),
    // set fullscreen switch method (workaround for buggy WMs)
    {"fsmode", "-fsmode is obsolete, avoid it and use -fstype instead.\nIf you really want it, try -fsmode-dontuse, but don't report bugs!\n", CONF_TYPE_PRINT, CONF_RANGE, 0, 31, NULL},
    {"fsmode-dontuse", &vo_fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
    // set bpp (x11+vm, dga, fbdev, vesa, svga?)
    OPT_INTRANGE("bpp", vo_dbpp, 0, 0, 32),
    {"colorkey", &vo_colorkey, CONF_TYPE_INT, 0, 0, 0, NULL},
    {"nocolorkey", &vo_colorkey, CONF_TYPE_FLAG, 0, 0, 0x1000000, NULL},
    {"double", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nodouble", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    // wait for v-sync (vesa)
    {"vsync", &vo_vsync, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"novsync", &vo_vsync, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"panscan", &vo_panscan, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
    OPT_FLOATRANGE("panscanrange", vo_panscanrange, 0, -19.0, 99.0),

    {"grabpointer", &vo_grabpointer, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nograbpointer", &vo_grabpointer, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    {"adapter", &vo_adapter_num, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
    {"refreshrate",&vo_refresh_rate,CONF_TYPE_INT,CONF_RANGE, 0,100, NULL},
    {"wid", &WinID, CONF_TYPE_INT64, 0, 0, 0, NULL},
#ifdef CONFIG_X11
    {"icelayer", "-icelayer has been removed. Use -fstype layer:<number> instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"stop-xscreensaver", &stop_xscreensaver, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nostop-xscreensaver", &stop_xscreensaver, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"stop_xscreensaver", "Use -stop-xscreensaver instead, options with _ have been obsoleted.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"fstype", &vo_fstype_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL},
#endif
    {"heartbeat-cmd", &heartbeat_cmd, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"mouseinput", &vo_nomouse_input, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"nomouseinput", &vo_nomouse_input, CONF_TYPE_FLAG,0, 0, 1, NULL},

    {"xineramascreen", &xinerama_screen, CONF_TYPE_INT, CONF_RANGE, -2, 32, NULL},

    OPT_INTRANGE("brightness", vo_gamma_brightness, 0, -100, 100),
    OPT_INTRANGE("saturation", vo_gamma_saturation, 0, -100, 100),
    OPT_INTRANGE("contrast", vo_gamma_contrast, 0, -100, 100),
    OPT_INTRANGE("hue", vo_gamma_hue, 0, -100, 100),
    {"keepaspect", &vo_keepaspect, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nokeepaspect", &vo_keepaspect, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    // direct rendering (decoding to video out buffer)
    {"dr", &vo_directrendering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nodr", &vo_directrendering, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"vaa_dr", "-vaa_dr has been removed, use -dr.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"vaa_nodr", "-vaa_nodr has been removed, use -nodr.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},

#ifdef CONFIG_AA
    // -vo aa
    {"aa*", "-aa* has been removed. Use -vo aa:suboption instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif


//---------------------- mplayer-only options ------------------------

    {"use-filedir-conf", &use_filedir_conf, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nouse-filedir-conf", &use_filedir_conf, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"use-filename-title", &use_filename_title, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nouse-filename-title", &use_filename_title, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
#ifdef CONFIG_CRASH_DEBUG
    {"crash-debug", &crash_debug, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nocrash-debug", &crash_debug, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
#endif
    OPT_INTRANGE("osdlevel", osd_level, 0, 0, 3),
    OPT_INTRANGE("osd-duration", osd_duration, 0, 0, 3600000),
#ifdef CONFIG_MENU
    {"menu", &use_menu, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nomenu", &use_menu, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    {"menu-root", &menu_root, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
    {"menu-cfg", &menu_cfg, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
    {"menu-startup", &menu_startup, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"menu-keepdir", &menu_keepdir, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"menu-chroot", &menu_chroot, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef CONFIG_FRIBIDI
    {"menu-fribidi-charset", &menu_fribidi_charset, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"menu-flip-hebrew", &menu_flip_hebrew, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"menu-noflip-hebrew", &menu_flip_hebrew, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"menu-flip-hebrew-commas", &menu_fribidi_flip_commas, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"menu-noflip-hebrew-commas", &menu_fribidi_flip_commas, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif /* CONFIG_FRIBIDI */
#else
    {"menu", "OSD menu support was not compiled in.\n", CONF_TYPE_PRINT,0, 0, 0, NULL},
#endif /* CONFIG_MENU */

    {"vobsub", &vobsub_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"vobsubid", &vobsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
#ifdef CONFIG_UNRAR_EXEC
    {"unrarexec", &unrar_executable, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif

    {"sstep", &step_sec, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

    {"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2, NULL},
    {"noframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 1, 0, NULL},

    OPT_INTRANGE("autoq", auto_quality, 0, 0, 100),

    OPT_FLAG_ON("benchmark", benchmark, 0),

    // dump some stream out instead of playing the file
    OPT_STRING("dumpfile", stream_dump_name, 0),
    {"dumpaudio", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"dumpvideo", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 2, NULL},
    {"dumpsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 3, NULL},
    {"dumpmpsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 4, NULL},
    {"dumpstream", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 5, NULL},
    {"dumpsrtsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 6, NULL},
    {"dumpmicrodvdsub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 7, NULL},
    {"dumpjacosub", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 8, NULL},
    {"dumpsami", &stream_dump_type, CONF_TYPE_FLAG, 0, 0, 9, NULL},

    OPT_MAKE_FLAGS("capture", capture_dump, 0),

#ifdef CONFIG_LIRC
    {"lircconf", &lirc_configfile, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL},
#endif

    {"leak-report", "", CONF_TYPE_PRINT, 0, 0, 0, (void*)1},
    // these should be removed when gmplayer is forgotten
    {"gui", "Internal GUI was removed. Use some other frontend instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    {"nogui", "Internal GUI was removed, -nogui is no longer valid.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},

    OPT_FLAG_CONSTANTS("noloop", loop_times, 0, 0, -1),
    OPT_INTRANGE("loop", loop_times, 0, -1, 10000),
    {"playlist", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL},

    OPT_MAKE_FLAGS("ordered-chapters", ordered_chapters, 0),
    OPT_INTRANGE("chapter-merge-threshold", chapter_merge_threshold, 0, 0, 10000),

    // a-v sync stuff:
    OPT_MAKE_FLAGS("correct-pts", user_correct_pts, 0),
    OPT_CHOICE("pts-association-mode", user_pts_assoc_mode, 0,
               ({"auto", 0}, {"decoder", 1}, {"sort", 2})),
    OPT_MAKE_FLAGS("initial-audio-sync", initial_audio_sync, 0),
    OPT_CHOICE("hr-seek", hr_seek, 0,
               ({"off", -1}, {"absolute", 0}, {"always", 1}, {"on", 1})),
    OPT_FLAG_CONSTANTS("noautosync", autosync, 0, 0, -1),
    OPT_INTRANGE("autosync", autosync, 0, 0, 10000),

    OPT_FLAG_ON("softsleep", softsleep, 0),
#ifdef HAVE_RTC
    OPT_MAKE_FLAGS("rtc", rtc, 0),
    OPT_STRING("rtc-device", rtc_device, 0),
#endif

    OPT_MAKE_FLAGS("term-osd", term_osd, 0),
    OPT_STRING("term-osd-esc", term_osd_esc, 0),
    OPT_STRING("playing-msg", playing_msg, 0),

    {"slave", &slave_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL},
    OPT_MAKE_FLAGS("idle", player_idle_mode, CONF_GLOBAL),
    {"use-stdin", "-use-stdin has been renamed to -noconsolecontrols, use that instead.", CONF_TYPE_PRINT, 0, 0, 0, NULL},
    OPT_INTRANGE("key-fifo-size", key_fifo_size, CONF_GLOBAL, 2, 65000),
    OPT_MAKE_FLAGS("consolecontrols", consolecontrols, CONF_GLOBAL),
    {"mouse-movements", &enable_mouse_movements, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},
    {"nomouse-movements", &enable_mouse_movements, CONF_TYPE_FLAG, CONF_GLOBAL, 1, 0, NULL},
    OPT_INTRANGE("doubleclick-time", doubleclick_time, 0, 0, 1000),
#ifdef CONFIG_TV
    {"tvscan", (void *) tvscan_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
#else
    {"tvscan", "MPlayer was compiled without TV interface support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif /* CONFIG_TV */

    OPT_FLAG_ON("list-properties", list_properties, CONF_GLOBAL),
    {"identify", &mp_msg_levels[MSGT_IDENTIFY], CONF_TYPE_FLAG, CONF_GLOBAL, 0, MSGL_V, NULL},
    {"-help", (void *) help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
    {"help", (void *) help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
    {"h", (void *) help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},

    {"vd", (void *) vd_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#endif /* MPLAYER_CFG_MPLAYER_H */
