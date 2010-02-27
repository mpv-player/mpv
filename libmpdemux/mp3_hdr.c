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

#include "config.h"
#include "mp_msg.h"
#include "mp3_hdr.h"

//----------------------- mp3 audio frame header parser -----------------------

static int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,0} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0} }
};

static long freqs[9] = { 44100, 48000, 32000,	// MPEG 1.0
			 22050, 24000, 16000,   // MPEG 2.0
			 11025, 12000,  8000};  // MPEG 2.5

/*
 * return frame size or -1 (bad frame)
 */
int mp_get_mp3_header(unsigned char* hbuf,int* chans, int* srate, int* spf, int* mpa_layer, int* br){
    int stereo,ssize,lsf,framesize,padding,bitrate_index,sampling_frequency, divisor;
    int bitrate;
    int layer, mult[3] = { 12000, 144000, 144000 };
    unsigned long newhead =
      hbuf[0] << 24 |
      hbuf[1] << 16 |
      hbuf[2] <<  8 |
      hbuf[3];

//    printf("head=0x%08X\n",newhead);

    // head_check:
    if( (newhead & 0xffe00000) != 0xffe00000 ){
	mp_msg(MSGT_DEMUXER,MSGL_DBG2,"head_check failed\n");
	return -1;
    }

    layer = 4-((newhead>>17)&3);
    if(layer==4){
      mp_msg(MSGT_DEMUXER,MSGL_DBG2,"not layer-1/2/3\n");
      return -1;
    }

    sampling_frequency = ((newhead>>10)&0x3);  // valid: 0..2
    if(sampling_frequency==3){
	mp_msg(MSGT_DEMUXER,MSGL_DBG2,"invalid sampling_frequency\n");
	return -1;
    }

    if( newhead & ((long)1<<20) ) {
      // MPEG 1.0 (lsf==0) or MPEG 2.0 (lsf==1)
      lsf = (newhead & ((long)1<<19)) ? 0x0 : 0x1;
      sampling_frequency += (lsf*3);
    } else {
      // MPEG 2.5
      lsf = 1;
      sampling_frequency += 6;
    }

//    crc = ((newhead>>16)&0x1)^0x1;
    bitrate_index = ((newhead>>12)&0xf);  // valid: 1..14
    padding   = ((newhead>>9)&0x1);
//    fr->extension = ((newhead>>8)&0x1);
//    fr->mode      = ((newhead>>6)&0x3);
//    fr->mode_ext  = ((newhead>>4)&0x3);
//    fr->copyright = ((newhead>>3)&0x1);
//    fr->original  = ((newhead>>2)&0x1);
//    fr->emphasis  = newhead & 0x3;

    stereo    = ( (((newhead>>6)&0x3)) == 3) ? 1 : 2;

// !checked later through tabsel_123[]!
//    if(!bitrate_index || bitrate_index==15){
//      mp_msg(MSGT_DEMUXER,MSGL_DBG2,"Free format not supported.\n");
//      return -1;
//    }

    if(lsf)
      ssize = (stereo == 1) ? 9 : 17;
    else
      ssize = (stereo == 1) ? 17 : 32;
    if(!((newhead>>16)&0x1)) ssize += 2; // CRC

    bitrate = tabsel_123[lsf][layer-1][bitrate_index];
    framesize = bitrate * mult[layer-1];

    mp_msg(MSGT_DEMUXER,MSGL_DBG2,"FRAMESIZE: %d, layer: %d, bitrate: %d, mult: %d\n",
    	framesize, layer, tabsel_123[lsf][layer-1][bitrate_index], mult[layer-1]);
    if(!framesize){
	mp_msg(MSGT_DEMUXER,MSGL_DBG2,"invalid framesize/bitrate_index\n");
	return -1;
    }

    divisor = (layer == 3 ? (freqs[sampling_frequency] << lsf) : freqs[sampling_frequency]);
    framesize /= divisor;
    if(layer==1)
      framesize = (framesize+padding)*4;
    else
      framesize += padding;

//    if(framesize<=0 || framesize>MAXFRAMESIZE) return FALSE;
    if(srate) {
      *srate = freqs[sampling_frequency];
      if(spf) {
        if(layer == 1)
	  *spf = 384;
        else if(layer == 2)
	  *spf = 1152;
        else if(*srate < 32000)
          *spf = 576;
        else
	  *spf = 1152;
      }
    }
    if(mpa_layer) *mpa_layer = layer;
    if(chans) *chans = stereo;
    if(br) *br = bitrate;

    return framesize;
}
