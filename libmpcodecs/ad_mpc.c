/**
 * Musepack audio files decoder for MPlayer
 * by Reza Jelveh <reza.jelveh@tuhh.de> and
 * Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>
 * License: GPL
 * This code may be be relicensed under the terms of the GNU LGPL when it
 * becomes part of the FFmpeg project (ffmpeg.org)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"
#include "libaf/af_format.h"
#include "libvo/fastmemcpy.h"

static ad_info_t info = 
{
	"Musepack audio decoder",
	"mpcdec",
	"Reza Jelveh and Reimar Döffinger",
	"",
	""
};

LIBAD_EXTERN(libmusepack)

#include <mpcdec/mpcdec.h>

// BUFFER_LENGTH is in MPC_SAMPLE_FORMAT units
#define MAX_FRAMESIZE (4 * MPC_DECODER_BUFFER_LENGTH)
//! this many frames should decode good after seeking
#define MIN_SEEK_GOOD 5
//! how many frames to discard at most after seeking
#define MAX_SEEK_DISCARD 50

typedef struct context_s {
  char *header;
  int header_len;
  sh_audio_t *sh;
  uint32_t pos;
  mpc_decoder decoder;
} context_t;

/**
 * \brief mpc_reader callback function for reading the header
 */
static mpc_int32_t cb_read(void *data, void *buf, mpc_int32_t size) {
  context_t *d = (context_t *)data;
  char *p = (char *)buf;
  int s = size;
  if (d->pos < d->header_len) {
    if (s > d->header_len - d->pos)
      s = d->header_len - d->pos;
    fast_memcpy(p, &d->header[d->pos], s);
  } else
    s = 0;
  memset(&p[s], 0, size - s);
  d->pos += size;
  return size;
}

/**
 * \brief dummy mpc_reader callback function for seeking
 */
static mpc_bool_t cb_seek(void *data, mpc_int32_t offset ) {
  context_t *d = (context_t *)data;
  d->pos = offset;
  return 1;
}

/**
 * \brief dummy mpc_reader callback function for getting stream position
 */
static mpc_int32_t cb_tell(void *data) {
  context_t *d = (context_t *)data;
  return d->pos;
}

/**
 * \brief dummy mpc_reader callback function for getting stream length
 */
static mpc_int32_t cb_get_size(void *data) {
  return 1 << 30;
}

/**
 * \brief mpc_reader callback function, we cannot seek.
 */
static mpc_bool_t cb_canseek(void *data) {
  return 0;
}


mpc_reader header_reader = {
  .read = cb_read, .seek = cb_seek, .tell = cb_tell,
  .get_size = cb_get_size, .canseek = cb_canseek
};

static int preinit(sh_audio_t *sh) {
  sh->audio_out_minsize = MAX_FRAMESIZE;
  return 1;
}

static void uninit(sh_audio_t *sh) {
  if (sh->context)
    free(sh->context);
  sh->context = NULL;
}

static int init(sh_audio_t *sh) {
  mpc_streaminfo info;
  context_t *cd = malloc(sizeof(context_t));

  if (!sh->wf || (sh->wf->cbSize < 6 * 4)) {
    mp_msg(MSGT_DECAUDIO, MSGL_FATAL, "Missing extradata!\n");
    return 0;
  }
  cd->header = (char *)sh->wf;
  cd->header = &cd->header[sizeof(WAVEFORMATEX)];
  cd->header_len = sh->wf->cbSize;
  cd->sh = sh;
  cd->pos = 0;
  sh->context = (char *)cd;

  /* read file's streaminfo data */
  mpc_streaminfo_init(&info);
  header_reader.data = cd;
  if (mpc_streaminfo_read(&info, &header_reader) != ERROR_CODE_OK) {
    mp_msg(MSGT_DECAUDIO, MSGL_FATAL, "Not a valid musepack file.\n");
    return 0;
  }
// this value is nonsense, since it relies on the get_size function.
// use the value from the demuxer instead.
//  sh->i_bps = info.average_bitrate / 8;
  sh->channels = info.channels;
  sh->samplerate = info.sample_freq;
  sh->samplesize = 4;
  sh->sample_format =
#if MPC_SAMPLE_FORMAT == float
             AF_FORMAT_FLOAT_NE;
#elif  MPC_SAMPLE_FORMAT == mpc_int32_t
             AF_FORMAT_S32_NE;
#else
  #error musepack lib must use either float or mpc_int32_t sample format
#endif

  mpc_decoder_setup(&cd->decoder, NULL);
  mpc_decoder_set_streaminfo(&cd->decoder, &info);
  return 1;
}

// FIXME: minlen is currently ignored
static int decode_audio(sh_audio_t *sh, unsigned char *buf,
                        int minlen, int maxlen) {
  int status, len;
  MPC_SAMPLE_FORMAT *sample_buffer = (MPC_SAMPLE_FORMAT *)buf;
  mpc_uint32_t *packet = NULL;
  
  context_t *cd = (context_t *) sh->context;
  if (maxlen < MAX_FRAMESIZE) {
    mp_msg(MSGT_DECAUDIO, MSGL_V, "maxlen too small in decode_audio\n");
    return -1;
  }
  len = ds_get_packet(sh->ds, (unsigned char **)&packet);
  if (len <= 0) return -1;
  status = mpc_decoder_decode_frame(&cd->decoder, packet, len, sample_buffer);
  if (status == -1) // decode error
    mp_msg(MSGT_DECAUDIO, MSGL_FATAL, "Error decoding file.\n");
  if (status <= 0) // error or EOF
    return -1;

  status = MPC_FRAME_LENGTH * sh->channels; // one sample per channel
#if MPC_SAMPLE_FORMAT == float || MPC_SAMPLE_FORMAT == mpc_int32_t
  status *= 4;
#else
  // should not happen
  status *= 2;
#endif
  return status;
}

/**
 * \brief check if the decoded values are in a sane range
 * \param buf decoded buffer
 * \param len length of buffer in bytes
 * \return 1 if all values are in (-1.01, 1.01) range, 0 otherwise
 */
static int check_clip(void *buf, int len) {
#if MPC_SAMPLE_FORMAT == float
  float *p = buf;
  if (len < 4) return 1;
  len = -len / 4;
  p = &p[-len];
  do {
    if (p[len] < -1 || p[len] > 1) return 0;
  } while (++len);
#endif
  return 1;
}

static int control(sh_audio_t *sh, int cmd, void* arg, ...) {
  if (cmd == ADCTRL_RESYNC_STREAM) {
    unsigned char *buf = malloc(MAX_FRAMESIZE);
    int i;
    int nr_ok = 0;
    for (i = 0; i < MAX_SEEK_DISCARD; i++) {
      int len = decode_audio(sh, buf, 0, MAX_FRAMESIZE);
      if (check_clip(buf, len)) nr_ok++; else nr_ok = 0;
      if (nr_ok > MIN_SEEK_GOOD) break;
    }
    free(buf);
  }
  return CONTROL_UNKNOWN;
}

