#ifndef MPLAYER_AE_H
#define MPLAYER_AE_H

#include "libmpdemux/muxer.h"

#define ACODEC_COPY 0
#define ACODEC_PCM 1
#define ACODEC_VBRMP3 2
#define ACODEC_NULL 3
#define ACODEC_LAVC 4
#define ACODEC_TOOLAME 5
#define ACODEC_FAAC 6
#define ACODEC_TWOLAME 7

#define AE_NEEDS_COMPRESSED_INPUT 1

typedef struct {
	int channels;
	int sample_rate;
	int bitrate;
	int samples_per_frame;
	int audio_preload;
} audio_encoding_params_t;

typedef struct audio_encoder_s {
	int codec;
	int flags;
	muxer_stream_t *stream;
	audio_encoding_params_t params;
	int audio_preload;	//in ms
	int input_format;
	int min_buffer_size, max_buffer_size;	//for init_audio_filters
	unsigned char *decode_buffer;
	int decode_buffer_size;
	int decode_buffer_len;
	void *priv;
	int (*bind)(struct audio_encoder_s*, muxer_stream_t*);
	int (*get_frame_size)(struct audio_encoder_s*);
	int (*set_decoded_len)(struct audio_encoder_s *encoder, int len);
	int (*encode)(struct audio_encoder_s *encoder, uint8_t *dest, void *src, int nsamples, int max_size);
	void (*fixup)(struct audio_encoder_s *encoder);
	int (*close)(struct audio_encoder_s *encoder);
} audio_encoder_t;

audio_encoder_t *new_audio_encoder(muxer_stream_t *stream, audio_encoding_params_t *params);

#endif /* MPLAYER_AE_H */
