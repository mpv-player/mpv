
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "config.h"
#include "version.h"

#include "aviheader.h"
#include "ms_hdr.h"

#include "muxer.h"
#include "stream.h"
#include "demuxer.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "stheader.h"

muxer_t *muxer_new_muxer(int type,FILE *f){
    muxer_t* muxer=malloc(sizeof(muxer_t));
    memset(muxer,0,sizeof(muxer_t));
    muxer->file = f;
    switch (type) {
      case MUXER_TYPE_MPEG:
	if(! muxer_init_muxer_mpeg(muxer))
	  return NULL;
	break;
      case MUXER_TYPE_RAWVIDEO:
        if(! muxer_init_muxer_rawvideo(muxer))
	  return NULL;
	break;
      case MUXER_TYPE_RAWAUDIO:
        if(! muxer_init_muxer_rawaudio(muxer))
          return NULL;
        break;
#ifdef USE_LIBAVFORMAT
      case MUXER_TYPE_LAVF:
        if(! muxer_init_muxer_lavf(muxer))
	  return NULL;
        break;
#endif
      case MUXER_TYPE_AVI:
      default:
	if(! muxer_init_muxer_avi(muxer))
	  return NULL;
    }
    return muxer;
}

/* buffer frames until we either:
 * (a) have at least one frame from each stream
 * (b) run out of memory */
void muxer_write_chunk(muxer_stream_t *s, size_t len, unsigned int flags) {
    if (s->muxer->muxbuf_skip_buffer) {
      s->muxer->cont_write_chunk(s, len, flags);
    }
    else {
      int num = s->muxer->muxbuf_num++;
      muxbuf_t *buf, *tmp;
      
      tmp = realloc(s->muxer->muxbuf, (num+1) * sizeof(muxbuf_t));
      if(!tmp) {
        mp_msg(MSGT_MUXER, MSGL_FATAL, MSGTR_MuxbufReallocErr);
        return;
      }
      s->muxer->muxbuf = tmp;
      buf = s->muxer->muxbuf + num;
      
      /* buffer this frame */
      buf->stream = s;
      buf->timer = s->timer;
      buf->len = len;
      buf->flags = flags;
      buf->buffer = malloc(len * sizeof (unsigned char));
      if (!buf->buffer) {
        mp_msg(MSGT_MUXER, MSGL_FATAL, MSGTR_MuxbufMallocErr);
        return;
      }
      memcpy(buf->buffer, s->buffer, buf->len);
      s->muxbuf_seen = 1;

      /* see if we need to keep buffering */
      s->muxer->muxbuf_skip_buffer = 1;
      for (num = 0; s->muxer->streams[num]; ++num)
        if (!s->muxer->streams[num]->muxbuf_seen)
          s->muxer->muxbuf_skip_buffer = 0;
      
      /* see if we can flush buffer now */
      if (s->muxer->muxbuf_skip_buffer) {
        muxbuf_t *tmp_buf = malloc(sizeof(muxbuf_t));
        if (!tmp_buf) {
          mp_msg(MSGT_MUXER, MSGL_FATAL, MSGTR_MuxbufMallocErr);
          return;
        }
        mp_msg(MSGT_MUXER, MSGL_V, MSGTR_MuxbufSending, s->muxer->muxbuf_num);
        
        /* fix parameters for all streams */
        for (num = 0; s->muxer->streams[num]; ++num) {
          muxer_stream_t *str = s->muxer->streams[num];
          if(str->muxer->fix_stream_parameters)
            muxer_stream_fix_parameters(str->muxer, str);
        }
        
        /* write header */
        if (s->muxer->cont_write_header)
          muxer_write_header(s->muxer);
        
        /* send all buffered frames to muxer */
        for (num = 0; num < s->muxer->muxbuf_num; ++num) {
          buf = s->muxer->muxbuf + num;
          s = buf->stream;
          
          /* 1. save timer and buffer (might have changed by now) */
          tmp_buf->timer = s->timer;
          tmp_buf->buffer = s->buffer;
          
          /* 2. move stored timer and buffer into stream and mux it */
          s->timer = buf->timer;
          s->buffer = buf->buffer;
          s->muxer->cont_write_chunk(s, buf->len, buf->flags);
          
          /* 3. restore saved timer and buffer */
          s->timer = tmp_buf->timer;
          s->buffer = tmp_buf->buffer;
        }
        free(tmp_buf);
        
        free(s->muxer->muxbuf);
        s->muxer->muxbuf_num = 0;
      }
    }
    
    /* this code moved directly from muxer_avi.c */
    // alter counters:
    if(s->h.dwSampleSize){
      // CBR
      s->h.dwLength+=len/s->h.dwSampleSize;
      if(len%s->h.dwSampleSize) mp_msg(MSGT_MUXER, MSGL_WARN, MSGTR_WarningLenIsntDivisible);
    } else {
      // VBR
      s->h.dwLength++;
    }
    s->timer=(double)s->h.dwLength*s->h.dwScale/s->h.dwRate;
    s->size+=len;
    
    return;
}

