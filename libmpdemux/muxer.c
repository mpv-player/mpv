
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "config.h"
#include "../version.h"

#include "aviheader.h"
#include "ms_hdr.h"

#include "muxer.h"

muxer_t *muxer_new_muxer(int type,FILE *f){
    muxer_t* muxer=malloc(sizeof(muxer_t));
    memset(muxer,0,sizeof(muxer_t));
    muxer->file = f;
    switch (type) {
      case MUXER_TYPE_MPEG:
	muxer_init_muxer_mpeg(muxer);
	break;
      case MUXER_TYPE_RAWVIDEO:
        muxer_init_muxer_rawvideo(muxer);
	break;
      case MUXER_TYPE_AVI:
      default:
	muxer_init_muxer_avi(muxer);
    }
    return muxer;
}
