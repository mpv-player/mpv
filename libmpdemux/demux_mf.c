
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "mf.h"

static void demux_seek_mf(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
  mf_t * mf = (mf_t *)demuxer->priv;
  sh_video_t   * sh_video = demuxer->video->sh;
  int newpos = (flags & 1)?0:mf->curr_frame - 1;
  
  if ( flags & 2 ) newpos+=rel_seek_secs*(mf->nr_of_files - 1);
   else newpos+=rel_seek_secs * sh_video->fps;
  if ( newpos < 0 ) newpos=0;
  if( newpos >= mf->nr_of_files) newpos=mf->nr_of_files - 1;
  mf->curr_frame=newpos;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_mf_fill_buffer(demuxer_t *demuxer, demux_stream_t *ds){
  mf_t         * mf;
  struct stat    fs;
  FILE         * f;

  mf=(mf_t*)demuxer->priv;
  if ( mf->curr_frame >= mf->nr_of_files ) return 0;

  stat( mf->names[mf->curr_frame],&fs );
//  printf( "[demux_mf] frame: %d (%s,%d)\n",mf->curr_frame,mf->names[mf->curr_frame],fs.st_size );

#ifdef WIN32
  if ( !( f=fopen( mf->names[mf->curr_frame],"rb" ) ) ) return 0;
#else
  if ( !( f=fopen( mf->names[mf->curr_frame],"r" ) ) ) return 0;
#endif
  {
   sh_video_t     * sh_video = demuxer->video->sh;
   demux_packet_t * dp = new_demux_packet( fs.st_size );
   if ( !fread( dp->buffer,fs.st_size,1,f ) ) return 0;
   dp->pts=mf->curr_frame / sh_video->fps;
   dp->pos=mf->curr_frame;
   dp->flags=0;
   // append packet to DS stream:
   ds_add_packet( demuxer->video,dp );
  }
  fclose( f );

  mf->curr_frame++;
  return 1;
}

static demuxer_t* demux_open_mf(demuxer_t* demuxer){
  sh_video_t   *sh_video = NULL;
  mf_t         *mf = NULL;
  
  if(!demuxer->stream->url) return NULL;
  if(strncmp(demuxer->stream->url, "mf://", 5)) return NULL;


  mf=open_mf(demuxer->stream->url + 5);
  if(!mf) return NULL;

  if(!mf_type){
    char* p=strrchr(mf->names[0],'.');
    if(!p){
      mp_msg(MSGT_DEMUX, MSGL_INFO, "[demux_mf] file type was not set! (try -mf type=xxx)\n" );
      free( mf ); return NULL;
    }
    mf_type=strdup(p+1);
    mp_msg(MSGT_DEMUX, MSGL_INFO, "[demux_mf] file type was not set! trying 'type=%s'...\n", mf_type);
  }

  mf->curr_frame=0;

  demuxer->movi_start = 0;
  demuxer->movi_end = mf->nr_of_files - 1;

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);
  // make sure the demuxer knows about the new video stream header
  // (even though new_sh_video() ought to take care of it)
  demuxer->video->sh = sh_video;

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream (this is getting wacky), or else
  // video_read_properties() will choke
  sh_video->ds = demuxer->video;
  
  if ( !strcasecmp( mf_type,"jpg" ) || 
        !(strcasecmp(mf_type, "jpeg"))) sh_video->format = mmioFOURCC('I', 'J', 'P', 'G');
  else 
     if ( !strcasecmp( mf_type,"png" )) sh_video->format = mmioFOURCC('M', 'P', 'N', 'G' );
  else
     if ( !strcasecmp( mf_type,"tga" )) sh_video->format = mmioFOURCC('M', 'T', 'G', 'A' );
   else
     if (!strcasecmp( mf_type,"sgi" )) sh_video->format = mmioFOURCC('S', 'G', 'I', '1');
  else { mp_msg(MSGT_DEMUX, MSGL_INFO, "[demux_mf] unknown input file type.\n" ); free( mf ); return NULL; }

  sh_video->disp_w = mf_w;
  sh_video->disp_h = mf_h;
  sh_video->fps = mf_fps;
  sh_video->frametime = 1 / sh_video->fps;

  // emulate BITMAPINFOHEADER:
  sh_video->bih=malloc(sizeof(BITMAPINFOHEADER));
  memset(sh_video->bih,0,sizeof(BITMAPINFOHEADER));
  sh_video->bih->biSize=40;
  sh_video->bih->biWidth = mf_w;
  sh_video->bih->biHeight = mf_h;
  sh_video->bih->biPlanes=1;
  sh_video->bih->biBitCount=24;
  sh_video->bih->biCompression=sh_video->format;
  sh_video->bih->biSizeImage=sh_video->bih->biWidth*sh_video->bih->biHeight*3;

  /* disable seeking */
//  demuxer->seekable = 0;

  demuxer->priv=(void*)mf;

  return demuxer;
}

static void demux_close_mf(demuxer_t* demuxer) {
  mf_t *mf = demuxer->priv;

  if(!mf)
    return;
  free(mf);  
}

static int demux_control_mf(demuxer_t *demuxer, int cmd, void *arg) {
  mf_t *mf = (mf_t *)demuxer->priv;
  sh_video_t *sh_video = demuxer->video->sh;

  switch(cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
      *((double *)arg) = (double)mf->nr_of_files / sh_video->fps;
      return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_PERCENT_POS:
      if (mf->nr_of_files <= 1)
        return DEMUXER_CTRL_DONTKNOW;
      *((int *)arg) = 100 * mf->curr_frame / (mf->nr_of_files - 1);
      return DEMUXER_CTRL_OK;

    default:
      return DEMUXER_CTRL_NOTIMPL;
  }
}

demuxer_desc_t demuxer_desc_mf = {
  "mf demuxer",
  "mf",
  "MF",
  "?",
  "multiframe?, pictures demuxer",
  DEMUXER_TYPE_MF,
  0, // no autodetect
  NULL,
  demux_mf_fill_buffer,
  demux_open_mf,
  demux_close_mf,
  demux_seek_mf,
  demux_control_mf
};
