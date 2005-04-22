
#ifndef __MPAE_H__
#define __MPAE_H__

#define ACODEC_COPY 0
#define ACODEC_PCM 1
#define ACODEC_VBRMP3 2
#define ACODEC_NULL 3
#define ACODEC_LAVC 4
#define ACODEC_TOOLAME 5

#define AE_NEEDS_COMPRESSED_INPUT 1

typedef struct {
	int channels;
	int sample_rate;
	int bitrate;
	int samples_per_frame;
	int audio_preload;
} audio_encoding_params_t;

typedef struct {
	int codec;
	int flags;
	muxer_stream_t *stream;
	audio_encoding_params_t params;
	int audio_preload;	//in ms
	int input_format;
	int min_buffer_size, max_buffer_size;	//for init_audio_filters
	int *decode_buffer;
	int decode_buffer_size;
	int decode_buffer_len;
	void *priv;
	int (*bind)(void*, muxer_stream_t*);
	int (*get_frame_size)(void*);
	int (*set_decoded_len)(void *encoder, int len);
	int (*encode)(void *encoder, uint8_t *dest, void *src, int nsamples, int max_size);
	int (*fixup)();
	int (*close)();
} audio_encoder_t;

audio_encoder_t *new_audio_encoder(muxer_stream_t *stream, audio_encoding_params_t *params);

#endif
