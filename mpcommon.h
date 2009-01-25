#ifndef MPLAYER_MPCOMMON_H
#define MPLAYER_MPCOMMON_H

#include "subreader.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

extern double sub_last_pts;
extern struct ass_track_s *ass_track;
extern subtitle *vo_sub_last;

void print_version(const char* name);
void update_subtitles(sh_video_t *sh_video, demux_stream_t *d_dvdsub, int reset);
void update_teletext(sh_video_t *sh_video, demuxer_t *demuxer, int reset);
int select_audio(demuxer_t* demuxer, int audio_id, char* audio_lang);

extern int disable_system_conf;
extern int disable_user_conf;

#endif /* MPLAYER_MPCOMMON_H */
