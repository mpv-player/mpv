
// dec_video.c:
extern int video_read_properties(sh_video_t *sh_video);

extern void vfm_help(void);

extern int init_best_video_codec(sh_video_t *sh_video,char** video_codec_list,char** video_fm_list);

//extern int init_video(sh_video_t *sh_video, int *pitches);
extern int init_video(sh_video_t *sh_video,char* codecname,char* vfm,int status);
extern void uninit_video(sh_video_t *sh_video);

extern void *decode_video(sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame, double pts);
extern int filter_video(sh_video_t *sh_video, void *frame, double pts);

extern int get_video_quality_max(sh_video_t *sh_video);
extern void set_video_quality(sh_video_t *sh_video,int quality);

extern int get_video_colors(sh_video_t *sh_video,char *item,int *value);
extern int set_video_colors(sh_video_t *sh_video,char *item,int value);
extern int set_rectangle(sh_video_t *sh_video,int param,int value);
extern void resync_video_stream(sh_video_t *sh_video);
extern int get_current_video_decoder_lag(sh_video_t *sh_video);

extern int divx_quality;
