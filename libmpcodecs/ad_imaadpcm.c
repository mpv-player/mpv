#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

#include "../adpcm.h"

static ad_info_t info = 
{
	"IMA ADPCM audio decoder",
	"imaadpcm",
	AFM_IMAADPCM,
	"Nick Kurshev",
	"Mike Melanson",
	"ima4 (MOV files)"
};

LIBAD_EXTERN(imaadpcm)

static int init(sh_audio_t *sh_audio)
{
  /* IMA-ADPCM 4:1 audio codec:*/
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  /* decodes 34 byte -> 64 short*/
  sh_audio->i_bps=IMA_ADPCM_BLOCK_SIZE*(sh_audio->channels*sh_audio->samplerate)/
		  IMA_ADPCM_SAMPLES_PER_BLOCK;  /* 1:4 */
  return 1;
}

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize=4096;
  sh_audio->ds->ss_div=IMA_ADPCM_SAMPLES_PER_BLOCK;
  sh_audio->ds->ss_mul=IMA_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels;
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
    unsigned char ibuf[IMA_ADPCM_BLOCK_SIZE * 2]; /* bytes / stereo frame */
    if (demux_read_data(sh_audio->ds, ibuf,
      IMA_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) != 
      IMA_ADPCM_BLOCK_SIZE * sh_audio->wf->nChannels) 
	return -1;
    return 2*ima_adpcm_decode_block((unsigned short*)buf,ibuf, sh_audio->wf->nChannels);
}
