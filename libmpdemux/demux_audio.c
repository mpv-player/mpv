
#include "config.h"
#include "../mp_msg.h"

#include <stdlib.h>
#include <stdio.h>
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "genres.h"

#include <string.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif

#define MP3 1
#define WAV 2


#define HDR_SIZE 4

typedef struct da_priv {
  int frmt;
} da_priv_t;

extern int mp_decode_mp3_header(unsigned char* hbuf);
extern void free_sh_audio(sh_audio_t* sh);
extern void resync_audio_stream(sh_audio_t *sh_audio);


int demux_audio_open(demuxer_t* demuxer) {
  stream_t *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  int st_pos = 0,frmt = 0, n = 0, pos = 0, step;
  da_priv_t* priv;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif
  
  s = demuxer->stream;

  while(n < 5 && ! s->eof) {
    st_pos = stream_tell(s);
    step = 1;
    if(pos < HDR_SIZE) {
      stream_read(s,&hdr[pos],HDR_SIZE-pos);
      pos = HDR_SIZE;
    }

    if( hdr[0] == 'R' && hdr[1] == 'I' && hdr[2] == 'F' && hdr[3] == 'F' ) {
      stream_skip(s,4);
      if(s->eof)
	break;
      stream_read(s,hdr,4);
      if(s->eof)
	break;
      if(hdr[0] != 'W' || hdr[1] != 'A' || hdr[2] != 'V'  || hdr[3] != 'E' )
	stream_skip(s,-8);
      else
      // We found wav header. Now we can have 'fmt ' or a mp3 header
      // empty the buffer
	step = 4;
    } else if( hdr[0] == 'f' && hdr[1] == 'm' && hdr[2] == 't' && hdr[3] == ' ' ) {
      frmt = WAV;
      break;      
    } else if((n = mp_decode_mp3_header(hdr)) > 0) {
      frmt = MP3;
      break;
    }
    // Add here some other audio format detection
    if(step < HDR_SIZE)
      memmove(hdr,&hdr[step],HDR_SIZE-step);
    pos -= step;
  }

  if(!frmt)
    return 0;

  sh_audio = new_sh_audio(demuxer,0);

  switch(frmt) {
  case MP3:
    sh_audio->format = 0x55;
    demuxer->movi_start = st_pos-HDR_SIZE+n;
    for(n = 0; n < 5 ; n++) {
      pos = mp_decode_mp3_header(hdr);
      if(pos < 0)
	return 0;
      stream_skip(s,pos-4);
      if(s->eof)
	return 0;
      stream_read(s,hdr,4);
      if(s->eof)
	return 0;
    }
    if(s->end_pos) {
      char tag[4];
      stream_seek(s,s->end_pos-128);
      stream_read(s,tag,3);
      tag[3] = '\0';
      if(strcmp(tag,"TAG"))
	demuxer->movi_end = s->end_pos;
      else {
	char buf[31];
	uint8_t g;
	demuxer->movi_end = stream_tell(s)-3;
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,"Title",buf);
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,"Artist",buf);
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,"Album",buf);
	stream_read(s,buf,4);
	buf[5] = '\0';
	demux_info_add(demuxer,"Year",buf);
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,"Comment",buf);
	if(buf[28] == 0 && buf[29] != 0) {
	  uint8_t trk = (uint8_t)buf[29];
	  sprintf(buf,"%d",trk);
	  demux_info_add(demuxer,"Track",buf);
	}
	g = stream_read_char(s);
	demux_info_add(demuxer,"Genre",genres[g]);
      }
    }
    break;
  case WAV: {
    WAVEFORMATEX* w;
    int l;
    sh_audio->wf = w = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
    l = stream_read_dword_le(s);
    if(l < 16) {
      printf("Bad wav header length : too short !!!\n");
      free_sh_audio(sh_audio);
      return 0;
    }
    w->wFormatTag = sh_audio->format = stream_read_word_le(s);
    w->nChannels = sh_audio->channels = stream_read_word_le(s);
    w->nSamplesPerSec = sh_audio->samplerate = stream_read_dword_le(s);
    w->nAvgBytesPerSec = stream_read_dword_le(s);
    w->nBlockAlign = stream_read_word_le(s);
    w->wBitsPerSample = sh_audio->samplesize = stream_read_word_le(s);
    w->cbSize = 0;
    l -= 16;
    if(l)
      stream_skip(s,l);
    demuxer->movi_start = stream_tell(s);
    demuxer->movi_end = s->end_pos;
  } break;
  }

  priv = (da_priv_t*)malloc(sizeof(da_priv_t));
  priv->frmt = frmt;
  demuxer->priv = priv;
  demuxer->audio->id = 0;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;

  if(stream_tell(s) != demuxer->movi_start)
    stream_seek(s,demuxer->movi_start);

  mp_msg(MSGT_DEMUX,MSGL_V,"demux_audio: audio data 0x%X - 0x%X  \n",demuxer->movi_start,demuxer->movi_end);

  return 1;
}


int demux_audio_fill_buffer(demux_stream_t *ds) {
  sh_audio_t* sh_audio;
  demuxer_t* demux;
  da_priv_t* priv;
  stream_t* s;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = ds->sh;
  demux = ds->demuxer;
  priv = demux->priv;
  s = demux->stream;

  if(s->eof || (demux->movi_end && stream_tell(s) >= demux->movi_end) )
    return 0;

  switch(priv->frmt) {
  case MP3 :
    while(! s->eof || (demux->movi_end && stream_tell(s) >= demux->movi_end) ) {
      uint8_t hdr[4];
      int len;
      stream_read(s,hdr,4);
      len = mp_decode_mp3_header(hdr);
      if(len < 0) {
	stream_skip(s,-3);
      } else {
	demux_packet_t* dp;
	if(s->eof  || (demux->movi_end && stream_tell(s) >= demux->movi_end) )
	  return 0;
	dp = new_demux_packet(len);
	memcpy(dp->buffer,hdr,4);
	stream_read(s,dp->buffer + 4,len-4);
	ds_add_packet(ds,dp);
	return 1;
      }
    } break;
  case WAV : {
    int l = sh_audio->wf->nAvgBytesPerSec;
    demux_packet_t*  dp = new_demux_packet(l);
    stream_read(s,dp->buffer,l);
    ds_add_packet(ds,dp);
    return 1;
  }
  default:
    printf("Audio demuxer : unknow format %d\n",priv->frmt);
  }


  return 0;
}

void demux_audio_seek(demuxer_t *demuxer,float rel_seek_secs,int flags){
  sh_audio_t* sh_audio;
  stream_t* s;
  int base,pos;
  float len;
  da_priv_t* priv;

  sh_audio = demuxer->audio->sh;
  s = demuxer->stream;
  priv = demuxer->priv;

  base = flags&1 ? demuxer->movi_start : stream_tell(s) ;
  len = (demuxer->movi_end && flags&2) ? (demuxer->movi_end - demuxer->movi_start)*rel_seek_secs : rel_seek_secs;

  pos = base+(len*sh_audio->i_bps);

  if(demuxer->movi_end && pos >= demuxer->movi_end) {
    sh_audio->timer = (stream_tell(s) - demuxer->movi_start)/sh_audio->i_bps;
    return;
  } else if(pos < demuxer->movi_start)
    pos = demuxer->movi_start;
  

  sh_audio->timer = flags&1 ? rel_seek_secs : (pos-demuxer->movi_start)/sh_audio->i_bps;

  switch(priv->frmt) {
  case WAV:
    pos += (pos % (sh_audio->channels * sh_audio->samplesize) );
    break;
  }

  stream_seek(s,pos);

  resync_audio_stream(sh_audio);

}
