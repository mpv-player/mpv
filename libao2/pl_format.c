/* This audio output plugin changes the format of a data block. Valid
   output formats are: AFMT_U8, AFMT_S8, AFMT_S16_LE, AFMT_S16_BE
   AFMT_U16_LE, AFMT_U16_BE, AFMT_S32_LE and AFMT_S32_BE. The output
   format is spedified using the cfg switch 'format=NR' where NR is
   the number as given in libao2/afmt.h
*/

#define PLUGIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"

static ao_info_t info =
{
        "Sample format conversion audio plugin",
        "format",
        "Anders",
        ""
};

LIBAO_PLUGIN_EXTERN(format)

// local data
typedef struct pl_format_s
{
  void*  data;       // local audio data block
  int    len;        // local buffer length
  int 	 in;  	     // input fomat
  int    out;        // output fomat
  double sz_mult;    // data size multiplier
} pl_format_t;

static pl_format_t pl_format={NULL,0,0,0,1};

// Number of bits
#define B08		(0<<0) 
#define B16  		(1<<0)	
#define B32  		(2<<0)
#define NBITS_MASK	(3<<0)

// Endianess
#define BE 		(0<<2) // Big Endian
#define LE 		(1<<2) // Little Endian
#define END_MASK	(1<<2)

// Signed
#define US		(0<<3) // Un Signed
#define SI		(1<<3) // SIgned
#define SIGN_MASK	(1<<3)

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    if(pl_format.data) 
      free(pl_format.data);
    pl_format.len = ao_plugin_data.len;
    pl_format.data=(void*)malloc(ao_plugin_data.len);
    if(!pl_format.data)
      return CONTROL_ERROR;
    ao_plugin_data.len=(int)(((double)ao_plugin_data.len)/pl_format.sz_mult);
    return CONTROL_OK;
  }
  return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){
  // Sheck input format
  switch(ao_plugin_data.format){
  case(AFMT_U8):
    pl_format.in=LE|B08|US; break;
  case(AFMT_S8):
    pl_format.in=LE|B08|SI; break;
  case(AFMT_S16_LE):
    pl_format.in=LE|B16|SI; break;
  case(AFMT_S16_BE):
    pl_format.in=BE|B16|SI; break;
  case(AFMT_U16_LE):	
    pl_format.in=LE|B16|US; break;
  case(AFMT_U16_BE):	
    pl_format.in=BE|B16|US; break;
  case(AFMT_S32_LE):
    pl_format.in=LE|B32|SI; break;
  case(AFMT_S32_BE):	
    pl_format.in=BE|B32|SI; break;
  case(AFMT_IMA_ADPCM):		
  case(AFMT_MU_LAW):
  case(AFMT_A_LAW):
  case(AFMT_MPEG):
  case(AFMT_AC3):
    printf("[pl_format] Input audio format not yet suported \n");
    return 0;
  default: 
    printf("[pl_format] Unrecognised input audio format\n"); //This can not happen .... 
    return 0;
  }
  // Sheck output format
  switch(ao_plugin_cfg.pl_format_type){
  case(AFMT_U8):
    pl_format.out=LE|B08|US; break;
  case(AFMT_S8):
    pl_format.out=LE|B08|SI; break;
  case(AFMT_S16_LE):
    pl_format.out=LE|B16|SI; break;
  case(AFMT_S16_BE):
    pl_format.out=BE|B16|SI; break;
  case(AFMT_U16_LE):	
    pl_format.out=LE|B16|US; break;
  case(AFMT_U16_BE):	
    pl_format.out=BE|B16|US; break;
  case(AFMT_S32_LE):
    pl_format.out=LE|B32|SI; break;
  case(AFMT_S32_BE):	
    pl_format.out=BE|B32|SI; break;
  case(AFMT_IMA_ADPCM):		
  case(AFMT_MU_LAW):
  case(AFMT_A_LAW):
  case(AFMT_MPEG):
  case(AFMT_AC3):
    printf("[pl_format] Output audio format not yet suported \n");
    return 0;
  default:
    printf("[pl_format] Unrecognised audio output format\n");
    return 0;
  }

  // Tell the world what we are up to
  printf("[pl_format] Input format: %s, output format: %s \n",
	 audio_out_format_name(ao_plugin_data.format),
	 audio_out_format_name(ao_plugin_cfg.pl_format_type));

  // We are changing the format
  ao_plugin_data.format=ao_plugin_cfg.pl_format_type;

  // And perhaps the buffer size
  pl_format.sz_mult=1;
  if((pl_format.in&NBITS_MASK) > (pl_format.out&NBITS_MASK))
    pl_format.sz_mult/=(double)(1<<((pl_format.in&NBITS_MASK)-(pl_format.out&NBITS_MASK)));
  if((pl_format.in&NBITS_MASK) < (pl_format.out&NBITS_MASK))
    pl_format.sz_mult*=(double)(1<<((pl_format.out&NBITS_MASK)-(pl_format.in&NBITS_MASK)));
  ao_plugin_data.sz_mult/=pl_format.sz_mult;

  return 1;
}

// close plugin
static void uninit(){
  if(pl_format.data) 
    free(pl_format.data);
  pl_format.data=NULL;
}

// empty buffers
static void reset(){
  memset(pl_format.data, 0, pl_format.len);
}

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
// FIXME: this routine needs to be optimized (it is probably possible to do a lot here)
static int play(){
  register int i=0;
  void* in_data=ao_plugin_data.data;
  void* out_data=pl_format.data;
  int len=(ao_plugin_data.len)>>(pl_format.in&NBITS_MASK);
  ao_plugin_data.len=(int)(((double)ao_plugin_data.len)*pl_format.sz_mult);
  
  // Change to little endian (Is this true for sun ?)
  if((pl_format.in&END_MASK)!=LE){
    switch(pl_format.in&NBITS_MASK){
    case(B16):{
      register uint16_t s;
      for(i=1;i<len;i++){
	s=((uint16_t*)in_data)[i];
	((uint16_t*)in_data)[i]=(uint16_t)(((s&0x00FF)<<8) | (s&0xFF00)>>8);
      }
    }
    break;
    case(B32):{
      register uint32_t s;
      for(i=1;i<len;i++){
	s=((uint32_t*)in_data)[i];
	((uint32_t*)in_data)[i]=(uint32_t)(((s&0x000000FF)<<24) | ((s&0x0000FF00)<<8) |
					   ((s&0x00FF0000)>>8)  | ((s&0xFF000000)>>24));
      }
    }
    break;
    }
  }
  // Change signed/unsigned
  if((pl_format.in&SIGN_MASK) != (pl_format.out&SIGN_MASK)){
    switch((pl_format.in&NBITS_MASK)){
    case(B08):
      switch(pl_format.in&SIGN_MASK){
      case(US):
	for(i=0;i<len;i++)
	((int8_t*)in_data)[i]=(int8_t)(-127+((int)((uint8_t*)in_data)[i]));
	break;
      case(SI):
	for(i=0;i<len;i++)
	((uint8_t*)in_data)[i]=(uint8_t)(+128+((int)((int8_t*)in_data)[i]));
	break;
      }
      break;
    case(B16):
      switch(pl_format.in&SIGN_MASK){
      case(US):
	for(i=0;i<len;i++)
	  ((int16_t*)in_data)[i]=(int16_t)(-32767+((int)((uint16_t*)in_data)[i]));
	break;
      case(SI):
	for(i=0;i<len;i++)
	  ((uint16_t*)in_data)[i]=(uint16_t)(+32768+((int)((int16_t*)in_data)[i]));
	break;
      }
      break;
    case(B32):
      switch(pl_format.in&SIGN_MASK){
      case(US):
	for(i=0;i<len;i++)
	((int32_t*)in_data)[i]=(int32_t)(-(1<<31-1)+((uint32_t*)in_data)[i]);
	break;
      case(SI):
	for(i=0;i<len;i++)
	((uint32_t*)in_data)[i]=(uint32_t)(+(1<<31)+((int32_t*)in_data)[i]);
	break;
      }
      break;
    }	
  }
  // Change the number of bits
  if((pl_format.in&NBITS_MASK) == (pl_format.out&NBITS_MASK)){
    int sz=(int)((double)ao_plugin_data.len/pl_format.sz_mult);
    for(i=0;i<sz;i++)
      ((char*)out_data)[i]=((char*)in_data)[i];
  } else {
    switch(pl_format.in&NBITS_MASK){
    case(B08):
      switch(pl_format.out&NBITS_MASK){
      case(B16):
	for(i=1;i<len;i++)
	  ((uint16_t*)out_data)[i]=((uint16_t)((uint8_t*)in_data)[i])<<8;
	break;
      case(B32):
	for(i=1;i<len;i++)
	  ((uint32_t*)out_data)[i]=((uint32_t)((uint8_t*)in_data)[i])<<24;
	break;
      }
      break;
    case(B16):
      switch(pl_format.out&NBITS_MASK){
      case(B08):
	for(i=0;i<len;i++)
	  ((uint8_t*)out_data)[i]=(uint8_t)((((uint16_t*)in_data)[i])>>8);
	break;
      case(B32):
	for(i=1;i<len;i++)
	  ((uint32_t*)out_data)[i]=((uint32_t)((uint16_t*)in_data)[i])<<16;
	break;
      }
      break;
    case(B32):
      switch(pl_format.out&NBITS_MASK){
      case(B08):
	for(i=0;i<len;i++)
	  ((uint8_t*)out_data)[i]=(uint8_t)((((uint32_t*)in_data)[i])>>24);
	break;
      case(B16):
	for(i=1;i<len;i++)
	  ((uint16_t*)out_data)[i]=(uint16_t)((((uint32_t*)in_data)[i])>>16);
	break;
      }
      break;      
    }
  }
  // Switch to the correct endainess (agiain the problem with sun?)
  if((pl_format.out&END_MASK)!=LE){
    switch(pl_format.in&NBITS_MASK){
    case(B16):{
      register uint16_t s;
      for(i=1;i<len;i++){
	s=((uint16_t*)out_data)[i];
	((uint16_t*)out_data)[i]=(uint16_t)(((s&0x00FF)<<8) | (s&0xFF00)>>8);
      }
    }
    break;
    case(B32):{
      register uint32_t s;
      for(i=1;i<len;i++){
	s=((uint32_t*)out_data)[i];
	((uint32_t*)out_data)[i]=(uint32_t)(((s&0x000000FF)<<24) | ((s&0x0000FF00)<<8) |
					    ((s&0x00FF0000)>>8)  | ((s&0xFF000000)>>24));
      }
    }
    break;
    }
  }
  ao_plugin_data.data=out_data;
  return 1;
}





