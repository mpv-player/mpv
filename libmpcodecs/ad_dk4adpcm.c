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
	"Mike Melanson",
	"This format number was used by Duck Corp. but not officially registered with Microsoft"
};

LIBAD_EXTERN(dk4adpcm)

#define DK4_ADPCM_PREAMBLE_SIZE 4

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize = 
    (((sh_audio->wf->nBlockAlign - DK4_ADPCM_PREAMBLE_SIZE) * 2) + 1) * 4;
  sh_audio->ds->ss_div =
    ((sh_audio->wf->nBlockAlign - DK4_ADPCM_PREAMBLE_SIZE) * 2) + 1;
  sh_audio->audio_in_minsize=
  sh_audio->ds->ss_mul=sh_audio->wf->nBlockAlign;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps = sh_audio->wf->nBlockAlign *
    (sh_audio->channels*sh_audio->samplerate) /
    (((sh_audio->wf->nBlockAlign - DK4_ADPCM_PREAMBLE_SIZE) * 2) + 1);
  return 1;
}

static void uninit(sh_audio_t *sh_audio)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    // TODO!
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  if (demux_read_data(sh_audio->ds, sh_audio->a_in_buffer,
    sh_audio->wf->nBlockAlign) != 
    sh_audio->wf->nBlockAlign)
      return -1; /* EOF */

  return 2 * dk4_adpcm_decode_block((unsigned short*)buf,
    sh_audio->a_in_buffer,
    sh_audio->wf->nChannels, sh_audio->wf->nBlockAlign);
}
