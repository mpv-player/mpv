/* This is a null audio out plugin it doesnt't really do anything
   useful but serves an example of how audio plugins work. It delays
   the output signal by the nuber of samples set by aop_delay n
   where n is the number of bytes.
 */
#define PLUGIN

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
  int 	 in;  	     // Input fomat
  int    out;        // Output fomat
  double sz_mult;    // data size multiplier
} pl_format_t;

static pl_format_t pl_format={NULL,0,0,0,1};

// Number of bits
#define B08		0 
#define B16  		1	
#define B32  		2
#define NBITS_MASK	3

// Endianess
#define BE 		(0<<3) // Big endian
#define LE 		(1<<3) // Little endian
#define END_MASK	(1<<3)

// Signed
#define US		(0<<4)
#define SI		(1<<4)
#define SIGN_MASK	(1<<4)

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    if(pl_format.data) 
      uninit();
    pl_format.len = ao_plugin_data.len;
    pl_format.data=(void*)malloc(ao_plugin_data.len);
    ao_plugin_data.len=(int)((double)ao_plugin_data.len*pl_format.sz_mult);
    return CONTROL_OK;
  }
  return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){
  int i=0;
  int sign=0;
  int nbits=8;
  int be_le=BE;

  // Sheck input format
  switch(ao_plugin_data.format){
  case(AFMT_U8):
    pl_format.in=LE|B08|US; break;
  case(AFMT_S8):
    pl_format.in=LE|B08|SI; break;
  case(AFMT_S16_LE):
    pl_format.in=LE|B16|US; break;
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
    printf("[pl_format] Audio format not yet suported \n");
    return 0;
  default: 
    printf("[pl_format] Unsupported audio format\n"); // Should never happen...
    return 0;
  }
  // Sheck output format
  switch(ao_plugin_cfg.pl_format_type){
  case(AFMT_U8):
    pl_format.in=LE|B08|US; break;
  case(AFMT_S8):
    pl_format.in=LE|B08|SI; break;
  case(AFMT_S16_LE):
    pl_format.in=LE|B16|US; break;
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
    printf("[pl_format] Audio format not yet suported \n");
    return 0;
  default:
    printf("[pl_format] Unsupported audio format\n"); // Should never happen...
    return 0;
  }
  // We are changing the format
  ao_plugin_data.format=ao_plugin_cfg.pl_format_type;
  
  // And perhaps the buffer size
  pl_format.sz_mult=1;
  if((pl_format.in&NBITS_MASK) < (pl_format.out&NBITS_MASK))
    pl_format.sz_mult/=(double)(1<<(pl_format.out-pl_format.in));
  if((pl_format.in&NBITS_MASK) > (pl_format.out&NBITS_MASK))
    pl_format.sz_mult*=(double)(1<<(pl_format.out-pl_format.in));
  ao_plugin_data.sz_mult*=pl_format.sz_mult;
  return 1;
}

// close plugin
static void uninit(){
  if(pl_format.data) 
    free(pl_format.data);
}

// empty buffers
static void reset(){
  int i = 0;
  for(i=0;i<pl_format.len;i++)
    ((char*)pl_format.data)[i]=0;
}

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){
  register int i=0;
  void* in_data=ao_plugin_data.data;
  void* out_data=pl_format.data;
  int in_len=((int)(double)pl_format.len*pl_format.sz_mult);
  in_len>>=pl_format.in&NBITS_MASK;

  if((pl_format.in&END_MASK)!=(pl_format.out&END_MASK)){
    switch(pl_format.in&NBITS_MASK){
    case(B16):{
      register int16_t s;
      for(i=1;i<in_len;i++){
	s=((int16_t*)in_data)[i];
	((int16_t*)in_data)[i]=(int16_t)(((s&0x00FF)<<8) | (s&0xFF00)>>8);
      }
      break;
    }
    case(B32):{
      register int32_t s;
      for(i=1;i<in_len;i++){
	s=((int32_t*)in_data)[i];
	((int32_t*)in_data)[i]=(int32_t)(((s&0x000000FF)<<24) | ((s&0x0000FF00)<<8) |
	                                 ((s&0x00FF0000)>>8)  | ((s&0xFF000000)>>24));
      }
      break;
    }
    }
  }
  
  return 1;
}



