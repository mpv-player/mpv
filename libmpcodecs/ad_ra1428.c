// SAMPLE audio decoder - you can use this file as template when creating new codec!

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info =  {
	"RealAudio 1.0 and 2.0 native decoder",
	"ra1428",
	"Roberto Togni",
	"http://www.honeypot.net/audio",
	"Decoders taken from a public domain Amiga player"
};

LIBAD_EXTERN(ra1428)

#include "native/common1428.h"



static int preinit(sh_audio_t *sh) {
  sh->samplerate=sh->wf->nSamplesPerSec;
  sh->samplesize=sh->wf->wBitsPerSample/8;
  sh->channels=sh->wf->nChannels;
  sh->sample_format=AFMT_S16_LE;

	switch (sh->format) {
		case mmioFOURCC('1','4','_','4'):
			mp_msg(MSGT_DECAUDIO,MSGL_INFO,"[ra1428] RealAudio 1.0 (14_4)\n");
		  sh->i_bps=1800;
		  break;
		case mmioFOURCC('2','8','_','8'):
			mp_msg(MSGT_DECAUDIO,MSGL_INFO,"[ra1428] RealAudio 2.0 (28_8)\n");
	  	sh->i_bps=1200;
	  	break;
		default:
			mp_msg(MSGT_DECAUDIO,MSGL_ERR,"[ra1428] Unhandled format in preinit: %x\n", sh->format);
			return 0;
	}
  
  sh->audio_out_minsize=128000; // no idea how to get... :(
  sh->audio_in_minsize=((short*)(sh->wf+1))[1]*sh->wf->nBlockAlign;

  return 1; // return values: 1=OK 0=ERROR
}



static int init(sh_audio_t *sh) {
	switch (sh->format) {
		case mmioFOURCC('1','4','_','4'):
			sh->context = init_144();
		  break;
		case mmioFOURCC('2','8','_','8'):
			sh->context = init_288();
	  	break;
		default:
			mp_msg(MSGT_DECAUDIO,MSGL_ERR,"[ra1428] Unhandled format in init: %x\n", sh->format);
			return 0;
	}

  return 1; // return values: 1=OK 0=ERROR
}



static void uninit(sh_audio_t *sh) {
	switch (sh->format) {
		case mmioFOURCC('1','4','_','4'):
			free_144((Real_144*)sh->context);
		  break;
		case mmioFOURCC('2','8','_','8'):
			free_288((Real_288*)sh->context);
	  	break;
		default:
			mp_msg(MSGT_DECAUDIO,MSGL_ERR,"[ra1428] Unhandled format in uninit: %x\n", sh->format);
			return;
	}
}



static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen) {
  int w=sh->wf->nBlockAlign;
  int h=((short*)(sh->wf+1))[1];
  int cfs=((short*)(sh->wf+1))[3];
	int i,j;

  if(sh->a_in_buffer_len<=0){
		switch (sh->format) {
			case mmioFOURCC('1','4','_','4'):
				demux_read_data(sh->ds, sh->a_in_buffer, sh->wf->nBlockAlign);
				sh->a_in_buffer_size=
				sh->a_in_buffer_len=sh->wf->nBlockAlign;
		 	 break;
			case mmioFOURCC('2','8','_','8'):
				for (j = 0; j < h; j++)
					for (i = 0; i < h/2; i++)
						demux_read_data(sh->ds, sh->a_in_buffer+i*2*w+j*cfs, cfs);
				sh->a_in_buffer_size=
				sh->a_in_buffer_len=sh->wf->nBlockAlign*h;
	 	 	break;
			default:
				mp_msg(MSGT_DECAUDIO,MSGL_ERR,"[ra1428] Unhandled format in decode_audio: %x\n", sh->format);
				return 0;
		}
	}
	
	switch (sh->format) {
		case mmioFOURCC('1','4','_','4'):
			decode_144((Real_144*)sh->context,sh->a_in_buffer+sh->a_in_buffer_size-sh->a_in_buffer_len,(signed short*)buf);
		  break;
		case mmioFOURCC('2','8','_','8'):
			decode_288((Real_288*)sh->context,sh->a_in_buffer+sh->a_in_buffer_size-sh->a_in_buffer_len,(signed short*)buf);
	  	break;
		default:
			mp_msg(MSGT_DECAUDIO,MSGL_ERR,"[ra1428] Unhandled format in init: %x\n", sh->format);
			return 0;
	}

  sh->a_in_buffer_len-=cfs;

  return AUDIOBLOCK*2; // return value: number of _bytes_ written to output buffer,
              // or -1 for EOF (or uncorrectable error)
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...){
    // various optional functions you MAY implement:
    switch(cmd){
      case ADCTRL_RESYNC_STREAM:
        // it is called once after seeking, to resync.
	// Note: sh_audio->a_in_buffer_len=0; is done _before_ this call!
//	...
	return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
        // it is called to skip (jump over) small amount (1/10 sec or 1 frame)
	// of audio data - used to sync audio to video after seeking
	// if you don't return CONTROL_TRUE, it will defaults to:
	//      ds_fill_buffer(sh_audio->ds);  // skip 1 demux packet
//	...
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}
