/*
 * LMLM4 MPEG4 Compression Card stream & file parser
 * Copyright (C) 2003 Maxim Yevtyushkin <max@linuxmedialabs.com>
 * based on SMJPEG file parser by Alex Beregszaszi
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
#include <string.h> /* strtok */

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

typedef struct FrameInfo
{
    ssize_t frameSize;
    ssize_t paddingSize;
    int     frameType;
    int     channelNo;
} FrameInfo;

#define FRAMETYPE_I             0
#define FRAMETYPE_P             1
#define FRAMETYPE_B             2
#define FRAMETYPE_AUDIO_MPEG1L2 4
#define FRAMETYPE_AUDIO_ULAW    5
#define FRAMETYPE_AUDIO_ADPCM   6

#define PACKET_BLOCK_SIZE   0x00000200
#define PACKET_BLOCK_LAST   0x000001FF
#define PACKET_BLOCK_MASK   0xFFFFFE00

#define MAX_PACKET_SIZE   1048576 // 1 Mb

#define STREAM_START_CODE_SIZE   4

/*
// codes in MSB first
static unsigned int start_code [] =
{
    0xB0010000,         // VISUAL_OBJECT_SEQUENCE_START_CODE
    0xB6010000,         // VOP_START_CODE
    0x04C4FDFF,          // MPEG1LAYERII_START_CODE
    0x00000000          // end of start codes list
};
*/

static int imeHeaderValid(FrameInfo *frame)
{
    if ( frame->channelNo > 7 ||
         frame->frameSize > MAX_PACKET_SIZE || frame->frameSize <= 0)
    {
        mp_msg(MSGT_DEMUX, MSGL_V, "Invalid packet in LMLM4 stream: ch=%d size=%d\n", frame->channelNo, frame->frameSize);
        return 0;
    }
    switch (frame->frameType) {
    case FRAMETYPE_I:
    case FRAMETYPE_P:
    case FRAMETYPE_B:
    case FRAMETYPE_AUDIO_MPEG1L2:
    case FRAMETYPE_AUDIO_ULAW:
    case FRAMETYPE_AUDIO_ADPCM:
        break;
    default:
        mp_msg(MSGT_DEMUX, MSGL_V, "Invalid packet in LMLM4 stream (wrong packet type %d)\n", frame->frameType);
        return 0;
    }
    return 1;
}

/*
int searchMPEG4Stream(demuxer_t* demuxer, IME6400Header *imeHeader)
{
    void *data;
    ssize_t imeHeaderSize = sizeof(IME6400Header);
    ssize_t dataSize = sizeof(IME6400Header) * 3;
    ssize_t ptr = imeHeaderSize * 2;
    int errNo, startCodeNo;
    off_t pos;

    data = malloc(dataSize);

    imeHeaderSwap(imeHeader);
    memcpy(data + imeHeaderSize, imeHeader, imeHeaderSize);

//    printHex(data + imeHeaderSize, imeHeaderSize);

    while ((errNo = stream_read(demuxer->stream, data + imeHeaderSize * 2 , imeHeaderSize)) == imeHeaderSize)
    {
//        printHex(data + imeHeaderSize * 2, imeHeaderSize);

        pos = stream_tell(demuxer->stream);
        while (dataSize - ptr >= STREAM_START_CODE_SIZE) {
            startCodeNo = 0;
            while (start_code[startCodeNo])
            {
                if (memcmp(&start_code[startCodeNo], data + ptr, STREAM_START_CODE_SIZE) == 0) // start code match
                {
                    memcpy(imeHeader, data + ptr - imeHeaderSize, imeHeaderSize);
                    imeHeaderSwap(imeHeader);
                    if (imeHeaderValid(imeHeader))
                    {
                        stream_seek(demuxer->stream, pos - (dataSize - ptr));
                        free(data);
                        return 0;
                    }
                }
                startCodeNo++;
            }
            ptr++;
        }
        memcpy(data,data + imeHeaderSize, imeHeaderSize * 2);
        ptr -= imeHeaderSize;
    }

    free(data);
    return errNo;
}
*/

static int getFrame(demuxer_t *demuxer, FrameInfo *frameInfo)
{
    unsigned int packetSize;

    frameInfo->channelNo = stream_read_word(demuxer->stream);
    frameInfo->frameType = stream_read_word(demuxer->stream);
    packetSize=stream_read_dword(demuxer->stream);

    if(stream_eof(demuxer->stream)){
        frameInfo->frameSize = 0;
	return 0;
    }

    frameInfo->frameSize = packetSize - 8; //sizeof(IME6400Header);
    frameInfo->paddingSize = (packetSize & PACKET_BLOCK_LAST) ? PACKET_BLOCK_SIZE - (packetSize & PACKET_BLOCK_LAST) : 0;

    mp_msg(MSGT_DEMUX, MSGL_DBG2, "typ: %d chan: %d size: %d pad: %d\n",
            frameInfo->frameType,
            frameInfo->channelNo,
            frameInfo->frameSize,
            frameInfo->paddingSize);

    if(!imeHeaderValid(frameInfo)){
	// skip this packet
	stream_skip(demuxer->stream,PACKET_BLOCK_SIZE-8);
        frameInfo->frameSize = 0;
	return -1;
    }

    return 1;
}

static int lmlm4_check_file(demuxer_t* demuxer)
{
    FrameInfo frameInfo;
    unsigned int first;

    mp_msg(MSGT_DEMUX, MSGL_V, "Checking for LMLM4 Stream Format\n");

    if(getFrame(demuxer, &frameInfo)!=1){
	stream_skip(demuxer->stream,-8);
        mp_msg(MSGT_DEMUX, MSGL_V, "LMLM4 Stream Format not found\n");
        return 0;
    }
    first=stream_read_dword(demuxer->stream);
    stream_skip(demuxer->stream,-12);

    mp_msg(MSGT_DEMUXER,MSGL_V,"LMLM4: first=0x%08X\n",first);

    switch(frameInfo.frameType){
    case FRAMETYPE_AUDIO_MPEG1L2:
	if( (first & 0xffe00000) != 0xffe00000 ){
    	    mp_msg(MSGT_DEMUXER,MSGL_V,"LMLM4: not mpeg audio\n");
    	    return 0;
	}
	if((4-((first>>17)&3))!=2){
    	    mp_msg(MSGT_DEMUXER,MSGL_V,"LMLM4: not layer-2\n");
    	    return 0;
	}
	if(((first>>10)&0x3)==3){
    	    mp_msg(MSGT_DEMUXER,MSGL_V,"LMLM4: invalid audio sampelrate\n");
    	    return 0;
	}
	mp_msg(MSGT_DEMUXER,MSGL_V,"LMLM4: first packet is audio, header checks OK!\n");
	break;
    // TODO: add checks for video header too, for case of disabled audio
    }


//    stream_reset(demuxer->stream);
    mp_msg(MSGT_DEMUX, MSGL_V, "LMLM4 Stream Format found\n");

    return DEMUXER_TYPE_LMLM4;
}

static int video = 0;
static int frames= 0;

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_lmlm4_fill_buffer(demuxer_t *demux, demux_stream_t *ds)
{
    FrameInfo frameInfo;
    double pts;
    int id=1;
    int ret;

//hdr:
    demux->filepos = stream_tell(demux->stream);
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "fpos = %"PRId64"\n", (int64_t)demux->filepos);

    ret=getFrame(demux, &frameInfo);
    if(ret<=0) return ret; // EOF/error

    pts=demux->video->sh ? frames*((sh_video_t*)(demux->video->sh))->frametime : 0;

    switch(frameInfo.frameType){
    case FRAMETYPE_AUDIO_MPEG1L2:
        mp_dbg(MSGT_DEMUX, MSGL_DBG2, "Audio Packet\n");
        if (!video)
        {
            stream_skip(demux->stream, frameInfo.frameSize + frameInfo.paddingSize);
            mp_msg(MSGT_DEMUX, MSGL_V, "Skip Audio Packet\n");
            return -1; //goto hdr;
        }
	if(demux->audio->id==-1){
	    if(!demux->a_streams[id]) new_sh_audio(demux,id);
	    demux->audio->id=id;
	    demux->audio->sh=demux->a_streams[id];
	    ((sh_audio_t*)(demux->audio->sh))->format=0x50; // mpeg audio layer 1/2
	}
	if(demux->audio->id==id)
	    ds_read_packet(demux->audio, demux->stream, frameInfo.frameSize,
		pts, demux->filepos, 0);
	else
	    stream_skip(demux->stream,frameInfo.frameSize);
	break;
    case FRAMETYPE_I:
        if (!video) {
            video = 1;
            mp_dbg(MSGT_DEMUX, MSGL_DBG2, "First Video Packet\n");
        }
    case FRAMETYPE_P:
	frames=(frames+1)&(1024*1024-1); // wrap around at 4 hrs to avoid inaccurate float calculations
        if (!video)
        {
            stream_skip(demux->stream, frameInfo.frameSize + frameInfo.paddingSize);
            mp_msg(MSGT_DEMUX, MSGL_V, "Skip Video P Packet\n");
            return -1; //goto hdr;
        }
        mp_dbg(MSGT_DEMUX, MSGL_DBG2, "Video Packet\n");
	if(demux->video->id==-1){
	    if(!demux->v_streams[id]) new_sh_video(demux,id);
	    demux->video->id=id;
	    demux->video->sh=demux->v_streams[id];
	    ((sh_video_t*)(demux->video->sh))->format=0x10000004; // mpeg4-ES
	}
	if(demux->video->id==id)
	    ds_read_packet(demux->video, demux->stream, frameInfo.frameSize,
		pts, demux->filepos, 0);
	break;
    default:
	stream_skip(demux->stream,frameInfo.frameSize);
    }

    stream_skip(demux->stream, frameInfo.paddingSize);

    return 1;
}

static demuxer_t* demux_open_lmlm4(demuxer_t* demuxer){
    sh_audio_t *sh_audio=NULL;
    sh_video_t *sh_video=NULL;

#if 0
    sh_video_t* sh_video;
    sh_audio_t* sh_audio;
    unsigned int htype = 0, hleng;
    int i = 0;

    sh_video = new_sh_video(demuxer, 0);
    demuxer->video->sh = sh_video;
    sh_video->ds = demuxer->video;
    sh_video->disp_w = 640;
    sh_video->disp_h = 480;
    sh_video->format = mmioFOURCC('D','I','V','X');

    sh_video->bih = malloc(sizeof(BITMAPINFOHEADER));
    memset(sh_video->bih, 0, sizeof(BITMAPINFOHEADER));

    /* these are false values */
    sh_video->bih->biSize = 40;
    sh_video->bih->biWidth = sh_video->disp_w;
    sh_video->bih->biHeight = sh_video->disp_h;
    sh_video->bih->biPlanes = 3;
    sh_video->bih->biBitCount = 16;
    sh_video->bih->biCompression = sh_video->format;
    sh_video->bih->biSizeImage = sh_video->disp_w*sh_video->disp_h;

    sh_audio = new_sh_audio(demuxer, 0);
    demuxer->audio->sh = sh_audio;
    sh_audio->ds = demuxer->audio;

    sh_audio->wf = malloc(sizeof(WAVEFORMATEX));
    memset(sh_audio->wf, 0, sizeof(WAVEFORMATEX));

    sh_audio->samplerate = 48000;
    sh_audio->wf->wBitsPerSample = 16;
    sh_audio->channels = 2;
    sh_audio->format = 0x50;
    sh_audio->wf->wFormatTag = sh_audio->format;
    sh_audio->wf->nChannels = sh_audio->channels;
    sh_audio->wf->nSamplesPerSec = sh_audio->samplerate;
    sh_audio->wf->nAvgBytesPerSec = sh_audio->wf->nChannels*
    sh_audio->wf->wBitsPerSample*sh_audio->wf->nSamplesPerSec/8;
    sh_audio->wf->nBlockAlign = sh_audio->channels *2;
    sh_audio->wf->cbSize = 0;

#endif

    demuxer->seekable = 0;

    if(!ds_fill_buffer(demuxer->video)){
        mp_msg(MSGT_DEMUXER,MSGL_INFO,"LMLM4: " MSGTR_MissingVideoStream);
        demuxer->video->sh=NULL;
    } else {
        sh_video=demuxer->video->sh;sh_video->ds=demuxer->video;
    }
    if(demuxer->audio->id!=-2) {
        if(!ds_fill_buffer(demuxer->audio)){
            mp_msg(MSGT_DEMUXER,MSGL_INFO,"LMLM4: " MSGTR_MissingAudioStream);
            demuxer->audio->sh=NULL;
        } else {
            sh_audio=demuxer->audio->sh;sh_audio->ds=demuxer->audio;
        }
    }

    return demuxer;
}

static void demux_close_lmlm4(demuxer_t *demuxer)
{
//    printf("Close LMLM4 Stream\n");
    return;
}


const demuxer_desc_t demuxer_desc_lmlm4 = {
  "LMLM4 MPEG4 Compression Card stream demuxer",
  "lmlm4",
  "RAW LMLM4",
  "Maxim Yevtyushkin",
  "",
  DEMUXER_TYPE_LMLM4,
  0, // unsafe autodetect
  lmlm4_check_file,
  demux_lmlm4_fill_buffer,
  demux_open_lmlm4,
  demux_close_lmlm4,
  NULL,
  NULL
};
