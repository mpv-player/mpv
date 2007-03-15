
#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include <stdlib.h>
#include <stdio.h>
#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

typedef struct dd_priv {
  demuxer_t* vd;
  demuxer_t* ad;
  demuxer_t* sd;
} dd_priv_t;

extern demuxer_desc_t demuxer_desc_demuxers;

demuxer_t*  new_demuxers_demuxer(demuxer_t* vd, demuxer_t* ad, demuxer_t* sd) {
  demuxer_t* ret;
  dd_priv_t* priv;

  ret = calloc(1,sizeof(demuxer_t));
  
  priv = malloc(sizeof(dd_priv_t));
  priv->vd = vd;
  priv->ad = ad;
  priv->sd = sd;
  ret->priv = priv;

  ret->type = ret->file_format = DEMUXER_TYPE_DEMUXERS;
  // Video is the most important :-)
  ret->stream = vd->stream;
  ret->seekable = vd->seekable && ad->seekable && sd->seekable;
 
  ret->video = vd->video;
  ret->audio = ad->audio;
  ret->sub = sd->sub;

  ret->desc = &demuxer_desc_demuxers;

  return ret;
}

static int demux_demuxers_fill_buffer(demuxer_t *demux,demux_stream_t *ds) {
  dd_priv_t* priv;

  priv=demux->priv;

  if(ds->demuxer == priv->vd)
    return demux_fill_buffer(priv->vd,ds);
  else if(ds->demuxer == priv->ad)
    return demux_fill_buffer(priv->ad,ds);
  else if(ds->demuxer == priv->sd)
    return demux_fill_buffer(priv->sd,ds);
 
  mp_msg(MSGT_DEMUX,MSGL_WARN,MSGTR_MPDEMUX_DEMUXERS_FillBufferError);
  return 0;
}

static void demux_demuxers_seek(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags) {
  dd_priv_t* priv;
  float pos;
  priv=demuxer->priv;

  priv->ad->stream->eof = 0;
  priv->sd->stream->eof = 0;

  // Seek video
  demux_seek(priv->vd,rel_seek_secs,audio_delay,flags);
  // Get the new pos
  pos = demuxer->video->pts;
  if (!pos) {
    demux_fill_buffer(priv->vd, demuxer->video);
    if (demuxer->video->first)
      pos = demuxer->video->first->pts;
  }

  if(priv->ad != priv->vd) {
    sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;
    demux_seek(priv->ad,pos,audio_delay,1);
    // In case the demuxer don't set pts
    if(!demuxer->audio->pts)
      demuxer->audio->pts = pos-((ds_tell_pts(demuxer->audio)-sh->a_in_buffer_len)/(float)sh->i_bps);
  }

  if(priv->sd != priv->vd)
      demux_seek(priv->sd,pos,audio_delay,1);

}

static void demux_close_demuxers(demuxer_t* demuxer) {
  dd_priv_t* priv = demuxer->priv;
  stream_t *s;

  if(priv->vd)
    free_demuxer(priv->vd);
  if(priv->ad && priv->ad != priv->vd) {
    // That's a hack to free the audio file stream
    // It's ok atm but we shouldn't free that here
    s = priv->ad->stream;
    free_demuxer(priv->ad);
    free_stream(s);
  } if(priv->sd && priv->sd != priv->vd && priv->sd != priv->ad) {
    s = priv->sd->stream;
    free_demuxer(priv->sd);
    free_stream(s);
  }

  free(priv);
}
  

static int demux_demuxers_control(demuxer_t *demuxer,int cmd, void *arg){
  dd_priv_t* priv = demuxer->priv;
  switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
      *((double *)arg) = demuxer_get_time_length(priv->vd);
      return DEMUXER_CTRL_OK;
    case DEMUXER_CTRL_GET_PERCENT_POS:
      *((int *)arg) = demuxer_get_percent_pos(priv->vd);
      return DEMUXER_CTRL_OK;
  }
  return DEMUXER_CTRL_NOTIMPL;
}

demuxer_desc_t demuxer_desc_demuxers = {
  "Demuxers demuxer",
  "", // Not selectable
  "",
  "?",
  "internal use only",
  DEMUXER_TYPE_DEMUXERS,
  0, // no autodetect
  NULL,
  demux_demuxers_fill_buffer,
  NULL,
  demux_close_demuxers,
  demux_demuxers_seek,
  demux_demuxers_control
};
