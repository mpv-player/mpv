#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"Duck DK4 ADPCM (rogue format number) audio decoder",
	"dk4adpcm",
	AFM_DK4ADPCM,
	"Nick Kurshev",
	"This format number was used by Duck Corp. but not officially registered with Microsoft"
};

LIBAD_EXTERN(dk4adpcm)

#include "adpcm.h"

static int init(sh_audio_t *sh_audio)
{
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = sh_audio->wf->nBlockAlign *
    (sh_audio->channels*sh_audio->samplerate) / DK4_ADPCM_SAMPLES_PER_BLOCK;
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize=DK4_ADPCM_SAMPLES_PER_BLOCK * 4;
  sh_audio->ds->ss_div=DK4_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul=sh_audio->wf->nBlockAlign;
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    // TODO!
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  int len=-1;
  static unsigned char *ibuf = NULL;
  if (!ibuf)
    ibuf = (unsigned char *)malloc(sh_audio->wf->nBlockAlign);
  if (demux_read_data(sh_audio->ds, ibuf, sh_audio->wf->nBlockAlign) != 
      sh_audio->wf->nBlockAlign)
          return len; /* EOF */
  len=2*dk4_adpcm_decode_block((unsigned short*)buf,ibuf,
          sh_audio->wf->nChannels, sh_audio->wf->nBlockAlign);
  return len;
}
