#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"MS ADPCM audio decoder",
	"msadpcm",
	AFM_MSADPCM,
	"Nick Kurshev",
	"Mike Melanson",
	""
};

LIBAD_EXTERN(msadpcm)

#include "../adpcm.h"

static int init(sh_audio_t *sh_audio)
{
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = sh_audio->wf->nBlockAlign *
    (sh_audio->channels*sh_audio->samplerate) / MS_ADPCM_SAMPLES_PER_BLOCK;
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize=sh_audio->wf->nBlockAlign * 8;
  sh_audio->ds->ss_div = MS_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul = sh_audio->wf->nBlockAlign;
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    // TODO!!!
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  static unsigned char *ibuf = NULL;	// FIXME!!! TODO!!! use sh->a_in_buffer!
  if (!ibuf)
   ibuf = (unsigned char *)malloc
        (sh_audio->wf->nBlockAlign * sh_audio->wf->nChannels);
  if (demux_read_data(sh_audio->ds, ibuf,
      sh_audio->wf->nBlockAlign) != 
      sh_audio->wf->nBlockAlign) 
         return -1; /* EOF */
  return 2 * ms_adpcm_decode_block(
          (unsigned short*)buf,ibuf, sh_audio->wf->nChannels,
          sh_audio->wf->nBlockAlign);
}
