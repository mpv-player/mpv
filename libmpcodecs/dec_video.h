
// dec_video.c:
extern int video_read_properties(sh_video_t *sh_video);

extern void vfm_help();

//extern int init_video(sh_video_t *sh_video, int *pitches);
extern int init_video(sh_video_t *sh_video,char* codecname,char* vfm,int status);
extern void uninit_video(sh_video_t *sh_video);

extern int decode_video(sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame);

extern int get_video_quality_max(sh_video_t *sh_video);
extern void set_video_quality(sh_video_t *sh_video,int quality);

int get_video_colors(sh_video_t *sh_video,char *item,int *value);
extern int set_video_colors(sh_video_t *sh_video,char *item,int value);
extern int set_rectangle(sh_video_t *sh_video,int param,int value);

extern int divx_quality;
