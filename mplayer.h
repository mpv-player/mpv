
#ifndef __MPLAYER_MAIN
#define __MPLAYER_MAIN

extern int divx_quality;

extern char* filename;
extern int i;
extern int seek_to_sec;
extern int seek_to_byte;
extern int f; // filedes
extern int stream_type;
extern stream_t* stream;
extern int file_format;
extern int has_audio;
//int has_video=1;
//
extern int audio_format;
#ifdef ALSA_TIMER
extern int alsa;
#else
extern int alsa;
#endif
extern int audio_buffer_size;
extern int audio_id;
extern int video_id;
extern int dvdsub_id;
extern float default_max_pts_correction;
extern int delay_corrected;
extern float force_fps;
extern float audio_delay;
extern int vcd_track;
#ifdef VCD_CACHE
extern int vcd_cache_size;
#endif
extern int index_mode;  // -1=untouched  0=don't use index  1=use (geneate) index
#ifdef AVI_SYNC_BPS
extern int pts_from_bps;
#else
extern int pts_from_bps;
#endif
extern char* title;
// screen info:
extern char* video_driver; //"mga"; // default
vo_functions_t *video_out;
extern int fullscreen;
extern int vidmode;
extern int softzoom;
extern int screen_size_x;//SCREEN_SIZE_X;
extern int screen_size_y;//SCREEN_SIZE_Y;
extern int screen_size_xy;
// movie info:
extern int out_fmt;
extern char *dsp;
extern int force_ni;
extern char *conffile;
extern int conffile_fd;
extern char *font_name;
extern float font_factor;
extern char *sub_name;
extern float sub_delay;
extern float sub_fps;
extern int   sub_auto;
extern char *stream_dump_name;
extern int stream_dump_type;
//int user_bpp=0;

extern int verbose;

extern int osd_level;
extern int nogui;

extern int rel_seek_secs;

extern int osd_visible;
extern int osd_function;
extern int osd_last_pts;

extern int mplayer(int argc,char* argv[], char *envp[]);
extern void parse_cfgfiles( void );
extern void exit_player(char* how);

#endif