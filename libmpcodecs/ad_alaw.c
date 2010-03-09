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

static const ad_info_t info =
{
	"aLaw/uLaw audio decoder",
	"alaw",
	"Nick Kurshev",
	"A'rpi",
	""
};

LIBAD_EXTERN(alaw)

#include "native/alaw.h"

static int init(sh_audio_t *sh_audio)
{
  /* aLaw audio codec:*/
  if(!sh_audio->wf) return 0;
  sh_audio->channels=sh_audio->wf->nChannels;
  sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
  sh_audio->i_bps=sh_audio->channels*sh_audio->samplerate;
  sh_audio->samplesize=2;
  return 1;
}

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=2048;
  sh->ds->ss_div = 1; // 1 samples/packet
  sh->ds->ss_mul = sh->wf->nChannels; // bytes/packet
  return 1;
}

static void uninit(sh_audio_t *sh)
{
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  int skip;
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
	skip=sh->i_bps/16;
	skip=skip&(~3);
	demux_read_data(sh->ds,NULL,skip);
	return CONTROL_TRUE;
      default:
	return CONTROL_UNKNOWN;
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
 int len;
 int l=demux_read_data(sh_audio->ds,buf,minlen/2);
 unsigned short *d=(unsigned short *) buf;
 unsigned char *s=buf;
 len=2*l;
 if(sh_audio->format==6 || sh_audio->format==0x77616C61){
 /* aLaw */
   while(l>0){ --l; d[l]=alaw2short[s[l]]; }
 } else {
 /* uLaw */
    while(l>0){ --l; d[l]=ulaw2short[s[l]]; }
 }
 return len;
}
