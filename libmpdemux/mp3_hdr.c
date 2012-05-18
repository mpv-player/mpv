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
#include <stdint.h>

#include "config.h"
#include "mp3_hdr.h"
#include "mp_msg.h"

//----------------------- mp3 audio frame header parser -----------------------

static const uint16_t tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,0} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0} }
};

static const int freqs[9] = { 44100, 48000, 32000,   // MPEG 1.0
                              22050, 24000, 16000,   // MPEG 2.0
                              11025, 12000,  8000};  // MPEG 2.5

/*
 * return frame size or -1 (bad frame)
 */
int mp_get_mp3_header(unsigned char* hbuf,int* chans, int* srate, int* spf, int* mpa_layer, int* br){
    int stereo,lsf,framesize,padding,bitrate_index,sampling_frequency, divisor;
    int bitrate;
    int layer;
    static const int mult[3] = { 12000, 144000, 144000 };
    uint32_t newhead =
      hbuf[0] << 24 |
      hbuf[1] << 16 |
      hbuf[2] <<  8 |
      hbuf[3];

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

    sampling_frequency = (newhead>>10)&0x3;  // valid: 0..2
    if(sampling_frequency==3){
      mp_msg(MSGT_DEMUXER,MSGL_DBG2,"invalid sampling_frequency\n");
      return -1;
    }

    if( newhead & (1<<20) ) {
      // MPEG 1.0 (lsf==0) or MPEG 2.0 (lsf==1)
      lsf = !(newhead & (1<<19));
      sampling_frequency += lsf*3;
    } else {
      // MPEG 2.5
      lsf = 1;
      sampling_frequency += 6;
    }

    bitrate_index = (newhead>>12)&0xf;  // valid: 1..14
    padding   = (newhead>>9)&0x1;

    stereo    = ( ((newhead>>6)&0x3) == 3) ? 1 : 2;

    bitrate = tabsel_123[lsf][layer-1][bitrate_index];
    framesize = bitrate * mult[layer-1];

    mp_msg(MSGT_DEMUXER,MSGL_DBG2,"FRAMESIZE: %d, layer: %d, bitrate: %d, mult: %d\n",
           framesize, layer, tabsel_123[lsf][layer-1][bitrate_index], mult[layer-1]);
    if(!framesize){
      mp_msg(MSGT_DEMUXER,MSGL_DBG2,"invalid framesize/bitrate_index\n");
      return -1;
    }

    divisor = layer == 3 ? (freqs[sampling_frequency] << lsf) : freqs[sampling_frequency];
    framesize /= divisor;
    framesize += padding;
    if(layer==1)
      framesize *= 4;

    if(srate)
      *srate = freqs[sampling_frequency];
    if(spf) {
      if(layer == 1)
        *spf = 384;
      else if(layer == 2)
        *spf = 1152;
      else if(sampling_frequency > 2) // not 1.0
        *spf = 576;
      else
        *spf = 1152;
    }
    if(mpa_layer) *mpa_layer = layer;
    if(chans) *chans = stereo;
    if(br) *br = bitrate;

    return framesize;
}
