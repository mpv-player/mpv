extern double sub_last_pts;
#ifdef USE_ASS
extern ass_track_t *ass_track;
#endif
extern sub_data *subdata;
extern subtitle *vo_sub_last;
void update_subtitles(sh_video_t *sh_video, demux_stream_t *d_dvdsub, int reset);
