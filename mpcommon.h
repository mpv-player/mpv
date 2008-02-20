#ifndef MPCOMMON_H
#define MPCOMMON_H

extern double sub_last_pts;
extern ass_track_t *ass_track;
extern subtitle *vo_sub_last;
void update_subtitles(sh_video_t *sh_video, demux_stream_t *d_dvdsub, int reset);
void update_teletext(sh_video_t *sh_video, demuxer_t *demuxer, int reset);

#endif /* MPCOMMON_H */
