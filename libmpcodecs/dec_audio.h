
// dec_audio.c:
extern void afm_help();
//extern int init_best_audio_codec(sh_audio_t *sh_audio,char* audio_codec,char* audio_fm);
extern int init_audio_codec(sh_audio_t *sh_audio);
extern int init_audio(sh_audio_t *sh_audio,char* codecname,char* afm,int status);
extern int init_best_audio_codec(sh_audio_t *sh_audio,char** audio_codec_list,char** audio_fm_list);
extern int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen);
extern void resync_audio_stream(sh_audio_t *sh_audio);
extern void skip_audio_frame(sh_audio_t *sh_audio);
extern void uninit_audio(sh_audio_t *sh_audio);
