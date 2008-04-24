
#ifndef MPLAYER_MPLAYER_H
#define MPLAYER_MPLAYER_H

#include "mp_msg.h"

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
extern char * sub_font_name;
extern float  font_factor;
extern float movie_aspect;
extern double force_fps;

//extern char **sub_name;
extern float  sub_delay;
extern float  sub_fps;
extern int    sub_auto;

extern char * filename;

extern int stream_cache_size;
extern int autosync;

extern int frame_dropping;

extern int auto_quality;

extern int vobsub_id;

static inline void exit_player(const char *how)
{
    if (how)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Deprecated exit call: %s", how);
    exit(1);
}

struct MPContext;

extern void update_set_of_subtitles(struct MPContext *mpctx);

#endif /* MPLAYER_MPLAYER_H */
