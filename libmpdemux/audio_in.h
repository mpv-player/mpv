#ifndef _audio_in_h 
#define _audio_in_h 

#define AUDIO_IN_ALSA 1
#define AUDIO_IN_OSS 2

#include "config.h"

#ifdef HAVE_ALSA9
#include <alsa/asoundlib.h>

typedef struct {
    char *device;

    snd_pcm_t *handle;
    snd_output_t *log;
    int buffer_time, period_time, chunk_size;
    size_t bits_per_sample, bits_per_frame;
} ai_alsa_t;
#endif

typedef struct {
    char *device;

    int audio_fd;
} ai_oss_t;

typedef struct 
{
    int type;
    int setup;
    
    /* requested values */
    int req_channels;
    int req_samplerate;

    /* real values read-only */
    int channels;
    int samplerate;
    int blocksize;
    int bytes_per_sample;
    int samplesize;
    
#ifdef HAVE_ALSA9
    ai_alsa_t alsa;
#endif
    ai_oss_t oss;
} audio_in_t;

int audio_in_init(audio_in_t *ai, int type);
int audio_in_setup(audio_in_t *ai);
int audio_in_set_device(audio_in_t *ai, char *device);
int audio_in_set_samplerate(audio_in_t *ai, int rate);
int audio_in_set_channels(audio_in_t *ai, int channels);
int audio_in_uninit(audio_in_t *ai);
int audio_in_start_capture(audio_in_t *ai);
int audio_in_read_chunk(audio_in_t *ai, unsigned char *buffer);

#ifdef HAVE_ALSA9
int ai_alsa_setup(audio_in_t *ai);
int ai_alsa_init(audio_in_t *ai);
#endif

int ai_oss_set_samplerate(audio_in_t *ai);
int ai_oss_set_channels(audio_in_t *ai);
int ai_oss_init(audio_in_t *ai);

#endif /* _audio_in_h */
