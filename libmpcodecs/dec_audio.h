#ifndef MPLAYER_DEC_AUDIO_H
#define MPLAYER_DEC_AUDIO_H

#include "libmpdemux/stheader.h"

// dec_audio.c:
void afm_help(void);
int init_best_audio_codec(sh_audio_t *sh_audio, char** audio_codec_list, char** audio_fm_list);
int decode_audio(sh_audio_t *sh_audio, int minlen);
void resync_audio_stream(sh_audio_t *sh_audio);
void skip_audio_frame(sh_audio_t *sh_audio);
void uninit_audio(sh_audio_t *sh_audio);

int init_audio_filters(sh_audio_t *sh_audio, int in_samplerate,
                       int *out_samplerate, int *out_channels, int *out_format);

#endif /* MPLAYER_DEC_AUDIO_H */
