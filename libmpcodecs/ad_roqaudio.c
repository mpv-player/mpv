#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"
#include "roqav.h"

static ad_info_t info = 
{
	"Id RoQ File Audio Decoder",
	"roqaudio",
	"Nick Kurshev",
	"Mike Melanson",
	"" //"RoQA is an internal MPlayer FOURCC"
};

LIBAD_EXTERN(roqaudio)

static int preinit(sh_audio_t *sh_audio)
{
  // minsize was stored in wf->nBlockAlign by the RoQ demuxer
  sh_audio->audio_out_minsize=sh_audio->wf->nBlockAlign;
  sh_audio->audio_in_minsize=sh_audio->audio_out_minsize / 2; // FIXME?
  sh_audio->context = roq_decode_audio_init();
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = 44100;
  return 1;
}

static void uninit(sh_audio_t *sh_audio)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    // TODO!!!
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  unsigned char header_data[6];
  int read_len;

  /* figure out how much data to read */
  if (demux_read_data(sh_audio->ds, header_data, 6) != 6) return -1; /* EOF */
  read_len = (header_data[5] << 24) | (header_data[4] << 16) |
	     (header_data[3] << 8) | header_data[2];
  read_len += 2;  /* 16-bit arguments */
  if (demux_read_data(sh_audio->ds, sh_audio->a_in_buffer, read_len) !=
    read_len) return -1;
  return 2 * roq_decode_audio((unsigned short *)buf, sh_audio->a_in_buffer,
    read_len, sh_audio->channels, sh_audio->context);
}
