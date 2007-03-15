#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include "libavutil/common.h"
#include "mpbswap.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "libmpcodecs/vqf.h"

static int demux_probe_vqf(demuxer_t* demuxer) 
{
  char buf[KEYWORD_BYTES];
  stream_t *s;
  s = demuxer->stream;
  if(stream_read(s,buf,KEYWORD_BYTES)!=KEYWORD_BYTES)
    return 0;
  if(memcmp(buf,"TWIN",KEYWORD_BYTES)==0) return DEMUXER_TYPE_VQF; /*version: 97012000*/
  return 0;
}

static demuxer_t* demux_open_vqf(demuxer_t* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;
  stream_t *s;
  headerInfo *hi;

  s = demuxer->stream;

  sh_audio = new_sh_audio(demuxer,0);
  sh_audio->wf = w = malloc(sizeof(WAVEFORMATEX)+sizeof(headerInfo));
  hi = (headerInfo *)&w[1];
  memset(hi,0,sizeof(headerInfo));
  w->wFormatTag = 0x1;
  sh_audio->format = mmioFOURCC('T','W','I','N'); /* TWinVQ */
  w->nChannels = sh_audio->channels = 2;
  w->nSamplesPerSec = sh_audio->samplerate = 44100;
  w->nAvgBytesPerSec = w->nSamplesPerSec*sh_audio->channels*2;
  w->nBlockAlign = 0;
  sh_audio->samplesize = 2;
  w->wBitsPerSample = 8*sh_audio->samplesize;
  w->cbSize = 0;
  strcpy(hi->ID,"TWIN");
  stream_read(s,hi->ID+KEYWORD_BYTES,VERSION_BYTES); /* fourcc+version_id */
  while(1)
  {
    char chunk_id[4];
    unsigned chunk_size;
    hi->size=chunk_size=stream_read_dword(s); /* include itself */
    stream_read(s,chunk_id,4);
    if(*((uint32_t *)&chunk_id[0])==mmioFOURCC('C','O','M','M'))
    {
    char buf[chunk_size-8];
    unsigned i,subchunk_size;
    if(stream_read(s,buf,chunk_size-8)!=chunk_size-8) return NULL;
    i=0;
    subchunk_size=be2me_32(*((uint32_t *)&buf[0]));
    hi->channelMode=be2me_32(*((uint32_t *)&buf[4]));
    w->nChannels=sh_audio->channels=hi->channelMode+1; /*0-mono;1-stereo*/
    hi->bitRate=be2me_32(*((uint32_t *)&buf[8]));
    sh_audio->i_bps=hi->bitRate*1000/8; /* bitrate kbit/s */
    w->nAvgBytesPerSec = sh_audio->i_bps;
    hi->samplingRate=be2me_32(*((uint32_t *)&buf[12]));
    switch(hi->samplingRate){
    case 44:
        w->nSamplesPerSec=44100;
        break;
    case 22:
        w->nSamplesPerSec=22050;
        break;
    case 11:
        w->nSamplesPerSec=11025;
        break;
    default:
        w->nSamplesPerSec=hi->samplingRate*1000;
        break;
    }
    sh_audio->samplerate=w->nSamplesPerSec;
    hi->securityLevel=be2me_32(*((uint32_t *)&buf[16]));
    w->nBlockAlign = 0;
    sh_audio->samplesize = 4;
    w->wBitsPerSample = 8*sh_audio->samplesize;
    w->cbSize = 0;
    i+=subchunk_size+4;
    while(i<chunk_size-8)
    {
        unsigned slen,sid;
        char sdata[chunk_size];
        sid=*((uint32_t *)&buf[i]); i+=4;
        slen=be2me_32(*((uint32_t *)&buf[i])); i+=4;
        if(sid==mmioFOURCC('D','S','I','Z'))
        {
        hi->Dsiz=be2me_32(*((uint32_t *)&buf[i]));
        continue; /* describes the same info as size of DATA chunk */
        }
        memcpy(sdata,&buf[i],slen); sdata[slen]=0; i+=slen;
        if(sid==mmioFOURCC('N','A','M','E'))
        {
        memcpy(hi->Name,sdata,FFMIN(BUFSIZ,slen));
        demux_info_add(demuxer,"Title",sdata);
        }
        else
        if(sid==mmioFOURCC('A','U','T','H'))
        {
        memcpy(hi->Auth,sdata,FFMIN(BUFSIZ,slen));
        demux_info_add(demuxer,"Author",sdata);
        }
        else
        if(sid==mmioFOURCC('C','O','M','T'))
        {
        memcpy(hi->Comt,sdata,FFMIN(BUFSIZ,slen));
        demux_info_add(demuxer,"Comment",sdata);
        }
        else
        if(sid==mmioFOURCC('(','c',')',' '))
        {
        memcpy(hi->Cpyr,sdata,FFMIN(BUFSIZ,slen));
        demux_info_add(demuxer,"Copyright",sdata);
        }
        else
        if(sid==mmioFOURCC('F','I','L','E'))
        {
        memcpy(hi->File,sdata,FFMIN(BUFSIZ,slen));
        }
        else
        if(sid==mmioFOURCC('A','L','B','M')) demux_info_add(demuxer,"Album",sdata);
        else
        if(sid==mmioFOURCC('Y','E','A','R')) demux_info_add(demuxer,"Date",sdata);
        else
        if(sid==mmioFOURCC('T','R','A','C')) demux_info_add(demuxer,"Track",sdata);
        else
        if(sid==mmioFOURCC('E','N','C','D')) demux_info_add(demuxer,"Encoder",sdata);
        else
        mp_msg(MSGT_DEMUX, MSGL_V, "Unhandled subchunk '%c%c%c%c'='%s'\n",((char *)&sid)[0],((char *)&sid)[1],((char *)&sid)[2],((char *)&sid)[3],sdata);
        /* other stuff is unrecognized due untranslatable japan's idiomatics */
    }
    }
    else
    if(*((uint32_t *)&chunk_id[0])==mmioFOURCC('D','A','T','A'))
    {
    demuxer->movi_start=stream_tell(s);
    demuxer->movi_end=demuxer->movi_start+chunk_size-8;
    mp_msg(MSGT_DEMUX, MSGL_V, "Found data at %"PRIX64" size %"PRIu64"\n",demuxer->movi_start,demuxer->movi_end);
    /* Done! play it */
    break;
    }
    else
    {
    mp_msg(MSGT_DEMUX, MSGL_V, "Unhandled chunk '%c%c%c%c' %u bytes\n",((char *)&chunk_id)[0],((char *)&chunk_id)[1],((char *)&chunk_id)[2],((char *)&chunk_id)[3],chunk_size);
    stream_skip(s,chunk_size-8); /*unknown chunk type */
    }
  }

  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;
  stream_seek(s,demuxer->movi_start);
  demuxer->seekable=0;
  return demuxer;
}

static int demux_vqf_fill_buffer(demuxer_t* demuxer, demux_stream_t *ds) {
  sh_audio_t* sh_audio = demuxer->audio->sh;
  int l = sh_audio->wf->nAvgBytesPerSec;
  off_t spos = stream_tell(demuxer->stream);
  demux_packet_t*  dp;

  if(stream_eof(demuxer->stream))
    return 0;

  dp = new_demux_packet(l);
  ds->pts = spos / (float)(sh_audio->wf->nAvgBytesPerSec);
  ds->pos = spos;

  l=stream_read(demuxer->stream,dp->buffer,l);
  resize_demux_packet(dp,l);
  ds_add_packet(ds,dp);

  return 1;
}

static void demux_seek_vqf(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
#if 0
  stream_t* s = demuxer->stream;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  off_t base,pos;

  base = (flags & 1) ? demuxer->movi_start : stream_tell(s);
  if(flags & 2)
    pos = base + ((demuxer->movi_end - demuxer->movi_start)*rel_seek_secs);
  else
    pos = base + (rel_seek_secs*sh_audio->i_bps);

  pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
  stream_seek(s,pos);
#endif
}

static void demux_close_vqf(demuxer_t* demuxer) {}


demuxer_desc_t demuxer_desc_vqf = {
  "TwinVQ demuxer",
  "vqf",
  "VQF",
  "Nick Kurshev",
  "ported frm MPlayerXP",
  DEMUXER_TYPE_VQF,
  1, // safe autodetect
  demux_probe_vqf,
  demux_vqf_fill_buffer,
  demux_open_vqf,
  demux_close_vqf,
  demux_seek_vqf,
  NULL
};
