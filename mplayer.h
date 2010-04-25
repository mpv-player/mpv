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

#ifndef MPLAYER_MPLAYER_H
#define MPLAYER_MPLAYER_H

#include "mp_msg.h"

extern char* current_module;

extern char ** audio_fm_list;
extern char ** video_fm_list;
extern char ** video_driver_list;
extern char ** audio_driver_list;
extern char * video_driver;
extern char * audio_driver;
extern float  audio_delay;

extern unsigned int osd_visible;

extern char * font_name;
extern char * sub_font_name;
extern float  font_factor;
extern double force_fps;

//extern char **sub_name;
extern float  sub_delay;
extern float  sub_fps;
extern int    sub_auto;

extern int stream_cache_size;
extern int autosync;

extern int frame_dropping;

extern int auto_quality;

extern int vobsub_id;

static inline void exit_player_bad(const char *how)
{
    if (how)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Deprecated exit call: %s", how);
    exit(1);
}

struct MPContext;

#endif /* MPLAYER_MPLAYER_H */
