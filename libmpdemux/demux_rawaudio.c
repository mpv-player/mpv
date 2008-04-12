
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "m_option.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"


static int channels = 2;
static int samplerate = 44100;
static int samplesize = 2;
static int bitrate = 0;
static int format = 0x1; // Raw PCM

const m_option_t demux_rawaudio_opts[] = {
  { "channels", &channels, CONF_TYPE_INT,CONF_RANGE,1,8, NULL },
  { "rate", &samplerate, CONF_TYPE_INT,CONF_RANGE,1000,8*48000, NULL },
  { "samplesize", &samplesize, CONF_TYPE_INT,CONF_RANGE,1,8, NULL },
  { "bitrate", &bitrate, CONF_TYPE_INT,CONF_MIN,0,0, NULL },
  { "format", &format, CONF_TYPE_INT, CONF_MIN, 0 , 0, NULL },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};


static demuxer_t* demux_rawaudio_open(demuxer_t* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;

  sh_audio = new_sh_audio(demuxer,0);
  sh_audio->wf = w = malloc(sizeof(WAVEFORMATEX));
  w->wFormatTag = sh_audio->format = format;
  w->nChannels = sh_audio->channels = channels;
  w->nSamplesPerSec = sh_audio->samplerate = samplerate;
  if (bitrate > 999)
    w->nAvgBytesPerSec = bitrate/8;
  else if (bitrate > 0)
    w->nAvgBytesPerSec = bitrate*125;
  else
    w->nAvgBytesPerSec = samplerate*samplesize*channels;
  w->nBlockAlign = channels*samplesize;
  sh_audio->samplesize = samplesize;
  w->wBitsPerSample = 8*samplesize;
  w->cbSize = 0;

  demuxer->movi_start = demuxer->stream->start_pos;
  demuxer->movi_end = demuxer->stream->end_pos;

  demuxer->audio->id = 0;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;

  return demuxer;
}

static int demux_rawaudio_fill_buffer(demuxer_t* demuxer, demux_stream_t *ds) {
  sh_audio_t* sh_audio = demuxer->audio->sh;
  int l = sh_audio->wf->nAvgBytesPerSec;
  off_t spos = stream_tell(demuxer->stream);
  demux_packet_t*  dp;

  if(demuxer->stream->eof)
    return 0;

  dp = new_demux_packet(l);
  dp->pts = (spos - demuxer->movi_start)  / (float)(sh_audio->wf->nAvgBytesPerSec);
  dp->pos = (spos - demuxer->movi_start);

  l = stream_read(demuxer->stream,dp->buffer,l);
  resize_demux_packet(dp, l);
  ds_add_packet(ds,dp);

  return 1;
}

static void demux_rawaudio_seek(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
  stream_t* s = demuxer->stream;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  off_t base,pos;

  base = (flags & SEEK_ABSOLUTE) ? demuxer->movi_start : stream_tell(s);
  if(flags & SEEK_FACTOR)
    pos = base + ((demuxer->movi_end - demuxer->movi_start)*rel_seek_secs);
  else
    pos = base + (rel_seek_secs*sh_audio->i_bps);

  pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
  stream_seek(s,pos);
//  printf("demux_rawaudio: streamtell=%d\n",(int)stream_tell(demuxer->stream));
}


const demuxer_desc_t demuxer_desc_rawaudio = {
  "Raw audio demuxer",
  "rawaudio",
  "rawaudio",
  "?",
  "",
  DEMUXER_TYPE_RAWAUDIO,
  0, // no autodetect
  NULL,
  demux_rawaudio_fill_buffer,
  demux_rawaudio_open,
  NULL,
  demux_rawaudio_seek,
  NULL
};
