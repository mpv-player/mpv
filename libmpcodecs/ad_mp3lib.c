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
#include <unistd.h>

#include "config.h"

#include "ad_internal.h"
#include "ad_mp3lib.h"

static const ad_info_t info =
{
	"MPEG layer-2, layer-3",
	"mp3lib",
	"Nick Kurshev",
	"mpg123",
	"Optimized to MMX/SSE/3Dnow!"
};

LIBAD_EXTERN(mp3lib)

#include "mp3lib/mp3.h"

extern int fakemono;

static sh_audio_t* dec_audio_sh=NULL;

// MP3 decoder buffer callback:
int mplayer_audio_read(char *buf,int size){
  return demux_read_data(dec_audio_sh->ds,buf,size);
}

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=32*36*2*2; //4608;
  return 1;
}

static int init(sh_audio_t *sh)
{
  // MPEG Audio:
  dec_audio_sh=sh; // save sh_audio for the callback:
//  MP3_Init(fakemono,mplayer_accel,&mplayer_audio_read); // TODO!!!
#ifdef CONFIG_FAKE_MONO
  MP3_Init(fakemono);
#else
  MP3_Init();
#endif
  MP3_samplerate=MP3_channels=0;
  sh->a_buffer_len=MP3_DecodeFrame(sh->a_buffer,-1);
  if(!sh->a_buffer_len) return 0; // unsupported layer/format
  sh->channels=2; // hack
  sh->samplesize=2;
  sh->samplerate=MP3_samplerate;
  sh->i_bps=MP3_bitrate*(1000/8);
  MP3_PrintHeader();
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    switch(cmd)
    {
      case ADCTRL_RESYNC_STREAM:
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
          MP3_DecodeFrame(NULL,-2); // resync
	  return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
	  MP3_DecodeFrame(NULL,-2); // skip MPEG frame
	  return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
   return MP3_DecodeFrame(buf,-1);
}
