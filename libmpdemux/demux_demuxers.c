
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

typedef struct dd_priv {
  demuxer_t* vd;
  demuxer_t* ad;
  demuxer_t* sd;
} dd_priv_t;


demuxer_t*  new_demuxers_demuxer(demuxer_t* vd, demuxer_t* ad, demuxer_t* sd) {
  demuxer_t* ret;
  dd_priv_t* priv;

  ret = (demuxer_t*)calloc(1,sizeof(demuxer_t));
  
  priv = (dd_priv_t*)malloc(sizeof(dd_priv_t));
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
  
  return ret;
}

int demux_demuxers_fill_buffer(demuxer_t *demux,demux_stream_t *ds) {
  dd_priv_t* priv;

  priv=demux->priv;

  if(ds->demuxer == priv->vd)
    return demux_fill_buffer(priv->vd,ds);
  else if(ds->demuxer == priv->ad)
    return demux_fill_buffer(priv->ad,ds);
  else if(ds->demuxer == priv->sd)
    return demux_fill_buffer(priv->sd,ds);
 
  printf("Demux demuxers fill_buffer error : bad demuxer : not vd, ad nor sd\n");
  return 0;
}

void demux_demuxers_seek(demuxer_t *demuxer,float rel_seek_secs,int flags) {
  dd_priv_t* priv;
  float pos;
  priv=demuxer->priv;

  priv->ad->stream->eof = 0;
  priv->sd->stream->eof = 0;

  // Seek video
  demux_seek(priv->vd,rel_seek_secs,flags);
  // Get the new pos
  pos = demuxer->video->pts;

  if(priv->ad != priv->vd) {
    sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;
    demux_seek(priv->ad,pos,1);
    // In case the demuxer don't set pts
    if(!demuxer->audio->pts)
      demuxer->audio->pts = pos-((ds_tell_pts(demuxer->audio)-sh->a_in_buffer_len)/(float)sh->i_bps);
    if(sh->timer)
      sh->timer = 0;
  }

  if(priv->sd != priv->vd)
      demux_seek(priv->sd,pos,1);

}

void demux_close_demuxers(demuxer_t* demuxer) {
  int i;
  dd_priv_t* priv = demuxer->priv;

  if(priv->vd)
    free_demuxer(priv->vd);
  if(priv->ad && priv->ad != priv->vd)
    free_demuxer(priv->ad);
  if(priv->sd && priv->sd != priv->vd && priv->sd != priv->ad)
    free_demuxer(priv->sd);

  free(priv);
  if(demuxer->info) {
    for(i=0;demuxer->info[i] != NULL; i++)
      free(demuxer->info[i]);
    free(demuxer->info);
  }
  free(demuxer);
}
  
