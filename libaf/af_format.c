/* This audio output filter changes the format of a data block. Valid
   formats are: AFMT_U8, AFMT_S8, AFMT_S16_LE, AFMT_S16_BE
   AFMT_U16_LE, AFMT_U16_BE, AFMT_S32_LE and AFMT_S32_BE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../libao2/afmt.h"

#include "af.h"

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

int decode(int format)
{
  // Check input format
  switch(format){
  case(AFMT_U8):
    return LE|B08|US;
  case(AFMT_S8):
    return LE|B08|SI; break;
  case(AFMT_S16_LE):
    return LE|B16|SI; break;
  case(AFMT_S16_BE):
    return BE|B16|SI; break;
  case(AFMT_U16_LE):	
    return LE|B16|US; break;
  case(AFMT_U16_BE):	
    return BE|B16|US; break;
  case(AFMT_S32_LE):
    return LE|B32|SI; break;
  case(AFMT_S32_BE):	
    return BE|B32|SI; break;
  case(AFMT_IMA_ADPCM):		
  case(AFMT_MU_LAW):
  case(AFMT_A_LAW):
  case(AFMT_MPEG):
  case(AFMT_AC3):
    mp_msg(MSGT_AFILTER,MSGL_ERR,"[af_format] Input audio format not yet supported \n");
    return 0;
  default: 
    //This can not happen .... 
    mp_msg(MSGT_AFILTER,MSGL_ERR,"Unrecognized input audio format\n");
    return 0;
  }

}

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT:
    // Make sure this filter isn't redundant 
    if(af->data->format == ((af_data_t*)arg)->format && af->data->bps == ((af_data_t*)arg)->bps)
      return AF_DETACH;

    af->data->rate = ((af_data_t*)arg)->rate;
    af->data->nch  = ((af_data_t*)arg)->nch;
    af->mul.n      = af->data->bps;
    af->mul.d      = ((af_data_t*)arg)->bps;
    return AF_OK;
  case AF_CONTROL_FORMAT:
    // Reinit must be called after this function has been called
    
    // Sanity check for sample format
    if(0 == ((int)af->setup=decode(((af_data_t*)arg)->format)))
      return AF_ERROR;
    af->data->format = ((af_data_t*)arg)->format;

    // Sanity check for bytes per sample
    if(((af_data_t*)arg)->bps != 4 && ((af_data_t*)arg)->bps != 2 && ((af_data_t*)arg)->bps != 1){
      mp_msg(MSGT_AFILTER,MSGL_ERR,"[format] The number of output bytes per sample must be 1, 2 or 4. Current value is%i \n",((af_data_t*)arg)->bps);
      return AF_ERROR;
    }
    af->data->bps=((af_data_t*)arg)->bps; 

    mp_msg(MSGT_AFILTER,MSGL_V,"[format] Changing number sample format to 0x%08X and/or bytes per sample to %i \n",af->data->format,af->data->bps);
    return AF_OK;
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
  if(af->data)
    free(af->data);
  (int)af->setup = 0;  
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   l   = af->data;		// Local data
  void*        la  = NULL;		// Local audio
  int	       lf  = (int)af->setup;	// Local format
  af_data_t*   c   = data;		// Current working data
  void*        ca  = c->audio;	   	// Current audio
  int	       cf  = decode(c->format); // Current format
  register int i   = 0;			// Counter
  int 	       len = c->len>>(cf&NBITS_MASK); // Loop end

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  la = l->audio;

  // Change to little endian
  if((cf&END_MASK)!=LE){
    switch(cf&NBITS_MASK){
    case(B16):{
      register uint16_t s;
      for(i=1;i<len;i++){
	s=((uint16_t*)ca)[i];
	((uint16_t*)ca)[i]=(uint16_t)(((s&0x00FF)<<8) | (s&0xFF00)>>8);
      }
    }
    break;
    case(B32):{
      register uint32_t s;
      for(i=1;i<len;i++){
	s=((uint32_t*)ca)[i];
	((uint32_t*)ca)[i]=(uint32_t)(((s&0x000000FF)<<24) | ((s&0x0000FF00)<<8) |
				      ((s&0x00FF0000)>>8)  | ((s&0xFF000000)>>24));
      }
    }
    break;
    }
  }
  // Change signed/unsigned
  if((cf&SIGN_MASK) != (lf&SIGN_MASK)){
    switch((cf&NBITS_MASK)){
    case(B08):
      switch(cf&SIGN_MASK){
      case(US):
	for(i=0;i<len;i++)
	((int8_t*)ca)[i]=(int8_t)(SCHAR_MIN+((int)((uint8_t*)ca)[i]));
	break;
      case(SI):
	for(i=0;i<len;i++)
	((uint8_t*)ca)[i]=(uint8_t)(SCHAR_MAX+((int)((int8_t*)ca)[i]));
	break;
      }
      break;
    case(B16):
      switch(cf&SIGN_MASK){
      case(US):
	for(i=0;i<len;i++)
	  ((int16_t*)ca)[i]=(int16_t)(SHRT_MIN+((int)((uint16_t*)ca)[i]));
	break;
      case(SI):
	for(i=0;i<len;i++)
	  ((uint16_t*)ca)[i]=(uint16_t)(SHRT_MAX+((int)((int16_t*)ca)[i]));
	break;
      }
      break;
    case(B32):
      switch(cf&SIGN_MASK){
      case(US):
	for(i=0;i<len;i++)
	((int32_t*)ca)[i]=(int32_t)(INT_MIN+((uint32_t*)ca)[i]);
	break;
      case(SI):
	for(i=0;i<len;i++)
	((uint32_t*)ca)[i]=(uint32_t)(INT_MAX+((int32_t*)ca)[i]);
	break;
      }
      break;
    }	
  }
  // Change the number of bits
  if((cf&NBITS_MASK) == (lf&NBITS_MASK)){
    memcpy(la,ca,c->len);
  } else {
    switch(cf&NBITS_MASK){
    case(B08):
      switch(lf&NBITS_MASK){
      case(B16):
	for(i=1;i<len;i++)
	  ((uint16_t*)la)[i]=((uint16_t)((uint8_t*)ca)[i])<<8;
	break;
      case(B32):
	for(i=1;i<len;i++)
	  ((uint32_t*)la)[i]=((uint32_t)((uint8_t*)ca)[i])<<24;
	break;
      }
      break;
    case(B16):
      switch(lf&NBITS_MASK){
      case(B08):
	for(i=0;i<len;i++)
	  ((uint8_t*)la)[i]=(uint8_t)((((uint16_t*)ca)[i])>>8);
	break;
      case(B32):
	for(i=1;i<len;i++)
	  ((uint32_t*)la)[i]=((uint32_t)((uint16_t*)ca)[i])<<16;
	break;
      }
      break;
    case(B32):
      switch(lf&NBITS_MASK){
      case(B08):
	for(i=0;i<len;i++)
	  ((uint8_t*)la)[i]=(uint8_t)((((uint32_t*)ca)[i])>>24);
	break;
      case(B16):
	for(i=1;i<len;i++)
	  ((uint16_t*)la)[i]=(uint16_t)((((uint32_t*)ca)[i])>>16);
	break;
      }
      break;      
    }
  }
  // Switch to the correct endainess (again the problem with sun?)
  if((lf&END_MASK)!=LE){
    switch(cf&NBITS_MASK){
    case(B16):{
      register uint16_t s;
      for(i=1;i<len;i++){
	s=((uint16_t*)la)[i];
	((uint16_t*)la)[i]=(uint16_t)(((s&0x00FF)<<8) | (s&0xFF00)>>8);
      }
    }
    break;
    case(B32):{
      register uint32_t s;
      for(i=1;i<len;i++){
	s=((uint32_t*)la)[i];
	((uint32_t*)la)[i]=(uint32_t)(((s&0x000000FF)<<24) | ((s&0x0000FF00)<<8) |
				      ((s&0x00FF0000)>>8)  | ((s&0xFF000000)>>24));
      }
    }
    break;
    }
  }

  // Set output data

  // Make sure no samples are lost
  c->len    = (c->len*l->bps)/c->bps;
  c->audio  = l->audio;
  c->bps    = l->bps;
  c->format = l->format;
  return c;
}

// Allocate memory and set function pointers
static int open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  if(af->data == NULL)
    return AF_ERROR;
  (int)af->setup = 0;  
  return AF_OK;
}

// Description of this filter
af_info_t af_info_format = {
  "Sample format conversion",
  "format",
  "Anders",
  "",
  AF_FLAGS_REENTRANT,
  open
};
