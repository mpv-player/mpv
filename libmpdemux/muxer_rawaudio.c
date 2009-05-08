/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "help_mp.h"
#include "mp_msg.h"

#include "aviheader.h"
#include "ms_hdr.h"

#include "stream/stream.h"
#include "muxer.h"

static muxer_stream_t* rawaudiofile_new_stream(muxer_t *muxer,int type){
    muxer_stream_t* s;
    if (!muxer) return NULL;
    if(type==MUXER_TYPE_AUDIO && muxer->avih.dwStreams>=1){
        mp_msg(MSGT_MUXER,MSGL_ERR,MSGTR_TooManyStreams" "MSGTR_RawMuxerOnlyOneStream);
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
    case MUXER_TYPE_AUDIO:
        s->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'d','c');
        s->h.fccType=streamtypeAUDIO;
        muxer->avih.dwStreams++;
        break;
    case MUXER_TYPE_VIDEO:
        mp_msg(MSGT_MUXER,MSGL_WARN,MSGTR_IgnoringVideoStream);
        s->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'d','c');
        s->h.fccType=streamtypeAUDIO;
        break;
    default:
        mp_msg(MSGT_MUXER,MSGL_ERR,MSGTR_UnknownStreamType,type);
        return NULL;
    }
    return s;
}

static void rawaudiofile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags, double dts, double pts){
    muxer_t *muxer=s->muxer;

    // write out the chunk:
    if (s->type==MUXER_TYPE_AUDIO)
        stream_write_buffer(muxer->stream, s->buffer, len);
}

static void rawaudiofile_write_header(muxer_t *muxer){
    return;
}

static void rawaudiofile_write_index(muxer_t *muxer){
    return;
}

int muxer_init_muxer_rawaudio(muxer_t *muxer){
    muxer->cont_new_stream = &rawaudiofile_new_stream;
    muxer->cont_write_chunk = &rawaudiofile_write_chunk;
    muxer->cont_write_header = &rawaudiofile_write_header;
    muxer->cont_write_index = &rawaudiofile_write_index;
    return 1;
}
