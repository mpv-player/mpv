
// Win32 VfW/ACM interface:

extern char* win32_codec_name;  // must be set before calling DrvOpen() !!!

int init_acm_audio_codec(sh_audio_t *sh_audio);
int close_acm_audio_codec(sh_audio_t *sh_audio);
int acm_decode_audio(sh_audio_t *sh_audio, void* a_buffer,int minlen,int maxlen);

int init_vfw_video_codec(sh_video_t *sh_video,int ex);
int vfw_close_video_codec(sh_video_t *sh_video, int ex);
int vfw_decode_video(sh_video_t* sh_video,void* start,int in_size,int drop_frame,int ex);
int vfw_set_postproc(sh_video_t* sh_video,int quality);

BITMAPINFOHEADER* vfw_open_encoder(char *dll_name, BITMAPINFOHEADER *input_bih,unsigned int out_fourcc);
int vfw_start_encoder(BITMAPINFOHEADER *input_bih, BITMAPINFOHEADER *output_bih);
int vfw_encode_frame(BITMAPINFOHEADER* biOutput,void* OutBuf,
		     BITMAPINFOHEADER* biInput,void* Image,
		     long* keyframe, int quality);
