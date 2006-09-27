/* ad_faad.c - MPlayer AAC decoder using libfaad2
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.  
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info = 
{
	"AAC (MPEG2/4 Advanced Audio Coding)",
	"faad",
	"Felix Buenemann",
	"faad2",
	"uses libfaad2"
};

LIBAD_EXTERN(faad)

#ifndef USE_FAAD_INTERNAL
#include <faad.h>
#else
#include "libfaad2/faad.h"
#endif

/* configure maximum supported channels, *
 * this is theoretically max. 64 chans   */
#define FAAD_MAX_CHANNELS 6
#define FAAD_BUFFLEN (FAAD_MIN_STREAMSIZE*FAAD_MAX_CHANNELS)		       

//#define AAC_DUMP_COMPRESSED  

static faacDecHandle faac_hdec;
static faacDecFrameInfo faac_finfo;

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=8192*FAAD_MAX_CHANNELS;
  sh->audio_in_minsize=FAAD_BUFFLEN;
  return 1;
}

static int aac_probe(unsigned char *buffer, int len)
{
  int i = 0, pos = 0;
  mp_msg(MSGT_DECAUDIO,MSGL_V, "\nAAC_PROBE: %d bytes\n", len);
  while(i <= len-4) {
    if(
       ((buffer[i] == 0xff) && ((buffer[i+1] & 0xf6) == 0xf0)) ||
       (buffer[i] == 'A' && buffer[i+1] == 'D' && buffer[i+2] == 'I' && buffer[i+3] == 'F')
    ) {
      pos = i;
      break;
    }
    mp_msg(MSGT_DECAUDIO,MSGL_V, "AUDIO PAYLOAD: %x %x %x %x\n", buffer[i], buffer[i+1], buffer[i+2], buffer[i+3]);
    i++;
  }
  mp_msg(MSGT_DECAUDIO,MSGL_V, "\nAAC_PROBE: ret %d\n", pos);
  return pos;
}
	
extern int audio_output_channels;
static int init(sh_audio_t *sh)
{
  unsigned long faac_samplerate;
  unsigned char faac_channels;
  int faac_init, pos = 0;
  faac_hdec = faacDecOpen();

  // If we don't get the ES descriptor, try manual config
  if(!sh->codecdata_len && sh->wf) {
    sh->codecdata_len = sh->wf->cbSize;
    sh->codecdata = (char*)(sh->wf+1);
    mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"FAAD: codecdata extracted from WAVEFORMATEX\n");
  }
  if(!sh->codecdata_len) {
#if 1
    faacDecConfigurationPtr faac_conf;
    /* Set the default object type and samplerate */
    /* This is useful for RAW AAC files */
    faac_conf = faacDecGetCurrentConfiguration(faac_hdec);
    if(sh->samplerate)
      faac_conf->defSampleRate = sh->samplerate;
    /* XXX: FAAD support FLOAT output, how do we handle
      * that (FAAD_FMT_FLOAT)? ::atmos
      */
    if (audio_output_channels <= 2) faac_conf->downMatrix = 1;
      switch(sh->samplesize){
	case 1: // 8Bit
	  mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: 8Bit samplesize not supported by FAAD, assuming 16Bit!\n");
	default:
	  sh->samplesize=2;
	case 2: // 16Bit
	  faac_conf->outputFormat = FAAD_FMT_16BIT;
	  break;
	case 3: // 24Bit
	  faac_conf->outputFormat = FAAD_FMT_24BIT;
	  break;
	case 4: // 32Bit
	  faac_conf->outputFormat = FAAD_FMT_32BIT;
	  break;
      }
    //faac_conf->defObjectType = LTP; // => MAIN, LC, SSR, LTP available.

    faacDecSetConfiguration(faac_hdec, faac_conf);
#endif

    sh->a_in_buffer_len = demux_read_data(sh->ds, sh->a_in_buffer, sh->a_in_buffer_size);
    pos = aac_probe(sh->a_in_buffer, sh->a_in_buffer_len);
    if(pos) {
      sh->a_in_buffer_len -= pos;
      memmove(sh->a_in_buffer, &(sh->a_in_buffer[pos]), sh->a_in_buffer_len);
      sh->a_in_buffer_len +=
	demux_read_data(sh->ds,&(sh->a_in_buffer[sh->a_in_buffer_len]),
	sh->a_in_buffer_size - sh->a_in_buffer_len);
      pos = 0;
    }

    /* init the codec */
    faac_init = faacDecInit(faac_hdec, sh->a_in_buffer,
       sh->a_in_buffer_len, &faac_samplerate, &faac_channels);

    sh->a_in_buffer_len -= (faac_init > 0)?faac_init:0; // how many bytes init consumed
    // XXX FIXME: shouldn't we memcpy() here in a_in_buffer ?? --A'rpi

  } else { // We have ES DS in codecdata
    faacDecConfigurationPtr faac_conf = faacDecGetCurrentConfiguration(faac_hdec);
    if (audio_output_channels <= 2) {
        faac_conf->downMatrix = 1;
        faacDecSetConfiguration(faac_hdec, faac_conf);
    }
    
    /*int i;
    for(i = 0; i < sh_audio->codecdata_len; i++)
      printf("codecdata_dump %d: 0x%02X\n", i, sh_audio->codecdata[i]);*/

    faac_init = faacDecInit2(faac_hdec, sh->codecdata,
       sh->codecdata_len,	&faac_samplerate, &faac_channels);
  }
  if(faac_init < 0) {
    mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: Failed to initialize the decoder!\n"); // XXX: deal with cleanup!
    faacDecClose(faac_hdec);
    // XXX: free a_in_buffer here or in uninit?
    return 0;
  } else {
    mp_msg(MSGT_DECAUDIO,MSGL_V,"FAAD: Decoder init done (%dBytes)!\n", sh->a_in_buffer_len); // XXX: remove or move to debug!
    mp_msg(MSGT_DECAUDIO,MSGL_V,"FAAD: Negotiated samplerate: %ldHz  channels: %d\n", faac_samplerate, faac_channels);
    sh->channels = faac_channels;
    if (audio_output_channels <= 2) sh->channels = faac_channels > 1 ? 2 : 1;
    sh->samplerate = faac_samplerate;
    sh->samplesize=2;
    //sh->o_bps = sh->samplesize*faac_channels*faac_samplerate;
    if(!sh->i_bps) {
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: compressed input bitrate missing, assuming 128kbit/s!\n");
      sh->i_bps = 128*1000/8; // XXX: HACK!!! ::atmos
    } else 
      mp_msg(MSGT_DECAUDIO,MSGL_V,"FAAD: got %dkbit/s bitrate from MP4 header!\n",sh->i_bps*8/1000);
  }  
  return 1;
}

static void uninit(sh_audio_t *sh)
{
  mp_msg(MSGT_DECAUDIO,MSGL_V,"FAAD: Closing decoder!\n");
  faacDecClose(faac_hdec);
}

static int aac_sync(sh_audio_t *sh)
{
  int pos = 0;
  if(!sh->codecdata_len) {
    if(sh->a_in_buffer_len < sh->a_in_buffer_size){
      sh->a_in_buffer_len +=
	demux_read_data(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],
	sh->a_in_buffer_size - sh->a_in_buffer_len);
    }
    pos = aac_probe(sh->a_in_buffer, sh->a_in_buffer_len);
    if(pos) {
      sh->a_in_buffer_len -= pos;
      memmove(sh->a_in_buffer, &(sh->a_in_buffer[pos]), sh->a_in_buffer_len);
      mp_msg(MSGT_DECAUDIO,MSGL_V, "\nAAC SYNC AFTER %d bytes\n", pos);
    }
  }
  return pos;
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    switch(cmd)
    {
      case ADCTRL_RESYNC_STREAM:
         aac_sync(sh);
	 return CONTROL_TRUE;
#if 0      
      case ADCTRL_SKIP_FRAME:
	  return CONTROL_TRUE;
#endif
    }
  return CONTROL_UNKNOWN;
}

#define MAX_FAAD_ERRORS 10
static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen)
{
  int j = 0, len = 0, last_dec_len = 1, errors = 0;	      
  void *faac_sample_buffer;

  while(len < minlen && last_dec_len > 0 && errors < MAX_FAAD_ERRORS) {

    /* update buffer for raw aac streams: */
  if(!sh->codecdata_len)
    if(sh->a_in_buffer_len < sh->a_in_buffer_size){
      sh->a_in_buffer_len +=
	demux_read_data(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],
	sh->a_in_buffer_size - sh->a_in_buffer_len);
    }
	  
#ifdef DUMP_AAC_COMPRESSED
    {int i;
    for (i = 0; i < 16; i++)
      printf ("%02X ", sh->a_in_buffer[i]);
    printf ("\n");}
#endif

  if(!sh->codecdata_len){
   // raw aac stream:
   do {
    faac_sample_buffer = faacDecDecode(faac_hdec, &faac_finfo, sh->a_in_buffer, sh->a_in_buffer_len);
	
    /* update buffer index after faacDecDecode */
    if(faac_finfo.bytesconsumed >= sh->a_in_buffer_len) {
      sh->a_in_buffer_len=0;
    } else {
      sh->a_in_buffer_len-=faac_finfo.bytesconsumed;
      memmove(sh->a_in_buffer,&sh->a_in_buffer[faac_finfo.bytesconsumed],sh->a_in_buffer_len);
    }

    if(faac_finfo.error > 0) {
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: error: %s, trying to resync!\n",
              faacDecGetErrorMessage(faac_finfo.error));
      if (sh->a_in_buffer_len <= 0) {
        errors = MAX_FAAD_ERRORS;
        break;
      }
      sh->a_in_buffer_len--;
      memmove(sh->a_in_buffer,&sh->a_in_buffer[1],sh->a_in_buffer_len);
      aac_sync(sh);
      errors++;
    } else
      break;
   } while(errors < MAX_FAAD_ERRORS);	  
  } else {
   // packetized (.mp4) aac stream:
    unsigned char* bufptr=NULL;
    double pts;
    int buflen=ds_get_packet_pts(sh->ds, &bufptr, &pts);
    if(buflen<=0) break;
    if (pts != MP_NOPTS_VALUE) {
	sh->pts = pts;
	sh->pts_bytes = 0;
    }
    faac_sample_buffer = faacDecDecode(faac_hdec, &faac_finfo, bufptr, buflen);
  }
  //for (j=0;j<faac_finfo.channels;j++) printf("%d:%d\n", j, faac_finfo.channel_position[j]);
  
    if(faac_finfo.error > 0) {
      mp_msg(MSGT_DECAUDIO,MSGL_WARN,"FAAD: Failed to decode frame: %s \n",
      faacDecGetErrorMessage(faac_finfo.error));
    } else if (faac_finfo.samples == 0) {
      mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"FAAD: Decoded zero samples!\n");
    } else {
      /* XXX: samples already multiplied by channels! */
      mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"FAAD: Successfully decoded frame (%ld Bytes)!\n",
      sh->samplesize*faac_finfo.samples);
      memcpy(buf+len,faac_sample_buffer, sh->samplesize*faac_finfo.samples);
      last_dec_len = sh->samplesize*faac_finfo.samples;
      len += last_dec_len;
      sh->pts_bytes += last_dec_len;
    //printf("FAAD: buffer: %d bytes  consumed: %d \n", k, faac_finfo.bytesconsumed);
    }
  }
  return len;
}
