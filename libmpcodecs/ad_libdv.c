#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "config.h"

#ifdef HAVE_LIBDV095

#include "img_format.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "ad_internal.h"

static ad_info_t info =
{
	"Raw DV Audio Decoder",
	"libdv",
	AFM_LIBDV,
	"Alexander Neundorf <neundorf@kde.org>",
	"http://libdv.sf.net",
	""
};

LIBAD_EXTERN(libdv)

// defined in vd_libdv.c:
dv_decoder_t*  init_global_rawdv_decoder();

static int preinit(sh_audio_t *sh_audio)
{
  sh_audio->audio_out_minsize=4*DV_AUDIO_MAX_SAMPLES*2;
  return 1;
}

static int16_t *audioBuffers[4]={NULL,NULL,NULL,NULL};

static int init(sh_audio_t *sh)
{
  int i;
  WAVEFORMATEX *h=sh->wf;

  if(!h) return 0;
   
  sh->i_bps=h->nAvgBytesPerSec;
  sh->channels=h->nChannels;
  sh->samplerate=h->nSamplesPerSec;
  sh->samplesize=(h->wBitsPerSample+7)/8;

  sh->context=init_global_rawdv_decoder();

  for (i=0; i < 4; i++)
    audioBuffers[i] = malloc(2*DV_AUDIO_MAX_SAMPLES);

  return 1;
}

static void uninit(sh_audio_t *sh_audio)
{
  int i;
  for (i=0; i < 4; i++)
    free(audioBuffers[i]);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    // TODO!!!
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *audio, unsigned char *buf, int minlen, int maxlen)
{
   int len=0;
   dv_decoder_t* decoder=audio->context;  //global_rawdv_decoder;
   unsigned char* dv_audio_frame=NULL;
   int xx=ds_get_packet(audio->ds,&dv_audio_frame);
   if(xx<=0 || !dv_audio_frame) return 0; // EOF?

   dv_parse_header(decoder, dv_audio_frame);
   
   if(xx!=decoder->frame_size)
       printf("warning! audio framesize differs! read=%d  hdr=%d  \n",
           xx, decoder->frame_size);

   if (dv_decode_full_audio(decoder, dv_audio_frame,(int16_t**) audioBuffers))
   {
      /* Interleave the audio into a single buffer */
      int i=0;
      int16_t *bufP=(int16_t*)buf;
      
//      printf("samples=%d/%d  chans=%d  mem=%d  \n",decoder->audio->samples_this_frame,DV_AUDIO_MAX_SAMPLES,
//          decoder->audio->num_channels, decoder->audio->samples_this_frame*decoder->audio->num_channels*2);

//   return (44100/30)*4;

      for (i=0; i < decoder->audio->samples_this_frame; i++)
      {
         int ch;
         for (ch=0; ch < decoder->audio->num_channels; ch++)
            bufP[len++] = audioBuffers[ch][i];
      }
   }
   return len*2;
}

#endif

