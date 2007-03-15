
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "config.h"
#include "version.h"

//#include "stream/stream.h"
//#include "demuxer.h"
//#include "stheader.h"
#include "aviheader.h"
#include "ms_hdr.h"

#include "stream/stream.h"
#include "muxer.h"

static muxer_stream_t* rawvideofile_new_stream(muxer_t *muxer,int type){
    muxer_stream_t* s;
    if (!muxer) return NULL;
    s=malloc(sizeof(muxer_stream_t));
    memset(s,0,sizeof(muxer_stream_t));
    if(!s) return NULL; // no mem!?
    muxer->streams[muxer->avih.dwStreams]=s;
    s->type=type;
    s->id=muxer->avih.dwStreams;
    s->timer=0.0;
    s->size=0;
    s->muxer=muxer;
    switch(type){
    case MUXER_TYPE_VIDEO:
      s->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'d','c');
      s->h.fccType=streamtypeVIDEO;
      if(!muxer->def_v) muxer->def_v=s;
      break;
    }
    muxer->avih.dwStreams++;
    return s;
}

static void write_rawvideo_chunk(stream_t *stream,int len,void* data){
    if(len>0){
	if(data){
	    // DATA
            stream_write_buffer(stream,data,len);
	}
    }
}

static void rawvideofile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags, double dts, double pts){
    muxer_t *muxer=s->muxer;

    // write out the chunk:
    if (s->type == MUXER_TYPE_VIDEO)
    write_rawvideo_chunk(muxer->stream,len,s->buffer); /* unsigned char */

    // if((unsigned int)len>s->h.dwSuggestedBufferSize) s->h.dwSuggestedBufferSize=len;

}

static void rawvideofile_write_header(muxer_t *muxer){
    return;
}

static void rawvideofile_write_index(muxer_t *muxer){
    return;
}

int muxer_init_muxer_rawvideo(muxer_t *muxer){
  muxer->cont_new_stream = &rawvideofile_new_stream;
  muxer->cont_write_chunk = &rawvideofile_write_chunk;
  muxer->cont_write_header = &rawvideofile_write_header;
  muxer->cont_write_index = &rawvideofile_write_index;
  return 1;
}
