#ifndef DEC_AUDIO_H
#define DEC_AUDIO_H

// dec_audio.c:
extern void afm_help(void);
extern int init_best_audio_codec(sh_audio_t *sh_audio,char** audio_codec_list,char** audio_fm_list);
extern int decode_audio(sh_audio_t *sh_audio, int minlen);
extern void resync_audio_stream(sh_audio_t *sh_audio);
extern void skip_audio_frame(sh_audio_t *sh_audio);
extern void uninit_audio(sh_audio_t *sh_audio);

extern int init_audio_filters(sh_audio_t *sh_audio, int in_samplerate,
		int *out_samplerate, int *out_channels, int *out_format);

#endif /* DEC_AUDIO_H */
