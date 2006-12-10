/* 
 * ao_openal.c - OpenAL audio output driver for MPlayer
 *
 * This driver is under the same license as MPlayer.
 * (http://www.mplayerhq.hu)
 *
 * Copyleft 2006 by Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#ifdef OPENAL_AL_H
#include <OpenAL/alc.h>
#include <OpenAL/al.h>
#else
#include <AL/alc.h>
#include <AL/al.h>
#endif

#include "mp_msg.h"
#include "help_mp.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "osdep/timer.h"
#include "subopt-helper.h"

static ao_info_t info = 
{
  "OpenAL audio output",
  "openal",
  "Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>",
  ""
};

LIBAO_EXTERN(openal)

#define MAX_CHANS 6
#define NUM_BUF 128
#define CHUNK_SIZE 512
static ALuint buffers[MAX_CHANS][NUM_BUF];
static ALuint sources[MAX_CHANS];

static int cur_buf[MAX_CHANS];
static int unqueue_buf[MAX_CHANS];
static int16_t *tmpbuf;


static int control(int cmd, void *arg) {
  switch (cmd) {
    case AOCONTROL_GET_VOLUME:
    case AOCONTROL_SET_VOLUME: {
      ALfloat volume;
      ao_control_vol_t *vol = (ao_control_vol_t *)arg;
      if (cmd == AOCONTROL_SET_VOLUME) {
        volume = (vol->left + vol->right) / 200.0;
        alListenerf(AL_GAIN, volume);
      }
      alGetListenerf(AL_GAIN, &volume);
      vol->left = vol->right = volume * 100;
      return CONTROL_TRUE;
    }
  }
  return CONTROL_UNKNOWN;
}

/**
 * \brief print suboption usage help
 */
static void print_help(void) {
  mp_msg(MSGT_AO, MSGL_FATAL,
          "\n-ao openal commandline help:\n"
          "Example: mplayer -ao openal\n"
          "\nOptions:\n"
        );
}

static int init(int rate, int channels, int format, int flags) {
  float position[3] = {0, 0, 0};
  float direction[6] = {0, 0, 1, 0, -1, 0};
  float sppos[6][3] = {
    {-1, 0, 0.5}, {1, 0, 0.5},
    {-1, 0,  -1}, {1, 0,  -1},
    {0,  0,   1}, {0, 0, 0.1},
  };
  ALCdevice *dev = NULL;
  ALCcontext *ctx = NULL;
  ALCint freq = 0;
  ALCint attribs[] = {ALC_FREQUENCY, rate, 0, 0};
  int i;
  opt_t subopts[] = {
    {NULL}
  };
  if (subopt_parse(ao_subdevice, subopts) != 0) {
    print_help();
    return 0;
  }
  if (channels > MAX_CHANS) {
    mp_msg(MSGT_AO, MSGL_FATAL, "[OpenAL] Invalid number of channels: %i\n", channels);
    goto err_out;
  }
  dev = alcOpenDevice(NULL);
  if (!dev) {
    mp_msg(MSGT_AO, MSGL_FATAL, "[OpenAL] could not open device\n");
    goto err_out;
  }
  ctx = alcCreateContext(dev, attribs);
  alcMakeContextCurrent(ctx);
  alListenerfv(AL_POSITION, position);
  alListenerfv(AL_ORIENTATION, direction);
  alGenSources(channels, sources);
  for (i = 0; i < channels; i++) {
    cur_buf[i] = 0;
    unqueue_buf[i] = 0;
    alGenBuffers(NUM_BUF, buffers[i]);
    alSourcefv(sources[i], AL_POSITION, sppos[i]);
    alSource3f(sources[i], AL_VELOCITY, 0, 0, 0);
  }
  if (channels == 1)
    alSource3f(sources[0], AL_POSITION, 0, 0, 1);
  ao_data.channels = channels;
  alcGetIntegerv(dev, ALC_FREQUENCY, 1, &freq);
  if (alcGetError(dev) == ALC_NO_ERROR && freq)
    rate = freq;
  ao_data.samplerate = rate;
  ao_data.format = AF_FORMAT_S16_NE;
  ao_data.bps = channels * rate * 2;
  ao_data.buffersize = CHUNK_SIZE * NUM_BUF;
  ao_data.outburst = channels * CHUNK_SIZE;
  tmpbuf = malloc(CHUNK_SIZE);
  return 1;

err_out:
  return 0;
}

// close audio device
static void uninit(int immed) {
  ALCcontext *ctx = alcGetCurrentContext();
  ALCdevice *dev = alcGetContextsDevice(ctx);
  free(tmpbuf);
  if (!immed) {
    ALint state;
    alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
    while (state == AL_PLAYING) {
      usec_sleep(10000);
      alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
    }
  }
  reset();
  alcMakeContextCurrent(NULL);
  alcDestroyContext(ctx);
  alcCloseDevice(dev);
}

static void unqueue_buffers(void) {
  ALint p;
  int s, i;
  for (s = 0;  s < ao_data.channels; s++) {
    alGetSourcei(sources[s], AL_BUFFERS_PROCESSED, &p);
    for (i = 0; i < p; i++) {
      alSourceUnqueueBuffers(sources[s], 1, &buffers[s][unqueue_buf[s]]);
      unqueue_buf[s] = (unqueue_buf[s] + 1) % NUM_BUF;
    }
  }
}

/**
 * \brief stop playing and empty buffers (for seeking/pause)
 */
static void reset(void) {
  alSourceRewindv(ao_data.channels, sources);
  unqueue_buffers();
}

/**
 * \brief stop playing, keep buffers (for pause)
 */
static void audio_pause(void) {
  alSourcePausev(ao_data.channels, sources);
}

/**
 * \brief resume playing, after audio_pause()
 */
static void audio_resume(void) {
  alSourcePlayv(ao_data.channels, sources);
}

static int get_space(void) {
  ALint queued;
  unqueue_buffers();
  alGetSourcei(sources[0], AL_BUFFERS_QUEUED, &queued);
  return (NUM_BUF - queued) * CHUNK_SIZE * ao_data.channels;
}

/**
 * \brief write data into buffer and reset underrun flag
 */
static int play(void *data, int len, int flags) {
  ALint state;
  int i, j, k;
  int ch;
  int16_t *d = data;
  len /= ao_data.outburst;
  for (i = 0; i < len; i++) {
    for (ch = 0; ch < ao_data.channels; ch++) {
      for (j = 0, k = ch; j < CHUNK_SIZE / 2; j++, k += ao_data.channels)
        tmpbuf[j] = d[k];
      alBufferData(buffers[ch][cur_buf[ch]], AL_FORMAT_MONO16, tmpbuf,
                     CHUNK_SIZE, ao_data.samplerate);
      alSourceQueueBuffers(sources[ch], 1, &buffers[ch][cur_buf[ch]]);
      cur_buf[ch] = (cur_buf[ch] + 1) % NUM_BUF;
    }
    d += ao_data.channels * CHUNK_SIZE / 2;
  }
  alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
  if (state != AL_PLAYING) // checked here in case of an underrun
    alSourcePlayv(ao_data.channels, sources);
  return len * ao_data.outburst;
}

static float get_delay(void) {
  ALint queued;
  unqueue_buffers();
  alGetSourcei(sources[0], AL_BUFFERS_QUEUED, &queued);
  return queued * CHUNK_SIZE / 2 / (float)ao_data.samplerate;
}

