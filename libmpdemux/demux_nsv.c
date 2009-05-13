/*
 * Nullsoft Streaming Video demuxer
 * copyright (c) 2004 by Reza Jelveh <reza.jelveh@tuhh.de>
 * Based on A'rpis G2 work
 *
 * seeking and PCM audio not yet supported
 * PCM needs extra audio chunk "miniheader" parsing
 *
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
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

typedef struct {
    float   v_pts;
    int video_pack_no;
    unsigned int a_format;
    unsigned int v_format;
    unsigned char fps;
} nsv_priv_t;

#define HEADER_SEARCH_SIZE 256000


/**
 * Seeking still to be implemented
 */
static void demux_seek_nsv ( demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags )
{
// seeking is not yet implemented
}


static int demux_nsv_fill_buffer ( demuxer_t *demuxer, demux_stream_t *ds )
{
    unsigned char hdr[17];
    // for the extra data
    unsigned char aux[6];
    int i_aux = 0;
    // videolen = audio chunk length, audiolen = video chunk length
    int videolen,audiolen;

    sh_video_t *sh_video = demuxer->video->sh;
    sh_audio_t *sh_audio = demuxer->audio->sh;

    nsv_priv_t * priv = demuxer->priv;

    // if the audio/video chunk has no new header the first 2 bytes will be discarded 0xBEEF
    // or rather 0xEF 0xBE
    stream_read(demuxer->stream,hdr,7);
    if(stream_eof(demuxer->stream)) return 0;
    // sometimes instead of 0xBEEF as described for the next audio/video chunk we get
    // a whole new header

    mp_dbg(MSGT_DEMUX,MSGL_DBG2,"demux_nsv: %08X %08X\n",hdr[0]<<8|hdr[1],stream_tell(demuxer->stream));
    switch(hdr[0]<<8|hdr[1]) {
        case 0x4E53:
            if(hdr[2]==0x56 && hdr[3]==0x73){
                // NSVs
                // get the header since there is no more metaheader after the first one
                // there is no more need to skip that
                stream_read(demuxer->stream,hdr+7,17-7);
                stream_read(demuxer->stream,hdr,7);
            }
            break;

        case 0xEFBE:
            break;

        default:
            mp_dbg(MSGT_DEMUX,MSGL_WARN,"demux_nsv: sync lost\n");
            break;
    }

    if (sh_video)
    sh_video->pts = priv->v_pts =demuxer->video->pts=  priv->video_pack_no *
         (float)sh_video->frametime;
    else
        priv->v_pts = priv->video_pack_no;

    demuxer->filepos=stream_tell(demuxer->stream);


    mp_dbg(MSGT_DEMUX,MSGL_DBG2,"demux_nsv: %08X: %02X %02X | %02X %02X %02X | %02X %02X  \n",
    (int)demuxer->filepos, hdr[0],hdr[1],hdr[2],hdr[3],hdr[4],hdr[5],hdr[6]);

    // read video:
    videolen=(hdr[2]>>4)|(hdr[3]<<4)|(hdr[4]<<0xC);
    //check if we got extra data like subtitles here
    if( (hdr[2]&0x0f) != 0x0 ) {
        stream_read( demuxer->stream, aux, 6);

        i_aux = aux[0]|aux[1]<<8;
        // We skip this extra data
        stream_skip( demuxer->stream, i_aux );
        i_aux+=6;
        videolen -= i_aux;
    }


    // we need to return an empty packet when the
    // video frame is empty otherwise the stream will fasten up
    if(sh_video) {
        if( (hdr[2]&0x0f) != 0x0 )
            ds_read_packet(demuxer->video,demuxer->stream,videolen,priv->v_pts,demuxer->filepos-i_aux,0);
        else
            ds_read_packet(demuxer->video,demuxer->stream,videolen,priv->v_pts,demuxer->filepos,0);
    }
    else
        stream_skip(demuxer->stream,videolen);

    // read audio:
    audiolen=(hdr[5])|(hdr[6]<<8);
    // we need to return an empty packet when the
    // audio frame is empty otherwise the stream will fasten up
    if(sh_audio) {
        ds_read_packet(demuxer->audio,demuxer->stream,audiolen,priv->v_pts,demuxer->filepos+videolen,0);
    }
    else
        stream_skip(demuxer->stream,audiolen);

    ++priv->video_pack_no;

    return 1;

}


static demuxer_t* demux_open_nsv ( demuxer_t* demuxer )
{
    // last 2 bytes 17 and 18 are unknown but right after that comes the length
    unsigned char hdr[17];
    int videolen,audiolen;
    unsigned char buf[10];
    sh_video_t *sh_video = NULL;
    sh_audio_t *sh_audio = NULL;


    nsv_priv_t * priv = malloc(sizeof(nsv_priv_t));
    demuxer->priv=priv;
    priv->video_pack_no=0;

      /* disable seeking yet to be fixed*/
    demuxer->seekable = 0;

    stream_read(demuxer->stream,hdr,4);
    if(stream_eof(demuxer->stream)) return 0;

    if(hdr[0]==0x4E && hdr[1]==0x53 && hdr[2]==0x56){
        // NSV header!
        if(hdr[3]==0x73){
            // NSVs
            stream_read(demuxer->stream,hdr+4,17-4);
        }

        if(hdr[3]==0x66){
            // NSVf
            int len=stream_read_dword_le(demuxer->stream);
            // TODO: parse out metadata!!!!
            stream_skip(demuxer->stream,len-8);

            // NSVs
            stream_read(demuxer->stream,hdr,17);
            if (stream_eof(demuxer->stream) || strncmp(hdr, "NSVs", 4))
                return 0;
        }

        // dummy debug message
        mp_msg(MSGT_DEMUX,MSGL_V,"demux_nsv: Header: %.12s\n",hdr);

        //   bytes 8-11   audio codec fourcc
        // PCM fourcc needs extra parsing for every audio chunk, yet to implement
        if((demuxer->audio->id != -2) && strncmp(hdr+8,"NONE", 4)){//&&strncmp(hdr+8,"VLB ", 4)){
            sh_audio = new_sh_audio ( demuxer, 0 );
            demuxer->audio->id = 0;
            demuxer->audio->sh = sh_audio;
            sh_audio->format=mmioFOURCC(hdr[8],hdr[9],hdr[10],hdr[11]);
            sh_audio->ds = demuxer->audio;
            priv->a_format=mmioFOURCC(hdr[8],hdr[9],hdr[10],hdr[11]);
        }

        // store hdr fps
        priv->fps=hdr[16];

        if ((demuxer->video->id != -2) && strncmp(hdr+4,"NONE", 4)) {
            /* Create a new video stream header */
            sh_video = new_sh_video ( demuxer, 0 );

            /* Make sure the demuxer knows about the new video stream header
             * (even though new_sh_video() ought to take care of it)
             */
            demuxer->video->sh = sh_video;

            /* Make sure that the video demuxer stream header knows about its
             * parent video demuxer stream (this is getting wacky), or else
             * video_read_properties() will choke
             */
            sh_video->ds = demuxer->video;

            //   bytes 4-7  video codec fourcc
            priv->v_format = sh_video->format=mmioFOURCC(hdr[4],hdr[5],hdr[6],hdr[7]);

            // new video stream! parse header
            sh_video->disp_w=hdr[12]|(hdr[13]<<8);
            sh_video->disp_h=hdr[14]|(hdr[15]<<8);
            sh_video->bih=calloc(1,sizeof(BITMAPINFOHEADER));
            sh_video->bih->biSize=sizeof(BITMAPINFOHEADER);
            sh_video->bih->biPlanes=1;
            sh_video->bih->biBitCount=24;
            sh_video->bih->biWidth=hdr[12]|(hdr[13]<<8);
            sh_video->bih->biHeight=hdr[14]|(hdr[15]<<8);
            memcpy(&sh_video->bih->biCompression,hdr+4,4);
            sh_video->bih->biSizeImage=sh_video->bih->biWidth*sh_video->bih->biHeight*3;

            // here we search for the correct keyframe
            // vp6 keyframe is when the 2nd byte of the vp6 header is
            // 0x36 for VP61 and 0x46 for VP62
            if((priv->v_format==mmioFOURCC('V','P','6','1')) ||
               (priv->v_format==mmioFOURCC('V','P','6','2')) ||
               (priv->v_format==mmioFOURCC('V','P','3','1'))) {
                stream_read(demuxer->stream,buf,10);
                if (((((priv->v_format>>16) & 0xff) == '6') && ((buf[8]&0x0e)!=0x06)) ||
                    ((((priv->v_format>>16) & 0xff) == '3') && (buf[8]!=0x00 || buf[9]!=0x08))) {
                    mp_msg(MSGT_DEMUX,MSGL_V,"demux_nsv: searching %.4s keyframe...\n", (char*)&priv->v_format);
                    while(((((priv->v_format>>16) & 0xff) == '6') && ((buf[8]&0x0e)!=0x06)) ||
                          ((((priv->v_format>>16) & 0xff) == '3') && (buf[8]!=0x00 || buf[9]!=0x08))){
                        mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_nsv: %.4s block skip.\n", (char*)&priv->v_format);
                        videolen=(buf[2]>>4)|(buf[3]<<4)|(buf[4]<<0xC);
                        audiolen=(buf[5])|(buf[6]<<8);
                        stream_skip(demuxer->stream, videolen+audiolen-3);
                        stream_read(demuxer->stream,buf,10);
                        if(stream_eof(demuxer->stream)) return 0;
                        if(buf[0]==0x4E){
                            mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_nsv: Got NSVs block.\n");
                            stream_skip(demuxer->stream,7);
                            stream_read(demuxer->stream,buf,10);
                        }
                    }
                }

                // data starts 10 bytes before current pos but later
                // we seek 17 backwards
                stream_skip(demuxer->stream,7);
            }

        switch(priv->fps){
        case 0x80:
            sh_video->fps=30;
            break;
        case 0x81:
            sh_video->fps=(float)30000.0/1001.0;
            break;
        case 0x82:
            sh_video->fps=25;
            break;
        case 0x83:
            sh_video->fps=(float)24000.0/1001.0;
            break;
        case 0x85:
            sh_video->fps=(float)15000.0/1001.0;
            break;
        case 0x89:
            sh_video->fps=(float)10000.0/1001.0;
            break;
        default:
            sh_video->fps = (float)priv->fps;
        }
        sh_video->frametime = (float)1.0 / (float)sh_video->fps;
       }
    }

    // seek to start of NSV header
    stream_seek(demuxer->stream,stream_tell(demuxer->stream)-17);

    return demuxer;
}

static int nsv_check_file ( demuxer_t* demuxer )
{
    uint32_t hdr = 0;
    int i;

    mp_msg ( MSGT_DEMUX, MSGL_V, "Checking for Nullsoft Streaming Video\n" );

    for (i = 0; i < HEADER_SEARCH_SIZE; i++) {
        uint8_t c = stream_read_char(demuxer->stream);
        if (stream_eof(demuxer->stream))
            return 0;
        if (hdr == mmioFOURCC('s', 'V', 'S', 'N') ||
            (hdr == mmioFOURCC('f', 'V', 'S', 'N') && !c)) {
            stream_seek(demuxer->stream,stream_tell(demuxer->stream)-5);
            return DEMUXER_TYPE_NSV;
        }
        hdr = (hdr << 8) | c;
    }

    return 0;
}

static void demux_close_nsv(demuxer_t* demuxer) {
    nsv_priv_t* priv = demuxer->priv;

    if(!priv)
        return;
    free(priv);

}


const demuxer_desc_t demuxer_desc_nsv = {
  "NullsoftVideo demuxer",
  "nsv",
  "Nullsoft Streaming Video",
  "Reza Jelveh",
  "nsv and nsa streaming files",
  DEMUXER_TYPE_NSV,
  0, // safe but expensive autodetect
  nsv_check_file,
  demux_nsv_fill_buffer,
  demux_open_nsv,
  demux_close_nsv,
  demux_seek_nsv,
  NULL
};
