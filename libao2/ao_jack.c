/*
 * JACK audio output driver for MPlayer
 *
 * Copyleft 2001 by Felix Bünemann (atmosfear@users.sf.net)
 * and Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "osdep/timer.h"
#include "subopt-helper.h"

#include "libvo/fastmemcpy.h"

#include <jack/jack.h>

static ao_info_t info = 
{
  "JACK audio output",
  "jack",
  "Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>",
  "based on ao_sdl.c"
};

LIBAO_EXTERN(jack)

//! maximum number of channels supported, avoids lots of mallocs
#define MAX_CHANS 6
static jack_port_t *ports[MAX_CHANS];
static int num_ports; ///< Number of used ports == number of channels
static jack_client_t *client;
static float jack_latency;
static int estimate;
static volatile int paused = 0; ///< set if paused
static volatile int underrun = 0; ///< signals if an underrun occured

static volatile float callback_interval = 0;
static volatile float callback_time = 0;

//! size of one chunk, if this is too small MPlayer will start to "stutter"
//! after a short time of playback
#define CHUNK_SIZE (16 * 1024)
//! number of "virtual" chunks the buffer consists of
#define NUM_CHUNKS 8
// This type of ring buffer may never fill up completely, at least
// one byte must always be unused.
// For performance reasons (alignment etc.) one whole chunk always stays
// empty, not only one byte.
#define BUFFSIZE ((NUM_CHUNKS + 1) * CHUNK_SIZE)

//! buffer for audio data
static unsigned char *buffer = NULL;

//! buffer read position, may only be modified by playback thread or while it is stopped
static volatile int read_pos;
//! buffer write position, may only be modified by MPlayer's thread
static volatile int write_pos;

/**
 * \brief get the number of free bytes in the buffer
 * \return number of free bytes in buffer
 * 
 * may only be called by MPlayer's thread
 * return value may change between immediately following two calls,
 * and the real number of free bytes might be larger!
 */
static int buf_free(void) {
  int free = read_pos - write_pos - CHUNK_SIZE;
  if (free < 0) free += BUFFSIZE;
  return free;
}

/**
 * \brief get amount of data available in the buffer
 * \return number of bytes available in buffer
 *
 * may only be called by the playback thread
 * return value may change between immediately following two calls,
 * and the real number of buffered bytes might be larger!
 */
static int buf_used(void) {
  int used = write_pos - read_pos;
  if (used < 0) used += BUFFSIZE;
  return used;
}

/**
 * \brief insert len bytes into buffer
 * \param data data to insert
 * \param len length of data
 * \return number of bytes inserted into buffer
 *
 * If there is not enough room, the buffer is filled up
 */
static int write_buffer(unsigned char* data, int len) {
  int first_len = BUFFSIZE - write_pos;
  int free = buf_free();
  if (len > free) len = free;
  if (first_len > len) first_len = len;
  // till end of buffer
  fast_memcpy (&buffer[write_pos], data, first_len);
  if (len > first_len) { // we have to wrap around
    // remaining part from beginning of buffer
    fast_memcpy (buffer, &data[first_len], len - first_len);
  }
  write_pos = (write_pos + len) % BUFFSIZE;
  return len;
}

static void silence(float **bufs, int cnt, int num_bufs);

/**
 * \brief read data from buffer and splitting it into channels
 * \param bufs num_bufs float buffers, each will contain the data of one channel
 * \param cnt number of samples to read per channel
 * \param num_bufs number of channels to split the data into
 * \return number of samples read per channel, equals cnt unless there was too
 *         little data in the buffer
 *
 * Assumes the data in the buffer is of type float, the number of bytes
 * read is res * num_bufs * sizeof(float), where res is the return value.
 * If there is not enough data in the buffer remaining parts will be filled
 * with silence.
 */
static int read_buffer(float **bufs, int cnt, int num_bufs) {
  int buffered = buf_used();
  int i, j;
  if (cnt * sizeof(float) * num_bufs > buffered) {
    silence(bufs, cnt, num_bufs);
    cnt = buffered / sizeof(float) / num_bufs;
  }
  for (i = 0; i < cnt; i++) {
    for (j = 0; j < num_bufs; j++) {
      bufs[j][i] = *(float *)&buffer[read_pos];
      read_pos = (read_pos + sizeof(float)) % BUFFSIZE;
    }
  }
  return cnt;
}

// end ring buffer stuff

static int control(int cmd, void *arg) {
  return CONTROL_UNKNOWN;
}

/**
 * \brief fill the buffers with silence
 * \param bufs num_bufs float buffers, each will contain the data of one channel
 * \param cnt number of samples in each buffer
 * \param num_bufs number of buffers
 */
static void silence(float **bufs, int cnt, int num_bufs) {
  int i;
  for (i = 0; i < num_bufs; i++)
    memset(bufs[i], 0, cnt * sizeof(float));
}

/**
 * \brief JACK Callback function
 * \param nframes number of frames to fill into buffers
 * \param arg unused
 * \return currently always 0
 *
 * Write silence into buffers if paused or an underrun occured
 */
static int outputaudio(jack_nframes_t nframes, void *arg) {
  float *bufs[MAX_CHANS];
  int i;
  for (i = 0; i < num_ports; i++)
    bufs[i] = jack_port_get_buffer(ports[i], nframes);
  if (paused || underrun)
    silence(bufs, nframes, num_ports);
  else
    if (read_buffer(bufs, nframes, num_ports) < nframes)
      underrun = 1;
  if (estimate) {
    float now = (float)GetTimer() / 1000000.0;
    float diff = callback_time + callback_interval - now;
    if ((diff > -0.002) && (diff < 0.002))
      callback_time += callback_interval;
    else
      callback_time = now;
    callback_interval = (float)nframes / (float)ao_data.samplerate;
  }
  return 0;
}

/**
 * \brief print suboption usage help
 */
static void print_help (void)
{
  mp_msg (MSGT_AO, MSGL_FATAL,
           "\n-ao jack commandline help:\n"
           "Example: mplayer -ao jack:port=myout\n"
           "  connects MPlayer to the jack ports named myout\n"
           "\nOptions:\n"
           "  port=<port name>\n"
           "    Connects to the given ports instead of the default physical ones\n"
           "  name=<client name>\n"
           "    Client name to pass to JACK\n"
           "  estimate\n"
           "    Estimates the amount of data in buffers (experimental)\n"
           "  autostart\n"
           "    Automatically start JACK server if necessary\n"
         );
}

static int init(int rate, int channels, int format, int flags) {
  const char **matching_ports = NULL;
  char *port_name = NULL;
  char *client_name = NULL;
  int autostart = 0;
  opt_t subopts[] = {
    {"port", OPT_ARG_MSTRZ, &port_name, NULL},
    {"name", OPT_ARG_MSTRZ, &client_name, NULL},
    {"estimate", OPT_ARG_BOOL, &estimate, NULL},
    {"autostart", OPT_ARG_BOOL, &autostart, NULL},
    {NULL}
  };
  jack_options_t open_options = JackUseExactName;
  int port_flags = JackPortIsInput;
  int i;
  estimate = 1;
  if (subopt_parse(ao_subdevice, subopts) != 0) {
    print_help();
    return 0;
  }
  if (channels > MAX_CHANS) {
    mp_msg(MSGT_AO, MSGL_FATAL, "[JACK] Invalid number of channels: %i\n", channels);
    goto err_out;
  }
  if (!client_name) {
    client_name = malloc(40);
    sprintf(client_name, "MPlayer [%d]", getpid());
  }
  if (!autostart)
    open_options |= JackNoStartServer;
  client = jack_client_open(client_name, open_options, NULL);
  if (!client) {
    mp_msg(MSGT_AO, MSGL_FATAL, "[JACK] cannot open server\n");
    goto err_out;
  }
  reset();
  jack_set_process_callback(client, outputaudio, 0);

  // list matching ports
  if (!port_name)
    port_flags |= JackPortIsPhysical;
  matching_ports = jack_get_ports(client, port_name, NULL, port_flags);
  if (!matching_ports || !matching_ports[0]) {
    mp_msg(MSGT_AO, MSGL_FATAL, "[JACK] no physical ports available\n");
    goto err_out;
  }
  i = 1;
  while (matching_ports[i]) i++;
  if (channels > i) channels = i;
  num_ports = channels;

  // create out output ports
  for (i = 0; i < num_ports; i++) {
    char pname[30];
    snprintf(pname, 30, "out_%d", i);
    ports[i] = jack_port_register(client, pname, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (!ports[i]) {
      mp_msg(MSGT_AO, MSGL_FATAL, "[JACK] not enough ports available\n");
      goto err_out;
    }
  }
  if (jack_activate(client)) {
    mp_msg(MSGT_AO, MSGL_FATAL, "[JACK] activate failed\n");
    goto err_out;
  }
  for (i = 0; i < num_ports; i++) {
    if (jack_connect(client, jack_port_name(ports[i]), matching_ports[i])) {
      mp_msg(MSGT_AO, MSGL_FATAL, "[JACK] connecting failed\n");
      goto err_out;
    }
  }
  rate = jack_get_sample_rate(client);
  jack_latency = (float)(jack_port_get_total_latency(client, ports[0]) +
                         jack_get_buffer_size(client)) / (float)rate;
  callback_interval = 0;
  buffer = malloc(BUFFSIZE);

  ao_data.channels = channels;
  ao_data.samplerate = rate;
  ao_data.format = AF_FORMAT_FLOAT_NE;
  ao_data.bps = channels * rate * sizeof(float);
  ao_data.buffersize = CHUNK_SIZE * NUM_CHUNKS;
  ao_data.outburst = CHUNK_SIZE;
  free(matching_ports);
  free(port_name);
  free(client_name);
  return 1;

err_out:
  free(matching_ports);
  free(port_name);
  free(client_name);
  if (client)
    jack_client_close(client);
  free(buffer);
  buffer = NULL;
  return 0;
}

// close audio device
static void uninit(int immed) {
  if (!immed)
    usec_sleep(get_delay() * 1000 * 1000);
  // HACK, make sure jack doesn't loop-output dirty buffers
  reset();
  usec_sleep(100 * 1000);
  jack_client_close(client);
  free(buffer);
  buffer = NULL;
}

/**
 * \brief stop playing and empty buffers (for seeking/pause)
 */
static void reset(void) {
  paused = 1;
  read_pos = 0;
  write_pos = 0;
  paused = 0;
}

/**
 * \brief stop playing, keep buffers (for pause)
 */
static void audio_pause(void) {
  paused = 1;
}

/**
 * \brief resume playing, after audio_pause()
 */
static void audio_resume(void) {
  paused = 0;
}

static int get_space(void) {
  return buf_free();
}

/**
 * \brief write data into buffer and reset underrun flag
 */
static int play(void *data, int len, int flags) {
  if (!(flags & AOPLAY_FINAL_CHUNK))
    len -= len % ao_data.outburst;
  underrun = 0;
  return write_buffer(data, len);
}

static float get_delay(void) {
  int buffered = BUFFSIZE - CHUNK_SIZE - buf_free(); // could be less
  float in_jack = jack_latency;
  if (estimate && callback_interval > 0) {
    float elapsed = (float)GetTimer() / 1000000.0 - callback_time;
    in_jack += callback_interval - elapsed;
    if (in_jack < 0) in_jack = 0;
  }
  return (float)buffered / (float)ao_data.bps + in_jack;
}

