/*
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
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "img_format.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "ad_internal.h"
#include "vd_libdv.h"

static const ad_info_t info =
{
	"Raw DV Audio Decoder",
	"libdv",
	"Alexander Neundorf <neundorf@kde.org>",
	"http://libdv.sf.net",
	""
};

LIBAD_EXTERN(libdv)

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
       mp_msg(MSGT_GLOBAL,MSGL_WARN,MSGTR_MPCODECS_AudioFramesizeDiffers,
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
