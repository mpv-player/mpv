#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"Duck DK3 ADPCM decoder",
	"dk3adpcm",
	AFM_DK3ADPCM,
	"Nick Kurshev",
	"Mike Melanson",
	"This format number was used by Duck Corp. but not officially registered with Microsoft"
};

LIBAD_EXTERN(dk3adpcm)

#include "adpcm.h"

static int init(sh_audio_t *sh_audio)
{
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=DK3_ADPCM_BLOCK_SIZE*
    (sh_audio->channels*sh_audio->samplerate) / DK3_ADPCM_SAMPLES_PER_BLOCK;
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize=DK3_ADPCM_SAMPLES_PER_BLOCK * 4;
  sh_audio->ds->ss_div=DK3_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul=DK3_ADPCM_BLOCK_SIZE;
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
   unsigned char ibuf[DK3_ADPCM_BLOCK_SIZE * 2]; /* bytes / stereo frame */
    if (demux_read_data(sh_audio->ds, ibuf,
          DK3_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) != 
          DK3_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) 
      return len; /* EOF */
    len = 2 * dk3_adpcm_decode_block(
          (unsigned short*)buf,ibuf);
    return len;
}
