
#ifndef __MPLAYER_MAIN
#define __MPLAYER_MAIN

extern char* current_module;

extern char * dvd_device;
extern char * cdrom_device;

extern char ** audio_fm_list;
extern char ** video_fm_list;
extern char ** video_driver_list;
extern char ** audio_driver_list;
extern char * video_driver;
extern char * audio_driver;
extern float  audio_delay;

extern int osd_level;
extern unsigned int osd_visible;

extern char * font_name;
extern float  font_factor;
extern float movie_aspect;
extern float force_fps;

//extern char **sub_name;
extern float  sub_delay;
extern float  sub_fps;
extern int    sub_auto;
extern int    sub_pos;
extern int    sub_unicode;
extern char * sub_cp;
extern int    suboverlap_enabled;

extern char * filename;

extern int stream_cache_size;
extern int force_ni;
extern int index_mode;
extern int autosync;

// libmpcodecs:
extern int fullscreen;
extern int flip;

extern int frame_dropping;

extern int auto_quality;

extern int audio_id;
extern int video_id;
extern int dvdsub_id;
extern int vobsub_id;

extern void exit_player(const char* how);
extern void update_set_of_subtitles(void);

#endif
