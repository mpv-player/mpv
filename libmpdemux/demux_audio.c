
#include "config.h"
#include "../mp_msg.h"

#include <stdlib.h>
#include <stdio.h>
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "genres.h"
#include "mp3_hdr.h"

#include <string.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif

#define MP3 1
#define WAV 2


#define HDR_SIZE 4

typedef struct da_priv {
  int frmt;
  float last_pts;
} da_priv_t;

extern void free_sh_audio(sh_audio_t* sh);
extern void resync_audio_stream(sh_audio_t *sh_audio);

static int hr_mp3_seek = 0;

int demux_audio_open(demuxer_t* demuxer) {
  stream_t *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  int st_pos = 0,frmt = 0, n = 0, pos = 0, step, mp3_freq,mp3_chans;
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
    } else if((n = mp_get_mp3_header(hdr,&mp3_chans,&mp3_freq)) > 0) {
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
    sh_audio->audio.dwSampleSize= 0;
    sh_audio->audio.dwScale = 1152;
    sh_audio->audio.dwRate = mp3_freq;
    sh_audio->wf = malloc(sizeof(WAVEFORMATEX));
    sh_audio->wf->wFormatTag = sh_audio->format;
    sh_audio->wf->nChannels = mp3_chans;
    sh_audio->wf->nSamplesPerSec = mp3_freq;
    sh_audio->wf->nBlockAlign = 1;
    sh_audio->wf->wBitsPerSample = 16;
    sh_audio->wf->cbSize = 0;    
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
	buf[4] = '\0';
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
    unsigned int chunk_type;
    unsigned int chunk_size;
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
    do
    {
      chunk_type = stream_read_fourcc(demuxer->stream);
      chunk_size = stream_read_dword_le(demuxer->stream);
      if (chunk_type != mmioFOURCC('d', 'a', 't', 'a'))
        stream_skip(demuxer->stream, chunk_size);
    } while (chunk_type != mmioFOURCC('d', 'a', 't', 'a'));
    demuxer->movi_start = stream_tell(s);
    demuxer->movi_end = s->end_pos;
  } break;
  }

  priv = (da_priv_t*)malloc(sizeof(da_priv_t));
  priv->frmt = frmt;
  priv->last_pts = -1;
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
	priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + 1152/(float)sh_audio->samplerate;
	ds->pts = priv->last_pts - (ds_tell_pts(demux->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	ds_add_packet(ds,dp);
	return 1;
      }
    } break;
  case WAV : {
    int l = sh_audio->wf->nAvgBytesPerSec;
    demux_packet_t*  dp = new_demux_packet(l);
    stream_read(s,dp->buffer,l);
    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + l/(float)sh_audio->i_bps;
    ds->pts = priv->last_pts - (ds_tell_pts(demux->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    ds_add_packet(ds,dp);
    return 1;
  }
  default:
    printf("Audio demuxer : unknown format %d\n",priv->frmt);
  }


  return 0;
}

static void high_res_mp3_seek(demuxer_t *demuxer,float time) {
  uint8_t hdr[4];
  int len,nf;
  da_priv_t* priv = demuxer->priv;
  sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;

  nf = time*sh->samplerate/1152;
  while(nf > 0) {
    stream_read(demuxer->stream,hdr,4);
    len = mp_decode_mp3_header(hdr);
    if(len < 0) {
      stream_skip(demuxer->stream,-3);
      continue;
    }
    stream_skip(demuxer->stream,len-4);
    priv->last_pts += 1152/(float)sh->samplerate;
    nf--;
  }
}

void demux_audio_seek(demuxer_t *demuxer,float rel_seek_secs,int flags){
  sh_audio_t* sh_audio;
  stream_t* s;
  int base,pos;
  float len;
  da_priv_t* priv;

  if(!(sh_audio = demuxer->audio->sh))
    return;
  s = demuxer->stream;
  priv = demuxer->priv;

  if(priv->frmt == MP3 && hr_mp3_seek && !(flags & 2)) {
    len = (flags & 1) ? rel_seek_secs - priv->last_pts : rel_seek_secs;
    if(len < 0) {
      stream_seek(s,demuxer->movi_start);
      len = priv->last_pts + len;
      priv->last_pts = 0;
    }
    if(len > 0)
      high_res_mp3_seek(demuxer,len);
    sh_audio->timer = priv->last_pts -  (ds_tell_pts(demuxer->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    resync_audio_stream(sh_audio);
    return;
  }

  base = flags&1 ? demuxer->movi_start : stream_tell(s);
  if(flags&2)
    pos = base + ((demuxer->movi_end - demuxer->movi_start)*rel_seek_secs);
  else
    pos = base + (rel_seek_secs*sh_audio->i_bps);

  if(demuxer->movi_end && pos >= demuxer->movi_end) {
    sh_audio->timer = (stream_tell(s) - demuxer->movi_start)/(float)sh_audio->i_bps;
    return;
  } else if(pos < demuxer->movi_start)
    pos = demuxer->movi_start;

  priv->last_pts = (pos-demuxer->movi_start)/(float)sh_audio->i_bps;
  sh_audio->timer = priv->last_pts - (ds_tell_pts(demuxer->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
  
  switch(priv->frmt) {
  case WAV:
    pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
    // We need to decrease the pts by one step to make it the "last one"
    priv->last_pts -= sh_audio->wf->nAvgBytesPerSec/(float)sh_audio->i_bps;
    break;
  }

  stream_seek(s,pos);

  resync_audio_stream(sh_audio);

}

void demux_close_audio(demuxer_t* demuxer) {
  da_priv_t* priv = demuxer->priv;

  if(!priv)
    return;
  free(priv);
}

/****************** Options stuff ******************/

#include "../cfgparser.h"

static config_t demux_audio_opts[] = {
  { "hr-mp3-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 0, 1, NULL },
  { "nohr-mp3-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 1, 0, NULL},
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

void demux_audio_register_options(m_config_t* cfg) {
  m_config_register_options(cfg,demux_audio_opts);
}
