
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "config.h"
#include "../version.h"

//#include "stream.h"
//#include "demuxer.h"
//#include "stheader.h"
#include "aviheader.h"
#include "ms_hdr.h"

#include "bswap.h"

#include "muxer.h"

static muxer_stream_t* rawvideofile_new_stream(muxer_t *muxer,int type){
    muxer_stream_t* s;
    if (!muxer) return NULL;
    if (type == MUXER_TYPE_AUDIO) {
	printf("Rawvideo muxer does not support audio !\n");
	return NULL;
    }
    if(muxer->avih.dwStreams>=1){
	printf("Too many streams! Rawvideo muxer supports only one video stream !\n");
	return NULL;
    }
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
    default:
      printf("WarninG! unknown stream type: %d\n",type);
      return NULL;
    }
    muxer->avih.dwStreams++;
    return s;
}

static void write_rawvideo_chunk(FILE *f,int len,void* data){
    if(len>0){
	if(data){
	    // DATA
	    fwrite(data,len,1,f);
	}
    }
}

static void rawvideofile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags){
    muxer_t *muxer=s->muxer;

    // write out the chunk:
    write_rawvideo_chunk(muxer->file,len,s->buffer); /* unsigned char */

    // alter counters:
    if(s->h.dwSampleSize){
	// CBR
	s->h.dwLength+=len/s->h.dwSampleSize;
	if(len%s->h.dwSampleSize) printf("Warning! len isn't divisable by samplesize!\n");
    } else {
	// VBR
	s->h.dwLength++;
    }
    s->timer=(double)s->h.dwLength*s->h.dwScale/s->h.dwRate;
    s->size+=len;
    // if((unsigned int)len>s->h.dwSuggestedBufferSize) s->h.dwSuggestedBufferSize=len;

}

static void rawvideofile_write_header(muxer_t *muxer){
    return;
}

static void rawvideofile_write_index(muxer_t *muxer){
    return;
}

void muxer_init_muxer_rawvideo(muxer_t *muxer){
  muxer->cont_new_stream = &rawvideofile_new_stream;
  muxer->cont_write_chunk = &rawvideofile_write_chunk;
  muxer->cont_write_header = &rawvideofile_write_header;
  muxer->cont_write_index = &rawvideofile_write_index;
}
