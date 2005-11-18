
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
