
// dec_video.c:
extern int video_read_properties(sh_video_t *sh_video);

//extern int init_video(sh_video_t *sh_video, int *pitches);
extern int init_video(sh_video_t *sh_video,char* codecname,int vfm,int status);
extern void uninit_video(sh_video_t *sh_video);

#ifdef USE_LIBVO2
extern int decode_video(vo2_handle_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame);
#else
extern int decode_video(vo_functions_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame);
#endif

extern int get_video_quality_max(sh_video_t *sh_video);
extern void set_video_quality(sh_video_t *sh_video,int quality);

extern int set_video_colors(sh_video_t *sh_video,char *item,int value);

extern int divx_quality;
