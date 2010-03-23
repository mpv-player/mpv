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

#ifndef MPLAYER_MPCOMMON_H
#define MPLAYER_MPCOMMON_H

#include "subreader.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libass/ass_mp.h"

extern double sub_last_pts;
extern ass_track_t *ass_track;
extern subtitle *vo_sub_last;

void print_version(const char* name);
void update_subtitles(sh_video_t *sh_video, double refpts, demux_stream_t *d_dvdsub, int reset);
void update_teletext(sh_video_t *sh_video, demuxer_t *demuxer, int reset);
int select_audio(demuxer_t* demuxer, int audio_id, char* audio_lang);
void set_osd_subtitle(subtitle *subs);

extern int disable_system_conf;
extern int disable_user_conf;

#endif /* MPLAYER_MPCOMMON_H */
