#include <stdio.h>

#include "config.h"
#include "../mp_msg.h"

//----------------------- mp3 audio frame header parser -----------------------

static int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};
static long freqs[9] = { 44100, 48000, 32000, 22050, 24000, 16000 , 11025 , 12000 , 8000 };

int mp_mp3_get_lsf(unsigned char* hbuf){
    unsigned long newhead = 
      hbuf[0] << 24 |
      hbuf[1] << 16 |
      hbuf[2] <<  8 |
      hbuf[3];
    if( newhead & ((long)1<<20) ) {
      return (newhead & ((long)1<<19)) ? 0x0 : 0x1;
    }
    return 1;
}

/*
 * return frame size or -1 (bad frame)
 */
int mp_decode_mp3_header(unsigned char* hbuf){
    int stereo,ssize,crc,lsf,mpeg25,framesize,padding,bitrate_index,sampling_frequency;
    unsigned long newhead = 
      hbuf[0] << 24 |
      hbuf[1] << 16 |
      hbuf[2] <<  8 |
      hbuf[3];

//    printf("head=0x%08X\n",newhead);

#if 1
    // head_check:
    if( (newhead & 0xffe00000) != 0xffe00000 ||  
        (newhead & 0x0000fc00) == 0x0000fc00){
	mp_msg(MSGT_DEMUXER,MSGL_DBG2,"head_check failed\n");
	return -1;
    }
#endif

    if((4-((newhead>>17)&3))!=3){ 
      mp_msg(MSGT_DEMUXER,MSGL_DBG2,"not layer-3\n"); 
      return -1;
    }

    if( newhead & ((long)1<<20) ) {
      lsf = (newhead & ((long)1<<19)) ? 0x0 : 0x1;
      mpeg25 = 0;
    } else {
      lsf = 1;
      mpeg25 = 1;
    }

    if(mpeg25)
      sampling_frequency = 6 + ((newhead>>10)&0x3);
    else
      sampling_frequency = ((newhead>>10)&0x3) + (lsf*3);

    if(sampling_frequency>8){
	mp_msg(MSGT_DEMUXER,MSGL_DBG2,"invalid sampling_frequency\n");
	return -1;  // valid: 0..8
    }

    crc = ((newhead>>16)&0x1)^0x1;
    bitrate_index = ((newhead>>12)&0xf);
    padding   = ((newhead>>9)&0x1);
//    fr->extension = ((newhead>>8)&0x1);
//    fr->mode      = ((newhead>>6)&0x3);
//    fr->mode_ext  = ((newhead>>4)&0x3);
//    fr->copyright = ((newhead>>3)&0x1);
//    fr->original  = ((newhead>>2)&0x1);
//    fr->emphasis  = newhead & 0x3;

    stereo    = ( (((newhead>>6)&0x3)) == 3) ? 1 : 2;

    if(!bitrate_index){
      mp_msg(MSGT_DEMUXER,MSGL_DBG2,"Free format not supported.\n");
      return -1;
    }

    if(lsf)
      ssize = (stereo == 1) ? 9 : 17;
    else
      ssize = (stereo == 1) ? 17 : 32;
    if(crc) ssize += 2;

    framesize  = (long) tabsel_123[lsf][2][bitrate_index] * 144000;
    framesize /= freqs[sampling_frequency]<<lsf;
    framesize += padding;

//    if(framesize<=0 || framesize>MAXFRAMESIZE) return FALSE;

    return framesize;
}

